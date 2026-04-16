# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5044/6381 lines (79.05%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits |  Line | Source |
| ------: | ----: | :--- |
|       - |     1 | `/**` |
|       - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |     5 | ` */` |
|       - |     6 | `#include "ph7int.h"` |
|       - |     7 | `/*` |
|       - |     8 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|       - |     9 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|       - |    10 | ` * PH7 bytecode instructions.` |
|       - |    11 | ` */` |
|       - |    12 | `/* Forward declaration */` |
|       - |    13 | `typedef struct LangConstruct LangConstruct;` |
|       - |    14 | `typedef struct JumpFixup     JumpFixup;` |
|       - |    15 | `typedef struct Label         Label;` |
|       - |    16 | `/* Block [i.e: set of statements] control flags */` |
|       - |    17 | `#define GEN_BLOCK_LOOP        0x001    /* Loop block [i.e: for,while,...] */` |
|       - |    18 | `#define GEN_BLOCK_PROTECTED   0x002    /* Protected block */` |
|       - |    19 | `#define GEN_BLOCK_COND        0x004    /* Conditional block [i.e: if(condition){} ]*/` |
|       - |    20 | `#define GEN_BLOCK_FUNC        0x008    /* Function body */` |
|       - |    21 | `#define GEN_BLOCK_GLOBAL      0x010    /* Global block (always set)*/` |
|       - |    22 | `#define GEN_BLOC_NESTED_FUNC  0x020    /* Nested function body */` |
|       - |    23 | `#define GEN_BLOCK_EXPR        0x040    /* Expression */` |
|       - |    24 | `#define GEN_BLOCK_STD         0x080    /* Standard block */` |
|       - |    25 | `#define GEN_BLOCK_EXCEPTION   0x100    /* Exception block [i.e: try{ } }*/` |
|       - |    26 | `#define GEN_BLOCK_SWITCH      0x200    /* Switch statement */` |
|       - |    27 | `/*` |
|       - |    28 | ` * Each label seen in the input is recorded in an instance` |
|       - |    29 | ` * of the following structure.` |
|       - |    30 | ` * A label is a target point [i.e: a jump destination] that is specified` |
|       - |    31 | ` * by an identifier followed by a colon.` |
|       - |    32 | ` * Example` |
|       - |    33 | ` *  LABEL:` |
|       - |    34 | ` *		echo "hello\n";` |
|       - |    35 | ` */` |
|       - |    36 | `struct Label` |
|       - |    37 |  |
|       - |    38 | `	ph7_vm_func *pFunc;  /* Compiled function where the label was declared.NULL otherwise */` |
|       - |    39 | `	sxu32 nJumpDest;     /* Jump destination */` |
|       - |    40 | `	SyString sName;      /* Label name */` |
|       - |    41 | `	sxu32 nLine;         /* Line number this label occurs */` |
|       - |    42 | `	sxu8 bRef;           /* True if the label was referenced */` |
|       - |    43 | `};` |
|       - |    44 | `/*` |
|       - |    45 | ` * Compilation of some PHP constructs such as if, for, while, the logical or` |
|       - |    46 | ` * (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |    47 | ` * generation of forward jumps.` |
|       - |    48 | ` * Since the destination PC target of these jumps isn't known when the jumps` |
|       - |    49 | ` * are emitted, we record each forward jump in an instance of the following` |
|       - |    50 | ` * structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |    51 | ` */` |
|       - |    52 | `struct JumpFixup` |
|       - |    53 |  |
|       - |    54 | `	sxi32 nJumpType;     /* Jump type. Either TRUE jump, FALSE jump or Unconditional jump */` |
|       - |    55 | `	sxu32 nInstrIdx;     /* Instruction index to fix later when the jump destination is resolved. */` |
|       - |    56 | `	/* The following fields are only used by the goto statement */` |
|       - |    57 | `	SyString sLabel;    /* Label name */` |
|       - |    58 | `	ph7_vm_func *pFunc; /* Compiled function inside which the goto was emitted. NULL otherwise */` |
|       - |    59 | `	sxu32 nLine;        /* Track line number */` |
|       - |    60 | `};` |
|       - |    61 | `/*` |
|       - |    62 | ` * Each language construct is represented by an instance` |
|       - |    63 | ` * of the following structure.` |
|       - |    64 | ` */` |
|       - |    65 | `struct LangConstruct` |
|       - |    66 |  |
|       - |    67 | `	sxu32 nID;                     /* Language construct ID [i.e: PH7_TKWRD_WHILE,PH7_TKWRD_FOR,PH7_TKWRD_IF...] */` |
|       - |    68 | `	ProcLangConstruct xConstruct;  /* C function implementing the language construct */` |
|       - |    69 | `};` |
|       - |    70 | `/* Compilation flags */` |
|       - |    71 | `#define PH7_COMPILE_SINGLE_STMT 0x001 /* Compile a single statement */` |
|       - |    72 | `/* Token stream synchronization macros */` |
|       - |    73 | `#define SWAP_TOKEN_STREAM(GEN,START,END)\` |
|       - |    74 | `	pTmp  = GEN->pEnd;\` |
|       - |    75 | `	pGen->pIn  = START;\` |
|       - |    76 | `	pGen->pEnd = END` |
|       - |    77 | `#define UPDATE_TOKEN_STREAM(GEN)\` |
|       - |    78 | `	if( GEN->pIn < pTmp ){\` |
|       - |    79 | `	    GEN->pIn++;\` |
|       - |    80 | `	}\` |
|       - |    81 | `	GEN->pEnd = pTmp` |
|       - |    82 | `#define SWAP_DELIMITER(GEN,START,END)\` |
|       - |    83 | `	pTmpIn  = GEN->pIn;\` |
|       - |    84 | `	pTmpEnd = GEN->pEnd;\` |
|       - |    85 | `	GEN->pIn = START;\` |
|       - |    86 | `	GEN->pEnd = END` |
|       - |    87 | `#define RE_SWAP_DELIMITER(GEN)\` |
|       - |    88 | `	GEN->pIn  = pTmpIn;\` |
|       - |    89 | `	GEN->pEnd = pTmpEnd` |
|       - |    90 | `/* Flags related to expression compilation */` |
|       - |    91 | `#define EXPR_FLAG_LOAD_IDX_STORE    0x001 /* Set the iP2 flag when dealing with the LOAD_IDX instruction */` |
|       - |    92 | `#define EXPR_FLAG_RDONLY_LOAD       0x002 /* Read-only load, refer to the 'PH7_OP_LOAD' VM instruction for more information */` |
|       - |    93 | `#define EXPR_FLAG_COMMA_STATEMENT   0x004 /* Treat comma expression as a single statement (used by class attributes) */` |
|       - |    94 | `/* Forward declaration */` |
|       - |    95 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|       - |    96 | `/*` |
|       - |    97 | ` * Local utility routines used in the code generation phase.` |
|       - |    98 | ` */` |
|       - |    99 | `/*` |
|       - |   100 | ` * Check if the given name refer to a valid label.` |
|       - |   101 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|       - |   102 | ` * Any other return value indicates no such label.` |
|       - |   103 | ` */` |
|     148 |   104 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|       2 |   105 |  |
|       - |   106 | `	Label *aLabel;` |
|       - |   107 | `	sxu32 n;` |
|       - |   108 | `	/* Perform a linear scan on the label table */` |
|     150 |   109 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     330 |   110 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     274 |   111 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |   112 | `			/* Jump destination found */` |
|      94 |   113 | `			aLabel[n].bRef = TRUE;` |
|      94 |   114 | `			if( ppOut ){` |
|      94 |   115 | `				*ppOut = &aLabel[n];` |
|      46 |   116 | `			}` |
|      94 |   117 | `			return SXRET_OK;` |
|       - |   118 | `		}` |
|      92 |   119 | `	}` |
|       - |   120 | `	/* No such destination */` |
|      57 |   121 | `	return SXERR_NOTFOUND;` |
|      76 |   122 |  |
|       - |   123 | `/*` |
|       - |   124 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   125 | ` * compiled blocks.` |
|       - |   126 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   127 | ` */` |
|    3184 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3186 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    8987 |   131 | `	for(;;){` |
|   17976 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    3078 |   133 | `			iCount--; /* Decrement nesting level */` |
|    3078 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    3052 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   14926 |   140 | `		pBlock = pBlock->pParent;` |
|   14926 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1594 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  687700 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  687702 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  687702 |   162 | `	pBlock->pUserData   = pUserData;` |
|  687702 |   163 | `	pBlock->pGen        = pGen;` |
|  687702 |   164 | `	pBlock->iFlags      = iType;` |
|  687702 |   165 | `	pBlock->pParent     = 0;` |
|  687702 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  687702 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  687702 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  684782 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  684784 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  684784 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  684784 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  684784 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  684784 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  684784 |   200 | `	pGen->pCurrent = pBlock;` |
|  684784 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  332602 |   203 | `		*ppBlock = pBlock;` |
|  166300 |   204 | `	}` |
|  684784 |   205 | `	return SXRET_OK;` |
|  342393 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  684774 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  684776 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  684776 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  684776 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  684774 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  684776 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  684776 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  684776 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  684776 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  684774 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  684776 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  684776 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  684776 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  684776 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  684776 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  684776 |   244 | `	return SXRET_OK;` |
|  342389 |   245 |  |
|       - |   246 | `/*` |
|       - |   247 | ` * Emit a forward jump.` |
|       - |   248 | ` * Notes on forward jumps` |
|       - |   249 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   250 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   251 | ` *  generation of forward jumps.` |
|       - |   252 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   253 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   254 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |   255 | ` */` |
|  194668 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  194670 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  194670 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  194670 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  194670 |   265 | `	return rc;` |
|       2 |   266 |  |
|       - |   267 | `/*` |
|       - |   268 | ` * Fix a forward jump now the jump destination is resolved.` |
|       - |   269 | ` * Return the total number of fixed jumps.` |
|       - |   270 | ` * Notes on forward jumps:` |
|       - |   271 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   272 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   273 | ` *  generation of forward jumps.` |
|       - |   274 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   275 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   276 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|       - |   277 | ` */` |
|  479400 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  479402 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  863262 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  383862 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  153386 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  230478 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   35812 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  194668 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  194668 |   298 | `		if( pInstr ){` |
|  194668 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  194668 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  194668 |   302 | `			aFix[n].nJumpType = -1;` |
|   97333 |   303 | `		}` |
|   97335 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  479402 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  194684 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  194686 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  194832 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     150 |   326 | `		pJump = &aJumps[n];` |
|       - |   327 | `		/* Extract the target label */` |
|     150 |   328 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     150 |   329 | `		if( rc != SXRET_OK ){` |
|       - |   330 | `			/* No such label */` |
|      57 |   331 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      57 |   332 | `			if( rc == SXERR_ABORT ){` |
|       3 |   333 | `				return SXERR_ABORT;` |
|       - |   334 | `			}` |
|      55 |   335 | `			continue;` |
|       - |   336 | `		}` |
|       - |   337 | `		/* Make sure the target label is reachable */` |
|      94 |   338 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|       9 |   339 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|       9 |   340 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   341 | `				return SXERR_ABORT;` |
|       - |   342 | `			}` |
|       4 |   343 | `		}` |
|       - |   344 | `		/* Fix the jump now the destination is resolved */` |
|      94 |   345 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      94 |   346 | `		if( pInstr ){` |
|      94 |   347 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   348 | `		}` |
|      48 |   349 | `	}` |
|  194684 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  194816 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  194684 |   358 | `	return SXRET_OK;` |
|   97344 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  615486 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  615488 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  615488 |   367 | `	if( pEntry == 0 ){` |
|  267636 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  347854 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  347854 |   371 | `	return SXRET_OK;` |
|  307745 |   372 |  |
|       - |   373 | `/*` |
|       - |   374 | ` * Install a given constant index in the literal table.` |
|       - |   375 | ` * In order to be installed, the ph7_value must be of type string.` |
|       - |   376 | ` *` |
|       - |   377 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|       - |   378 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|       - |   379 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|       - |   380 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|       - |   381 | ` * many "" literals appear in user code.` |
|       - |   382 | ` */` |
|  267634 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  267636 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  267636 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  133817 |   387 | `	}` |
|  267636 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|  102088 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|  102090 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|  102090 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  102090 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|  102090 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|  102090 |   408 | `	return pObj;` |
|   51046 |   409 |  |
|       - |   410 | `/*` |
|       - |   411 | ` * Implementation of the PHP language constructs.` |
|       - |   412 | ` */` |
|       - |   413 | `/* Forward declaration */` |
|       - |   414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |   415 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|       - |   416 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|       - |   417 | `/* Forward decl: union type parser is defined later in this file. */` |
|       - |   418 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |   419 | `	ph7_gen_state *pGen,` |
|       - |   420 | `	sxu32 *pnType,` |
|       - |   421 | `	SyString *pClass,` |
|       - |   422 | `	SySet *pAlts,` |
|       - |   423 | `	sxi32 *piTypeFlags,` |
|       - |   424 | `	SyString *pTypeText,` |
|       - |   425 | `	int iNullableFlag,` |
|       - |   426 | `	int iUnionFlag,` |
|       - |   427 | `	int bAllowVoid,` |
|       - |   428 | `	sxu32 nLine` |
|       - |   429 | `);` |
|       - |   430 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |   431 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |   432 | `/*` |
|       - |   433 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |   434 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |   435 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |   436 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |   437 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |   438 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |   439 | ` * inputs like a thousand-digit number.` |
|       - |   440 | ` */` |
|       - |   441 | `#define GEN_NUM_SCRATCH 128` |
|       - |   442 | `/*` |
|       - |   443 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |   444 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |   445 | ` *   base  2 => 0 or 1` |
|       - |   446 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |   447 | ` *              decimal scan in the lexer)` |
|       - |   448 | ` */` |
|    1076 |   449 | `static int GenStateIsBaseDigit(int c, int base)` |
|       2 |   450 |  |
|    1078 |   451 | `	if( base == 16 ){ return SyisHex(c); }` |
|     980 |   452 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     702 |   453 | `	return SyisDigit(c);` |
|     540 |   454 |  |
|       - |   455 | `/*` |
|       - |   456 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |   457 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |   458 | ` * the exact wording PHP uses:` |
|       - |   459 | ` *` |
|       - |   460 | ` *   syntax error, unexpected identifier "X"` |
|       - |   461 | ` *` |
|       - |   462 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |   463 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |   464 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |   465 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |   466 | ` * no forward rescan needed.` |
|       - |   467 | ` *` |
|       - |   468 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |   469 | ` * returns 0 when it is well-formed.` |
|       - |   470 | ` */` |
|  102612 |   471 | `static int GenStateFindBadNumericSeparator(` |
|       - |   472 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   473 |  |
|  102614 |   474 | `	const char *z = pRaw->zString;` |
|  102614 |   475 | `	sxu32 n = pRaw->nByte;` |
|  102614 |   476 | `	int base = 10;` |
|       - |   477 | `	sxu32 i, start;` |
|  102614 |   478 | `	if( n < 2 ) return 0;` |
|    8648 |   479 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   480 | `		base = 16;` |
|    8613 |   481 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   482 | `		base = 2;` |
|     139 |   483 | `	}` |
|   31898 |   484 | `	for( i = 0; i < n; ++i ){` |
|   23266 |   485 | `		if( z[i] != '_' ) continue;` |
|     814 |   486 | `		if( i > 0 && i + 1 < n` |
|     543 |   487 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     540 |   488 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |   489 | `			continue; /* well-placed separator */` |
|       - |   490 | `		}` |
|       - |   491 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   492 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      15 |   493 | `		start = i;` |
|      20 |   494 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |   495 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       5 |   496 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |   497 | `		}` |
|      15 |   498 | `		*pBadStart = &z[start];` |
|      15 |   499 | `		*pBadLen = n - start;` |
|      15 |   500 | `		return 1;` |
|     ! 0 |   501 | `	}` |
|    8634 |   502 | `	return 0;` |
|   51308 |   503 |  |
|       - |   504 | `/*` |
|       - |   505 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   506 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   507 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   508 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   509 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   510 | ` * so callers can bail from the current construct).` |
|       - |   511 | ` */` |
|  102612 |   512 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   513 |  |
|  102614 |   514 | `	const char *zBad = 0;` |
|  102614 |   515 | `	sxu32 nBad = 0;` |
|       - |   516 | `	SyString sBad;` |
|       - |   517 | `	sxi32 rc;` |
|  102614 |   518 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  102600 |   519 | `		return SXRET_OK;` |
|       - |   520 | `	}` |
|      15 |   521 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   522 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   523 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   524 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   525 | `		return SXERR_ABORT;` |
|       - |   526 | `	}` |
|      15 |   527 | `	return SXERR_SYNTAX;` |
|   51308 |   528 |  |
|       - |   529 | `/*` |
|       - |   530 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |   531 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |   532 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |   533 | ` *` |
|       - |   534 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |   535 | ` * and *pzAlloc is set to NULL.` |
|       - |   536 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |   537 | ` * and *pzAlloc is set to NULL.` |
|       - |   538 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |   539 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |   540 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |   541 | ` *` |
|       - |   542 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |   543 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |   544 | ` */` |
|  102598 |   545 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   546 | `	SyMemBackend *pAlloc,` |
|       - |   547 | `	const SyString *pToken,` |
|       - |   548 | `	char *zScratch, sxu32 nScratch,` |
|       - |   549 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   550 |  |
|       - |   551 | `	sxu32 i, j;` |
|  102600 |   552 | `	int hasUnderscore = 0;` |
|       - |   553 | `	char *zBuf;` |
|  102600 |   554 | `	*pzAlloc = 0;` |
|  217750 |   555 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  115404 |   556 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   57577 |   557 | `	}` |
|  102600 |   558 | `	if( !hasUnderscore ){` |
|  102348 |   559 | `		SyStringDupPtr(pOut, pToken);` |
|  102348 |   560 | `		return SXRET_OK;` |
|       - |   561 | `	}` |
|     253 |   562 | `	if( pToken->nByte <= nScratch ){` |
|     251 |   563 | `		zBuf = zScratch;` |
|     126 |   564 | `	}else{` |
|       3 |   565 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |   566 | `		if( zBuf == 0 ){` |
|     ! 0 |   567 | `			return SXERR_ABORT;` |
|       - |   568 | `		}` |
|       3 |   569 | `		*pzAlloc = zBuf;` |
|       - |   570 | `	}` |
|     253 |   571 | `	j = 0;` |
|    2895 |   572 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |   573 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |   574 | `	}` |
|     253 |   575 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |   576 | `	return SXRET_OK;` |
|   51301 |   577 |  |
|       - |   578 | `/*` |
|       - |   579 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |   580 | ` * Notes on the integer type.` |
|       - |   581 | ` *  According to the PHP language reference manual` |
|       - |   582 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |   583 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |   584 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |   585 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |   586 | ` * Symisc eXtension to the integer type.` |
|       - |   587 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |   588 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |   589 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |   590 | ` *  [i.e: either 32bit or 64bit].` |
|       - |   591 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |   592 | ` *  documentation.` |
|       - |   593 | ` */` |
|  102584 |   594 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   595 |  |
|  102586 |   596 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  102586 |   597 | `	sxu32 nIdx = 0;` |
|       - |   598 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  102586 |   599 | `	char *zAlloc = 0;` |
|       - |   600 | `	SyString sNum;` |
|       - |   601 | `	sxi32 rc;` |
|   51292 |   602 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  102586 |   603 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  102586 |   604 | `	if( rc != SXRET_OK ){` |
|      11 |   605 | `		return rc;` |
|       - |   606 | `	}` |
|  153863 |   607 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   51287 |   608 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  102576 |   609 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   610 | `		return SXERR_ABORT;` |
|       - |   611 | `	}` |
|  102576 |   612 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   613 | `		ph7_value *pObj;` |
|       - |   614 | `		sxi64 iValue;` |
|  102090 |   615 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  102090 |   616 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  102090 |   617 | `		if( pObj == 0 ){` |
|     ! 0 |   618 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   619 | `			return SXERR_ABORT;` |
|       - |   620 | `		}` |
|  102090 |   621 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   51046 |   622 | `	}else{` |
|       - |   623 | `		/* Real number */` |
|       - |   624 | `		ph7_value *pObj;` |
|       - |   625 | `		/* Reserve a new constant */` |
|     488 |   626 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     488 |   627 | `		if( pObj == 0 ){` |
|     ! 0 |   628 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   629 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   630 | `			return SXERR_ABORT;` |
|       - |   631 | `		}` |
|     488 |   632 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     488 |   633 | `		PH7_MemObjToReal(pObj);` |
|       - |   634 | `	}` |
|  102576 |   635 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   636 | `	/* Emit the load constant instruction */` |
|  102576 |   637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   638 | `	/* Node successfully compiled */` |
|  102576 |   639 | `	return SXRET_OK;` |
|   51294 |   640 |  |
|       - |   641 | `/*` |
|       - |   642 | ` * Compile a single quoted string.` |
|       - |   643 | ` * According to the PHP language reference manual:` |
|       - |   644 | ` *` |
|       - |   645 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   646 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   647 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   648 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   649 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   650 | ` *` |
|       - |   651 | ` */` |
|   74066 |   652 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   653 |  |
|   74068 |   654 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   655 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   656 | `	ph7_value *pObj;` |
|       - |   657 | `	sxu32 nIdx;` |
|   74068 |   658 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   659 | `	/* Delimit the string */` |
|   74068 |   660 | `	zIn  = pStr->zString;` |
|   74068 |   661 | `	zEnd = &zIn[pStr->nByte];` |
|   74068 |   662 | `	if( zIn >= zEnd ){` |
|       - |   663 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   664 | `		 * rather than reserving a new object each time. */` |
|    5980 |   665 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    5980 |   666 | `		return SXRET_OK;` |
|       - |   667 | `	}` |
|   68090 |   668 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   669 | `		/* Already processed,emit the load constant instruction` |
|       - |   670 | `		 * and return.` |
|       - |   671 | `		 */` |
|   26982 |   672 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   26982 |   673 | `		return SXRET_OK;` |
|       - |   674 | `	}` |
|       - |   675 | `	/* Reserve a new constant */` |
|   41110 |   676 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   41110 |   677 | `	if( pObj == 0 ){` |
|     ! 0 |   678 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   679 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   680 | `		return SXERR_ABORT;` |
|       - |   681 | `	}` |
|   41110 |   682 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   683 | `	/* Compile the node */` |
|   41150 |   684 | `	for(;;){` |
|   82302 |   685 | `		if( zIn >= zEnd ){` |
|       - |   686 | `			/* End of input */` |
|   41110 |   687 | `			break;` |
|       - |   688 | `		}` |
|   41194 |   689 | `		zCur = zIn;` |
|  648182 |   690 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  606990 |   691 | `			zIn++;` |
|       2 |   692 | `		}` |
|   41194 |   693 | `		if( zIn > zCur ){` |
|       - |   694 | `			/* Append raw contents*/` |
|   41174 |   695 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   20586 |   696 | `		}` |
|   41194 |   697 | `		zIn++;` |
|   41194 |   698 | `		if( zIn < zEnd ){` |
|     105 |   699 | `			if( zIn[0] == '\\' ){` |
|       - |   700 | `				/* A literal backslash */` |
|      23 |   701 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |   702 | `			}else if( zIn[0] == '\'' ){` |
|       - |   703 | `				/* A single quote */` |
|      11 |   704 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   705 | `			}else{` |
|       - |   706 | `				/* verbatim copy */` |
|      73 |   707 | `				zIn--;` |
|      73 |   708 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |   709 | `				zIn++;` |
|       - |   710 | `			}` |
|      52 |   711 | `		}` |
|       - |   712 | `		/* Advance the stream cursor */` |
|   41194 |   713 | `		zIn++;` |
|       2 |   714 | `	}` |
|       - |   715 | `	/* Emit the load constant instruction */` |
|   41110 |   716 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   41110 |   717 | `	if( pStr->nByte < 1024 ){` |
|       - |   718 | `		/* Install in the literal table */` |
|   41110 |   719 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   20554 |   720 | `	}` |
|       - |   721 | `	/* Node successfully compiled */` |
|   41110 |   722 | `	return SXRET_OK;` |
|   37035 |   723 |  |
|       - |   724 | `/*` |
|       - |   725 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   726 | ` *` |
|       - |   727 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   728 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   729 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   730 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   731 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   732 | ` *` |
|       - |   733 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   734 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   735 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   736 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   737 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   738 | ` *     whitespace.` |
|       - |   739 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   740 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   741 | ` */` |
|     106 |   742 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       2 |   743 |  |
|     108 |   744 | `	SyString *pIn = &pGen->pIn->sData;` |
|     108 |   745 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   746 | `	const char *zPrefix;` |
|       - |   747 | `	const char *z, *zEnd;` |
|       - |   748 | `	char *zBuf, *zDst;` |
|     108 |   749 | `	if( nIndent == 0 ){` |
|       - |   750 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      64 |   751 | `		*pOut = *pIn;` |
|      64 |   752 | `		return SXRET_OK;` |
|       - |   753 | `	}` |
|       - |   754 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   755 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   756 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   757 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   758 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   759 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      46 |   760 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      46 |   761 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   762 | `		zPrefix += 2;` |
|     ! 0 |   763 | `	}else{` |
|      46 |   764 | `		zPrefix += 1;` |
|       - |   765 | `	}` |
|       - |   766 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      46 |   767 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      46 |   768 | `	if( zBuf == 0 ){` |
|     ! 0 |   769 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   770 | `		return SXERR_ABORT;` |
|       - |   771 | `	}` |
|      46 |   772 | `	zDst = zBuf;` |
|      46 |   773 | `	z = pIn->zString;` |
|      46 |   774 | `	zEnd = z + pIn->nByte;` |
|     128 |   775 | `	while( z < zEnd ){` |
|      70 |   776 | `		const char *zLine = z;` |
|       - |   777 | `		sxu32 nLine;` |
|       - |   778 | `		int bEmpty;` |
|     798 |   779 | `		while( z < zEnd && z[0] != '\n' ){` |
|     730 |   780 | `			z++;` |
|       2 |   781 | `		}` |
|      70 |   782 | `		nLine = (sxu32)(z - zLine);` |
|      70 |   783 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      70 |   784 | `		if( !bEmpty ){` |
|       - |   785 | `			sxu32 i;` |
|      66 |   786 | `			if( nLine < nIndent ){` |
|     ! 0 |   787 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   788 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   789 | `					nIndent);` |
|     ! 0 |   790 | `				return SXERR_ABORT;` |
|       - |   791 | `			}` |
|     268 |   792 | `			for( i = 0; i < nIndent; i++ ){` |
|     212 |   793 | `				if( zLine[i] != zPrefix[i] ){` |
|       9 |   794 | `					unsigned char c = (unsigned char)zLine[i];` |
|       9 |   795 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |   796 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   797 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |   798 | `					}else{` |
|       7 |   799 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   800 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   801 | `							nIndent);` |
|       - |   802 | `					}` |
|       9 |   803 | `					return SXERR_ABORT;` |
|       - |   804 | `				}` |
|     103 |   805 | `			}` |
|      57 |   806 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   807 | `			zDst += nLine - nIndent;` |
|      33 |   808 | `		}else if( nLine == 1 ){` |
|       - |   809 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   810 | `			*zDst++ = '\r';` |
|     ! 0 |   811 | `		}` |
|      61 |   812 | `		if( z < zEnd ){` |
|      25 |   813 | `			*zDst++ = '\n';` |
|      25 |   814 | `			z++;` |
|      12 |   815 | `		}` |
|       1 |   816 | `	}` |
|      37 |   817 | `	pOut->zString = zBuf;` |
|      37 |   818 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   819 | `	return SXRET_OK;` |
|      55 |   820 |  |
|       - |   821 | `/*` |
|       - |   822 | ` * Compile a nowdoc string.` |
|       - |   823 | ` * According to the PHP language reference manual:` |
|       - |   824 | ` *` |
|       - |   825 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   826 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   827 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   828 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   829 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   830 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   831 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   832 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   833 | ` *  of the closing identifier.` |
|       - |   834 | ` */` |
|      42 |   835 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   836 |  |
|       - |   837 | `	SyString sStripped;` |
|       - |   838 | `	SyString *pStr;` |
|       - |   839 | `	ph7_value *pObj;` |
|       - |   840 | `	sxu32 nIdx;` |
|       - |   841 | `	sxi32 rc;` |
|      44 |   842 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      44 |   843 | `	if( rc != SXRET_OK ){` |
|       5 |   844 | `		return rc;` |
|       - |   845 | `	}` |
|      40 |   846 | `	pStr = &sStripped;` |
|      40 |   847 | `	nIdx = 0; /* Prevent compiler warning */` |
|      40 |   848 | `	if( pStr->nByte <= 0 ){` |
|       - |   849 | `		/* Empty string,load NULL */` |
|       7 |   850 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   851 | `		return SXRET_OK;` |
|       - |   852 | `	}` |
|       - |   853 | `	/* Reserve a new constant */` |
|      34 |   854 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      34 |   855 | `	if( pObj == 0 ){` |
|     ! 0 |   856 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   857 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   858 | `		return SXERR_ABORT;` |
|       - |   859 | `	}` |
|       - |   860 | `	/* No processing is done here, simply a memcpy() operation */` |
|      34 |   861 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   862 | `	/* Emit the load constant instruction */` |
|      34 |   863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   864 | `	/* Node successfully compiled */` |
|      34 |   865 | `	return SXRET_OK;` |
|      23 |   866 |  |
|       - |   867 | `/*` |
|       - |   868 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |   869 | ` * According to the PHP language reference manual` |
|       - |   870 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |   871 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |   872 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |   873 | ` *  property in a string with a minimum of effort.` |
|       - |   874 | ` *  Simple syntax` |
|       - |   875 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |   876 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |   877 | ` *   the end of the name.` |
|       - |   878 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |   879 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |   880 | ` *   as to simple variables.` |
|       - |   881 | ` *  Complex (curly) syntax` |
|       - |   882 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |   883 | ` *   of complex expressions.` |
|       - |   884 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |   885 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |   886 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |   887 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |   888 | ` */` |
|    1940 |   889 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   890 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   891 | `	sxu32 nLine,         /* Line number */` |
|       - |   892 | `	const char *zIn,     /* Raw expression */` |
|       - |   893 | `	const char *zEnd     /* End of the expression */` |
|       - |   894 | `	)` |
|       2 |   895 |  |
|       - |   896 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   897 | `	SySet sToken;` |
|       - |   898 | `	sxi32 rc;` |
|       - |   899 | `	/* Initialize the token set */` |
|    1942 |   900 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   901 | `	/* Preallocate some slots */` |
|    1942 |   902 | `	SySetAlloc(&sToken,0x08);` |
|       - |   903 | `	/* Tokenize the text */` |
|    1942 |   904 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   905 | `	/* Swap delimiter */` |
|    1942 |   906 | `	pTmpIn  = pGen->pIn;` |
|    1942 |   907 | `	pTmpEnd = pGen->pEnd;` |
|    1942 |   908 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1942 |   909 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   910 | `	/* Compile the expression */` |
|    1942 |   911 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   912 | `	/* Restore token stream */` |
|    1942 |   913 | `	pGen->pIn  = pTmpIn;` |
|    1942 |   914 | `	pGen->pEnd = pTmpEnd;` |
|       - |   915 | `	/* Release the token set */` |
|    1942 |   916 | `	SySetRelease(&sToken);` |
|       - |   917 | `	/* Compilation result */` |
|    1942 |   918 | `	return rc;` |
|       2 |   919 |  |
|       - |   920 | `/*` |
|       - |   921 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   922 | ` */` |
|   18554 |   923 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   924 |  |
|       - |   925 | `	ph7_value *pConstObj;` |
|   18556 |   926 | `	sxu32 nIdx = 0;` |
|       - |   927 | `	/* Reserve a new constant */` |
|   18556 |   928 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   18556 |   929 | `	if( pConstObj == 0 ){` |
|     ! 0 |   930 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   931 | `		return 0;` |
|       - |   932 | `	}` |
|   18556 |   933 | `	(*pCount)++;` |
|   18556 |   934 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   935 | `	/* Emit the load constant instruction */` |
|   18556 |   936 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18556 |   937 | `	return pConstObj;` |
|    9279 |   938 |  |
|       - |   939 | `/*` |
|       - |   940 | ` * Compile a double quoted/heredoc string.` |
|       - |   941 | ` * According to the PHP language reference manual` |
|       - |   942 | ` * Heredoc` |
|       - |   943 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |   944 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |   945 | ` *  to close the quotation.` |
|       - |   946 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |   947 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |   948 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |   949 | ` *  Warning` |
|       - |   950 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |   951 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |   952 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |   953 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |   954 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |   955 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |   956 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |   957 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |   958 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |   959 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |   960 | ` * Double quoted` |
|       - |   961 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |   962 | ` *  Escaped characters Sequence 	Meaning` |
|       - |   963 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |   964 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |   965 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |   966 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |   967 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |   968 | ` *  \\ backslash` |
|       - |   969 | ` *  \$ dollar sign` |
|       - |   970 | ` *  \" double-quote` |
|       - |   971 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |   972 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |   973 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |   974 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |   975 | ` * See string parsing for details.` |
|       - |   976 | ` */` |
|   17164 |   977 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |   978 |  |
|   17166 |   979 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |   980 | `	const char *zIn,*zCur,*zEnd;` |
|   17166 |   981 | `	ph7_value *pObj = 0;` |
|       - |   982 | `	sxi32 iCons;` |
|       - |   983 | `	sxi32 rc;` |
|       - |   984 | `	/* Delimit the string */` |
|   17166 |   985 | `	zIn  = pStr->zString;` |
|   17166 |   986 | `	zEnd = &zIn[pStr->nByte];` |
|   17166 |   987 | `	if( zIn >= zEnd ){` |
|       - |   988 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |   989 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |   990 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |   991 | `		 */` |
|     234 |   992 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     234 |   993 | `		return SXRET_OK;` |
|       - |   994 | `	}` |
|   16934 |   995 | `	zCur = 0;` |
|       - |   996 | `	/* Compile the node */` |
|   16934 |   997 | `	iCons = 0;` |
|    9436 |   998 | `	for(;;){` |
|   28554 |   999 | `		zCur = zIn;` |
|  145290 |  1000 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  118678 |  1001 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      59 |  1002 | `				break;` |
|  118564 |  1003 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1828 |  1004 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     914 |  1005 | `					break;` |
|       - |  1006 | `			}` |
|  116738 |  1007 | `			zIn++;` |
|       2 |  1008 | `		}` |
|   28554 |  1009 | `		if( zIn > zCur ){` |
|   12936 |  1010 | `			if( pObj == 0 ){` |
|   12660 |  1011 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12660 |  1012 | `				if( pObj == 0 ){` |
|     ! 0 |  1013 | `					return SXERR_ABORT;` |
|       - |  1014 | `				}` |
|    6329 |  1015 | `			}` |
|   12936 |  1016 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6467 |  1017 | `		}` |
|   28554 |  1018 | `		if( zIn >= zEnd ){` |
|   16934 |  1019 | `			break;` |
|       - |  1020 | `		}` |
|   11622 |  1021 | `		if( zIn[0] == '\\' ){` |
|    9682 |  1022 | `			const char *zPtr = 0;` |
|       - |  1023 | `			sxu32 n;` |
|    9682 |  1024 | `			zIn++;` |
|    9682 |  1025 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1026 | `				break;` |
|       - |  1027 | `			}` |
|    9682 |  1028 | `			if( pObj == 0 ){` |
|    5898 |  1029 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5898 |  1030 | `				if( pObj == 0 ){` |
|     ! 0 |  1031 | `					return SXERR_ABORT;` |
|       - |  1032 | `				}` |
|    2948 |  1033 | `			}` |
|    9682 |  1034 | `			n = sizeof(char); /* size of conversion */` |
|    9682 |  1035 | `			switch( zIn[0] ){` |
|       3 |  1036 | `			case '$':` |
|       - |  1037 | `				/* Dollar sign */` |
|       7 |  1038 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  1039 | `				break;` |
|      38 |  1040 | `			case '\\':` |
|       - |  1041 | `				/* A literal backslash */` |
|      78 |  1042 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 |  1043 | `				break;` |
|       2 |  1044 | `			case 'a':` |
|       - |  1045 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  1046 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  1047 | `				break;` |
|       2 |  1048 | `			case 'b':` |
|       - |  1049 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  1050 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  1051 | `				break;` |
|       4 |  1052 | `			case 'f':` |
|       - |  1053 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1054 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1055 | `				break;` |
|    4475 |  1056 | `			case 'n':` |
|       - |  1057 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8952 |  1058 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8952 |  1059 | `				break;` |
|      19 |  1060 | `			case 'r':` |
|       - |  1061 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 |  1062 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 |  1063 | `				break;` |
|      24 |  1064 | `			case 't':` |
|       - |  1065 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 |  1066 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 |  1067 | `				break;` |
|       3 |  1068 | `			case 'v':` |
|       - |  1069 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1070 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1071 | `				break;` |
|       1 |  1072 | `			case '\'':` |
|       - |  1073 | `				/* Single quote */` |
|       3 |  1074 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1075 | `				break;` |
|      50 |  1076 | `			case '"':` |
|       - |  1077 | `				/* Double quote */` |
|     102 |  1078 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 |  1079 | `				break;` |
|       5 |  1080 | `			case '0':` |
|       - |  1081 | `				/* NUL byte */` |
|      11 |  1082 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 |  1083 | `				break;` |
|     188 |  1084 | `			case 'x':` |
|     377 |  1085 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1086 | `					int c;` |
|       - |  1087 | `					/* Hex digit */` |
|     363 |  1088 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 |  1089 | `					if( &zIn[2] < zEnd ){` |
|     363 |  1090 | `						c +=  SyHexToint(zIn[2]);` |
|     181 |  1091 | `					}` |
|       - |  1092 | `					/* Output char */` |
|     363 |  1093 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 |  1094 | `					n += sizeof(char) * 2;` |
|     182 |  1095 | `				}else{` |
|       - |  1096 | `					/* Output literal character  */` |
|      15 |  1097 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1098 | `				}` |
|     377 |  1099 | `				break;` |
|      15 |  1100 | `			case 'o':` |
|      31 |  1101 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  1102 | `					/* Octal digit stream */` |
|       - |  1103 | `					int c;` |
|      21 |  1104 | `					c = 0;` |
|      21 |  1105 | `					zIn++;` |
|      61 |  1106 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  1107 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  1108 | `							break;` |
|       - |  1109 | `						}` |
|      41 |  1110 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  1111 | `					}` |
|      21 |  1112 | `					if ( c > 0 ){` |
|      15 |  1113 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  1114 | `					}` |
|      21 |  1115 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  1116 | `				}else{` |
|       - |  1117 | `					/* Output literal character  */` |
|      11 |  1118 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  1119 | `				}` |
|      31 |  1120 | `				break;` |
|      11 |  1121 | `			default:` |
|       - |  1122 | `				/* Output without a slash */` |
|      23 |  1123 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  1124 | `				break;` |
|       - |  1125 | `			}` |
|       - |  1126 | `			/* Advance the stream cursor */` |
|    9682 |  1127 | `			zIn += n;` |
|    9682 |  1128 | `			continue;` |
|       - |  1129 | `		}` |
|    1942 |  1130 | `		if( zIn[0] == '{' ){` |
|       - |  1131 | `			/* Curly syntax */` |
|       - |  1132 | `			const char *zExpr;` |
|     117 |  1133 | `			sxi32 iNest = 1;` |
|     117 |  1134 | `			zIn++;` |
|     117 |  1135 | `			zExpr = zIn;` |
|       - |  1136 | `			/* Synchronize with the next closing curly braces */` |
|    1243 |  1137 | `			while( zIn < zEnd ){` |
|    1243 |  1138 | `				if( zIn[0] == '{' ){` |
|       - |  1139 | `					/* Increment nesting level */` |
|       9 |  1140 | `					iNest++;` |
|    1239 |  1141 | `				}else if(zIn[0] == '}' ){` |
|       - |  1142 | `					/* Decrement nesting level */` |
|     125 |  1143 | `					iNest--;` |
|     125 |  1144 | `					if( iNest <= 0 ){` |
|     117 |  1145 | `						break;` |
|       - |  1146 | `					}` |
|       4 |  1147 | `				}` |
|    1127 |  1148 | `				zIn++;` |
|       1 |  1149 | `			}` |
|       - |  1150 | `			/* Process the expression */` |
|     117 |  1151 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     117 |  1152 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1153 | `				return SXERR_ABORT;` |
|       - |  1154 | `			}` |
|     117 |  1155 | `			if( rc != SXERR_EMPTY ){` |
|     117 |  1156 | `				++iCons;` |
|      58 |  1157 | `			}` |
|     117 |  1158 | `			if( zIn < zEnd ){` |
|       - |  1159 | `				/* Jump the trailing curly */` |
|     117 |  1160 | `				zIn++;` |
|      58 |  1161 | `			}` |
|      59 |  1162 | `		}else{` |
|       - |  1163 | `			/* Simple syntax */` |
|    1826 |  1164 | `			const char *zExpr = zIn;` |
|       - |  1165 | `			/* Assemble variable name */` |
|     918 |  1166 | `			for(;;){` |
|       - |  1167 | `				/* Jump leading dollars */` |
|    3662 |  1168 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1826 |  1169 | `					zIn++;` |
|       2 |  1170 | `				}` |
|     918 |  1171 | `				for(;;){` |
|   10770 |  1172 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8016 |  1173 | `						zIn++;` |
|       2 |  1174 | `					}` |
|    1838 |  1175 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1176 | `						/* UTF-8 stream */` |
|     ! 0 |  1177 | `						zIn++;` |
|     ! 0 |  1178 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1179 | `							zIn++;` |
|     ! 0 |  1180 | `						}` |
|     ! 0 |  1181 | `						continue;` |
|       - |  1182 | `					}` |
|    1838 |  1183 | `					break;` |
|     ! 0 |  1184 | `				}` |
|    1838 |  1185 | `				if( zIn >= zEnd ){` |
|     112 |  1186 | `					break;` |
|       - |  1187 | `				}` |
|    1728 |  1188 | `				if( zIn[0] == '[' ){` |
|       9 |  1189 | `					sxi32 iSquare = 1;` |
|       9 |  1190 | `					zIn++;` |
|      17 |  1191 | `					while( zIn < zEnd ){` |
|      17 |  1192 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1193 | `							iSquare++;` |
|      17 |  1194 | `						}else if (zIn[0] == ']' ){` |
|       9 |  1195 | `							iSquare--;` |
|       9 |  1196 | `							if( iSquare <= 0 ){` |
|       9 |  1197 | `								break;` |
|       - |  1198 | `							}` |
|     ! 0 |  1199 | `						}` |
|       9 |  1200 | `						zIn++;` |
|       1 |  1201 | `					}` |
|       9 |  1202 | `					if( zIn < zEnd ){` |
|       9 |  1203 | `						zIn++;` |
|       4 |  1204 | `					}` |
|       9 |  1205 | `					break;` |
|    1720 |  1206 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1207 | `					sxi32 iCurly = 1;` |
|       6 |  1208 | `					zIn++;` |
|      18 |  1209 | `					while( zIn < zEnd ){` |
|      16 |  1210 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1211 | `							iCurly++;` |
|      16 |  1212 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1213 | `							iCurly--;` |
|       3 |  1214 | `							if( iCurly <= 0 ){` |
|       3 |  1215 | `								break;` |
|       - |  1216 | `							}` |
|     ! 0 |  1217 | `						}` |
|      14 |  1218 | `						zIn++;` |
|       2 |  1219 | `					}` |
|       6 |  1220 | `					if( zIn < zEnd ){` |
|       3 |  1221 | `						zIn++;` |
|       1 |  1222 | `					}` |
|       6 |  1223 | `					break;` |
|    1716 |  1224 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1225 | `					/* Member access operator '->' */` |
|      13 |  1226 | `					zIn += 2;` |
|    1710 |  1227 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1228 | `					/* Static member access operator '::' */` |
|     ! 0 |  1229 | `					zIn += 2;` |
|     ! 0 |  1230 | `				}else{` |
|     853 |  1231 | `					break;` |
|       - |  1232 | `				}` |
|       1 |  1233 | `			}` |
|       - |  1234 | `			/* Process the expression */` |
|    1826 |  1235 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1826 |  1236 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1237 | `				return SXERR_ABORT;` |
|       - |  1238 | `			}` |
|    1826 |  1239 | `			if( rc != SXERR_EMPTY ){` |
|    1824 |  1240 | `				++iCons;` |
|     911 |  1241 | `			}` |
|       - |  1242 | `		}` |
|       - |  1243 | `		/* Invalidate the previously used constant */` |
|    1942 |  1244 | `		pObj = 0;` |
|       2 |  1245 | `	}/*for(;;)*/` |
|   16934 |  1246 | `	if( iCons > 1 ){` |
|       - |  1247 | `		/* Concatenate all compiled constants */` |
|    1438 |  1248 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     718 |  1249 | `	}` |
|       - |  1250 | `	/* Node successfully compiled */` |
|   16934 |  1251 | `	return SXRET_OK;` |
|    8584 |  1252 |  |
|       - |  1253 | `/*` |
|       - |  1254 | ` * Compile a double quoted string.` |
|       - |  1255 | ` *  See the block-comment above for more information.` |
|       - |  1256 | ` */` |
|   17104 |  1257 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1258 |  |
|       - |  1259 | `	sxi32 rc;` |
|   17106 |  1260 | `	rc = GenStateCompileString(&(*pGen));` |
|    8552 |  1261 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1262 | `	/* Compilation result */` |
|   17106 |  1263 | `	return rc;` |
|       2 |  1264 |  |
|       - |  1265 | `/*` |
|       - |  1266 | ` * Compile a Heredoc string.` |
|       - |  1267 | ` *  See the block-comment above for more information.` |
|       - |  1268 | ` */` |
|      64 |  1269 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1270 |  |
|       - |  1271 | `	SyString sOrig, sStripped;` |
|       - |  1272 | `	sxi32 rc;` |
|      66 |  1273 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      66 |  1274 | `	if( rc != SXRET_OK ){` |
|       5 |  1275 | `		return rc;` |
|       - |  1276 | `	}` |
|       - |  1277 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1278 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1279 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1280 | `	 * unaffected, including on the error path. */` |
|      62 |  1281 | `	sOrig = pGen->pIn->sData;` |
|      62 |  1282 | `	pGen->pIn->sData = sStripped;` |
|      62 |  1283 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 |  1284 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1285 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 |  1286 | `	return rc;` |
|      34 |  1287 |  |
|       - |  1288 | `/*` |
|       - |  1289 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1290 | ` *  Notes on array entries.` |
|       - |  1291 | ` *  According to the PHP language reference manual` |
|       - |  1292 | ` *  An array can be created by the array() language construct.` |
|       - |  1293 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1294 | ` *  array(  key =>  value` |
|       - |  1295 | ` *    , ...` |
|       - |  1296 | ` *    )` |
|       - |  1297 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1298 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1299 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1300 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1301 | ` *  contain integer and string indices.` |
|       - |  1302 | ` *  A value can be any PHP type.` |
|       - |  1303 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1304 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1305 | ` *  is specified, that value will be overwritten.` |
|       - |  1306 | ` */` |
|   17082 |  1307 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1308 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1309 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1310 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1311 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1312 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1313 | `	)` |
|       2 |  1314 |  |
|       - |  1315 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1316 | `	sxi32 rc;` |
|       - |  1317 | `	/* Swap token stream */` |
|   17084 |  1318 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1319 | `	/* Compile the expression*/` |
|   17084 |  1320 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1321 | `	/* Restore token stream */` |
|   17084 |  1322 | `	RE_SWAP_DELIMITER(pGen);` |
|   17084 |  1323 | `	return rc;` |
|       2 |  1324 |  |
|       - |  1325 | `/*` |
|       - |  1326 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1327 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1328 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1329 | ` * error message.` |
|       - |  1330 | ` * See the routine responible of compiling the array language construct` |
|       - |  1331 | ` * for more inforation.` |
|       - |  1332 | ` */` |
|      30 |  1333 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1334 |  |
|      32 |  1335 | `	sxi32 rc = SXRET_OK;` |
|      32 |  1336 | `	if( pRoot->pOp ){` |
|      19 |  1337 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1338 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 |  1339 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1340 | `			/* Unexpected expression */` |
|      11 |  1341 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1342 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 |  1343 | `			if( rc != SXERR_ABORT ){` |
|      11 |  1344 | `				rc = SXERR_INVALID;` |
|       5 |  1345 | `			}` |
|       7 |  1346 | `		}` |
|      25 |  1347 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1348 | `		/* Unexpected expression */` |
|       3 |  1349 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1350 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1351 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1352 | `			rc = SXERR_INVALID;` |
|       1 |  1353 | `		}` |
|       1 |  1354 | `	}` |
|      32 |  1355 | `	return rc;` |
|       2 |  1356 |  |
|       - |  1357 | `/*` |
|       - |  1358 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1359 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1360 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1361 | ` */` |
|   25406 |  1362 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1363 |  |
|       - |  1364 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1365 | `	SyToken *pKey,*pCur;` |
|   25408 |  1366 | `	sxi32 iEmitRef = 0;` |
|   25408 |  1367 | `	sxi32 nPair = 0;` |
|       - |  1368 | `	sxi32 iNest;` |
|       - |  1369 | `	sxi32 rc;` |
|   25408 |  1370 | `	xValidator = 0;` |
|   20577 |  1371 | `	for(;;){` |
|       - |  1372 | `		/* Jump leading commas */` |
|   46442 |  1373 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5288 |  1374 | `			pGen->pIn++;` |
|       2 |  1375 | `		}` |
|   41156 |  1376 | `		pCur = pGen->pIn;` |
|   41156 |  1377 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1378 | `			/* No more entry to process */` |
|   25396 |  1379 | `			break;` |
|       - |  1380 | `		}` |
|   15762 |  1381 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1382 | `			continue;` |
|       - |  1383 | `		}` |
|       - |  1384 | `		/* Compile the key if available */` |
|   15762 |  1385 | `		pKey = pCur;` |
|   15762 |  1386 | `		iNest = 0;` |
|   43924 |  1387 | `		while( pCur < pGen->pIn ){` |
|   29384 |  1388 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 |  1389 | `				break;` |
|       - |  1390 | `			}` |
|       - |  1391 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1392 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1393 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1394 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1395 | `			 */` |
|   28168 |  1396 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      76 |  1397 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      76 |  1398 | `				SyToken *pFn = pCur;` |
|      74 |  1399 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 |  1400 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 |  1401 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1402 | `					pFn = &pCur[1];` |
|     ! 0 |  1403 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1404 | `				}` |
|      76 |  1405 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1406 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1407 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1408 | `						pCur++;` |
|     ! 0 |  1409 | `					}` |
|       5 |  1410 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1411 | `						pCur++;` |
|       5 |  1412 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1413 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1414 | `						if( pCur < pGen->pIn ){` |
|       5 |  1415 | `							pCur++;` |
|       2 |  1416 | `						}` |
|       2 |  1417 | `					}` |
|       5 |  1418 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1419 | `						pCur++;` |
|     ! 0 |  1420 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1421 | `							&& pCur->sData.nByte == 1` |
|     ! 0 |  1422 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1423 | `							pCur++;` |
|     ! 0 |  1424 | `						}` |
|     ! 0 |  1425 | `						if( pCur < pGen->pIn` |
|     ! 0 |  1426 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1427 | `							pCur++;` |
|     ! 0 |  1428 | `						}` |
|     ! 0 |  1429 | `					}` |
|       - |  1430 | `					/* The rest of the entry is the arrow function body — no` |
|       - |  1431 | `					 * outer key to extract. Stop the scan here. */` |
|       5 |  1432 | `					pCur = pGen->pIn;` |
|       5 |  1433 | `					break;` |
|       - |  1434 | `				}` |
|       - |  1435 | `				/* Match expression (PHP 8.0): 'match (subject) { ... }'.` |
|       - |  1436 | `				 * The '=>' inside match arms is not an array key/value separator —` |
|       - |  1437 | `				 * it introduces each arm's result expression. Skip past the full` |
|       - |  1438 | `				 * match span so the outer scan sees no false '=>'. */` |
|      72 |  1439 | `				if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1440 | `					pCur++; /* past 'match' */` |
|       3 |  1441 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1442 | `						pCur++;` |
|       3 |  1443 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1444 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1445 | `						if( pCur < pGen->pIn ){` |
|       3 |  1446 | `							pCur++;` |
|       1 |  1447 | `						}` |
|       1 |  1448 | `					}` |
|       3 |  1449 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1450 | `						pCur++;` |
|       3 |  1451 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1452 | `							PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1453 | `						if( pCur < pGen->pIn ){` |
|       3 |  1454 | `							pCur++;` |
|       1 |  1455 | `						}` |
|       1 |  1456 | `					}` |
|       3 |  1457 | `					continue;` |
|       - |  1458 | `				}` |
|      34 |  1459 | `			}` |
|   28162 |  1460 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      86 |  1461 | `				iNest++;` |
|   28120 |  1462 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - |  1463 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - |  1464 | `				 * parser will shortly detect any syntax error.` |
|       - |  1465 | `				 */` |
|      86 |  1466 | `				iNest--;` |
|      42 |  1467 | `			}` |
|   28162 |  1468 | `			pCur++;` |
|       2 |  1469 | `		}` |
|   15762 |  1470 | `		rc = SXERR_EMPTY;` |
|   15762 |  1471 | `		if( pCur < pGen->pIn ){` |
|    1218 |  1472 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1473 | `				/* Missing value */` |
|      11 |  1474 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 |  1475 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1476 | `					return SXERR_ABORT;` |
|       - |  1477 | `				}` |
|      11 |  1478 | `				return SXRET_OK;` |
|       - |  1479 | `			}` |
|       - |  1480 | `			/* Compile the expression holding the key */` |
|    1208 |  1481 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1482 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1208 |  1483 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1484 | `				return SXERR_ABORT;` |
|       - |  1485 | `			}` |
|    1208 |  1486 | `			pCur++; /* Jump the '=>' operator */` |
|   15149 |  1487 | `		}else if( pKey == pCur ){` |
|       - |  1488 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1489 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1490 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1491 | `		}else{` |
|       - |  1492 | `			/* Reset back the cursor and point to the entry value */` |
|   14546 |  1493 | `			pCur = pKey;` |
|       - |  1494 | `		}` |
|   15752 |  1495 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1496 | `			/* No available key,load NULL */` |
|   14548 |  1497 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7273 |  1498 | `		}` |
|   15752 |  1499 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1500 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 |  1501 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 |  1502 | `			iEmitRef = 1;` |
|      34 |  1503 | `			pCur++; /* Jump the '&' token */` |
|      34 |  1504 | `			if( pCur >= pGen->pIn ){` |
|       - |  1505 | `				/* Missing value */` |
|       3 |  1506 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1507 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1508 | `					return SXERR_ABORT;` |
|       - |  1509 | `				}` |
|       3 |  1510 | `				return SXRET_OK;` |
|       - |  1511 | `			}` |
|      15 |  1512 | `		}` |
|       - |  1513 | `		/* Compile indice value */` |
|   15750 |  1514 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15750 |  1515 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1516 | `			return SXERR_ABORT;` |
|       - |  1517 | `		}` |
|   15750 |  1518 | `		if( iEmitRef ){` |
|       - |  1519 | `			/* Emit the load reference instruction */` |
|      32 |  1520 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1521 | `		}` |
|   15750 |  1522 | `		xValidator = 0;` |
|   15750 |  1523 | `		iEmitRef = 0;` |
|   15750 |  1524 | `		nPair++;` |
|       2 |  1525 | `	}` |
|       - |  1526 | `	/* Emit the load map instruction */` |
|   25396 |  1527 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1528 | `	/* Node successfully compiled */` |
|   25396 |  1529 | `	return SXRET_OK;` |
|   12705 |  1530 |  |
|       - |  1531 | `/*` |
|       - |  1532 | ` * Compile the 'array' language construct.` |
|       - |  1533 | ` *	 According to the PHP language reference manual` |
|       - |  1534 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1535 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1536 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1537 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1538 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1539 | ` */` |
|   25108 |  1540 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1541 |  |
|       - |  1542 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   25110 |  1543 | `	pGen->pIn += 2;` |
|   25110 |  1544 | `	pGen->pEnd--;` |
|   12554 |  1545 | `	SXUNUSED(iCompileFlag);` |
|   25110 |  1546 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1547 |  |
|       - |  1548 | `/*` |
|       - |  1549 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1550 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1551 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1552 | ` */` |
|     298 |  1553 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1554 |  |
|       - |  1555 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     300 |  1556 | `	pGen->pIn++;` |
|     300 |  1557 | `	pGen->pEnd--;` |
|     149 |  1558 | `	SXUNUSED(iCompileFlag);` |
|     300 |  1559 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1560 |  |
|       - |  1561 | `/*` |
|       - |  1562 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1563 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1564 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1565 | ` * error message.` |
|       - |  1566 | ` * See the routine responible of compiling the list language construct` |
|       - |  1567 | ` * for more inforation.` |
|       - |  1568 | ` */` |
|     128 |  1569 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1570 |  |
|     130 |  1571 | `	sxi32 rc = SXRET_OK;` |
|     130 |  1572 | `	if( pRoot->pOp ){` |
|     ! 0 |  1573 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 |  1574 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1575 | `				/* Unexpected expression */` |
|     ! 0 |  1576 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1577 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1578 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1579 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1580 | `				}` |
|     ! 0 |  1581 | `		}` |
|     130 |  1582 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1583 | `		/* Unexpected expression */` |
|       5 |  1584 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1585 | `			"list(): Expecting a variable not an expression");` |
|       5 |  1586 | `		if( rc != SXERR_ABORT ){` |
|       5 |  1587 | `			rc = SXERR_INVALID;` |
|       2 |  1588 | `		}` |
|       2 |  1589 | `	}` |
|     130 |  1590 | `	return rc;` |
|       2 |  1591 |  |
|       - |  1592 | `/*` |
|       - |  1593 | ` * Compile the 'list' language construct.` |
|       - |  1594 | ` *  According to the PHP language reference` |
|       - |  1595 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1596 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1597 | ` *  Description` |
|       - |  1598 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1599 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1600 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1601 | ` *  Parameters` |
|       - |  1602 | ` *   $varname: A variable.` |
|       - |  1603 | ` *  Return Values` |
|       - |  1604 | ` *   The assigned array.` |
|       - |  1605 | ` */` |
|       - |  1606 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1607 | `struct NestedListEntry {` |
|       - |  1608 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1609 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1610 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1611 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1612 | `};` |
|       - |  1613 | `/*` |
|       - |  1614 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1615 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1616 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1617 | ` */` |
|      74 |  1618 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 |  1619 |  |
|       - |  1620 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1621 | `	SyToken *pNext;` |
|       - |  1622 | `	sxi32 nExpr;` |
|       - |  1623 | `	sxi32 rc;` |
|      76 |  1624 | `	nExpr = 0;` |
|      76 |  1625 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 |  1626 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 |  1627 | `		if( pGen->pIn < pNext ){` |
|       - |  1628 | `			/* Check for nested list() */` |
|     144 |  1629 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1630 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1631 | `				/* Record this nested list for post-processing */` |
|       3 |  1632 | `				SyToken *pListEnd = 0;` |
|       3 |  1633 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1634 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1635 | `				}` |
|       3 |  1636 | `				if( pListEnd ){` |
|       - |  1637 | `					struct NestedListEntry sEntry;` |
|       3 |  1638 | `					sEntry.nIndex = nExpr;` |
|       3 |  1639 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1640 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1641 | `					sEntry.isShort = 0;` |
|       3 |  1642 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1643 | `				}` |
|       - |  1644 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1645 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 |  1646 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1647 | `				/* Nested short destructuring [...] */` |
|      13 |  1648 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1649 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1650 | `				if( pBracketEnd ){` |
|       - |  1651 | `					struct NestedListEntry sEntry;` |
|      13 |  1652 | `					sEntry.nIndex = nExpr;` |
|      13 |  1653 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1654 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1655 | `					sEntry.isShort = 1;` |
|      13 |  1656 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1657 | `				}` |
|       - |  1658 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1659 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1660 | `			}else{` |
|       - |  1661 | `				/* Compile the expression holding the variable */` |
|     130 |  1662 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 |  1663 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1664 | `					SySetRelease(&sNested);` |
|     ! 0 |  1665 | `					return SXRET_OK;` |
|       - |  1666 | `				}` |
|       - |  1667 | `			}` |
|      73 |  1668 | `		}else{` |
|       - |  1669 | `			/* Empty entry,load NULL */` |
|      13 |  1670 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1671 | `		}` |
|     156 |  1672 | `		nExpr++;` |
|       - |  1673 | `		/* Advance the stream cursor */` |
|     156 |  1674 | `		pGen->pIn = &pNext[1];` |
|       2 |  1675 | `	}` |
|       - |  1676 | `	/* Emit the LOAD_LIST instruction */` |
|      76 |  1677 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1678 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1679 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1680 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1681 | `	 */` |
|      76 |  1682 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1683 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1684 | `		sxu32 i;` |
|      27 |  1685 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1686 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1687 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1688 | `			ph7_value *pIdx;` |
|       - |  1689 | `			sxu32 nConstIdx;` |
|       - |  1690 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1691 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1692 | `			/* Push the integer index for this nested entry */` |
|      15 |  1693 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1694 | `			if( pIdx == 0 ){` |
|     ! 0 |  1695 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1696 | `				SySetRelease(&sNested);` |
|     ! 0 |  1697 | `				return SXERR_ABORT;` |
|       - |  1698 | `			}` |
|      15 |  1699 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1700 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1701 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1702 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1703 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1704 | `			 */` |
|      15 |  1705 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1706 | `			/* Recursively compile the inner list */` |
|      15 |  1707 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1708 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1709 | `			if( apNested[i].isShort ){` |
|      13 |  1710 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1711 | `			}else{` |
|       3 |  1712 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1713 | `			}` |
|      15 |  1714 | `			pGen->pIn = pSavedIn;` |
|      15 |  1715 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1716 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1717 | `				SySetRelease(&sNested);` |
|     ! 0 |  1718 | `				return SXERR_ABORT;` |
|       - |  1719 | `			}` |
|       - |  1720 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1721 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1722 | `		}` |
|       6 |  1723 | `	}` |
|      76 |  1724 | `	SySetRelease(&sNested);` |
|       - |  1725 | `	/* Node successfully compiled */` |
|      76 |  1726 | `	return SXRET_OK;` |
|      39 |  1727 |  |
|      32 |  1728 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1729 |  |
|       - |  1730 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 |  1731 | `	pGen->pIn += 2;` |
|      34 |  1732 | `	pGen->pEnd--;` |
|      16 |  1733 | `	SXUNUSED(iCompileFlag);` |
|      34 |  1734 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1735 |  |
|      42 |  1736 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1737 |  |
|       - |  1738 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 |  1739 | `	pGen->pIn++;` |
|      44 |  1740 | `	pGen->pEnd--;` |
|      21 |  1741 | `	SXUNUSED(iCompileFlag);` |
|      44 |  1742 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1743 |  |
|       - |  1744 | `/* Forward declarations */` |
|       - |  1745 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1746 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1747 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1748 | `/*` |
|       - |  1749 | ` * Compile an annoynmous function or a closure.` |
|       - |  1750 | ` * According to the PHP language reference` |
|       - |  1751 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1752 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1753 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1754 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1755 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1756 | ` *  Example Anonymous function variable assignment example` |
|       - |  1757 | ` * <?php` |
|       - |  1758 | ` * $greet = function($name)` |
|       - |  1759 | ` * {` |
|       - |  1760 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1761 | ` * };` |
|       - |  1762 | ` * $greet('World');` |
|       - |  1763 | ` * $greet('PHP');` |
|       - |  1764 | ` * ?>` |
|       - |  1765 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1766 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1767 | ` */` |
|     176 |  1768 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1769 |  |
|       - |  1770 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1771 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1772 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1773 | `							  * one thread is allowed to compile the script.` |
|       - |  1774 | `						      */` |
|       - |  1775 | `	ph7_value *pObj;` |
|       - |  1776 | `	SyString sName;` |
|       - |  1777 | `	sxu32 nIdx;` |
|       - |  1778 | `	sxu32 nLen;` |
|       - |  1779 | `	sxi32 rc;` |
|       - |  1780 |  |
|     178 |  1781 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     178 |  1782 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1783 | `		pGen->pIn++;` |
|     ! 0 |  1784 | `	}` |
|       - |  1785 | `	/* Reserve a constant for the lambda */` |
|     178 |  1786 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     178 |  1787 | `	if( pObj == 0 ){` |
|     ! 0 |  1788 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1789 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1790 | `		return SXERR_ABORT;` |
|       - |  1791 | `	}` |
|       - |  1792 | `	/* Generate a unique name */` |
|     178 |  1793 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1794 | `	/* Make sure the generated name is unique */` |
|     178 |  1795 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1796 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1797 | `	}` |
|     178 |  1798 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     178 |  1799 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1800 | `	/* Compile the lambda body */` |
|     178 |  1801 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     178 |  1802 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1803 | `		return SXERR_ABORT;` |
|       - |  1804 | `	}` |
|     178 |  1805 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1806 | `		/* Emit the load closure instruction */` |
|      16 |  1807 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 |  1808 | `	}else{` |
|       - |  1809 | `		/* Emit the load constant instruction */` |
|     164 |  1810 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1811 | `	}` |
|       - |  1812 | `	/* Node successfully compiled */` |
|     178 |  1813 | `	return SXRET_OK;` |
|      90 |  1814 |  |
|       - |  1815 | `/*` |
|       - |  1816 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1817 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1818 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1819 | ` */` |
|     122 |  1820 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1821 | `	ph7_gen_state *pGen,` |
|       - |  1822 | `	ph7_vm_func *pFunc,` |
|       - |  1823 | `	const char *zName,` |
|       - |  1824 | `	sxu32 nByte,` |
|       - |  1825 | `	SyString *aShadow,` |
|       - |  1826 | `	sxu32 nShadow)` |
|       1 |  1827 |  |
|       - |  1828 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1829 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1830 | `	sxu32 n, nEnv;` |
|       - |  1831 | `	char *zDup;` |
|     123 |  1832 | `	if( nByte == 0 ){` |
|     ! 0 |  1833 | `		return SXRET_OK;` |
|       - |  1834 | `	}` |
|     122 |  1835 | `	if( nByte == sizeof("this")-1` |
|      66 |  1836 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1837 | `		return SXRET_OK;` |
|       - |  1838 | `	}` |
|     147 |  1839 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      94 |  1840 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      90 |  1841 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      69 |  1842 | `			return SXRET_OK;` |
|       - |  1843 | `		}` |
|      14 |  1844 | `	}` |
|      53 |  1845 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1846 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1847 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1848 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1849 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1850 | `			return SXRET_OK;` |
|       - |  1851 | `		}` |
|      15 |  1852 | `	}` |
|      53 |  1853 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1854 | `	if( zDup == 0 ){` |
|     ! 0 |  1855 | `		return SXERR_ABORT;` |
|       - |  1856 | `	}` |
|      53 |  1857 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1858 | `	sEnv.iFlags = 0;` |
|      53 |  1859 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1860 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1861 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1862 | `	return SXRET_OK;` |
|      62 |  1863 |  |
|       - |  1864 | `/*` |
|       - |  1865 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1866 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1867 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1868 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1869 | ` */` |
|      14 |  1870 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1871 | `	ph7_gen_state *pGen,` |
|       - |  1872 | `	ph7_vm_func *pFunc,` |
|       - |  1873 | `	const char *zIn,` |
|       - |  1874 | `	const char *zEnd,` |
|       - |  1875 | `	SyString *aShadow,` |
|       - |  1876 | `	sxu32 nShadow)` |
|       1 |  1877 |  |
|       - |  1878 | `	sxi32 rc;` |
|     159 |  1879 | `	while( zIn < zEnd ){` |
|     145 |  1880 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1881 | `			zIn++;` |
|     ! 0 |  1882 | `			if( zIn < zEnd ){` |
|     ! 0 |  1883 | `				zIn++;` |
|     ! 0 |  1884 | `			}` |
|     ! 0 |  1885 | `			continue;` |
|       - |  1886 | `		}` |
|     144 |  1887 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1888 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1889 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1890 | `			const char *zName;` |
|      13 |  1891 | `			zIn++; /* skip '$' */` |
|      13 |  1892 | `			zName = zIn;` |
|      39 |  1893 | `			while( zIn < zEnd ){` |
|      35 |  1894 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1895 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1896 | `					zIn++;` |
|     ! 0 |  1897 | `					while( zIn < zEnd` |
|     ! 0 |  1898 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1899 | `						zIn++;` |
|     ! 0 |  1900 | `					}` |
|     ! 0 |  1901 | `					continue;` |
|       - |  1902 | `				}` |
|      35 |  1903 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1904 | `					break;` |
|       - |  1905 | `				}` |
|      27 |  1906 | `				zIn++;` |
|       1 |  1907 | `			}` |
|      13 |  1908 | `			if( zIn > zName ){` |
|      19 |  1909 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1910 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1911 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1912 | `					return SXERR_ABORT;` |
|       - |  1913 | `				}` |
|       6 |  1914 | `			}` |
|      13 |  1915 | `			continue;` |
|       - |  1916 | `		}` |
|     133 |  1917 | `		zIn++;` |
|       1 |  1918 | `	}` |
|      15 |  1919 | `	return SXRET_OK;` |
|       8 |  1920 |  |
|       - |  1921 | `/*` |
|       - |  1922 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1923 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1924 | ` *   - plain $<id> pairs` |
|       - |  1925 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1926 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1927 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1928 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1929 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1930 | ` *     are never mistakenly captured.` |
|       - |  1931 | ` */` |
|     104 |  1932 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1933 | `	ph7_gen_state *pGen,` |
|       - |  1934 | `	ph7_vm_func *pFunc,` |
|       - |  1935 | `	SyToken *pStart,` |
|       - |  1936 | `	SyToken *pEnd,` |
|       - |  1937 | `	SyString *aShadow,` |
|       - |  1938 | `	sxu32 nShadow)` |
|       1 |  1939 |  |
|     105 |  1940 | `	SyToken *pScan = pStart;` |
|       - |  1941 | `	sxi32 rc;` |
|     389 |  1942 | `	while( pScan < pEnd ){` |
|     285 |  1943 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1944 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1945 | `				pScan->sData.zString,` |
|      14 |  1946 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1947 | `				aShadow,nShadow);` |
|      15 |  1948 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1949 | `				return SXERR_ABORT;` |
|       - |  1950 | `			}` |
|      15 |  1951 | `			pScan++;` |
|      15 |  1952 | `			continue;` |
|       - |  1953 | `		}` |
|     271 |  1954 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  1955 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  1956 | `			SyToken *pFnKw = pScan;` |
|      20 |  1957 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  1958 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  1959 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1960 | `				pFnKw = &pScan[1];` |
|     ! 0 |  1961 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1962 | `			}` |
|      21 |  1963 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  1964 | `				SyToken *pInnerSigStart;` |
|       - |  1965 | `				SyToken *pInnerSigEnd;` |
|       - |  1966 | `				SyToken *pInnerBodyEnd;` |
|       - |  1967 | `				SyString *aInnerShadow;` |
|       - |  1968 | `				sxu32 nInnerShadow;` |
|       - |  1969 | `				sxu32 nInnerParamMax;` |
|       - |  1970 | `				SyToken *p;` |
|       - |  1971 | `				int iNestInner;` |
|      19 |  1972 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  1973 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1974 | `					pScan++;` |
|     ! 0 |  1975 | `				}` |
|      19 |  1976 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  1977 | `					pScan++;` |
|     ! 0 |  1978 | `					continue;` |
|       - |  1979 | `				}` |
|      19 |  1980 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  1981 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  1982 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  1983 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  1984 | `					pScan = pEnd;` |
|     ! 0 |  1985 | `					continue;` |
|       - |  1986 | `				}` |
|       - |  1987 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  1988 | `				nInnerParamMax = 0;` |
|      57 |  1989 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  1990 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  1991 | `						nInnerParamMax++;` |
|       6 |  1992 | `					}` |
|      20 |  1993 | `				}` |
|      19 |  1994 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  1995 | `					&pGen->pVm->sAllocator,` |
|      18 |  1996 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  1997 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  1998 | `					return SXERR_ABORT;` |
|       - |  1999 | `				}` |
|      19 |  2000 | `				nInnerShadow = 0;` |
|      25 |  2001 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2002 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2003 | `				}` |
|      57 |  2004 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2005 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2006 | `						continue;` |
|       - |  2007 | `					}` |
|      13 |  2008 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2009 | `						break;` |
|       - |  2010 | `					}` |
|      13 |  2011 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2012 | `						continue;` |
|       - |  2013 | `					}` |
|      13 |  2014 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2015 | `				}` |
|      19 |  2016 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2017 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2018 | `					pScan++;` |
|     ! 0 |  2019 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2020 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2021 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2022 | `						pScan++;` |
|     ! 0 |  2023 | `					}` |
|     ! 0 |  2024 | `					if( pScan < pEnd` |
|     ! 0 |  2025 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2026 | `						pScan++;` |
|     ! 0 |  2027 | `					}` |
|     ! 0 |  2028 | `				}` |
|      19 |  2029 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2030 | `					pScan++; /* past '=>' */` |
|       9 |  2031 | `				}` |
|      19 |  2032 | `				pInnerBodyEnd = pScan;` |
|      19 |  2033 | `				iNestInner = 0;` |
|     131 |  2034 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2035 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2036 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2037 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2038 | `						break;` |
|       - |  2039 | `					}` |
|     113 |  2040 | `					if( pInnerBodyEnd->nType &` |
|       - |  2041 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2042 | `						iNestInner++;` |
|     112 |  2043 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2044 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2045 | `						iNestInner--;` |
|       1 |  2046 | `					}` |
|     113 |  2047 | `					pInnerBodyEnd++;` |
|       1 |  2048 | `				}` |
|       - |  2049 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2050 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2051 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2052 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2053 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2054 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2055 | `				 *` |
|       - |  2056 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2057 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2058 | `				 * range after the '=' sign. */` |
|       - |  2059 | `				{` |
|      19 |  2060 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2061 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2062 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2063 | `						SyToken *pEq = 0;` |
|      13 |  2064 | `						int iNestArg = 0;` |
|      49 |  2065 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2066 | `							if( iNestArg == 0` |
|      39 |  2067 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2068 | `								break;` |
|       - |  2069 | `							}` |
|      37 |  2070 | `							if( pArgEnd->nType &` |
|       - |  2071 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2072 | `								iNestArg++;` |
|      37 |  2073 | `							}else if( pArgEnd->nType &` |
|       - |  2074 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2075 | `								iNestArg--;` |
|     ! 0 |  2076 | `							}` |
|      36 |  2077 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2078 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2079 | `								pEq = pArgEnd;` |
|       3 |  2080 | `							}` |
|      37 |  2081 | `							pArgEnd++;` |
|       1 |  2082 | `						}` |
|      13 |  2083 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2084 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2085 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2086 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2087 | `								return SXERR_ABORT;` |
|       - |  2088 | `							}` |
|       3 |  2089 | `						}` |
|      13 |  2090 | `						pArgStart = pArgEnd;` |
|      12 |  2091 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2092 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2093 | `							pArgStart++;` |
|       1 |  2094 | `						}` |
|       1 |  2095 | `					}` |
|       - |  2096 | `				}` |
|      28 |  2097 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2098 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2099 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2100 | `					return SXERR_ABORT;` |
|       - |  2101 | `				}` |
|      19 |  2102 | `				pScan = pInnerBodyEnd;` |
|      19 |  2103 | `				continue;` |
|       - |  2104 | `			}` |
|       1 |  2105 | `		}` |
|     253 |  2106 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     143 |  2107 | `			pScan++;` |
|     143 |  2108 | `			continue;` |
|       - |  2109 | `		}` |
|       - |  2110 | `		{` |
|       - |  2111 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     111 |  2112 | `			SyToken *pDollar = pScan;` |
|     165 |  2113 | `			while( &pDollar[1] < pEnd` |
|     111 |  2114 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2115 | `				pDollar++;` |
|     ! 0 |  2116 | `			}` |
|     111 |  2117 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2118 | `				break;` |
|       - |  2119 | `			}` |
|     111 |  2120 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2121 | `				pScan = pDollar + 1;` |
|     ! 0 |  2122 | `				continue;` |
|       - |  2123 | `			}` |
|     166 |  2124 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     110 |  2125 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      55 |  2126 | `				aShadow,nShadow);` |
|     111 |  2127 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2128 | `				return SXERR_ABORT;` |
|       - |  2129 | `			}` |
|     111 |  2130 | `			pScan = pDollar + 2;` |
|       - |  2131 | `		}` |
|       1 |  2132 | `	}` |
|     105 |  2133 | `	return SXRET_OK;` |
|      53 |  2134 |  |
|       - |  2135 | `/*` |
|       - |  2136 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2137 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2138 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2139 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2140 | ` * $this is also made available.` |
|       - |  2141 | ` */` |
|      86 |  2142 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2143 |  |
|       - |  2144 | `	ph7_vm_func *pFunc;` |
|       - |  2145 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2146 | `	GenBlock *pBlock;` |
|       - |  2147 | `	SySet *pInstrContainer;` |
|       - |  2148 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2149 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2150 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2151 | `	SyToken *pSavedEnd;` |
|       - |  2152 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2153 | `	char zName[512];` |
|       - |  2154 | `	static int iCnt = 1;` |
|       - |  2155 | `	char *zDup;` |
|       - |  2156 | `	sxu32 nLen;` |
|       - |  2157 | `	sxu32 nLine;` |
|      88 |  2158 | `	sxi32 iFlags = 0;` |
|      88 |  2159 | `	int bStatic = 0;` |
|       - |  2160 | `	sxi32 rc;` |
|       - |  2161 | `	sxu32 n;` |
|      43 |  2162 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2163 |  |
|      88 |  2164 | `	nLine = pGen->pIn->nLine;` |
|       - |  2165 | `	/* Optional 'static' prefix */` |
|      86 |  2166 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      88 |  2167 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2168 | `		bStatic = 1;` |
|       3 |  2169 | `		pGen->pIn++;` |
|       1 |  2170 | `	}` |
|       - |  2171 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      86 |  2172 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      88 |  2173 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2174 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2175 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2176 | `		return SXERR_SYNTAX;` |
|       - |  2177 | `	}` |
|      88 |  2178 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2179 | `	/* Optional '&' — return by reference */` |
|      88 |  2180 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2181 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2182 | `		pGen->pIn++;` |
|     ! 0 |  2183 | `	}` |
|       - |  2184 | `	/* Expect '(' */` |
|      88 |  2185 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2186 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2187 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2188 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2189 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2190 | `		}else{` |
|     ! 0 |  2191 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2192 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2193 | `		}` |
|       3 |  2194 | `		return SXERR_SYNTAX;` |
|       - |  2195 | `	}` |
|      86 |  2196 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2197 | `	/* Delimit the parameter list */` |
|      86 |  2198 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      86 |  2199 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2200 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2201 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2202 | `		return SXERR_SYNTAX;` |
|       - |  2203 | `	}` |
|       - |  2204 | `	/* Allocate the function state */` |
|      84 |  2205 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      84 |  2206 | `	if( pFunc == 0 ){` |
|     ! 0 |  2207 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2208 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2209 | `		return SXERR_ABORT;` |
|       - |  2210 | `	}` |
|       - |  2211 | `	/* Generate a unique lambda name */` |
|      84 |  2212 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     168 |  2213 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      85 |  2214 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 |  2215 | `	}` |
|      84 |  2216 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      84 |  2217 | `	if( zDup == 0 ){` |
|     ! 0 |  2218 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2219 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2220 | `		return SXERR_ABORT;` |
|       - |  2221 | `	}` |
|      84 |  2222 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2223 | `	/* Collect function arguments */` |
|      84 |  2224 | `	if( pGen->pIn < pSigEnd ){` |
|      54 |  2225 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      54 |  2226 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2227 | `			return SXERR_ABORT;` |
|       - |  2228 | `		}` |
|      26 |  2229 | `	}` |
|       - |  2230 | `	/* Point past ')' and parse optional return type */` |
|      84 |  2231 | `	pGen->pIn = &pSigEnd[1];` |
|      84 |  2232 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      84 |  2233 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2234 | `		return SXERR_ABORT;` |
|      84 |  2235 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2236 | `		return SXERR_SYNTAX;` |
|       - |  2237 | `	}` |
|       - |  2238 | `	/* Expect '=>' */` |
|      84 |  2239 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2240 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2241 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2242 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2243 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2244 | `		}else{` |
|     ! 0 |  2245 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2246 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2247 | `		}` |
|       3 |  2248 | `		return SXERR_SYNTAX;` |
|       - |  2249 | `	}` |
|      81 |  2250 | `	pGen->pIn++; /* Jump '=>' */` |
|      81 |  2251 | `	pBodyStart = pGen->pIn;` |
|      81 |  2252 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2253 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2254 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2255 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2256 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      81 |  2257 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2258 | `	{` |
|      81 |  2259 | `		SyString *aShadow = 0;` |
|      81 |  2260 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      81 |  2261 | `		if( nShadow > 0 ){` |
|      51 |  2262 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      50 |  2263 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      51 |  2264 | `			if( aShadow == 0 ){` |
|     ! 0 |  2265 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2266 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2267 | `				return SXERR_ABORT;` |
|       - |  2268 | `			}` |
|     107 |  2269 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      57 |  2270 | `				aShadow[n] = aArgs[n].sName;` |
|      29 |  2271 | `			}` |
|      25 |  2272 | `		}` |
|     121 |  2273 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      40 |  2274 | `			aShadow,nShadow);` |
|      81 |  2275 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2276 | `			return SXERR_ABORT;` |
|       - |  2277 | `		}` |
|       - |  2278 | `	}` |
|       - |  2279 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2280 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2281 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2282 | `	 * $this. */` |
|      81 |  2283 | `	if( !bStatic ){` |
|       - |  2284 | `		char *zThisDup;` |
|      79 |  2285 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      79 |  2286 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2287 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2288 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2289 | `			return SXERR_ABORT;` |
|       - |  2290 | `		}` |
|      79 |  2291 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      79 |  2292 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      79 |  2293 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      79 |  2294 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      79 |  2295 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      39 |  2296 | `	}` |
|       - |  2297 | `	/* Arrow functions are always closures */` |
|      81 |  2298 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2299 | `	/* Compile the body expression as an implicit return */` |
|     121 |  2300 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      40 |  2301 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      81 |  2302 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2303 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2304 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2305 | `		return SXERR_ABORT;` |
|       - |  2306 | `	}` |
|      81 |  2307 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      81 |  2308 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      81 |  2309 | `	pSavedEnd = pGen->pEnd;` |
|      81 |  2310 | `	pGen->pIn = pBodyStart;` |
|      81 |  2311 | `	pGen->pEnd = pBodyEnd;` |
|      81 |  2312 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      81 |  2313 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2314 | `		return SXERR_ABORT;` |
|       - |  2315 | `	}` |
|       - |  2316 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2317 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2318 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2319 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      81 |  2320 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      81 |  2321 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      81 |  2322 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      81 |  2323 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      81 |  2324 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2325 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      81 |  2326 | `	pGen->pIn = pBodyEnd;` |
|      81 |  2327 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2328 | `	/* Emit the load-closure instruction */` |
|      81 |  2329 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      81 |  2330 | `	return SXRET_OK;` |
|      45 |  2331 |  |
|       - |  2332 | `/*` |
|       - |  2333 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2334 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2335 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2336 | ` * expression's value.` |
|       - |  2337 | ` */` |
|     346 |  2338 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2339 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       2 |  2340 |  |
|       - |  2341 | `	SySet *pInstrContainer;` |
|       - |  2342 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2343 | `	GenBlock *pArmBlock;` |
|       - |  2344 | `	sxi32 rc;` |
|     348 |  2345 | `	pTmpIn  = pGen->pIn;` |
|     348 |  2346 | `	pTmpEnd = pGen->pEnd;` |
|     348 |  2347 | `	pGen->pIn  = pStart;` |
|     348 |  2348 | `	pGen->pEnd = pStop;` |
|     348 |  2349 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     348 |  2350 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2351 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2352 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2353 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2354 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2355 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     521 |  2356 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2357 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     348 |  2358 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2359 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2360 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2361 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2362 | `		return SXERR_ABORT;` |
|       - |  2363 | `	}` |
|     348 |  2364 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     348 |  2365 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     348 |  2366 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     348 |  2367 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     348 |  2368 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     348 |  2369 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     348 |  2370 | `	pGen->pIn  = pTmpIn;` |
|     348 |  2371 | `	pGen->pEnd = pTmpEnd;` |
|     348 |  2372 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2373 | `		return SXERR_ABORT;` |
|       - |  2374 | `	}` |
|     348 |  2375 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2376 | `		return SXERR_EMPTY;` |
|       - |  2377 | `	}` |
|     348 |  2378 | `	return SXRET_OK;` |
|     175 |  2379 |  |
|       - |  2380 | `/*` |
|       - |  2381 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2382 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2383 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2384 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2385 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2386 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2387 | ` */` |
|       - |  2388 | `/*` |
|       - |  2389 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2390 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2391 | ` * caller can bail out of the current expression.` |
|       - |  2392 | ` */` |
|       2 |  2393 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2394 |  |
|       - |  2395 | `	va_list ap;` |
|       - |  2396 | `	sxi32 rc;` |
|       - |  2397 | `	SyBlob sMsg;` |
|       3 |  2398 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2399 | `	va_start(ap,zFmt);` |
|       3 |  2400 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2401 | `	va_end(ap);` |
|       3 |  2402 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2403 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2404 | `	SyBlobRelease(&sMsg);` |
|       3 |  2405 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2406 | `		return SXERR_ABORT;` |
|       - |  2407 | `	}` |
|       3 |  2408 | `	return SXERR_SYNTAX;` |
|       2 |  2409 |  |
|       - |  2410 | `/*` |
|       - |  2411 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2412 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2413 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2414 | ` */` |
|     348 |  2415 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       2 |  2416 |  |
|     350 |  2417 | `	SyToken *pCur = pStart;` |
|     350 |  2418 | `	int iNest = 0;` |
|     812 |  2419 | `	while( pCur < pEnd ){` |
|     778 |  2420 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2421 | `			iNest++;` |
|     772 |  2422 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2423 | `			iNest--;` |
|     760 |  2424 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     316 |  2425 | `			return pCur;` |
|       - |  2426 | `		}` |
|     464 |  2427 | `		pCur++;` |
|       2 |  2428 | `	}` |
|      36 |  2429 | `	return pEnd;` |
|     176 |  2430 |  |
|      70 |  2431 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2432 |  |
|       - |  2433 | `	ph7_match *pMatch;` |
|       - |  2434 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      72 |  2435 | `	int bHasDefault = 0;` |
|       - |  2436 | `	sxu32 nLine;` |
|       - |  2437 | `	sxi32 rc;` |
|      35 |  2438 | `	SXUNUSED(iCompileFlag);` |
|      72 |  2439 | `	nLine = pGen->pIn->nLine;` |
|      72 |  2440 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2441 | `	/* Expect '(' */` |
|      72 |  2442 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2443 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2444 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2445 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2446 | `	}` |
|      72 |  2447 | `	pGen->pIn++; /* Jump '(' */` |
|      72 |  2448 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      72 |  2449 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2450 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2451 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2452 | `	}` |
|      72 |  2453 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2454 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2455 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2456 | `	}` |
|       - |  2457 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      72 |  2458 | `	pSavedEnd = pGen->pEnd;` |
|      72 |  2459 | `	pGen->pEnd = pSubjEnd;` |
|      72 |  2460 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      72 |  2461 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2462 | `		return SXERR_ABORT;` |
|       - |  2463 | `	}` |
|      72 |  2464 | `	pGen->pEnd = pSavedEnd;` |
|      72 |  2465 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2466 | `	/* Expect '{' */` |
|      72 |  2467 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2468 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2469 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2470 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2471 | `	}` |
|      72 |  2472 | `	pGen->pIn++; /* Jump '{' */` |
|      72 |  2473 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      72 |  2474 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2475 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2476 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2477 | `	}` |
|       - |  2478 | `	/* Allocate ph7_match container */` |
|      72 |  2479 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      72 |  2480 | `	if( pMatch == 0 ){` |
|     ! 0 |  2481 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2482 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2483 | `		return SXERR_ABORT;` |
|       - |  2484 | `	}` |
|      72 |  2485 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      72 |  2486 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2487 | `	/* Iterate arms */` |
|     250 |  2488 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2489 | `		ph7_match_arm sArm;` |
|       - |  2490 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     184 |  2491 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     184 |  2492 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     184 |  2493 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     184 |  2494 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2495 | `		/* 'default' arm? */` |
|     182 |  2496 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     103 |  2497 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2498 | `			if( bHasDefault ){` |
|       3 |  2499 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2500 | `					"Match expressions may only contain one default arm");` |
|       4 |  2501 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2502 | `			}` |
|      20 |  2503 | `			sArm.bDefault = 1;` |
|      20 |  2504 | `			bHasDefault = 1;` |
|      20 |  2505 | `			pGen->pIn++;` |
|      20 |  2506 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2507 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2508 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2509 | `			}` |
|      20 |  2510 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2511 | `		}else{` |
|       - |  2512 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     164 |  2513 | `			pCondStart = pGen->pIn;` |
|     164 |  2514 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2515 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     172 |  2516 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2517 | `				SySet sCondBc;` |
|       9 |  2518 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2519 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2520 | `						"syntax error, empty match condition expression");` |
|       - |  2521 | `				}` |
|       9 |  2522 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2523 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2524 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2525 | `					return SXERR_ABORT;` |
|       - |  2526 | `				}` |
|       9 |  2527 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2528 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2529 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2530 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2531 | `			}` |
|     164 |  2532 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2533 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2534 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2535 | `			}` |
|     162 |  2536 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2537 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2538 | `					"syntax error, empty match condition expression");` |
|       - |  2539 | `			}` |
|       - |  2540 | `			{` |
|       - |  2541 | `				SySet sCondBc;` |
|     162 |  2542 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     162 |  2543 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     162 |  2544 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2545 | `					return SXERR_ABORT;` |
|       - |  2546 | `				}` |
|     162 |  2547 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2548 | `			}` |
|     162 |  2549 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2550 | `		}` |
|       - |  2551 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     180 |  2552 | `		pResStart = pGen->pIn;` |
|     180 |  2553 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     180 |  2554 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2555 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2556 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2557 | `		}` |
|     180 |  2558 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     180 |  2559 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2560 | `			return SXERR_ABORT;` |
|       - |  2561 | `		}` |
|     180 |  2562 | `		pGen->pIn = pResEnd;` |
|     180 |  2563 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     148 |  2564 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2565 | `		}` |
|     180 |  2566 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       2 |  2567 | `	}` |
|      68 |  2568 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      68 |  2569 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      68 |  2570 | `	return SXRET_OK;` |
|      37 |  2571 |  |
|       - |  2572 | `/*` |
|       - |  2573 | ` * Compile a backtick quoted string.` |
|       - |  2574 | ` */` |
|       4 |  2575 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  2576 |  |
|       - |  2577 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2578 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2579 | `	 */` |
|       7 |  2580 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2581 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2582 | `		ph7_lib_version()` |
|       - |  2583 | `		);` |
|       - |  2584 | `	/* Load NULL */` |
|       5 |  2585 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2586 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2587 | `	/* Node successfully compiled */` |
|       5 |  2588 | `	return SXRET_OK;` |
|       1 |  2589 |  |
|       - |  2590 | `/*` |
|       - |  2591 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2592 | ` * construct.` |
|       - |  2593 | ` */` |
|      72 |  2594 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2595 |  |
|       - |  2596 | `	SyString *pName;` |
|       - |  2597 | `	sxu32 nKeyID;` |
|       - |  2598 | `	sxi32 rc;` |
|       - |  2599 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 |  2600 | `	pName = &pGen->pIn->sData;` |
|      74 |  2601 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 |  2602 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 |  2603 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2604 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2605 | `		/* Compile arguments one after one */` |
|       9 |  2606 | `		pTmp = pGen->pEnd;` |
|       - |  2607 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2608 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2609 | `		 *  mean that the following expression is valid:` |
|       - |  2610 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2611 | `		 */` |
|       9 |  2612 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2613 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2614 | `			if( pGen->pIn < pNext ){` |
|       9 |  2615 | `				pGen->pEnd = pNext;` |
|       9 |  2616 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2617 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2618 | `					return SXERR_ABORT;` |
|       - |  2619 | `				}` |
|       9 |  2620 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2621 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2622 | `					 * without the overhead of a function call.` |
|       - |  2623 | `					 * This is a very powerful optimization that improve` |
|       - |  2624 | `					 * performance greatly.` |
|       - |  2625 | `					 */` |
|       9 |  2626 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2627 | `				}` |
|       4 |  2628 | `			}` |
|       - |  2629 | `			/* Jump trailing commas */` |
|       9 |  2630 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2631 | `				pNext++;` |
|     ! 0 |  2632 | `			}` |
|       9 |  2633 | `			pGen->pIn = pNext;` |
|       1 |  2634 | `		}` |
|       - |  2635 | `		/* Restore token stream */` |
|       9 |  2636 | `		pGen->pEnd = pTmp;` |
|       5 |  2637 | `	}else{` |
|      66 |  2638 | `		sxi32 nArg = 0;` |
|      66 |  2639 | `		sxu32 nIdx = 0;` |
|      66 |  2640 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 |  2641 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2642 | `			return SXERR_ABORT;` |
|      66 |  2643 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 |  2644 | `			nArg = 1;` |
|      32 |  2645 | `		}` |
|      66 |  2646 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2647 | `			ph7_value *pObj;` |
|       - |  2648 | `			/* Emit the call instruction */` |
|      20 |  2649 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2650 | `			if( pObj == 0 ){` |
|     ! 0 |  2651 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2652 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2653 | `				return SXERR_ABORT;` |
|       - |  2654 | `			}` |
|      20 |  2655 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2656 | `			/* Install in the literal table */` |
|      20 |  2657 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 |  2658 | `		}` |
|       - |  2659 | `		/* Emit the call instruction */` |
|      66 |  2660 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 |  2661 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - |  2662 | `	}` |
|       - |  2663 | `	/* Node successfully compiled */` |
|      74 |  2664 | `	return SXRET_OK;` |
|      38 |  2665 |  |
|       - |  2666 | `/*` |
|       - |  2667 | ` * Compile a node holding a variable declaration.` |
|       - |  2668 | ` * According to the PHP language reference` |
|       - |  2669 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2670 | ` *  The variable name is case-sensitive.` |
|       - |  2671 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2672 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2673 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2674 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2675 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2676 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2677 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2678 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2679 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2680 | ` *  the chapter on Expressions.` |
|       - |  2681 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2682 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2683 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2684 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2685 | ` *  is being assigned (the source variable).` |
|       - |  2686 | ` */` |
|  914088 |  2687 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2688 |  |
|  914090 |  2689 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2690 | `	sxi32 iVv;` |
|       - |  2691 | `	sxi32 iP1;` |
|       - |  2692 | `	void *p3;` |
|       - |  2693 | `	sxi32 rc;` |
|  914090 |  2694 | `	iVv = -1; /* Variable variable counter */` |
| 1828190 |  2695 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  914102 |  2696 | `		pGen->pIn++;` |
|  914102 |  2697 | `		iVv++;` |
|       2 |  2698 | `	}` |
|  914090 |  2699 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2700 | `		/* Invalid variable name */` |
|     ! 0 |  2701 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2702 | `		if( rc == SXERR_ABORT ){` |
|       - |  2703 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2704 | `			return SXERR_ABORT;` |
|       - |  2705 | `		}` |
|     ! 0 |  2706 | `		return SXRET_OK;` |
|       - |  2707 | `	}` |
|  914090 |  2708 | `	p3  = 0;` |
|  914090 |  2709 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2710 | `		/* Dynamic variable creation */` |
|      18 |  2711 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 |  2712 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 |  2713 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2714 | `			/* Empty expression */` |
|       3 |  2715 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2716 | `			return SXRET_OK;` |
|       - |  2717 | `		}` |
|       - |  2718 | `		/* Compile the expression holding the variable name */` |
|      16 |  2719 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2720 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2721 | `			return SXERR_ABORT;` |
|      16 |  2722 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2723 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2724 | `			return SXRET_OK;` |
|       - |  2725 | `		}` |
|       7 |  2726 | `	}else{` |
|       - |  2727 | `		SyHashEntry *pEntry;` |
|       - |  2728 | `		SyString *pName;` |
|  914074 |  2729 | `		char *zName = 0;` |
|       - |  2730 | `		/* Extract variable name */` |
|  914074 |  2731 | `		pName = &pGen->pIn->sData;` |
|       - |  2732 | `		/* Advance the stream cursor */` |
|  914074 |  2733 | `		pGen->pIn++;` |
|  914074 |  2734 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  914074 |  2735 | `		if( pEntry == 0 ){` |
|       - |  2736 | `			/* Duplicate name */` |
|  122678 |  2737 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  122678 |  2738 | `			if( zName == 0 ){` |
|     ! 0 |  2739 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2740 | `				return SXERR_ABORT;` |
|       - |  2741 | `			}` |
|       - |  2742 | `			/* Install in the hashtable */` |
|  122678 |  2743 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   61340 |  2744 | `		}else{` |
|       - |  2745 | `			/* Name already available */` |
|  791398 |  2746 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2747 | `		}` |
|  914074 |  2748 | `		p3 = (void *)zName;` |
|       - |  2749 | `	}` |
|  914086 |  2750 | `	iP1 = 0;` |
|  914086 |  2751 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  333260 |  2752 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2753 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  326802 |  2754 | `			iP1 = 1;` |
|  163400 |  2755 | `		}` |
|  166629 |  2756 | `	}` |
|       - |  2757 | `	/* Emit the load instruction */` |
|  914086 |  2758 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  914098 |  2759 | `	while( iVv > 0 ){` |
|      13 |  2760 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2761 | `		iVv--;` |
|       1 |  2762 | `	}` |
|       - |  2763 | `	/* Node successfully compiled */` |
|  914086 |  2764 | `	return SXRET_OK;` |
|  457046 |  2765 |  |
|       - |  2766 | `/*` |
|       - |  2767 | ` * Load a literal.` |
|       - |  2768 | ` */` |
|  642334 |  2769 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2770 |  |
|  642336 |  2771 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2772 | `	ph7_value *pObj;` |
|       - |  2773 | `	SyString *pStr;` |
|       - |  2774 | `	sxu32 nIdx;` |
|       - |  2775 | `	/* Extract token value */` |
|  642336 |  2776 | `	pStr = &pToken->sData;` |
|       - |  2777 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  642336 |  2778 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  136168 |  2779 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2780 | `			/* NULL constant are always indexed at 0 */` |
|   50152 |  2781 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   50152 |  2782 | `			return SXRET_OK;` |
|   86018 |  2783 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2784 | `			/* TRUE constant are always indexed at 1 */` |
|     520 |  2785 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     520 |  2786 | `			return SXRET_OK;` |
|       2 |  2787 | `		}` |
|  599874 |  2788 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  101910 |  2789 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2790 | `			/* FALSE constant are always indexed at 2 */` |
|   38520 |  2791 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   38520 |  2792 | `			return SXRET_OK;` |
|  513358 |  2793 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   91412 |  2794 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2795 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    8758 |  2796 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    8758 |  2797 | `			if( pObj == 0 ){` |
|     ! 0 |  2798 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2799 | `				return SXERR_ABORT;` |
|       - |  2800 | `			}` |
|    8758 |  2801 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2802 | `			/* Emit the load constant instruction */` |
|    8758 |  2803 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    8758 |  2804 | `			return SXRET_OK;` |
|  473615 |  2805 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   29438 |  2806 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2807 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2808 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2809 | `			if( pObj == 0 ){` |
|     ! 0 |  2810 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2811 | `				return SXERR_ABORT;` |
|       - |  2812 | `			}` |
|       7 |  2813 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2814 | `				SyString sNs;` |
|       7 |  2815 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2816 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2817 | `			}else{` |
|     ! 0 |  2818 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2819 | `			}` |
|       7 |  2820 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2821 | `			return SXRET_OK;` |
|  472747 |  2822 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   12276 |  2823 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  466603 |  2824 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   15444 |  2825 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2826 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2827 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2828 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2829 | `				/* Point to the upper block */` |
|      11 |  2830 | `				pBlock = pBlock->pParent;` |
|       1 |  2831 | `			}` |
|      11 |  2832 | `			if( pBlock == 0 ){` |
|       - |  2833 | `				/* Called in the global scope,load NULL */` |
|       5 |  2834 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2835 | `			}else{` |
|       - |  2836 | `				/* Extract the target function/method */` |
|       7 |  2837 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2838 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2839 | `					/* Not a class method,Load null */` |
|       3 |  2840 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2841 | `				}else{` |
|       5 |  2842 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2843 | `					if( pObj == 0 ){` |
|     ! 0 |  2844 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2845 | `						return SXERR_ABORT;` |
|       - |  2846 | `					}` |
|       5 |  2847 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2848 | `					/* Emit the load constant instruction */` |
|       5 |  2849 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2850 | `				}` |
|       - |  2851 | `			}` |
|      11 |  2852 | `			return SXRET_OK;` |
|       - |  2853 | `	}` |
|       - |  2854 | `	/* Query literal table */` |
|  544378 |  2855 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2856 | `		ph7_value *pLitObj;` |
|       - |  2857 | `		/* Unknown literal,install it in the literal table */` |
|  226086 |  2858 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  226086 |  2859 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2860 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2861 | `			return SXERR_ABORT;` |
|       - |  2862 | `		}` |
|  226086 |  2863 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  226086 |  2864 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  113042 |  2865 | `	}` |
|       - |  2866 | `	/* Emit the load constant instruction */` |
|  544378 |  2867 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  544378 |  2868 | `	return SXRET_OK;` |
|  321169 |  2869 |  |
|       - |  2870 | `/*` |
|       - |  2871 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2872 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2873 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2874 | ` * Otherwise, load the simple literal directly.` |
|       - |  2875 | ` */` |
|  642368 |  2876 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2877 |  |
|       - |  2878 | `	sxi32 rc;` |
|  642370 |  2879 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2880 | `		return SXRET_OK;` |
|       - |  2881 | `	}` |
|       - |  2882 | `	/* Check if this is a multi-token namespace path */` |
|  642370 |  2883 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2884 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      36 |  2885 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      36 |  2886 | `		int isAbsolute = 0;` |
|      36 |  2887 | `		SyBlobReset(pWorker);` |
|       - |  2888 | `		/* Check for leading backslash (absolute path) */` |
|      36 |  2889 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      34 |  2890 | `			isAbsolute = 1;` |
|      34 |  2891 | `			pGen->pIn++; /* Skip leading backslash */` |
|      16 |  2892 | `		}` |
|       - |  2893 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      36 |  2894 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2895 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2896 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2897 | `		}` |
|       - |  2898 | `		/* Collect all path components */` |
|     132 |  2899 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     132 |  2900 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      50 |  2901 | `				SyBlobAppend(pWorker,"\\",1);` |
|      26 |  2902 | `			}else{` |
|      84 |  2903 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2904 | `			}` |
|     132 |  2905 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      36 |  2906 | `				pGen->pIn++;` |
|      36 |  2907 | `				break;` |
|       - |  2908 | `			}` |
|      98 |  2909 | `			pGen->pIn++;` |
|       2 |  2910 | `		}` |
|      36 |  2911 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2912 | `			ph7_value *pObj;` |
|       - |  2913 | `			SyString sPath;` |
|       - |  2914 | `			sxu32 nIdx;` |
|      36 |  2915 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2916 | `			/* Install in the literal table */` |
|      36 |  2917 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      18 |  2918 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 |  2919 | `				if( pObj == 0 ){` |
|     ! 0 |  2920 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2921 | `					return SXERR_ABORT;` |
|       - |  2922 | `				}` |
|      18 |  2923 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      18 |  2924 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  2925 | `			}` |
|       - |  2926 | `			/* Emit the load constant instruction.` |
|       - |  2927 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  2928 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      53 |  2929 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      17 |  2930 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      17 |  2931 | `				nIdx,0,0);` |
|      36 |  2932 | `			return SXRET_OK;` |
|       - |  2933 | `		}` |
|     ! 0 |  2934 | `	}` |
|       - |  2935 | `	/* Single-token literal: load directly */` |
|  642336 |  2936 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  642336 |  2937 | `	return rc;` |
|  321186 |  2938 |  |
|       - |  2939 | `/*` |
|       - |  2940 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2941 | ` */` |
|  642368 |  2942 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2943 |  |
|       - |  2944 | `	sxi32 rc;` |
|  642370 |  2945 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  642370 |  2946 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2947 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2948 | `		return rc;` |
|       - |  2949 | `	}` |
|       - |  2950 | `	/* Node successfully compiled */` |
|  642370 |  2951 | `	return SXRET_OK;` |
|  321186 |  2952 |  |
|       - |  2953 | `/*` |
|       - |  2954 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  2955 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  2956 | ` */` |
|       8 |  2957 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  2958 |  |
|       - |  2959 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  2960 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  2961 | `		pGen->pIn++;` |
|       1 |  2962 | `	}` |
|       9 |  2963 | `	return SXRET_OK;` |
|       1 |  2964 |  |
|       - |  2965 | `/*` |
|       - |  2966 | ` * Check if the given identifier name is reserved or not.` |
|       - |  2967 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  2968 | ` */` |
|      56 |  2969 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 |  2970 |  |
|      58 |  2971 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 |  2972 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  2973 | `			return TRUE;` |
|      24 |  2974 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  2975 | `			return TRUE;` |
|       2 |  2976 | `		}` |
|      43 |  2977 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  2978 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  2979 | `			return TRUE;` |
|       - |  2980 | `		}` |
|     ! 0 |  2981 | `	}` |
|       - |  2982 | `	/* Not a reserved constant */` |
|      50 |  2983 | `	return FALSE;` |
|      30 |  2984 |  |
|       - |  2985 | `/*` |
|       - |  2986 | ` * Compile the 'const' statement.` |
|       - |  2987 | ` * According to the PHP language reference` |
|       - |  2988 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  2989 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  2990 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  2991 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  2992 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2993 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  2994 | ` *  Syntax` |
|       - |  2995 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  2996 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  2997 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  2998 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  2999 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3000 | ` *  to get a list of all defined constants.` |
|       - |  3001 | ` *` |
|       - |  3002 | ` * Symisc eXtension.` |
|       - |  3003 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3004 | ` *  would allow only simple scalar value.` |
|       - |  3005 | ` *  Example` |
|       - |  3006 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3007 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3008 | ` */` |
|      32 |  3009 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 |  3010 |  |
|       - |  3011 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 |  3012 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3013 | `	SyString *pName;` |
|       - |  3014 | `	sxi32 rc;` |
|      34 |  3015 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 |  3016 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3017 | `		/* Invalid constant name */` |
|       7 |  3018 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  3019 | `		if( rc == SXERR_ABORT ){` |
|       - |  3020 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3021 | `			return SXERR_ABORT;` |
|       - |  3022 | `		}` |
|       7 |  3023 | `		goto Synchronize;` |
|       - |  3024 | `	}` |
|       - |  3025 | `	/* Peek constant name */` |
|      28 |  3026 | `	pName = &pGen->pIn->sData;` |
|       - |  3027 | `	/* Make sure the constant name isn't reserved */` |
|      28 |  3028 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3029 | `		/* Reserved constant */` |
|       9 |  3030 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3031 | `		if( rc == SXERR_ABORT ){` |
|       - |  3032 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3033 | `			return SXERR_ABORT;` |
|       - |  3034 | `		}` |
|       9 |  3035 | `		goto Synchronize;` |
|       - |  3036 | `	}` |
|      20 |  3037 | `	pGen->pIn++;` |
|      20 |  3038 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3039 | `		/* Invalid statement*/` |
|       5 |  3040 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 |  3041 | `		if( rc == SXERR_ABORT ){` |
|       - |  3042 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3043 | `			return SXERR_ABORT;` |
|       - |  3044 | `		}` |
|       5 |  3045 | `		goto Synchronize;` |
|       - |  3046 | `	}` |
|      15 |  3047 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3048 | `	/* Allocate a new constant value container */` |
|      15 |  3049 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3050 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3051 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3052 | `		return SXERR_ABORT;` |
|       - |  3053 | `	}` |
|      15 |  3054 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3055 | `	/* Swap bytecode container */` |
|      15 |  3056 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3057 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3058 | `	/* Compile constant value */` |
|      15 |  3059 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3060 | `	/* Emit the done instruction */` |
|      15 |  3061 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3062 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3063 | `	if( rc == SXERR_ABORT ){` |
|       - |  3064 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3065 | `		return SXERR_ABORT;` |
|       - |  3066 | `	}` |
|      15 |  3067 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3068 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3069 | `	{` |
|       - |  3070 | `		SyBlob sFQN;` |
|       - |  3071 | `		SyString sFQNStr;` |
|      15 |  3072 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3073 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3074 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3075 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3076 | `		SyBlobRelease(&sFQN);` |
|       - |  3077 | `	}` |
|      15 |  3078 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3079 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3080 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3081 | `	}` |
|      15 |  3082 | `	return SXRET_OK;` |
|       9 |  3083 | `Synchronize:` |
|       - |  3084 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 |  3085 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 |  3086 | `		pGen->pIn++;` |
|       1 |  3087 | `	}` |
|      19 |  3088 | `	return SXRET_OK;` |
|      18 |  3089 |  |
|       - |  3090 | `/*` |
|       - |  3091 | ` * Compile the 'continue' statement.` |
|       - |  3092 | ` * According to the PHP language reference` |
|       - |  3093 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3094 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3095 | ` *  iteration.` |
|       - |  3096 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3097 | ` *  the purposes of continue.` |
|       - |  3098 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3099 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3100 | ` *  Note:` |
|       - |  3101 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3102 | ` */` |
|       - |  3103 | `/*` |
|       - |  3104 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3105 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3106 | ` * break/continue crosses a try boundary.` |
|       - |  3107 | ` *` |
|       - |  3108 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3109 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3110 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3111 | ` */` |
|    3046 |  3112 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3113 |  |
|    3048 |  3114 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17822 |  3115 | `	while( pBlock && pBlock != pTarget ){` |
|   14776 |  3116 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3117 | `			if( pBlock->pUserData ){` |
|       - |  3118 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3119 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3120 | `			}else{` |
|       - |  3121 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3122 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3123 | `				 * exception context from a sub-execution.` |
|       - |  3124 | `				 */` |
|     ! 0 |  3125 | `				break;` |
|       - |  3126 | `			}` |
|       1 |  3127 | `		}` |
|   14776 |  3128 | `		pBlock = pBlock->pParent;` |
|       2 |  3129 | `	}` |
|    3048 |  3130 |  |
|    2962 |  3131 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3132 |  |
|       - |  3133 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3134 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3135 | `	sxu32 nLineLocal;` |
|       - |  3136 | `	sxi32 rc;` |
|    2964 |  3137 | `	nLineLocal = pGen->pIn->nLine;` |
|    2964 |  3138 | `	iLevel = 0;` |
|       - |  3139 | `	/* Jump the 'continue' keyword */` |
|    2964 |  3140 | `	pGen->pIn++;` |
|    2964 |  3141 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3142 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3143 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3144 | `		 */` |
|       - |  3145 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3146 | `		char *zAlloc = 0;` |
|       - |  3147 | `		SyString sNum;` |
|      16 |  3148 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3149 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3150 | `			return SXERR_ABORT;` |
|       - |  3151 | `		}` |
|      16 |  3152 | `		if( rc == SXRET_OK ){` |
|      20 |  3153 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3154 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3155 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3156 | `				return SXERR_ABORT;` |
|       - |  3157 | `			}` |
|      14 |  3158 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3159 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3160 | `		}` |
|      16 |  3161 | `		if( iLevel < 2 ){` |
|       3 |  3162 | `			iLevel = 0;` |
|       1 |  3163 | `		}` |
|      16 |  3164 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3165 | `	}` |
|       - |  3166 | `	/* Point to the target loop */` |
|    2964 |  3167 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2964 |  3168 | `	if( pLoop == 0 ){` |
|       - |  3169 | `		/* Illegal continue */` |
|      11 |  3170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3171 | `		if( rc == SXERR_ABORT ){` |
|       - |  3172 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3173 | `			return SXERR_ABORT;` |
|       - |  3174 | `		}` |
|       6 |  3175 | `	}else{` |
|    2954 |  3176 | `		sxu32 nInstrIdx = 0;` |
|       - |  3177 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2954 |  3178 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2954 |  3179 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3180 | `			/* According to the PHP language reference manual` |
|       - |  3181 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3182 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3183 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3184 | `			 */` |
|       5 |  3185 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3186 | `			if( rc == SXRET_OK ){` |
|       5 |  3187 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3188 | `			}` |
|       3 |  3189 | `		}else{` |
|       - |  3190 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2950 |  3191 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2950 |  3192 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3193 | `				JumpFixup sJumpFix;` |
|       - |  3194 | `				/* Post-continue */` |
|      14 |  3195 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3196 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3197 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3198 | `			}` |
|       - |  3199 | `		}` |
|       - |  3200 | `	}` |
|    2964 |  3201 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3202 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3203 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3204 | `	}` |
|       - |  3205 | `	/* Statement successfully compiled */` |
|    2964 |  3206 | `	return SXRET_OK;` |
|    1483 |  3207 |  |
|       - |  3208 | `/*` |
|       - |  3209 | ` * Compile the 'break' statement.` |
|       - |  3210 | ` * According to the PHP language reference` |
|       - |  3211 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3212 | ` *  structure.` |
|       - |  3213 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3214 | ` *  enclosing structures are to be broken out of.` |
|       - |  3215 | ` */` |
|     110 |  3216 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 |  3217 |  |
|       - |  3218 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3219 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3220 | `	sxi32 rc;` |
|     112 |  3221 | `	iLevel = 0;` |
|       - |  3222 | `	/* Jump the 'break' keyword */` |
|     112 |  3223 | `	pGen->pIn++;` |
|     112 |  3224 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3225 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3226 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3227 | `		 */` |
|       - |  3228 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3229 | `		char *zAlloc = 0;` |
|       - |  3230 | `		SyString sNum;` |
|      16 |  3231 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3233 | `			return SXERR_ABORT;` |
|       - |  3234 | `		}` |
|      16 |  3235 | `		if( rc == SXRET_OK ){` |
|      20 |  3236 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3237 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3238 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3239 | `				return SXERR_ABORT;` |
|       - |  3240 | `			}` |
|      14 |  3241 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3242 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3243 | `		}` |
|      16 |  3244 | `		if( iLevel < 2 ){` |
|       3 |  3245 | `			iLevel = 0;` |
|       1 |  3246 | `		}` |
|      16 |  3247 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3248 | `	}` |
|       - |  3249 | `	/* Extract the target loop */` |
|     112 |  3250 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 |  3251 | `	if( pLoop == 0 ){` |
|       - |  3252 | `		/* Illegal break */` |
|      17 |  3253 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 |  3254 | `		if( rc == SXERR_ABORT ){` |
|       - |  3255 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3256 | `			return SXERR_ABORT;` |
|       - |  3257 | `		}` |
|       9 |  3258 | `	}else{` |
|       - |  3259 | `		sxu32 nInstrIdx;` |
|       - |  3260 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 |  3261 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 |  3262 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 |  3263 | `		if( rc == SXRET_OK ){` |
|       - |  3264 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 |  3265 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 |  3266 | `		}` |
|       - |  3267 | `	}` |
|     112 |  3268 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3269 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3270 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3271 | `	}` |
|       - |  3272 | `	/* Statement successfully compiled */` |
|     112 |  3273 | `	return SXRET_OK;` |
|      57 |  3274 |  |
|       - |  3275 | `/*` |
|       - |  3276 | ` * Compile or record a label.` |
|       - |  3277 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3278 | ` * Example` |
|       - |  3279 | ` *  goto LABEL;` |
|       - |  3280 | ` *   echo 'Foo';` |
|       - |  3281 | ` *  LABEL:` |
|       - |  3282 | ` *   echo 'Bar';` |
|       - |  3283 | ` */` |
|     112 |  3284 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 |  3285 |  |
|       - |  3286 | `	GenBlock *pBlock;` |
|       - |  3287 | `	Label sLabel;` |
|       - |  3288 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 |  3289 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 |  3290 | `	if( pBlock ){` |
|       - |  3291 | `		sxi32 rc;` |
|       7 |  3292 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3293 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 |  3294 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3295 | `			return SXERR_ABORT;` |
|       - |  3296 | `		}` |
|       3 |  3297 | `	}else{` |
|     110 |  3298 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3299 | `		char *zDup;` |
|       - |  3300 | `		/* Initialize label fields */` |
|     110 |  3301 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3302 | `		/* Duplicate label name */` |
|     110 |  3303 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 |  3304 | `		if( zDup == 0 ){` |
|     ! 0 |  3305 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3306 | `			return SXERR_ABORT;` |
|       - |  3307 | `		}` |
|     110 |  3308 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 |  3309 | `		sLabel.bRef  = FALSE;` |
|     110 |  3310 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 |  3311 | `		pBlock = pGen->pCurrent;` |
|     218 |  3312 | `		while( pBlock ){` |
|     130 |  3313 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3314 | `				break;` |
|       - |  3315 | `			}` |
|       - |  3316 | `			/* Point to the upper block */` |
|     110 |  3317 | `			pBlock = pBlock->pParent;` |
|       2 |  3318 | `		}` |
|     110 |  3319 | `		if( pBlock ){` |
|      22 |  3320 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3321 | `		}else{` |
|      90 |  3322 | `			sLabel.pFunc = 0;` |
|       - |  3323 | `		}` |
|       - |  3324 | `		/* Insert in label set */` |
|     110 |  3325 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3326 | `	}` |
|     114 |  3327 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 |  3328 | `	return SXRET_OK;` |
|      58 |  3329 |  |
|       - |  3330 | `/*` |
|       - |  3331 | ` * Compile the so hated 'goto' statement.` |
|       - |  3332 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3333 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3334 | ` * a compiler it has to do this.` |
|       - |  3335 | ` * According to the PHP language reference manual` |
|       - |  3336 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3337 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3338 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3339 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3340 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3341 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3342 | ` *   of a multi-level break` |
|       - |  3343 | ` */` |
|     152 |  3344 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 |  3345 |  |
|       - |  3346 | `	JumpFixup sJump;` |
|       - |  3347 | `	sxi32 rc;` |
|     154 |  3348 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 |  3349 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3350 | `		/* Missing label */` |
|     ! 0 |  3351 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3352 | `		if( rc == SXERR_ABORT ){` |
|       - |  3353 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3354 | `			return SXERR_ABORT;` |
|       - |  3355 | `		}` |
|     ! 0 |  3356 | `		return SXRET_OK;` |
|       - |  3357 | `	}` |
|     154 |  3358 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3359 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3360 | `		if( rc == SXERR_ABORT ){` |
|       - |  3361 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3362 | `			return SXERR_ABORT;` |
|       - |  3363 | `		}` |
|       3 |  3364 | `	}else{` |
|     150 |  3365 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3366 | `		GenBlock *pBlock;` |
|       - |  3367 | `		char *zDup;` |
|       - |  3368 | `		/* Prepare the jump destination */` |
|     150 |  3369 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 |  3370 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3371 | `		/* Duplicate label name */` |
|     150 |  3372 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 |  3373 | `		if( zDup == 0 ){` |
|     ! 0 |  3374 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3375 | `			return SXERR_ABORT;` |
|       - |  3376 | `		}` |
|     150 |  3377 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 |  3378 | `		pBlock = pGen->pCurrent;` |
|     312 |  3379 | `		while( pBlock ){` |
|     196 |  3380 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 |  3381 | `				break;` |
|       - |  3382 | `			}` |
|       - |  3383 | `			/* Point to the upper block */` |
|     164 |  3384 | `			pBlock = pBlock->pParent;` |
|       2 |  3385 | `		}` |
|     150 |  3386 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 |  3387 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 |  3388 | `			if( rc == SXERR_ABORT ){` |
|       - |  3389 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3390 | `				return SXERR_ABORT;` |
|       - |  3391 | `			}` |
|       3 |  3392 | `		}` |
|     150 |  3393 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 |  3394 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3395 | `		}else{` |
|     124 |  3396 | `			sJump.pFunc = 0;` |
|       - |  3397 | `		}` |
|       - |  3398 | `		/* Emit the unconditional jump */` |
|     150 |  3399 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 |  3400 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3401 | `		}` |
|       - |  3402 | `	}` |
|     154 |  3403 | `	pGen->pIn++; /* Jump the label name */` |
|     154 |  3404 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3405 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3406 | `	}` |
|       - |  3407 | `	/* Statement successfully compiled */` |
|     154 |  3408 | `	return SXRET_OK;` |
|      78 |  3409 |  |
|       - |  3410 | `/*` |
|       - |  3411 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3412 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3413 | ` * failure.` |
|       - |  3414 | ` */` |
|      20 |  3415 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3416 |  |
|       - |  3417 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3418 | `	sxu32 nRawObj;` |
|      10 |  3419 | `	sxu32 nObjIdx;` |
|       - |  3420 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3421 | `	 * a PHP block.` |
|       - |  3422 | `	 */` |
|      10 |  3423 | `Consume:` |
|      21 |  3424 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3425 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3426 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3427 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3428 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3429 | `			return SXERR_ABORT;` |
|       - |  3430 | `		}` |
|       - |  3431 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3432 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3433 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3434 | `		++nRawObj;` |
|     ! 0 |  3435 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3436 | `	}` |
|      21 |  3437 | `	if( nRawObj > 0 ){` |
|       - |  3438 | `		/* Emit the consume instruction */` |
|     ! 0 |  3439 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3440 | `	}` |
|      21 |  3441 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3442 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3443 | `		/* Reset the token set */` |
|     ! 0 |  3444 | `		SySetReset(pTokenSet);` |
|       - |  3445 | `		/* Tokenize input */` |
|     ! 0 |  3446 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3447 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3448 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3449 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3450 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3451 | `		/* Advance the stream cursor */` |
|     ! 0 |  3452 | `		pGen->pRawIn++;` |
|       - |  3453 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3454 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3455 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3456 | `			sxi32 rc;` |
|       - |  3457 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3458 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3459 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3460 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3461 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3462 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3463 | `				return SXERR_ABORT;` |
|     ! 0 |  3464 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3465 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3466 | `			}` |
|     ! 0 |  3467 | `			goto Consume;` |
|       - |  3468 | `		}` |
|     ! 0 |  3469 | `	}else{` |
|       - |  3470 | `		/* No more chunks to process */` |
|      21 |  3471 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3472 | `		return SXERR_EOF;` |
|       - |  3473 | `	}` |
|     ! 0 |  3474 | `	return SXRET_OK;` |
|      11 |  3475 |  |
|       - |  3476 | `/*` |
|       - |  3477 | ` * Compile a PHP block.` |
|       - |  3478 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3479 | ` * optionally delimited by braces {}.` |
|       - |  3480 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3481 | ` * and this function takes care of generating the appropriate error` |
|       - |  3482 | ` * message.` |
|       - |  3483 | ` */` |
|  353590 |  3484 | `static sxi32 PH7_CompileBlock(` |
|       - |  3485 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3486 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3487 | `	)` |
|       2 |  3488 |  |
|       - |  3489 | `	sxi32 rc;` |
|       - |  3490 | `	sxu32 nLine;` |
|  353592 |  3491 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  352184 |  3492 | `		nLine = pGen->pIn->nLine;` |
|  352184 |  3493 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  352184 |  3494 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3495 | `			return SXERR_ABORT;` |
|       - |  3496 | `		}` |
|  352184 |  3497 | `		pGen->pIn++;` |
|       - |  3498 | `		/* Compile until we hit the closing braces '}' */` |
|  481189 |  3499 | `		for(;;){` |
|  962380 |  3500 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3501 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3502 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3503 | `			 	   return SXERR_ABORT;` |
|       - |  3504 | `				}` |
|      21 |  3505 | `				if( rc == SXERR_EOF ){` |
|       - |  3506 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3507 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3508 | `					break;` |
|       - |  3509 | `				}` |
|     ! 0 |  3510 | `			}` |
|  962360 |  3511 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3512 | `				/* Closing braces found,break immediately*/` |
|  352164 |  3513 | `				pGen->pIn++;` |
|  352164 |  3514 | `				break;` |
|       - |  3515 | `			}` |
|       - |  3516 | `			/* Compile a single statement */` |
|  610198 |  3517 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  610198 |  3518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3519 | `				return SXERR_ABORT;` |
|       - |  3520 | `			}` |
|       2 |  3521 | `		}` |
|  352184 |  3522 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  177501 |  3523 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3524 | `		pGen->pIn++;` |
|     ! 0 |  3525 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3526 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3527 | `			return SXERR_ABORT;` |
|       - |  3528 | `		}` |
|       - |  3529 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3530 | `		for(;;){` |
|     ! 0 |  3531 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3532 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3533 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3534 | `			 	   return SXERR_ABORT;` |
|       - |  3535 | `				}` |
|     ! 0 |  3536 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3537 | `					/* No more token to process */` |
|     ! 0 |  3538 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3539 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3540 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3541 | `					}` |
|     ! 0 |  3542 | `					break;` |
|       - |  3543 | `				}` |
|     ! 0 |  3544 | `			}` |
|     ! 0 |  3545 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3546 | `				sxi32 nKwrd;` |
|       - |  3547 | `				/* Keyword found */` |
|     ! 0 |  3548 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3549 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3550 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3551 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3552 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3553 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3554 | `						}` |
|     ! 0 |  3555 | `						break;` |
|       - |  3556 | `				}` |
|     ! 0 |  3557 | `			}` |
|       - |  3558 | `			/* Compile a single statement */` |
|     ! 0 |  3559 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3560 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3561 | `				return SXERR_ABORT;` |
|       - |  3562 | `			}` |
|     ! 0 |  3563 | `		}` |
|     ! 0 |  3564 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3565 | `	}else{` |
|       - |  3566 | `		/* Compile a single statement */` |
|    1410 |  3567 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1410 |  3568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3569 | `			return SXERR_ABORT;` |
|       - |  3570 | `		}` |
|       - |  3571 | `	}` |
|       - |  3572 | `	/* Jump trailing semi-colons ';' */` |
|  353592 |  3573 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3574 | `		pGen->pIn++;` |
|     ! 0 |  3575 | `	}` |
|  353592 |  3576 | `	return SXRET_OK;` |
|  176797 |  3577 |  |
|       - |  3578 | `/*` |
|       - |  3579 | ` * Compile the gentle 'while' statement.` |
|       - |  3580 | ` * According to the PHP language reference` |
|       - |  3581 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3582 | ` *  The basic form of a while statement is:` |
|       - |  3583 | ` *  while (expr)` |
|       - |  3584 | ` *   statement` |
|       - |  3585 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3586 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3587 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3588 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3589 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3590 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3591 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3592 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3593 | ` *  while (expr):` |
|       - |  3594 | ` *    statement` |
|       - |  3595 | ` *   endwhile;` |
|       - |  3596 | ` */` |
|   11774 |  3597 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3598 |  |
|   11776 |  3599 | `	GenBlock *pWhileBlock = 0;` |
|   11776 |  3600 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3601 | `	sxu32 nFalseJump;` |
|       - |  3602 | `	sxu32 nLine;` |
|       - |  3603 | `	sxi32 rc;` |
|   11776 |  3604 | `	nLine = pGen->pIn->nLine;` |
|       - |  3605 | `	/* Jump the 'while' keyword */` |
|   11776 |  3606 | `	pGen->pIn++;` |
|   11776 |  3607 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3608 | `		/* Syntax error */` |
|     ! 0 |  3609 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3610 | `		if( rc == SXERR_ABORT ){` |
|       - |  3611 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3612 | `			return SXERR_ABORT;` |
|       - |  3613 | `		}` |
|     ! 0 |  3614 | `		goto Synchronize;` |
|       - |  3615 | `	}` |
|       - |  3616 | `	/* Jump the left parenthesis '(' */` |
|   11776 |  3617 | `	pGen->pIn++;` |
|       - |  3618 | `	/* Create the loop block */` |
|   11776 |  3619 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11776 |  3620 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3621 | `		return SXERR_ABORT;` |
|       - |  3622 | `	}` |
|       - |  3623 | `	/* Delimit the condition */` |
|   11776 |  3624 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11776 |  3625 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3626 | `		/* Empty expression */` |
|       3 |  3627 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3628 | `		if( rc == SXERR_ABORT ){` |
|       - |  3629 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3630 | `			return SXERR_ABORT;` |
|       - |  3631 | `		}` |
|       1 |  3632 | `	}` |
|       - |  3633 | `	/* Swap token streams */` |
|   11776 |  3634 | `	pTmp = pGen->pEnd;` |
|   11776 |  3635 | `	pGen->pEnd = pEnd;` |
|       - |  3636 | `	/* Compile the expression */` |
|   11776 |  3637 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11776 |  3638 | `	if( rc == SXERR_ABORT ){` |
|       - |  3639 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3640 | `		return SXERR_ABORT;` |
|       - |  3641 | `	}` |
|       - |  3642 | `	/* Update token stream */` |
|   11776 |  3643 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3644 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3645 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3646 | `			return SXERR_ABORT;` |
|       - |  3647 | `		}` |
|     ! 0 |  3648 | `		pGen->pIn++;` |
|     ! 0 |  3649 | `	}` |
|       - |  3650 | `	/* Synchronize pointers */` |
|   11776 |  3651 | `	pGen->pIn  = &pEnd[1];` |
|   11776 |  3652 | `	pGen->pEnd = pTmp;` |
|       - |  3653 | `	/* Emit the false jump */` |
|   11776 |  3654 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3655 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11776 |  3656 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3657 | `	/* Compile the loop body */` |
|   11776 |  3658 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11776 |  3659 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3660 | `		return SXERR_ABORT;` |
|       - |  3661 | `	}` |
|       - |  3662 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11776 |  3663 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3664 | `	/* Fix all jumps now the destination is resolved */` |
|   11776 |  3665 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3666 | `	/* Release the loop block */` |
|   11776 |  3667 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3668 | `	/* Statement successfully compiled */` |
|   11776 |  3669 | `	return SXRET_OK;` |
|     ! 0 |  3670 | `Synchronize:` |
|       - |  3671 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3672 | `	 * compiling this erroneous block.` |
|       - |  3673 | `	 */` |
|     ! 0 |  3674 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3675 | `		pGen->pIn++;` |
|     ! 0 |  3676 | `	}` |
|     ! 0 |  3677 | `	return SXRET_OK;` |
|    5889 |  3678 |  |
|       - |  3679 | `/*` |
|       - |  3680 | ` * Compile the ugly do..while() statement.` |
|       - |  3681 | ` * According to the PHP language reference` |
|       - |  3682 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3683 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3684 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3685 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3686 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3687 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3688 | ` *  would end immediately).` |
|       - |  3689 | ` *  There is just one syntax for do-while loops:` |
|       - |  3690 | ` *  <?php` |
|       - |  3691 | ` *  $i = 0;` |
|       - |  3692 | ` *  do {` |
|       - |  3693 | ` *   echo $i;` |
|       - |  3694 | ` *  } while ($i > 0);` |
|       - |  3695 | ` * ?>` |
|       - |  3696 | ` */` |
|       2 |  3697 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3698 |  |
|       3 |  3699 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3700 | `	GenBlock *pDoBlock = 0;` |
|       - |  3701 | `	sxu32 nLine;` |
|       - |  3702 | `	sxi32 rc;` |
|       3 |  3703 | `	nLine = pGen->pIn->nLine;` |
|       - |  3704 | `	/* Jump the 'do' keyword */` |
|       3 |  3705 | `	pGen->pIn++;` |
|       - |  3706 | `	/* Create the loop block */` |
|       3 |  3707 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3708 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3709 | `		return SXERR_ABORT;` |
|       - |  3710 | `	}` |
|       - |  3711 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3712 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3713 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3714 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3715 | `		return SXERR_ABORT;` |
|       - |  3716 | `	}` |
|       3 |  3717 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3718 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3719 | `	}` |
|       3 |  3720 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3721 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3722 | `			/* Missing 'while' statement */` |
|       3 |  3723 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3724 | `			if( rc == SXERR_ABORT ){` |
|       - |  3725 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3726 | `				return SXERR_ABORT;` |
|       - |  3727 | `			}` |
|       3 |  3728 | `			goto Synchronize;` |
|       - |  3729 | `	}` |
|       - |  3730 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3731 | `	pGen->pIn++;` |
|     ! 0 |  3732 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3733 | `		/* Syntax error */` |
|     ! 0 |  3734 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3735 | `		if( rc == SXERR_ABORT ){` |
|       - |  3736 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3737 | `			return SXERR_ABORT;` |
|       - |  3738 | `		}` |
|     ! 0 |  3739 | `		goto Synchronize;` |
|       - |  3740 | `	}` |
|       - |  3741 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3742 | `	pGen->pIn++;` |
|       - |  3743 | `	/* Delimit the condition */` |
|     ! 0 |  3744 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3745 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3746 | `		/* Empty expression */` |
|     ! 0 |  3747 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3748 | `		if( rc == SXERR_ABORT ){` |
|       - |  3749 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3750 | `			return SXERR_ABORT;` |
|       - |  3751 | `		}` |
|     ! 0 |  3752 | `		goto Synchronize;` |
|       - |  3753 | `	}` |
|       - |  3754 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3755 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3756 | `		JumpFixup *aPost;` |
|       - |  3757 | `		VmInstr *pInstr;` |
|       - |  3758 | `		sxu32 nJumpDest;` |
|       - |  3759 | `		sxu32 n;` |
|     ! 0 |  3760 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3761 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3762 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3763 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3764 | `			if( pInstr ){` |
|       - |  3765 | `				/* Fix */` |
|     ! 0 |  3766 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3767 | `			}` |
|     ! 0 |  3768 | `		}` |
|     ! 0 |  3769 | `	}` |
|       - |  3770 | `	/* Swap token streams */` |
|     ! 0 |  3771 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3772 | `	pGen->pEnd = pEnd;` |
|       - |  3773 | `	/* Compile the expression */` |
|     ! 0 |  3774 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3775 | `	if( rc == SXERR_ABORT ){` |
|       - |  3776 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3777 | `		return SXERR_ABORT;` |
|       - |  3778 | `	}` |
|       - |  3779 | `	/* Update token stream */` |
|     ! 0 |  3780 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3781 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3782 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3783 | `			return SXERR_ABORT;` |
|       - |  3784 | `		}` |
|     ! 0 |  3785 | `		pGen->pIn++;` |
|     ! 0 |  3786 | `	}` |
|     ! 0 |  3787 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3788 | `	pGen->pEnd = pTmp;` |
|       - |  3789 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3790 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3791 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3792 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3793 | `	/* Release the loop block */` |
|     ! 0 |  3794 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3795 | `	/* Statement successfully compiled */` |
|     ! 0 |  3796 | `	return SXRET_OK;` |
|       1 |  3797 | `Synchronize:` |
|       - |  3798 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3799 | `	 * compiling this erroneous block.` |
|       - |  3800 | `	 */` |
|       3 |  3801 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3802 | `		pGen->pIn++;` |
|     ! 0 |  3803 | `	}` |
|       3 |  3804 | `	return SXRET_OK;` |
|       2 |  3805 |  |
|       - |  3806 | `/*` |
|       - |  3807 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3808 | ` * According to the PHP language reference` |
|       - |  3809 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3810 | ` *  The syntax of a for loop is:` |
|       - |  3811 | ` *  for (expr1; expr2; expr3)` |
|       - |  3812 | ` *   statement` |
|       - |  3813 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3814 | ` *  the beginning of the loop.` |
|       - |  3815 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3816 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3817 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3818 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3819 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3820 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3821 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3822 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3823 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3824 | ` *  of using the for truth expression.` |
|       - |  3825 | ` */` |
|   11778 |  3826 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3827 |  |
|   11780 |  3828 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11780 |  3829 | `	GenBlock *pForBlock = 0;` |
|       - |  3830 | `	sxu32 nFalseJump;` |
|       - |  3831 | `	sxu32 nLine;` |
|       - |  3832 | `	sxi32 rc;` |
|   11780 |  3833 | `	nLine = pGen->pIn->nLine;` |
|       - |  3834 | `	/* Jump the 'for' keyword */` |
|   11780 |  3835 | `	pGen->pIn++;` |
|   11780 |  3836 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3837 | `		/* Syntax error */` |
|     ! 0 |  3838 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3839 | `		if( rc == SXERR_ABORT ){` |
|       - |  3840 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3841 | `			return SXERR_ABORT;` |
|       - |  3842 | `		}` |
|     ! 0 |  3843 | `		return SXRET_OK;` |
|       - |  3844 | `	}` |
|       - |  3845 | `	/* Jump the left parenthesis '(' */` |
|   11780 |  3846 | `	pGen->pIn++;` |
|       - |  3847 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11780 |  3848 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11780 |  3849 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3850 | `		/* Empty expression */` |
|     ! 0 |  3851 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3852 | `		if( rc == SXERR_ABORT ){` |
|       - |  3853 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3854 | `			return SXERR_ABORT;` |
|       - |  3855 | `		}` |
|       - |  3856 | `		/* Synchronize */` |
|     ! 0 |  3857 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3858 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3859 | `			pGen->pIn++;` |
|     ! 0 |  3860 | `		}` |
|     ! 0 |  3861 | `		return SXRET_OK;` |
|       - |  3862 | `	}` |
|       - |  3863 | `	/* Swap token streams */` |
|   11780 |  3864 | `	pTmp = pGen->pEnd;` |
|   11780 |  3865 | `	pGen->pEnd = pEnd;` |
|       - |  3866 | `	/* Compile initialization expressions if available */` |
|   11780 |  3867 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3868 | `	/* Pop operand lvalues */` |
|   11780 |  3869 | `	if( rc == SXERR_ABORT ){` |
|       - |  3870 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3871 | `		return SXERR_ABORT;` |
|   11780 |  3872 | `	}else if( rc != SXERR_EMPTY ){` |
|   11778 |  3873 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5888 |  3874 | `	}` |
|   11780 |  3875 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3876 | `		/* Syntax error */` |
|     ! 0 |  3877 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3878 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3879 | `		if( rc == SXERR_ABORT ){` |
|       - |  3880 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3881 | `			return SXERR_ABORT;` |
|       - |  3882 | `		}` |
|     ! 0 |  3883 | `		return SXRET_OK;` |
|       - |  3884 | `	}` |
|       - |  3885 | `	/* Jump the trailing ';' */` |
|   11780 |  3886 | `	pGen->pIn++;` |
|       - |  3887 | `	/* Create the loop block */` |
|   11780 |  3888 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11780 |  3889 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3890 | `		return SXERR_ABORT;` |
|       - |  3891 | `	}` |
|       - |  3892 | `	/* Deffer continue jumps */` |
|   11780 |  3893 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3894 | `	/* Compile the condition */` |
|   11780 |  3895 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11780 |  3896 | `	if( rc == SXERR_ABORT ){` |
|       - |  3897 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3898 | `		return SXERR_ABORT;` |
|   11780 |  3899 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3900 | `		/* Emit the false jump */` |
|   11778 |  3901 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3902 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11778 |  3903 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5888 |  3904 | `	}` |
|   11780 |  3905 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3906 | `		/* Syntax error */` |
|       5 |  3907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3908 | `			"for: Expected ';' after conditionals expressions");` |
|       5 |  3909 | `		if( rc == SXERR_ABORT ){` |
|       - |  3910 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3911 | `			return SXERR_ABORT;` |
|       - |  3912 | `		}` |
|       5 |  3913 | `		return SXRET_OK;` |
|       - |  3914 | `	}` |
|       - |  3915 | `	/* Jump the trailing ';' */` |
|   11776 |  3916 | `	pGen->pIn++;` |
|       - |  3917 | `	/* Save the post condition stream */` |
|   11776 |  3918 | `	pPostStart = pGen->pIn;` |
|       - |  3919 | `	/* Compile the loop body */` |
|   11776 |  3920 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11776 |  3921 | `	pGen->pEnd = pTmp;` |
|   11776 |  3922 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11776 |  3923 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3924 | `		return SXERR_ABORT;` |
|       - |  3925 | `	}` |
|       - |  3926 | `	/* Fix post-continue jumps */` |
|   11776 |  3927 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3928 | `		JumpFixup *aPost;` |
|       - |  3929 | `		VmInstr *pInstr;` |
|       - |  3930 | `		sxu32 nJumpDest;` |
|       - |  3931 | `		sxu32 n;` |
|      14 |  3932 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3933 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3934 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3935 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3936 | `			if( pInstr ){` |
|       - |  3937 | `				/* Fix jump */` |
|      14 |  3938 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3939 | `			}` |
|       8 |  3940 | `		}` |
|       6 |  3941 | `	}` |
|       - |  3942 | `	/* compile the post-expressions if available */` |
|   11776 |  3943 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3944 | `		pPostStart++;` |
|     ! 0 |  3945 | `	}` |
|   11776 |  3946 | `	if( pPostStart < pEnd ){` |
|       - |  3947 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11776 |  3948 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11776 |  3949 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11776 |  3950 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3951 | `			/* Syntax error */` |
|     ! 0 |  3952 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  3953 | `			if( rc == SXERR_ABORT ){` |
|       - |  3954 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3955 | `				return SXERR_ABORT;` |
|       - |  3956 | `			}` |
|     ! 0 |  3957 | `			return SXRET_OK;` |
|       - |  3958 | `		}` |
|   11776 |  3959 | `		RE_SWAP_DELIMITER(pGen);` |
|   11776 |  3960 | `		if( rc == SXERR_ABORT ){` |
|       - |  3961 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3962 | `			return SXERR_ABORT;` |
|   11776 |  3963 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  3964 | `			/* Pop operand lvalue */` |
|   11776 |  3965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5887 |  3966 | `		}` |
|    5887 |  3967 | `	}` |
|       - |  3968 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11776 |  3969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  3970 | `	/* Fix all jumps now the destination is resolved */` |
|   11776 |  3971 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3972 | `	/* Release the loop block */` |
|   11776 |  3973 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3974 | `	/* Statement successfully compiled */` |
|   11776 |  3975 | `	return SXRET_OK;` |
|    5891 |  3976 |  |
|       - |  3977 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  3978 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  3979 | ` * are allowed.` |
|       - |  3980 | ` */` |
|    6254 |  3981 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  3982 |  |
|    6256 |  3983 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6256 |  3984 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  3985 | `		/* Unexpected expression */` |
|     ! 0 |  3986 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  3987 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  3988 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  3989 | `			rc = SXERR_INVALID;` |
|     ! 0 |  3990 | `		}` |
|     ! 0 |  3991 | `	}` |
|    6256 |  3992 | `	return rc;` |
|       2 |  3993 |  |
|       - |  3994 | `/*` |
|       - |  3995 | ` * Compile the 'foreach' statement.` |
|       - |  3996 | ` * According to the PHP language reference` |
|       - |  3997 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  3998 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  3999 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4000 | ` *  is a minor but useful extension of the first:` |
|       - |  4001 | ` *  foreach (array_expression as $value)` |
|       - |  4002 | ` *    statement` |
|       - |  4003 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4004 | ` *   statement` |
|       - |  4005 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4006 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4007 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4008 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4009 | ` *  to the variable $key on each loop.` |
|       - |  4010 | ` *  Note:` |
|       - |  4011 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4012 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4013 | ` *  Note:` |
|       - |  4014 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4015 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4016 | ` *  or after the foreach without resetting it.` |
|       - |  4017 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4018 | ` *  of copying the value.` |
|       - |  4019 | ` */` |
|    3184 |  4020 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  4021 |  |
|    3186 |  4022 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3186 |  4023 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3186 |  4024 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4025 | `	ph7_foreach_info *pInfo;` |
|       - |  4026 | `	sxu32 nFalseJump;` |
|       - |  4027 | `	VmInstr *pInstr;` |
|       - |  4028 | `	sxu32 nLine;` |
|       - |  4029 | `	sxi32 rc;` |
|    3186 |  4030 | `	nLine = pGen->pIn->nLine;` |
|       - |  4031 | `	/* Jump the 'foreach' keyword */` |
|    3186 |  4032 | `	pGen->pIn++;` |
|    3186 |  4033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4034 | `		/* Syntax error */` |
|     ! 0 |  4035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4036 | `		if( rc == SXERR_ABORT ){` |
|       - |  4037 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4038 | `			return SXERR_ABORT;` |
|       - |  4039 | `		}` |
|     ! 0 |  4040 | `		goto Synchronize;` |
|       - |  4041 | `	}` |
|       - |  4042 | `	/* Jump the left parenthesis '(' */` |
|    3186 |  4043 | `	pGen->pIn++;` |
|       - |  4044 | `	/* Create the loop block */` |
|    3186 |  4045 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3186 |  4046 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4047 | `		return SXERR_ABORT;` |
|       - |  4048 | `	}` |
|       - |  4049 | `	/* Delimit the expression */` |
|    3186 |  4050 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3186 |  4051 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4052 | `		/* Empty expression */` |
|     ! 0 |  4053 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4054 | `		if( rc == SXERR_ABORT ){` |
|       - |  4055 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4056 | `			return SXERR_ABORT;` |
|       - |  4057 | `		}` |
|       - |  4058 | `		/* Synchronize */` |
|     ! 0 |  4059 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4060 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4061 | `			pGen->pIn++;` |
|     ! 0 |  4062 | `		}` |
|     ! 0 |  4063 | `		return SXRET_OK;` |
|       - |  4064 | `	}` |
|       - |  4065 | `	/* Compile the array expression */` |
|    3186 |  4066 | `	pCur = pGen->pIn;` |
|   21394 |  4067 | `	while( pCur < pEnd ){` |
|   21394 |  4068 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3196 |  4069 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3196 |  4070 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4071 | `				/* Break with the first 'as' found */` |
|    3186 |  4072 | `				break;` |
|       - |  4073 | `			}` |
|       5 |  4074 | `		}` |
|       - |  4075 | `		/* Advance the stream cursor */` |
|   18210 |  4076 | `		pCur++;` |
|       2 |  4077 | `	}` |
|    3186 |  4078 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4079 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4080 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4081 | `		if( rc == SXERR_ABORT ){` |
|       - |  4082 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4083 | `			return SXERR_ABORT;` |
|       - |  4084 | `		}` |
|     ! 0 |  4085 | `		goto Synchronize;` |
|       - |  4086 | `	}` |
|       - |  4087 | `	/* Swap token streams */` |
|    3186 |  4088 | `	pTmp = pGen->pEnd;` |
|    3186 |  4089 | `	pGen->pEnd = pCur;` |
|    3186 |  4090 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3186 |  4091 | `	if( rc == SXERR_ABORT ){` |
|       - |  4092 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4093 | `		return SXERR_ABORT;` |
|       - |  4094 | `	}` |
|       - |  4095 | `	/* Update token stream */` |
|    3186 |  4096 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4097 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4098 | `		if( rc == SXERR_ABORT ){` |
|       - |  4099 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4100 | `			return SXERR_ABORT;` |
|       - |  4101 | `		}` |
|     ! 0 |  4102 | `		pGen->pIn++;` |
|     ! 0 |  4103 | `	}` |
|    3186 |  4104 | `	pCur++; /* Jump the 'as' keyword */` |
|    3186 |  4105 | `	pGen->pIn = pCur;` |
|    3186 |  4106 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4107 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4108 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4109 | `			return SXERR_ABORT;` |
|       - |  4110 | `		}` |
|     ! 0 |  4111 | `	}` |
|       - |  4112 | `	/* Create the foreach context */` |
|    3186 |  4113 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3186 |  4114 | `	if( pInfo == 0 ){` |
|     ! 0 |  4115 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4116 | `		return SXERR_ABORT;` |
|       - |  4117 | `	}` |
|       - |  4118 | `	/* Zero the structure */` |
|    3186 |  4119 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4120 | `	/* Initialize structure fields */` |
|    3186 |  4121 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4122 | `	/* Check if we have a key field */` |
|    9604 |  4123 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6420 |  4124 | `		pCur++;` |
|       2 |  4125 | `	}` |
|    3186 |  4126 | `	if( pCur < pEnd ){` |
|       - |  4127 | `		/* Compile the expression holding the key name */` |
|    3082 |  4128 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4129 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4130 | `			if( rc == SXERR_ABORT ){` |
|       - |  4131 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4132 | `				return SXERR_ABORT;` |
|       - |  4133 | `			}` |
|     ! 0 |  4134 | `		}else{` |
|    3082 |  4135 | `			pGen->pEnd = pCur;` |
|    3082 |  4136 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3082 |  4137 | `			if( rc == SXERR_ABORT ){` |
|       - |  4138 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4139 | `				return SXERR_ABORT;` |
|       - |  4140 | `			}` |
|    3082 |  4141 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3082 |  4142 | `			if( pInstr->p3 ){` |
|       - |  4143 | `				/* Record key name */` |
|    3082 |  4144 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1540 |  4145 | `			}` |
|    3082 |  4146 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4147 | `		}` |
|    3082 |  4148 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1540 |  4149 | `	}` |
|    3186 |  4150 | `	pGen->pEnd = pEnd;` |
|    3186 |  4151 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4152 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4153 | `		if( rc == SXERR_ABORT ){` |
|       - |  4154 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4155 | `			return SXERR_ABORT;` |
|       - |  4156 | `		}` |
|     ! 0 |  4157 | `		goto Synchronize;` |
|       - |  4158 | `	}` |
|    3186 |  4159 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4160 | `		pGen->pIn++;` |
|       - |  4161 | `		/* Pass by reference  */` |
|      11 |  4162 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4163 | `	}` |
|       - |  4164 | `	/* Check if the value target is list() */` |
|    3186 |  4165 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4166 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4167 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4168 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4169 | `		 */` |
|       - |  4170 | `		static int iForeachListCnt = 0;` |
|       - |  4171 | `		char zTmp[128];` |
|       - |  4172 | `		sxu32 nLen;` |
|       - |  4173 | `		char *zDup;` |
|      10 |  4174 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4175 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4176 | `		if( zDup == 0 ){` |
|     ! 0 |  4177 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4178 | `			return SXERR_ABORT;` |
|       - |  4179 | `		}` |
|      10 |  4180 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4181 | `		/* Save list() token boundaries */` |
|      10 |  4182 | `		pListStart = pGen->pIn;` |
|       - |  4183 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4184 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4185 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4186 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4187 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4188 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4189 | `				return SXERR_ABORT;` |
|       - |  4190 | `			}` |
|       3 |  4191 | `			goto Synchronize;` |
|       - |  4192 | `		}` |
|       7 |  4193 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4194 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4195 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4196 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4197 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4198 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4199 | `				return SXERR_ABORT;` |
|       - |  4200 | `			}` |
|     ! 0 |  4201 | `			goto Synchronize;` |
|       - |  4202 | `		}` |
|       7 |  4203 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4204 | `		pListEnd = pGen->pIn;` |
|       7 |  4205 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3181 |  4206 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4207 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4208 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4209 | `		 */` |
|       - |  4210 | `		static int iForeachShortListCnt = 0;` |
|       - |  4211 | `		char zTmp[128];` |
|       - |  4212 | `		sxu32 nLen;` |
|       - |  4213 | `		char *zDup;` |
|       3 |  4214 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4215 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4216 | `		if( zDup == 0 ){` |
|     ! 0 |  4217 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4218 | `			return SXERR_ABORT;` |
|       - |  4219 | `		}` |
|       3 |  4220 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4221 | `		/* Save [...] token boundaries */` |
|       3 |  4222 | `		pListStart = pGen->pIn;` |
|       - |  4223 | `		/* Advance past [...] */` |
|       3 |  4224 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4225 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4226 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4227 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4228 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4229 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4230 | `				return SXERR_ABORT;` |
|       - |  4231 | `			}` |
|     ! 0 |  4232 | `			goto Synchronize;` |
|       - |  4233 | `		}` |
|       3 |  4234 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4235 | `		pListEnd = pGen->pIn;` |
|       3 |  4236 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4237 | `	}else{` |
|       - |  4238 | `		/* Compile the expression holding the value name */` |
|    3176 |  4239 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3176 |  4240 | `		if( rc == SXERR_ABORT ){` |
|       - |  4241 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4242 | `			return SXERR_ABORT;` |
|       - |  4243 | `		}` |
|    3176 |  4244 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3176 |  4245 | `		if( pInstr->p3 ){` |
|       - |  4246 | `			/* Record value name */` |
|    3176 |  4247 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1587 |  4248 | `		}` |
|       - |  4249 | `	}` |
|       - |  4250 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3184 |  4251 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4252 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3184 |  4253 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4254 | `	/* Record the first instruction to execute */` |
|    3184 |  4255 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4256 | `	/* Emit the FOREACH_STEP instruction */` |
|    3184 |  4257 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4258 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3184 |  4259 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4260 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3184 |  4261 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4262 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4263 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4264 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4265 | `		 */` |
|       9 |  4266 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4267 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4268 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4269 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4270 | `		 */` |
|       9 |  4271 | `		pSavedIn = pGen->pIn;` |
|       9 |  4272 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4273 | `		pGen->pIn = pListStart;` |
|       9 |  4274 | `		pGen->pEnd = pListEnd;` |
|       9 |  4275 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4276 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4277 | `		}else{` |
|       7 |  4278 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4279 | `		}` |
|       9 |  4280 | `		pGen->pIn = pSavedIn;` |
|       9 |  4281 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4282 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4283 | `			return SXERR_ABORT;` |
|       - |  4284 | `		}` |
|       - |  4285 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4286 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4287 | `	}` |
|       - |  4288 | `	/* Compile the loop body */` |
|    3184 |  4289 | `	pGen->pIn = &pEnd[1];` |
|    3184 |  4290 | `	pGen->pEnd = pTmp;` |
|    3184 |  4291 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3184 |  4292 | `	if( rc == SXERR_ABORT ){` |
|       - |  4293 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4294 | `		return SXERR_ABORT;` |
|       - |  4295 | `	}` |
|       - |  4296 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3184 |  4297 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4298 | `	/* Fix all jumps now the destination is resolved */` |
|    3184 |  4299 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4300 | `	/* Release the loop block */` |
|    3184 |  4301 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4302 | `	/* Statement successfully compiled */` |
|    3184 |  4303 | `	return SXRET_OK;` |
|       1 |  4304 | `Synchronize:` |
|       - |  4305 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4306 | `	 * compiling this erroneous block.` |
|       - |  4307 | `	 */` |
|       3 |  4308 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4309 | `		pGen->pIn++;` |
|     ! 0 |  4310 | `	}` |
|       3 |  4311 | `	return SXRET_OK;` |
|    1594 |  4312 |  |
|       - |  4313 | `/*` |
|       - |  4314 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4315 | ` * According to the PHP language reference` |
|       - |  4316 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4317 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4318 | ` *  that is similar to that of C:` |
|       - |  4319 | ` *  if (expr)` |
|       - |  4320 | ` *   statement` |
|       - |  4321 | ` *  else construct:` |
|       - |  4322 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4323 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4324 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4325 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4326 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4327 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4328 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4329 | ` *  elseif` |
|       - |  4330 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4331 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4332 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4333 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4334 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4335 | ` *   <?php` |
|       - |  4336 | ` *    if ($a > $b) {` |
|       - |  4337 | ` *     echo "a is bigger than b";` |
|       - |  4338 | ` *    } elseif ($a == $b) {` |
|       - |  4339 | ` *     echo "a is equal to b";` |
|       - |  4340 | ` *    } else {` |
|       - |  4341 | ` *     echo "a is smaller than b";` |
|       - |  4342 | ` *    }` |
|       - |  4343 | ` *    ?>` |
|       - |  4344 | ` */` |
|  122688 |  4345 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4346 |  |
|  122690 |  4347 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  122690 |  4348 | `	GenBlock *pCondBlock = 0;` |
|       - |  4349 | `	sxu32 nJumpIdx;` |
|       - |  4350 | `	sxu32 nKeyID;` |
|       - |  4351 | `	sxi32 rc;` |
|       - |  4352 | `	/* Jump the 'if' keyword */` |
|  122690 |  4353 | `	pGen->pIn++;` |
|  122690 |  4354 | `	pToken = pGen->pIn;` |
|       - |  4355 | `	/* Create the conditional block */` |
|  122690 |  4356 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  122690 |  4357 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4358 | `		return SXERR_ABORT;` |
|       - |  4359 | `	}` |
|       - |  4360 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   67193 |  4361 | `	for(;;){` |
|  134388 |  4362 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4363 | `			/* Syntax error */` |
|     ! 0 |  4364 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4365 | `				pToken--;` |
|     ! 0 |  4366 | `			}` |
|     ! 0 |  4367 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4368 | `			if( rc == SXERR_ABORT ){` |
|       - |  4369 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4370 | `				return SXERR_ABORT;` |
|       - |  4371 | `			}` |
|     ! 0 |  4372 | `			goto Synchronize;` |
|       - |  4373 | `		}` |
|       - |  4374 | `		/* Jump the left parenthesis '(' */` |
|  134388 |  4375 | `		pToken++;` |
|       - |  4376 | `		/* Delimit the condition */` |
|  134388 |  4377 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  134388 |  4378 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4379 | `			/* Syntax error */` |
|     ! 0 |  4380 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4381 | `				pToken--;` |
|     ! 0 |  4382 | `			}` |
|     ! 0 |  4383 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4384 | `			if( rc == SXERR_ABORT ){` |
|       - |  4385 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4386 | `				return SXERR_ABORT;` |
|       - |  4387 | `			}` |
|     ! 0 |  4388 | `			goto Synchronize;` |
|       - |  4389 | `		}` |
|       - |  4390 | `		/* Swap token streams */` |
|  134388 |  4391 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4392 | `		/* Compile the condition */` |
|  134388 |  4393 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4394 | `		/* Update token stream */` |
|  134388 |  4395 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4396 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4397 | `			pGen->pIn++;` |
|     ! 0 |  4398 | `		}` |
|  134388 |  4399 | `		pGen->pIn  = &pEnd[1];` |
|  134388 |  4400 | `		pGen->pEnd = pTmp;` |
|  134388 |  4401 | `		if( rc == SXERR_ABORT ){` |
|       - |  4402 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4403 | `			return SXERR_ABORT;` |
|       - |  4404 | `		}` |
|       - |  4405 | `		/* Emit the false jump */` |
|  134388 |  4406 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4407 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  134388 |  4408 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4409 | `		/* Compile the body */` |
|  134388 |  4410 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  134388 |  4411 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4412 | `			return SXERR_ABORT;` |
|       - |  4413 | `		}` |
|  134388 |  4414 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   37496 |  4415 | `			break;` |
|       - |  4416 | `		}` |
|       - |  4417 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   59400 |  4418 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   59400 |  4419 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   38220 |  4420 | `			break;` |
|       - |  4421 | `		}` |
|       - |  4422 | `		/* Emit the unconditional jump */` |
|   21182 |  4423 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4424 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21182 |  4425 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   21182 |  4426 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   15320 |  4427 | `			pToken = &pGen->pIn[1];` |
|   15320 |  4428 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5866 |  4429 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4743 |  4430 | `					break;` |
|       - |  4431 | `			}` |
|    5838 |  4432 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2918 |  4433 | `		}` |
|   11700 |  4434 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4435 | `		/* Synchronize cursors */` |
|   11700 |  4436 | `		pToken = pGen->pIn;` |
|       - |  4437 | `		/* Fix the false jump */` |
|   11700 |  4438 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4439 | `	} /* For(;;) */` |
|       - |  4440 | `	/* Fix the false jump */` |
|  122690 |  4441 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  122690 |  4442 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   47700 |  4443 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4444 | `			/* Compile the else block */` |
|    9484 |  4445 | `			pGen->pIn++;` |
|    9484 |  4446 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9484 |  4447 | `			if( rc == SXERR_ABORT ){` |
|       - |  4448 |  |
|     ! 0 |  4449 | `				return SXERR_ABORT;` |
|       - |  4450 | `			}` |
|    4741 |  4451 | `	}` |
|  122690 |  4452 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4453 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  122690 |  4454 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4455 | `	/* Release the conditional block */` |
|  122690 |  4456 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4457 | `	/* Statement successfully compiled */` |
|  122690 |  4458 | `	return SXRET_OK;` |
|     ! 0 |  4459 | `Synchronize:` |
|       - |  4460 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4461 | `	 */` |
|     ! 0 |  4462 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4463 | `		pGen->pIn++;` |
|     ! 0 |  4464 | `	}` |
|     ! 0 |  4465 | `	return SXRET_OK;` |
|   61346 |  4466 |  |
|       - |  4467 | `/*` |
|       - |  4468 | ` * Compile the global construct.` |
|       - |  4469 | ` * According to the PHP language reference` |
|       - |  4470 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4471 | ` *  to be used in that function.` |
|       - |  4472 | ` *  Example #1 Using global` |
|       - |  4473 | ` *  <?php` |
|       - |  4474 | ` *   $a = 1;` |
|       - |  4475 | ` *   $b = 2;` |
|       - |  4476 | ` *   function Sum()` |
|       - |  4477 | ` *   {` |
|       - |  4478 | ` *    global $a, $b;` |
|       - |  4479 | ` *    $b = $a + $b;` |
|       - |  4480 | ` *   }` |
|       - |  4481 | ` *   Sum();` |
|       - |  4482 | ` *   echo $b;` |
|       - |  4483 | ` *  ?>` |
|       - |  4484 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4485 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4486 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4487 | ` */` |
|      32 |  4488 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4489 |  |
|      34 |  4490 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4491 | `	sxi32 nExpr;` |
|       - |  4492 | `	sxi32 rc;` |
|       - |  4493 | `	/* Jump the 'global' keyword */` |
|      34 |  4494 | `	pGen->pIn++;` |
|      34 |  4495 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4496 | `		/* Nothing to process */` |
|     ! 0 |  4497 | `		return SXRET_OK;` |
|       - |  4498 | `	}` |
|      34 |  4499 | `	pTmp = pGen->pEnd;` |
|      34 |  4500 | `	nExpr = 0;` |
|      68 |  4501 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      36 |  4502 | `		if( pGen->pIn < pNext ){` |
|      36 |  4503 | `			pGen->pEnd = pNext;` |
|      36 |  4504 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4505 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4506 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4507 | `					return SXERR_ABORT;` |
|       - |  4508 | `				}` |
|     ! 0 |  4509 | `			}else{` |
|      36 |  4510 | `				pGen->pIn++;` |
|      36 |  4511 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4512 | `					/* Emit a warning */` |
|     ! 0 |  4513 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4514 | `				}else{` |
|      36 |  4515 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      36 |  4516 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4517 | `						return SXERR_ABORT;` |
|      36 |  4518 | `					}else if(rc != SXERR_EMPTY ){` |
|      36 |  4519 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      36 |  4520 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4521 | `							/* Variable name, not a constant */` |
|      36 |  4522 | `							pLast->iP1 = 0;` |
|      17 |  4523 | `						}` |
|      36 |  4524 | `						nExpr++;` |
|      17 |  4525 | `					}` |
|       - |  4526 | `				}` |
|       - |  4527 | `			}` |
|      17 |  4528 | `		}` |
|       - |  4529 | `		/* Next expression in the stream */` |
|      36 |  4530 | `		pGen->pIn = pNext;` |
|       - |  4531 | `		/* Jump trailing commas */` |
|      38 |  4532 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4533 | `			pGen->pIn++;` |
|       1 |  4534 | `		}` |
|       2 |  4535 | `	}` |
|       - |  4536 | `	/* Restore token stream */` |
|      34 |  4537 | `	pGen->pEnd = pTmp;` |
|      34 |  4538 | `	if( nExpr > 0 ){` |
|       - |  4539 | `		/* Emit the uplink instruction */` |
|      34 |  4540 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      16 |  4541 | `	}` |
|      34 |  4542 | `	return SXRET_OK;` |
|      18 |  4543 |  |
|       - |  4544 | `/*` |
|       - |  4545 | ` * Compile the return statement.` |
|       - |  4546 | ` * According to the PHP language reference` |
|       - |  4547 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4548 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4549 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4550 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4551 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4552 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4553 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4554 | ` *  from within the main script file, then script execution end.` |
|       - |  4555 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4556 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4557 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4558 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4559 | ` */` |
|  193342 |  4560 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4561 |  |
|  193344 |  4562 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4563 | `	sxi32 rc;` |
|       - |  4564 | `	/* Jump the 'return' keyword */` |
|  193344 |  4565 | `	pGen->pIn++;` |
|  193344 |  4566 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4567 | `		/* Compile the expression */` |
|  193322 |  4568 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  193322 |  4569 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4570 | `			return SXERR_ABORT;` |
|  193322 |  4571 | `		}else if(rc != SXERR_EMPTY ){` |
|  193322 |  4572 | `			nRet = 1;` |
|   96660 |  4573 | `		}` |
|   96660 |  4574 | `	}` |
|       - |  4575 | `	/* Emit the done instruction */` |
|  193344 |  4576 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  193344 |  4577 | `	return SXRET_OK;` |
|   96673 |  4578 |  |
|       - |  4579 | `/*` |
|       - |  4580 | ` * Compile a yield expression.` |
|       - |  4581 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4582 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4583 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4584 | ` */` |
|      34 |  4585 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  4586 |  |
|       - |  4587 | `	SyToken *pTmp, *pSplit;` |
|      36 |  4588 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      36 |  4589 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4590 | `	sxi32 rc;` |
|      17 |  4591 | `	(void)iCompileFlag;` |
|       - |  4592 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      36 |  4593 | `	pGen->pIn++;` |
|       - |  4594 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4595 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      36 |  4596 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4597 | `		/* Bare yield — no value */` |
|     ! 0 |  4598 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4599 | `		return SXRET_OK;` |
|       - |  4600 | `	}` |
|       - |  4601 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      36 |  4602 | `	pSplit = 0;` |
|       - |  4603 | `	{` |
|      36 |  4604 | `		SyToken *pCur = pGen->pIn;` |
|      36 |  4605 | `		sxi32 nNest = 0;` |
|      84 |  4606 | `		while( pCur < pGen->pEnd ){` |
|      56 |  4607 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4608 | `				nNest++;` |
|      56 |  4609 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4610 | `				nNest--;` |
|      56 |  4611 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 |  4612 | `				pSplit = pCur;` |
|       7 |  4613 | `				break;` |
|       - |  4614 | `			}` |
|      50 |  4615 | `			pCur++;` |
|       2 |  4616 | `		}` |
|       - |  4617 | `	}` |
|      36 |  4618 | `	pTmp = pGen->pEnd;` |
|      36 |  4619 | `	if( pSplit ){` |
|       - |  4620 | `		/* yield $key => $value */` |
|       7 |  4621 | `		pGen->pEnd = pSplit;` |
|       7 |  4622 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4623 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4624 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 |  4625 | `		pGen->pEnd = pTmp;` |
|       7 |  4626 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4627 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4628 | `		iP1 = 1;` |
|       7 |  4629 | `		iP2 = 1;` |
|       4 |  4630 | `	}else{` |
|       - |  4631 | `		/* yield $value */` |
|      30 |  4632 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      30 |  4633 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      30 |  4634 | `		if( rc != SXERR_EMPTY ){` |
|      30 |  4635 | `			iP1 = 1;` |
|      14 |  4636 | `		}` |
|       - |  4637 | `	}` |
|      36 |  4638 | `	pGen->pEnd = pTmp;` |
|      36 |  4639 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      36 |  4640 | `	return SXRET_OK;` |
|      19 |  4641 |  |
|       - |  4642 | `/*` |
|       - |  4643 | ` * Compile the die/exit language construct.` |
|       - |  4644 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4645 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4646 | ` */` |
|      88 |  4647 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 |  4648 |  |
|      90 |  4649 | `	sxi32 nExpr = 0;` |
|       - |  4650 | `	sxi32 rc;` |
|       - |  4651 | `	/* Jump the die/exit keyword */` |
|      90 |  4652 | `	pGen->pIn++;` |
|      90 |  4653 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4654 | `		/* Compile the expression */` |
|      90 |  4655 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 |  4656 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4657 | `			return SXERR_ABORT;` |
|      90 |  4658 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 |  4659 | `			nExpr = 1;` |
|      44 |  4660 | `		}` |
|      44 |  4661 | `	}` |
|       - |  4662 | `	/* Emit the HALT instruction */` |
|      90 |  4663 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 |  4664 | `	return SXRET_OK;` |
|      46 |  4665 |  |
|       - |  4666 | `/*` |
|       - |  4667 | ` * Compile the 'echo' language construct.` |
|       - |  4668 | ` */` |
|   11854 |  4669 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4670 |  |
|   11856 |  4671 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4672 | `	sxi32 rc;` |
|       - |  4673 | `	/* Jump the 'echo' keyword */` |
|   11856 |  4674 | `	pGen->pIn++;` |
|       - |  4675 | `	/* Compile arguments one after one */` |
|   11856 |  4676 | `	pTmp = pGen->pEnd;` |
|   24860 |  4677 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   13006 |  4678 | `		if( pGen->pIn < pNext ){` |
|   13006 |  4679 | `			pGen->pEnd = pNext;` |
|   13006 |  4680 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   13006 |  4681 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4682 | `				return SXERR_ABORT;` |
|   13006 |  4683 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4684 | `				/* Emit the consume instruction */` |
|   12982 |  4685 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6490 |  4686 | `			}` |
|    6502 |  4687 | `		}` |
|       - |  4688 | `		/* Jump trailing commas */` |
|   14156 |  4689 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    1152 |  4690 | `			pNext++;` |
|       2 |  4691 | `		}` |
|   13006 |  4692 | `		pGen->pIn = pNext;` |
|       2 |  4693 | `	}` |
|       - |  4694 | `	/* Restore token stream */` |
|   11856 |  4695 | `	pGen->pEnd = pTmp;` |
|   11856 |  4696 | `	return SXRET_OK;` |
|    5929 |  4697 |  |
|       - |  4698 | `/*` |
|       - |  4699 | ` * Compile the static statement.` |
|       - |  4700 | ` * According to the PHP language reference` |
|       - |  4701 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4702 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4703 | ` *  when program execution leaves this scope.` |
|       - |  4704 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4705 | ` * Symisc eXtension.` |
|       - |  4706 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4707 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4708 | ` *  Example` |
|       - |  4709 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4710 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4711 | ` */` |
|       2 |  4712 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 |  4713 |  |
|       - |  4714 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4715 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4716 | `	GenBlock *pBlock;` |
|       - |  4717 | `	SyString *pName;` |
|       - |  4718 | `	char *zDup;` |
|       - |  4719 | `	sxu32 nLine;` |
|       - |  4720 | `	sxi32 rc;` |
|       - |  4721 | `	/* Jump the static keyword */` |
|       3 |  4722 | `	nLine = pGen->pIn->nLine;` |
|       3 |  4723 | `	pGen->pIn++;` |
|       - |  4724 | `	/* Extract the enclosing function if any */` |
|       3 |  4725 | `	pBlock = pGen->pCurrent;` |
|       5 |  4726 | `	while( pBlock ){` |
|       5 |  4727 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 |  4728 | `			break;` |
|       - |  4729 | `		}` |
|       - |  4730 | `		/* Point to the upper block */` |
|       3 |  4731 | `		pBlock = pBlock->pParent;` |
|       1 |  4732 | `	}` |
|       3 |  4733 | `	if( pBlock == 0 ){` |
|       - |  4734 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4735 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4736 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4737 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4738 | `				return SXERR_ABORT;` |
|       - |  4739 | `			}` |
|     ! 0 |  4740 | `			goto Synchronize;` |
|       - |  4741 | `		}` |
|       - |  4742 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4743 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4744 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4745 | `			return SXERR_ABORT;` |
|     ! 0 |  4746 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4747 | `			/* Emit the POP instruction */` |
|     ! 0 |  4748 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4749 | `		}` |
|     ! 0 |  4750 | `		return SXRET_OK;` |
|       - |  4751 | `	}` |
|       3 |  4752 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4753 | `	/* Make sure we are dealing with a valid statement */` |
|       3 |  4754 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 |  4755 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4756 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4757 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4758 | `				return SXERR_ABORT;` |
|       - |  4759 | `			}` |
|       3 |  4760 | `			goto Synchronize;` |
|       - |  4761 | `	}` |
|     ! 0 |  4762 | `	pGen->pIn++;` |
|       - |  4763 | `	/* Extract variable name */` |
|     ! 0 |  4764 | `	pName = &pGen->pIn->sData;` |
|     ! 0 |  4765 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 |  4766 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4767 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4768 | `		goto Synchronize;` |
|       - |  4769 | `	}` |
|       - |  4770 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 |  4771 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 |  4772 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4773 | `	/* Duplicate variable name */` |
|     ! 0 |  4774 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 |  4775 | `	if( zDup == 0 ){` |
|     ! 0 |  4776 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4777 | `		return SXERR_ABORT;` |
|       - |  4778 | `	}` |
|     ! 0 |  4779 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4780 | `	/* Check if we have an expression to compile */` |
|     ! 0 |  4781 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4782 | `		SySet *pInstrContainer;` |
|       - |  4783 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4784 | `		 * Static variable can take any complex expression including function` |
|       - |  4785 | `		 * call as their initialization value.` |
|       - |  4786 | `		 * Example:` |
|       - |  4787 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4788 | `		 */` |
|     ! 0 |  4789 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4790 | `		/* Swap bytecode container */` |
|     ! 0 |  4791 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 |  4792 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4793 | `		/* Compile the expression */` |
|     ! 0 |  4794 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4795 | `		/* Emit the done instruction */` |
|     ! 0 |  4796 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4797 | `		/* Restore default bytecode container */` |
|     ! 0 |  4798 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  4799 | `	}` |
|       - |  4800 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 |  4801 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 |  4802 | `	return SXRET_OK;` |
|       1 |  4803 | `Synchronize:` |
|       - |  4804 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4805 | `	 * statement.` |
|       - |  4806 | `	 */` |
|       5 |  4807 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4808 | `		pGen->pIn++;` |
|       1 |  4809 | `	}` |
|       3 |  4810 | `	return SXRET_OK;` |
|       2 |  4811 |  |
|       - |  4812 | `/*` |
|       - |  4813 | ` * Compile the var statement.` |
|       - |  4814 | ` * Symisc Extension:` |
|       - |  4815 | ` *      var statement can be used outside of a class definition.` |
|       - |  4816 | ` */` |
|       4 |  4817 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4818 |  |
|       - |  4819 | `	sxu32 nLine;` |
|       - |  4820 | `	sxi32 rc;` |
|       5 |  4821 | `	nLine = pGen->pIn->nLine;` |
|       - |  4822 | `	/* Jump the 'var' keyword */` |
|       5 |  4823 | `	pGen->pIn++;` |
|       5 |  4824 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4825 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4826 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4827 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4828 | `			pGen->pIn++;` |
|     ! 0 |  4829 | `		}` |
|     ! 0 |  4830 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4831 | `			return SXERR_ABORT;` |
|       - |  4832 | `		}` |
|     ! 0 |  4833 | `	}else{` |
|       - |  4834 | `		/* Compile the expression */` |
|       5 |  4835 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4836 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4837 | `			return SXERR_ABORT;` |
|       5 |  4838 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4839 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4840 | `		}` |
|       - |  4841 | `	}` |
|       5 |  4842 | `	return SXRET_OK;` |
|       3 |  4843 |  |
|       - |  4844 | `/*` |
|       - |  4845 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4846 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4847 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4848 | ` */` |
|       - |  4849 | `/*` |
|       - |  4850 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4851 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4852 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4853 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4854 | ` *` |
|       - |  4855 | ` * Resolution order:` |
|       - |  4856 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4857 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4858 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4859 | ` *` |
|       - |  4860 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4861 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4862 | ` * Returns the (possibly new) literal index.` |
|       - |  4863 | ` */` |
|  360658 |  4864 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 |  4865 |  |
|       - |  4866 | `	ph7_value *pLit;` |
|       - |  4867 | `	const char *zLit;` |
|       - |  4868 | `	SyString sQualified;` |
|       - |  4869 | `	sxu32 nLit;` |
|       - |  4870 | `	sxu32 k;` |
|       - |  4871 | `	sxu32 nNewIdx;` |
|       - |  4872 | `	int hasNsSep;` |
|       - |  4873 | `	SyHashEntry *pImport;` |
|       - |  4874 | `	ph7_value *pNew;` |
|  360660 |  4875 | `	if( pFromImport ){` |
|  345086 |  4876 | `		*pFromImport = 0;` |
|  172542 |  4877 | `	}` |
|  360660 |  4878 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  360660 |  4879 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4880 | `		return nOrigIdx;` |
|       - |  4881 | `	}` |
|  360660 |  4882 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  360660 |  4883 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4884 | `	/* Skip if already qualified (contains backslash) */` |
|  360660 |  4885 | `	hasNsSep = 0;` |
| 3900812 |  4886 | `	for( k = 0; k < nLit; k++ ){` |
| 3540162 |  4887 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1770078 |  4888 | `	}` |
|  360660 |  4889 | `	if( hasNsSep ){` |
|       9 |  4890 | `		return nOrigIdx;` |
|       - |  4891 | `	}` |
|       - |  4892 | `	/* Check use imports first (works even outside namespaces) */` |
|  360652 |  4893 | `	SyBlobReset(&pGen->sWorker);` |
|  360652 |  4894 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  360652 |  4895 | `	if( pImport ){` |
|      38 |  4896 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4897 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4898 | `		if( pFromImport ){` |
|      18 |  4899 | `			*pFromImport = 1;` |
|       8 |  4900 | `		}` |
|      20 |  4901 | `	}else{` |
|  360616 |  4902 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  360526 |  4903 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4904 | `		}` |
|       - |  4905 | `		/* Prepend current namespace */` |
|      92 |  4906 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      92 |  4907 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      92 |  4908 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4909 | `	}` |
|       - |  4910 | `	/* Look up or create a new literal for the qualified name */` |
|     128 |  4911 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     128 |  4912 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 |  4913 | `		return nNewIdx; /* Already interned */` |
|       - |  4914 | `	}` |
|      76 |  4915 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      76 |  4916 | `	if( pNew == 0 ){` |
|     ! 0 |  4917 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4918 | `	}` |
|      76 |  4919 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      76 |  4920 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      76 |  4921 | `	return nNewIdx;` |
|  180331 |  4922 |  |
|       - |  4923 | `/*` |
|       - |  4924 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4925 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4926 | ` */` |
|   32554 |  4927 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4928 |  |
|       - |  4929 | `	SyHashEntry *pImport;` |
|       - |  4930 | `	/* Check use imports first */` |
|   32556 |  4931 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   32556 |  4932 | `	if( pImport ){` |
|      14 |  4933 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  4934 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  4935 | `		return;` |
|       - |  4936 | `	}` |
|       - |  4937 | `	/* Prepend current namespace if active */` |
|   32544 |  4938 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4939 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4940 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4941 | `	}` |
|   32544 |  4942 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   16279 |  4943 |  |
|       - |  4944 | `/*` |
|       - |  4945 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4946 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4947 | ` * The caller must release pOut when done.` |
|       - |  4948 | ` */` |
|   53392 |  4949 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4950 |  |
|   53394 |  4951 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      60 |  4952 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      60 |  4953 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  4954 | `	}` |
|   53394 |  4955 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   53394 |  4956 |  |
|       - |  4957 | `/*` |
|       - |  4958 | ` * Compile a namespace statement` |
|       - |  4959 | ` * According to the PHP language reference manual` |
|       - |  4960 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  4961 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  4962 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  4963 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  4964 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  4965 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  4966 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  4967 | ` *  programming world.` |
|       - |  4968 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  4969 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  4970 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  4971 | ` *  classes/functions/constants.` |
|       - |  4972 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  4973 | ` *  readability of source code.` |
|       - |  4974 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  4975 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  4976 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  4977 | ` *       class MyClass {}` |
|       - |  4978 | ` *       function myfunction() {}` |
|       - |  4979 | ` *       const MYCONST = 1;` |
|       - |  4980 | ` *       $a = new MyClass;` |
|       - |  4981 | ` *       $c = new \my\name\MyClass;` |
|       - |  4982 | ` *       $a = strlen('hi');` |
|       - |  4983 | ` *       $d = namespace\MYCONST;` |
|       - |  4984 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  4985 | ` *       echo constant($d);` |
|       - |  4986 | ` * NOTE` |
|       - |  4987 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  4988 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  4989 | ` */` |
|       - |  4990 | `/*` |
|       - |  4991 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  4992 | ` */` |
|      14 |  4993 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 |  4994 |  |
|      15 |  4995 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 |  4996 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 |  4997 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 |  4998 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 |  4999 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 |  5000 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5001 | `	return "token";` |
|       8 |  5002 |  |
|     104 |  5003 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 |  5004 |  |
|       - |  5005 | `	sxu32 nLine;` |
|       - |  5006 | `	sxi32 rc;` |
|     106 |  5007 | `	nLine = pGen->pIn->nLine;` |
|     106 |  5008 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5009 | `	/* Reset namespace and clear previous use imports */` |
|     106 |  5010 | `	SyBlobReset(&pGen->sNamespace);` |
|     106 |  5011 | `	SyHashRelease(&pGen->hUseImports);` |
|     106 |  5012 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5013 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     106 |  5014 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5015 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     106 |  5016 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5017 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5018 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5020 | `		return SXRET_OK;` |
|       - |  5021 | `	}` |
|     106 |  5022 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5023 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5024 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5025 | `		return SXRET_OK;` |
|       - |  5026 | `	}` |
|     106 |  5027 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5028 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5029 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5030 | `		return SXRET_OK;` |
|       - |  5031 | `	}` |
|       - |  5032 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     252 |  5033 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     148 |  5034 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5035 | `			/* Append backslash separator */` |
|      24 |  5036 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      24 |  5037 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5038 | `			}` |
|      13 |  5039 | `		}else{` |
|       - |  5040 | `			/* Append identifier */` |
|     126 |  5041 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5042 | `		}` |
|     148 |  5043 | `		pGen->pIn++;` |
|       2 |  5044 | `	}` |
|       - |  5045 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5046 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5047 | `	{` |
|     106 |  5048 | `		char *zNsDup = 0;` |
|     106 |  5049 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     155 |  5050 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     102 |  5051 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      51 |  5052 | `		}` |
|     106 |  5053 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5054 | `	}` |
|     106 |  5055 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 |  5056 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5057 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5058 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 |  5059 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5060 | `			return SXERR_ABORT;` |
|       - |  5061 | `		}` |
|       2 |  5062 | `	}` |
|     106 |  5063 | `	return SXRET_OK;` |
|      54 |  5064 |  |
|       - |  5065 | `/*` |
|       - |  5066 | ` * Compile the 'use' statement` |
|       - |  5067 | ` * According to the PHP language reference manual` |
|       - |  5068 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5069 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5070 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5071 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5072 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5073 | ` *  a function or constant is not supported.` |
|       - |  5074 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5075 | ` * NOTE` |
|       - |  5076 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5077 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5078 | ` */` |
|      68 |  5079 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 |  5080 |  |
|       - |  5081 | `	sxu32 nLine;` |
|       - |  5082 | `	sxi32 rc;` |
|       - |  5083 | `	SyBlob sPath;` |
|       - |  5084 | `	SyString sAlias;` |
|       - |  5085 | `	SyToken *pLast;` |
|       - |  5086 | `	char *zDup;` |
|       - |  5087 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5088 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5089 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      70 |  5090 | `	nLine = pGen->pIn->nLine;` |
|      70 |  5091 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5092 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      70 |  5093 | `	iUseType = 0;` |
|      70 |  5094 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5095 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5096 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5097 | `			iUseType = 1;` |
|      16 |  5098 | `			pGen->pIn++;` |
|      23 |  5099 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5100 | `			iUseType = 2;` |
|      16 |  5101 | `			pGen->pIn++;` |
|       7 |  5102 | `		}` |
|      14 |  5103 | `	}` |
|       - |  5104 | `	/* Select target hash tables based on import type */` |
|      70 |  5105 | `	switch( iUseType ){` |
|       7 |  5106 | `		case 1:` |
|      16 |  5107 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5108 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5109 | `			break;` |
|       7 |  5110 | `		case 2:` |
|      16 |  5111 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5112 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5113 | `			break;` |
|      20 |  5114 | `		default:` |
|      42 |  5115 | `			pGenHash = &pGen->hUseImports;` |
|      42 |  5116 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5117 | `			break;` |
|       - |  5118 | `	}` |
|      70 |  5119 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5120 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5121 | `	for(;;){` |
|      72 |  5122 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5123 | `			break;` |
|       - |  5124 | `		}` |
|      72 |  5125 | `		SyBlobReset(&sPath);` |
|      72 |  5126 | `		pLast = 0;` |
|       - |  5127 | `		/* Collect the full namespace path */` |
|     258 |  5128 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     188 |  5129 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     128 |  5130 | `				pLast = pGen->pIn;` |
|     128 |  5131 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 |  5132 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5133 | `				}` |
|     128 |  5134 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5135 | `			}` |
|     188 |  5136 | `			pGen->pIn++;` |
|       2 |  5137 | `		}` |
|      72 |  5138 | `		if( pLast == 0 ){` |
|       - |  5139 | `			/* Empty path */` |
|       5 |  5140 | `			break;` |
|       - |  5141 | `		}` |
|       - |  5142 | `		/* Default alias is the last component of the path */` |
|      68 |  5143 | `		sAlias = pLast->sData;` |
|       - |  5144 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5145 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      43 |  5146 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5147 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5148 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5149 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5150 | `				pGen->pIn++;` |
|       8 |  5151 | `			}` |
|       8 |  5152 | `		}` |
|       - |  5153 | `		/* Check for duplicate import alias (per-type) */` |
|      68 |  5154 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 |  5155 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5156 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5157 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 |  5158 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5159 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5160 | `				return SXERR_ABORT;` |
|       - |  5161 | `			}` |
|       2 |  5162 | `		}` |
|       - |  5163 | `		/* Register the import: alias -> FQN.` |
|       - |  5164 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5165 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5166 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     101 |  5167 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5168 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      68 |  5169 | `		if( zDup ){` |
|      68 |  5170 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      68 |  5171 | `			if( pVmHash ){` |
|       - |  5172 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5173 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      40 |  5174 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      40 |  5175 | `				if( zAliasDup ){` |
|      40 |  5176 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5177 | `				}` |
|      19 |  5178 | `			}` |
|      68 |  5179 | `			if( iUseType == 2 ){` |
|       - |  5180 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5181 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5182 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5183 | `				if( zAliasDup ){` |
|       - |  5184 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5185 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5186 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5187 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5188 | `					if( azPair ){` |
|      16 |  5189 | `						azPair[0] = zAliasDup;` |
|      16 |  5190 | `						azPair[1] = zDup;` |
|      16 |  5191 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5192 | `					}` |
|       7 |  5193 | `				}` |
|       7 |  5194 | `			}` |
|      33 |  5195 | `		}` |
|       - |  5196 | `		/* Check for comma (multiple use declarations) */` |
|      68 |  5197 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5198 | `			pGen->pIn++;` |
|       2 |  5199 | `		}else{` |
|      34 |  5200 | `			break;` |
|       - |  5201 | `		}` |
|       1 |  5202 | `	}` |
|      70 |  5203 | `	SyBlobRelease(&sPath);` |
|      70 |  5204 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5205 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5206 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5207 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5208 | `			return SXERR_ABORT;` |
|       - |  5209 | `		}` |
|       1 |  5210 | `	}` |
|      70 |  5211 | `	return SXRET_OK;` |
|      36 |  5212 |  |
|       - |  5213 | `/*` |
|       - |  5214 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5215 | ` *` |
|       - |  5216 | ` * According to the PHP language reference manual.` |
|       - |  5217 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5218 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5219 | ` *  declare (directive)` |
|       - |  5220 | ` *   statement` |
|       - |  5221 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5222 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5223 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5224 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5225 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5226 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5227 | ` * <?php` |
|       - |  5228 | ` * // these are the same:` |
|       - |  5229 | ` * // you can use this:` |
|       - |  5230 | ` * declare(ticks=1) {` |
|       - |  5231 | ` *   // entire script here` |
|       - |  5232 | ` * }` |
|       - |  5233 | ` * // or you can use this:` |
|       - |  5234 | ` * declare(ticks=1);` |
|       - |  5235 | ` * // entire script here` |
|       - |  5236 | ` * ?>` |
|       - |  5237 | ` *` |
|       - |  5238 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5239 | ` */` |
|       8 |  5240 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 |  5241 |  |
|       9 |  5242 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 |  5243 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - |  5244 | `	sxi32 rc;` |
|       9 |  5245 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 |  5246 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5247 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5248 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5249 | `			return SXERR_ABORT;` |
|       - |  5250 | `		}` |
|       5 |  5251 | `		goto Synchro;` |
|       - |  5252 | `	}` |
|       5 |  5253 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - |  5254 | `	/* Delimit the directive */` |
|       5 |  5255 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 |  5256 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  5257 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5258 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5259 | `			return SXERR_ABORT;` |
|       - |  5260 | `		}` |
|     ! 0 |  5261 | `		return SXRET_OK;` |
|       - |  5262 | `	}` |
|       - |  5263 | `	/* Update the cursor */` |
|       5 |  5264 | `	pGen->pIn = &pEnd[1];` |
|       5 |  5265 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 |  5266 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5267 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5268 | `			return SXERR_ABORT;` |
|       - |  5269 | `		}` |
|     ! 0 |  5270 | `	}` |
|       - |  5271 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 |  5272 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - |  5273 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5274 | `		ph7_lib_version()` |
|       - |  5275 | `		);` |
|       - |  5276 | `	/*All done */` |
|       5 |  5277 | `	return SXRET_OK;` |
|       2 |  5278 | `Synchro:` |
|       - |  5279 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5280 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5281 | `		pGen->pIn++;` |
|       1 |  5282 | `	}` |
|       5 |  5283 | `	return SXRET_OK;` |
|       5 |  5284 |  |
|       - |  5285 | `/*` |
|       - |  5286 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5287 | ` * as follows:` |
|       - |  5288 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5289 | ` * {` |
|       - |  5290 | ` *   return "Making a cup of $type.\n";` |
|       - |  5291 | ` * }` |
|       - |  5292 | ` * Symisc eXtension.` |
|       - |  5293 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5294 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5295 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5296 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5297 | ` *      {` |
|       - |  5298 | ` *       var_dump($a);` |
|       - |  5299 | ` *      }` |
|       - |  5300 | ` *     //call test without args` |
|       - |  5301 | ` *      test();` |
|       - |  5302 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5303 | ` *      Example:` |
|       - |  5304 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5305 | ` * 3 -) Function overloading!!` |
|       - |  5306 | ` *      Example:` |
|       - |  5307 | ` *      function foo($a) {` |
|       - |  5308 | ` *   	  return $a.PHP_EOL;` |
|       - |  5309 | ` *	    }` |
|       - |  5310 | ` *	    function foo($a, $b) {` |
|       - |  5311 | ` *   	  return $a + $b;` |
|       - |  5312 | ` *	    }` |
|       - |  5313 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5314 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5315 | ` *      // Same arg` |
|       - |  5316 | ` *	   function foo(string $a)` |
|       - |  5317 | ` *	   {` |
|       - |  5318 | ` *	     echo "a is a string\n";` |
|       - |  5319 | ` *	     var_dump($a);` |
|       - |  5320 | ` *	   }` |
|       - |  5321 | ` *	  function foo(int $a)` |
|       - |  5322 | ` *	  {` |
|       - |  5323 | ` *	    echo "a is integer\n";` |
|       - |  5324 | ` *	    var_dump($a);` |
|       - |  5325 | ` *	  }` |
|       - |  5326 | ` *	  function foo(array $a)` |
|       - |  5327 | ` *	  {` |
|       - |  5328 | ` * 	    echo "a is an array\n";` |
|       - |  5329 | ` * 	    var_dump($a);` |
|       - |  5330 | ` *	  }` |
|       - |  5331 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5332 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5333 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5334 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5335 | ` * introduced by the PH7 engine.` |
|       - |  5336 | ` */` |
|   55472 |  5337 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5338 |  |
|       - |  5339 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5340 | `	SySet *pInstrContainer;` |
|       - |  5341 | `	sxi32 rc;` |
|       - |  5342 | `	/* Swap token stream */` |
|   55474 |  5343 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   55474 |  5344 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   55474 |  5345 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5346 | `	/* Compile the expression holding the argument value */` |
|   55474 |  5347 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5348 | `	/* Emit the done instruction */` |
|   55474 |  5349 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   55474 |  5350 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   55474 |  5351 | `	RE_SWAP_DELIMITER(pGen);` |
|   55474 |  5352 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5353 | `		return SXERR_ABORT;` |
|       - |  5354 | `	}` |
|   55474 |  5355 | `	return SXRET_OK;` |
|   27738 |  5356 |  |
|       - |  5357 | `/*` |
|       - |  5358 | ` * Collect function arguments one after one.` |
|       - |  5359 | ` * According to the PHP language reference manual.` |
|       - |  5360 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5361 | ` * list of expressions.` |
|       - |  5362 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5363 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5364 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5365 | ` * for more information.` |
|       - |  5366 | ` * Example #1 Passing arrays to functions` |
|       - |  5367 | ` * <?php` |
|       - |  5368 | ` * function takes_array($input)` |
|       - |  5369 | ` * {` |
|       - |  5370 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5371 | ` * }` |
|       - |  5372 | ` * ?>` |
|       - |  5373 | ` * Making arguments be passed by reference` |
|       - |  5374 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5375 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5376 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5377 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5378 | ` * to the argument name in the function definition:` |
|       - |  5379 | ` * Example #2 Passing function parameters by reference` |
|       - |  5380 | ` * <?php` |
|       - |  5381 | ` * function add_some_extra(&$string)` |
|       - |  5382 | ` * {` |
|       - |  5383 | ` *   $string .= 'and something extra.';` |
|       - |  5384 | ` * }` |
|       - |  5385 | ` * $str = 'This is a string, ';` |
|       - |  5386 | ` * add_some_extra($str);` |
|       - |  5387 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5388 | ` * ?>` |
|       - |  5389 | ` *` |
|       - |  5390 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5391 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5392 | ` * on these extension.` |
|       - |  5393 | ` */` |
|   59200 |  5394 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       2 |  5395 |  |
|       - |  5396 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5397 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5398 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5399 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5400 | `	sxi32 rc;` |
|       - |  5401 |  |
|   59202 |  5402 | `	pIn = pGen->pIn;` |
|   59202 |  5403 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5404 | `	/* Process arguments one after one */` |
|   77003 |  5405 | `	for(;;){` |
|  154008 |  5406 | `		if( pIn >= pEnd ){` |
|       - |  5407 | `			/* No more arguments to process */` |
|   59190 |  5408 | `			break;` |
|       - |  5409 | `		}` |
|   94820 |  5410 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   94820 |  5411 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   94820 |  5412 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   94820 |  5413 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5414 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|   94820 |  5415 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   52654 |  5416 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   52654 |  5417 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      42 |  5418 | `				if( !bCtorCtx ){` |
|       5 |  5419 | `					if( bAbstractCtx ){` |
|       3 |  5420 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5421 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5422 | `					}else{` |
|       3 |  5423 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5424 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5425 | `					}` |
|       5 |  5426 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5427 | `						return SXERR_ABORT;` |
|       - |  5428 | `					}` |
|       5 |  5429 | `					return SXERR_SYNTAX;` |
|       - |  5430 | `				}` |
|      38 |  5431 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      38 |  5432 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5433 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      37 |  5434 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5435 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5436 | `				}else{` |
|      34 |  5437 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5438 | `				}` |
|      38 |  5439 | `				pIn++;` |
|      18 |  5440 | `			}` |
|   26324 |  5441 | `		}` |
|       - |  5442 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  125543 |  5443 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   79607 |  5444 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   62931 |  5445 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   61442 |  5446 | `			sxu32 nLineLocal = pIn->nLine;` |
|   61442 |  5447 | `			sxi32 iTFlags = 0;` |
|   61442 |  5448 | `			pGen->pIn = pIn;` |
|   61442 |  5449 | `			rc = GenStateParseUnionTypeDecl(` |
|   30720 |  5450 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   30720 |  5451 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5452 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5453 | `				/* bAllowVoid */ 0,` |
|   30720 |  5454 | `						nLineLocal);` |
|   61442 |  5455 | `			pIn = pGen->pIn;` |
|   61442 |  5456 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5457 | `				return SXERR_ABORT;` |
|   61442 |  5458 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5459 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5460 | `				return SXERR_SYNTAX;` |
|   61440 |  5461 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5462 | `				if( pIn < pEnd ){` |
|       7 |  5463 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5464 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5465 | `						&pIn->sData);` |
|       3 |  5466 | `				}else{` |
|     ! 0 |  5467 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5468 | `						"syntax error, unexpected end of file");` |
|       - |  5469 | `				}` |
|       5 |  5470 | `				return SXERR_SYNTAX;` |
|       - |  5471 | `			}` |
|   61436 |  5472 | `			sArg.iFlags \|= iTFlags;` |
|   30717 |  5473 | `		}` |
|   94810 |  5474 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5475 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5476 | `			return rc;` |
|       - |  5477 | `		}` |
|   94810 |  5478 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5479 | `			/* Pass by reference,record that */` |
|    2946 |  5480 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2946 |  5481 | `			pIn++;` |
|    1472 |  5482 | `		}` |
|   94810 |  5483 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5484 | `			/* Variadic parameter: ...$args */` |
|      40 |  5485 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      40 |  5486 | `			pIn++;` |
|      19 |  5487 | `		}` |
|   94810 |  5488 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5489 | `			/* Invalid argument */` |
|     ! 0 |  5490 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5491 | `			return rc;` |
|       - |  5492 | `		}` |
|   94810 |  5493 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5494 | `		/* Copy argument name */` |
|   94810 |  5495 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   94810 |  5496 | `		if( zDup == 0 ){` |
|     ! 0 |  5497 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5498 | `			return SXERR_ABORT;` |
|       - |  5499 | `		}` |
|   94810 |  5500 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   94810 |  5501 | `		pIn++;` |
|   94810 |  5502 | `		if( pIn < pEnd ){` |
|   61910 |  5503 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5504 | `				SyToken *pDefend;` |
|   55476 |  5505 | `				sxi32 iNest = 0;` |
|   55476 |  5506 | `				pIn++; /* Jump the equal sign */` |
|   55476 |  5507 | `				pDefend = pIn;` |
|       - |  5508 | `				/* Process the default value associated with this argument */` |
|  116784 |  5509 | `				while( pDefend < pEnd ){` |
|   90498 |  5510 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   29190 |  5511 | `						break;` |
|       - |  5512 | `					}` |
|   61310 |  5513 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5514 | `						/* Increment nesting level */` |
|    2920 |  5515 | `						iNest++;` |
|   59851 |  5516 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5517 | `						/* Decrement nesting level */` |
|    2920 |  5518 | `						iNest--;` |
|    1459 |  5519 | `					}` |
|   61310 |  5520 | `					pDefend++;` |
|       2 |  5521 | `				}` |
|   55476 |  5522 | `				if( pIn >= pDefend ){` |
|       3 |  5523 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5524 | `					return rc;` |
|       - |  5525 | `				}` |
|       - |  5526 | `				/* Process default value */` |
|   55474 |  5527 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   55474 |  5528 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5529 | `					return rc;` |
|       - |  5530 | `				}` |
|       - |  5531 | `				/* Point beyond the default value */` |
|   55474 |  5532 | `				pIn = pDefend;` |
|   27736 |  5533 | `			}` |
|   61908 |  5534 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5535 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5536 | `				return rc;` |
|       - |  5537 | `			}` |
|   61908 |  5538 | `			pIn++; /* Jump the trailing comma */` |
|   30953 |  5539 | `		}` |
|       - |  5540 | `		/* Append argument signature */` |
|   94808 |  5541 | `		if( sArg.nType > 0 ){` |
|   61396 |  5542 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5543 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    8772 |  5544 | `				int marker = 'o';` |
|    8772 |  5545 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    8772 |  5546 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4387 |  5547 | `			}else{` |
|       - |  5548 | `				int c;` |
|   52626 |  5549 | `				c = 'n'; /* cc warning */` |
|       - |  5550 | `				/* Type leading character */` |
|   52626 |  5551 | `				switch(sArg.nType){` |
|     ! 0 |  5552 | `				case MEMOBJ_HASHMAP:` |
|       - |  5553 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5554 | `					c = 'h';` |
|     ! 0 |  5555 | `					break;` |
|    7323 |  5556 | `				case MEMOBJ_INT:` |
|       - |  5557 | `					/* Integer */` |
|   14648 |  5558 | `					c = 'i';` |
|   14648 |  5559 | `					break;` |
|     ! 0 |  5560 | `				case MEMOBJ_BOOL:` |
|       - |  5561 | `					/* Bool */` |
|     ! 0 |  5562 | `					c = 'b';` |
|     ! 0 |  5563 | `					break;` |
|     ! 0 |  5564 | `				case MEMOBJ_REAL:` |
|       - |  5565 | `					/* Float */` |
|     ! 0 |  5566 | `					c = 'f';` |
|     ! 0 |  5567 | `					break;` |
|   18982 |  5568 | `				case MEMOBJ_STRING:` |
|       - |  5569 | `					/* String */` |
|   37966 |  5570 | `					c = 's';` |
|   37966 |  5571 | `					break;` |
|       7 |  5572 | `				case MEMOBJ_OBJ:` |
|       - |  5573 | `					/* Object */` |
|      16 |  5574 | `					c = 'o';` |
|      14 |  5575 | `					break;` |
|     ! 0 |  5576 | `				default:` |
|     ! 0 |  5577 | `					break;` |
|       - |  5578 | `				}` |
|   52626 |  5579 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5580 | `			}` |
|   30699 |  5581 | `		}else{` |
|       - |  5582 | `			/* No type is associated with this parameter which mean` |
|       - |  5583 | `			 * that this function is not condidate for overloading.` |
|       - |  5584 | `			 */` |
|   33414 |  5585 | `			SyBlobRelease(&sSig);` |
|       - |  5586 | `		}` |
|       - |  5587 | `		/* Save in the argument set */` |
|   94808 |  5588 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5589 | `	}` |
|   59190 |  5590 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5591 | `		/* Save function signature */` |
|   38032 |  5592 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   19015 |  5593 | `	}` |
|   59190 |  5594 | `	return SXRET_OK;` |
|   29602 |  5595 |  |
|       - |  5596 | `/*` |
|       - |  5597 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5598 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5599 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5600 | ` */` |
|  182262 |  5601 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5602 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5603 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5604 | `	)` |
|       2 |  5605 |  |
|       - |  5606 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5607 | `	GenBlock *pBlock;` |
|       - |  5608 | `	sxu32 nGotoOfft;` |
|       - |  5609 | `	sxi32 rc;` |
|       - |  5610 | `	/* Attach the new function */` |
|  182264 |  5611 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  182264 |  5612 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5613 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5614 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5615 | `		return SXERR_ABORT;` |
|       - |  5616 | `	}` |
|  182264 |  5617 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5618 | `	/* Swap bytecode containers */` |
|  182264 |  5619 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  182264 |  5620 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5621 | `	/* Emit constructor property promotion prologue:` |
|       - |  5622 | `	 *   $this->NAME = $NAME;` |
|       - |  5623 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5624 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5625 | `	{` |
|  182264 |  5626 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5627 | `		sxu32 i;` |
|  274074 |  5628 | `		for( i = 0; i < nArg; i++ ){` |
|   91812 |  5629 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5630 | `			char *zSrc;` |
|       - |  5631 | `			sxu32 nSrc,nName;` |
|       - |  5632 | `			SySet sToken;` |
|       - |  5633 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5634 | `			sxi32 rcPromote;` |
|   91812 |  5635 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   91784 |  5636 | `				continue;` |
|       - |  5637 | `			}` |
|       - |  5638 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5639 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5640 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5641 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5642 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      30 |  5643 | `			nName = SyStringLength(&pArg->sName);` |
|      30 |  5644 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      30 |  5645 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      30 |  5646 | `			if( zSrc == 0 ){` |
|     ! 0 |  5647 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5648 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5649 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5650 | `				return SXERR_ABORT;` |
|       - |  5651 | `			}` |
|       - |  5652 | `			{` |
|      30 |  5653 | `				char *z = zSrc;` |
|      30 |  5654 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      30 |  5655 | `				z += sizeof("$this->")-1;` |
|      30 |  5656 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5657 | `				z += nName;` |
|      30 |  5658 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      30 |  5659 | `				z += sizeof(" = $")-1;` |
|      30 |  5660 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5661 | `				z += nName;` |
|      30 |  5662 | `				*z = 0;` |
|       - |  5663 | `			}` |
|      30 |  5664 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      30 |  5665 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      30 |  5666 | `			pTmpIn = pGen->pIn;` |
|      30 |  5667 | `			pTmpEnd = pGen->pEnd;` |
|      30 |  5668 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      30 |  5669 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      30 |  5670 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  5671 | `			pGen->pIn = pTmpIn;` |
|      30 |  5672 | `			pGen->pEnd = pTmpEnd;` |
|      30 |  5673 | `			SySetRelease(&sToken);` |
|      30 |  5674 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5675 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5676 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5677 | `				return SXERR_ABORT;` |
|       - |  5678 | `			}` |
|       - |  5679 | `			/* Discard the assignment result — this is a statement expression. */` |
|      30 |  5680 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      16 |  5681 | `		}` |
|       - |  5682 | `	}` |
|       - |  5683 | `	/* Compile the body */` |
|  182264 |  5684 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5685 | `	/* Fix exception jumps now the destination is resolved */` |
|  182264 |  5686 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5687 | `	/* Emit the final return if not yet done */` |
|  182264 |  5688 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5689 | `	/* Fix gotos jumps now the destination is resolved */` |
|  182264 |  5690 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5691 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5692 | `	}` |
|  182264 |  5693 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5694 | `	/* Restore the default container */` |
|  182264 |  5695 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5696 | `	/* Leave function block */` |
|  182264 |  5697 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  182264 |  5698 | `	if( rc == SXERR_ABORT ){` |
|       - |  5699 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5700 | `		return SXERR_ABORT;` |
|       - |  5701 | `	}` |
|       - |  5702 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5703 | `	{` |
|  182264 |  5704 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5705 | `		sxu32 i;` |
| 3564320 |  5706 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3382076 |  5707 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5708 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5709 | `				break;` |
|       - |  5710 | `			}` |
| 1691030 |  5711 | `		}` |
|       - |  5712 | `	}` |
|       - |  5713 | `	/* All done, function body compiled */` |
|  182264 |  5714 | `	return SXRET_OK;` |
|   91133 |  5715 |  |
|       - |  5716 | `/*` |
|       - |  5717 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5718 | ` * According to the PHP language reference manual.` |
|       - |  5719 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5720 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5721 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5722 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5723 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5724 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5725 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5726 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5727 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5728 | ` *` |
|       - |  5729 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5730 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5731 | ` * on these extension.` |
|       - |  5732 | ` */` |
|       - |  5733 | `/*` |
|       - |  5734 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5735 | ` */` |
|      76 |  5736 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5737 |  |
|       - |  5738 | `	sxu32 i;` |
|     238 |  5739 | `	for( i = 0; i < n; i++ ){` |
|     212 |  5740 | `		int a = zA[i], b = zB[i];` |
|     212 |  5741 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     212 |  5742 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     212 |  5743 | `		if( a != b ) return a - b;` |
|      82 |  5744 | `	}` |
|      28 |  5745 | `	return 0;` |
|      40 |  5746 |  |
|       - |  5747 | `/*` |
|       - |  5748 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5749 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5750 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5751 | ` */` |
|       - |  5752 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5753 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5754 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5755 |  |
|       - |  5756 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5757 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5758 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5759 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5760 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5761 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5762 |  |
|       - |  5763 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5764 | `struct PhlTypeAtom {` |
|       - |  5765 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5766 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5767 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5768 | `	sxu32 nCanon;` |
|       - |  5769 | `};` |
|       - |  5770 |  |
|       - |  5771 | `/*` |
|       - |  5772 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5773 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5774 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5775 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5776 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5777 | ` * already be consumed by the caller.` |
|       - |  5778 | ` */` |
|   61734 |  5779 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5780 |  |
|   61736 |  5781 | `	SyToken *pIn = pGen->pIn;` |
|   61736 |  5782 | `	SyZero(pOut, sizeof(*pOut));` |
|   61736 |  5783 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   61736 |  5784 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5785 | `		return SXERR_SYNTAX;` |
|       - |  5786 | `	}` |
|       - |  5787 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   61736 |  5788 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5789 | `		pIn++;` |
|       8 |  5790 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5791 | `			return SXERR_SYNTAX;` |
|       - |  5792 | `		}` |
|       3 |  5793 | `	}` |
|   61736 |  5794 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5795 | `		return SXERR_SYNTAX;` |
|       - |  5796 | `	}` |
|   61736 |  5797 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   52902 |  5798 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   52902 |  5799 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5800 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   52895 |  5801 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       8 |  5802 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   52885 |  5803 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   14782 |  5804 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   45492 |  5805 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   38052 |  5806 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   19077 |  5807 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      22 |  5808 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      42 |  5809 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5810 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5811 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5812 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5813 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5814 | `			pOut->sClass = pIn->sData;` |
|       4 |  5815 | `		}else{` |
|       3 |  5816 | `			return SXERR_SYNTAX;` |
|       - |  5817 | `		}` |
|   52900 |  5818 | `		pIn++;` |
|   26451 |  5819 | `	}else{` |
|       - |  5820 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5821 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    8836 |  5822 | `		SyString *pT = &pIn->sData;` |
|    8836 |  5823 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5824 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5825 | `			pIn++;` |
|    8831 |  5826 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      12 |  5827 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      12 |  5828 | `			pIn++;` |
|    8821 |  5829 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5830 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5831 | `			pIn++;` |
|       2 |  5832 | `		}else{` |
|       - |  5833 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    8814 |  5834 | `			SyToken *pFirst = pIn;` |
|    8814 |  5835 | `			SyToken *pLast = pIn;` |
|    8814 |  5836 | `			pOut->nType = SXU32_HIGH;` |
|    8814 |  5837 | `			pOut->sClass = pIn->sData;` |
|    8814 |  5838 | `			pIn++;` |
|   13221 |  5839 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    8817 |  5840 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  5841 | `				pLast = &pIn[1];` |
|       3 |  5842 | `				pIn += 2;` |
|       1 |  5843 | `			}` |
|    8814 |  5844 | `			if( pLast != pFirst ){` |
|       3 |  5845 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  5846 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  5847 | `				pOut->sClass.zString = zFirst;` |
|       3 |  5848 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  5849 | `			}` |
|       - |  5850 | `		}` |
|       - |  5851 | `	}` |
|   61734 |  5852 | `	pGen->pIn = pIn;` |
|   61734 |  5853 | `	return SXRET_OK;` |
|   30869 |  5854 |  |
|       - |  5855 |  |
|       - |  5856 | `/*` |
|       - |  5857 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  5858 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  5859 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  5860 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  5861 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  5862 | ` */` |
|   61638 |  5863 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  5864 |  |
|       - |  5865 | `	int i;` |
|   61640 |  5866 | `	int nNonNull = 0;` |
|  123358 |  5867 | `	for( i = 0; i < nAtoms; i++ ){` |
|   61720 |  5868 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   61710 |  5869 | `			nNonNull++;` |
|   30854 |  5870 | `		}` |
|   30861 |  5871 | `	}` |
|   61640 |  5872 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  5873 | `		/* Shorthand: ?T */` |
|      54 |  5874 | `		for( i = 0; i < nAtoms; i++ ){` |
|      54 |  5875 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      54 |  5876 | `			SyBlobAppend(pBlob, "?", 1);` |
|      54 |  5877 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5878 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  5879 | `			}else{` |
|      44 |  5880 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  5881 | `			}` |
|      54 |  5882 | `			return;` |
|     ! 0 |  5883 | `		}` |
|     ! 0 |  5884 | `	}` |
|       - |  5885 | `	{` |
|   61588 |  5886 | `		int bFirst = 1;` |
|       - |  5887 | `		/* 1) Classes in declaration order */` |
|  123248 |  5888 | `		for( i = 0; i < nAtoms; i++ ){` |
|   61662 |  5889 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    8808 |  5890 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    8808 |  5891 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    8808 |  5892 | `				bFirst = 0;` |
|    4403 |  5893 | `			}` |
|   30832 |  5894 | `		}` |
|       - |  5895 | `		/* 2) Built-ins in canonical order */` |
|       - |  5896 | `		{` |
|       - |  5897 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  5898 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  5899 | `			int k;` |
|  431104 |  5900 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  686546 |  5901 | `				for( i = 0; i < nAtoms; i++ ){` |
|  369872 |  5902 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   52844 |  5903 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   52844 |  5904 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   52844 |  5905 | `						bFirst = 0;` |
|   52844 |  5906 | `						break;` |
|       - |  5907 | `					}` |
|  158516 |  5908 | `				}` |
|  184760 |  5909 | `			}` |
|       - |  5910 | `		}` |
|       - |  5911 | `		/* 3) null suffix */` |
|   61588 |  5912 | `		if( bNullable ){` |
|       6 |  5913 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  5914 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  5915 | `		}` |
|       - |  5916 | `	}` |
|   30821 |  5917 |  |
|       - |  5918 |  |
|       - |  5919 | `/*` |
|       - |  5920 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  5921 | ` *` |
|       - |  5922 | ` * Outputs:` |
|       - |  5923 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  5924 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  5925 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  5926 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  5927 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  5928 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  5929 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  5930 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  5931 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  5932 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  5933 | ` *` |
|       - |  5934 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  5935 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  5936 | ` */` |
|   61648 |  5937 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  5938 | `	ph7_gen_state *pGen,` |
|       - |  5939 | `	sxu32 *pnType,` |
|       - |  5940 | `	SyString *pClass,` |
|       - |  5941 | `	SySet *pAlts,` |
|       - |  5942 | `	sxi32 *piTypeFlags,` |
|       - |  5943 | `	SyString *pTypeText,` |
|       - |  5944 | `	int iNullableFlag,` |
|       - |  5945 | `	int iUnionFlag,` |
|       - |  5946 | `	int bAllowVoid,` |
|       - |  5947 | `	sxu32 nLine` |
|       2 |  5948 | `){` |
|       - |  5949 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   61650 |  5950 | `	int nAtoms = 0;` |
|   61650 |  5951 | `	int bShortNullable = 0;` |
|   61650 |  5952 | `	int bExplicitNull = 0;` |
|       - |  5953 | `	sxi32 rc;` |
|   61650 |  5954 | `	*pnType = 0;` |
|   61650 |  5955 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   61650 |  5956 | `	*piTypeFlags = 0;` |
|   61650 |  5957 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  5958 |  |
|   61650 |  5959 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5960 | `		return SXRET_OK;` |
|       - |  5961 | `	}` |
|       - |  5962 | ``	/* Optional `?` shorthand prefix */`` |
|   61648 |  5963 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      50 |  5964 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      50 |  5965 | `		bShortNullable = 1;` |
|      50 |  5966 | `		pGen->pIn++;` |
|      50 |  5967 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5968 | `			return SXERR_SYNTAX;` |
|       - |  5969 | `		}` |
|      24 |  5970 | `	}` |
|       - |  5971 | `	/* First atom is mandatory */` |
|   61650 |  5972 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   61650 |  5973 | `	if( rc != SXRET_OK ){` |
|       3 |  5974 | `		return rc;` |
|       - |  5975 | `	}` |
|   61648 |  5976 | `	nAtoms = 1;` |
|       - |  5977 | ``	/* Subsequent atoms separated by `\|` */`` |
|   92600 |  5978 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   61779 |  5979 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      90 |  5980 | `		if( bShortNullable ){` |
|       - |  5981 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  5982 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  5983 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  5984 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  5985 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  5986 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  5987 | `		}` |
|      88 |  5988 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  5989 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5990 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  5991 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  5992 | `		}` |
|      88 |  5993 | ``		pGen->pIn++; /* skip `\|` */`` |
|      88 |  5994 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      88 |  5995 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  5996 | `			return rc;` |
|       - |  5997 | `		}` |
|      88 |  5998 | `		nAtoms++;` |
|       2 |  5999 | `	}` |
|       - |  6000 | `	/* Validation pass.` |
|       - |  6001 | `	 *` |
|       - |  6002 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6003 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6004 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6005 | `	 */` |
|       - |  6006 | `	{` |
|       - |  6007 | `		int i, j;` |
|   61646 |  6008 | `		int bHasNonNull = 0;` |
|  123370 |  6009 | `		for( i = 0; i < nAtoms; i++ ){` |
|   61732 |  6010 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      12 |  6011 | `				if( nAtoms > 1 ){` |
|       3 |  6012 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6013 | `						"Void can only be used as a standalone type");` |
|       3 |  6014 | `					return SXERR_SYNTAX;` |
|       - |  6015 | `				}` |
|      10 |  6016 | `				if( !bAllowVoid ){` |
|     ! 0 |  6017 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6018 | `						"void cannot be used here");` |
|     ! 0 |  6019 | `					return SXERR_SYNTAX;` |
|       - |  6020 | `				}` |
|      10 |  6021 | `				if( bShortNullable ){` |
|     ! 0 |  6022 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6023 | `						"Void type cannot be nullable");` |
|     ! 0 |  6024 | `					return SXERR_SYNTAX;` |
|       - |  6025 | `				}` |
|       4 |  6026 | `			}` |
|   61730 |  6027 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6028 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6029 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6030 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6031 | `				 * does not return), and folding them would mislead any` |
|       - |  6032 | `				 * future return-enforcement work. */` |
|       3 |  6033 | `				if( nAtoms > 1 ){` |
|       3 |  6034 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6035 | `						"never can only be used as a standalone type");` |
|       3 |  6036 | `					return SXERR_SYNTAX;` |
|       - |  6037 | `				}` |
|     ! 0 |  6038 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6039 | `					"never type is not yet implemented");` |
|     ! 0 |  6040 | `				return SXERR_SYNTAX;` |
|       - |  6041 | `			}` |
|   61728 |  6042 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  6043 | `				bExplicitNull = 1;` |
|       7 |  6044 | `			}else{` |
|   61718 |  6045 | `				bHasNonNull = 1;` |
|       - |  6046 | `			}` |
|       - |  6047 | `			/* Duplicate detection */` |
|   61844 |  6048 | `			for( j = 0; j < i; j++ ){` |
|     120 |  6049 | `				int bDup = 0;` |
|     120 |  6050 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  6051 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6052 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6053 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6054 | `								aAtoms[j].sClass.zString,` |
|      12 |  6055 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6056 | `							bDup = 1;` |
|     ! 0 |  6057 | `						}` |
|       8 |  6058 | `					}else{` |
|       3 |  6059 | `						bDup = 1;` |
|       - |  6060 | `					}` |
|       7 |  6061 | `				}` |
|     120 |  6062 | `				if( bDup ){` |
|       - |  6063 | `					const char *zName;` |
|       - |  6064 | `					sxu32 nName;` |
|       3 |  6065 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6066 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6067 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6068 | `					}else{` |
|       3 |  6069 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6070 | `						nName = aAtoms[i].nCanon;` |
|       - |  6071 | `					}` |
|       4 |  6072 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6073 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6074 | `					return SXERR_SYNTAX;` |
|       - |  6075 | `				}` |
|      60 |  6076 | `			}` |
|   30864 |  6077 | `		}` |
|   61640 |  6078 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  6079 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6080 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  6081 | `			return SXERR_SYNTAX;` |
|       - |  6082 | `		}` |
|       - |  6083 | `	}` |
|       - |  6084 | `	/* Compute nullability flag */` |
|   61640 |  6085 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      58 |  6086 | `		*piTypeFlags \|= iNullableFlag;` |
|      28 |  6087 | `	}` |
|       - |  6088 | `	/* Build canonical type text */` |
|   61640 |  6089 | `	if( pTypeText ){` |
|       - |  6090 | `		SyBlob sBlob;` |
|   61640 |  6091 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   92436 |  6092 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   30819 |  6093 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   61640 |  6094 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   92447 |  6095 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   61630 |  6096 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   61632 |  6097 | `			if( zDup ){` |
|   61632 |  6098 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   30815 |  6099 | `			}` |
|   30815 |  6100 | `		}` |
|   61640 |  6101 | `		SyBlobRelease(&sBlob);` |
|   30819 |  6102 | `	}` |
|       - |  6103 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6104 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6105 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6106 | `	{` |
|   61640 |  6107 | `		int nNonNull = 0;` |
|   61640 |  6108 | `		int iNonNullIdx = -1;` |
|       - |  6109 | `		int i;` |
|  123358 |  6110 | `		for( i = 0; i < nAtoms; i++ ){` |
|   61720 |  6111 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   61710 |  6112 | `				nNonNull++;` |
|   61710 |  6113 | `				iNonNullIdx = i;` |
|   30854 |  6114 | `			}` |
|   30861 |  6115 | `		}` |
|   61640 |  6116 | `		if( nNonNull <= 1 ){` |
|       - |  6117 | `			/* Fast path: store as single type. */` |
|   61586 |  6118 | `			if( iNonNullIdx >= 0 ){` |
|   61586 |  6119 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   61586 |  6120 | `				if( pA->nType == SXU32_HIGH ){` |
|   13187 |  6121 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4395 |  6122 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    8792 |  6123 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    8792 |  6124 | `					*pnType = SXU32_HIGH;` |
|    8792 |  6125 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   57191 |  6126 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      10 |  6127 | `					*pnType = MEMOBJ_VOID;` |
|       6 |  6128 | `				}else{` |
|       - |  6129 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6130 | `					 * pass above rejects it as not-yet-implemented. */` |
|   52788 |  6131 | `					*pnType = pA->nType;` |
|       - |  6132 | `				}` |
|   30792 |  6133 | `			}` |
|   30794 |  6134 | `		}else{` |
|       - |  6135 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      56 |  6136 | `			*piTypeFlags \|= iUnionFlag;` |
|     184 |  6137 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6138 | `				ph7_type_alt sAlt;` |
|     130 |  6139 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     126 |  6140 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     126 |  6141 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      41 |  6142 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      13 |  6143 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      28 |  6144 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      28 |  6145 | `					sAlt.nType = SXU32_HIGH;` |
|      28 |  6146 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      15 |  6147 | `				}else{` |
|     100 |  6148 | `					sAlt.nType = aAtoms[i].nType;` |
|     100 |  6149 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6150 | `				}` |
|     126 |  6151 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      64 |  6152 | `			}` |
|       - |  6153 | `		}` |
|       - |  6154 | `	}` |
|   61640 |  6155 | `	return SXRET_OK;` |
|   30826 |  6156 |  |
|       - |  6157 |  |
|       - |  6158 | `/*` |
|       - |  6159 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6160 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6161 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6162 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6163 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6164 | `` *          and union types `: T\|U`.`` |
|       - |  6165 | ` */` |
|  229094 |  6166 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6167 |  |
|  229096 |  6168 | `	sxi32 iFlags = 0;` |
|       - |  6169 | `	sxi32 rc;` |
|       - |  6170 | `	sxu32 nLine;` |
|  229096 |  6171 | `	pFunc->nReturnType = 0;` |
|  229096 |  6172 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  229096 |  6173 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  229096 |  6174 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  229004 |  6175 | `		return SXRET_OK;` |
|       - |  6176 | `	}` |
|      94 |  6177 | `	pGen->pIn++; /* Skip ':' */` |
|      94 |  6178 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6179 | `		return SXRET_OK;` |
|       - |  6180 | `	}` |
|      94 |  6181 | `	nLine = pGen->pIn->nLine;` |
|      94 |  6182 | `	rc = GenStateParseUnionTypeDecl(` |
|      46 |  6183 | `		pGen,` |
|      46 |  6184 | `		&pFunc->nReturnType,` |
|      46 |  6185 | `		&pFunc->sReturnClass,` |
|      46 |  6186 | `		&pFunc->aReturnUnion,` |
|       - |  6187 | `		&iFlags,` |
|      46 |  6188 | `		&pFunc->sReturnTypeName,` |
|       - |  6189 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6190 | `		/* iUnionFlag */ 0,` |
|       - |  6191 | `		/* bAllowVoid */ 1,` |
|      46 |  6192 | `		nLine);` |
|      46 |  6193 | `	(void)iFlags;` |
|      94 |  6194 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6195 | `		return SXERR_ABORT;` |
|       - |  6196 | `	}` |
|      94 |  6197 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6198 | `		/* Error already reported */` |
|     ! 0 |  6199 | `		return SXERR_SYNTAX;` |
|       - |  6200 | `	}` |
|      94 |  6201 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6202 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6203 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6204 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6205 | `				&pGen->pIn->sData);` |
|       3 |  6206 | `		}else{` |
|     ! 0 |  6207 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6208 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6209 | `		}` |
|       5 |  6210 | `		return SXERR_SYNTAX;` |
|       - |  6211 | `	}` |
|      90 |  6212 | `	return SXRET_OK;` |
|  114549 |  6213 |  |
|       - |  6214 |  |
|   38780 |  6215 | `static sxi32 GenStateCompileFunc(` |
|       - |  6216 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6217 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6218 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6219 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6220 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6221 | `	)` |
|       2 |  6222 |  |
|       - |  6223 | `	ph7_vm_func *pFunc;` |
|       - |  6224 | `	SyToken *pEnd;` |
|       - |  6225 | `	sxu32 nLine;` |
|       - |  6226 | `	char *zName;` |
|       - |  6227 | `	sxi32 rc;` |
|       - |  6228 | `	/* Extract line number */` |
|   38782 |  6229 | `	nLine = pGen->pIn->nLine;` |
|       - |  6230 | `	/* Jump the left parenthesis '(' */` |
|   38782 |  6231 | `	pGen->pIn++;` |
|       - |  6232 | `	/* Delimit the function signature */` |
|   38782 |  6233 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   38782 |  6234 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6235 | `		/* Syntax error */` |
|       7 |  6236 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6237 | `		if( rc == SXERR_ABORT ){` |
|       - |  6238 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6239 | `			return SXERR_ABORT;` |
|       - |  6240 | `		}` |
|       7 |  6241 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6242 | `		return SXRET_OK;` |
|       - |  6243 | `	}` |
|       - |  6244 | `	/* Create the function state */` |
|   38776 |  6245 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   38776 |  6246 | `	if( pFunc == 0 ){` |
|     ! 0 |  6247 | `		goto OutOfMem;` |
|       - |  6248 | `	}` |
|       - |  6249 | `	/* Build the function name, prepending namespace if active */` |
|   38783 |  6250 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6251 | `		SyBlob sFQN;` |
|       - |  6252 | `		sxu32 nLen;` |
|      16 |  6253 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6254 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6255 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6256 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6257 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6258 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6259 | `		SyBlobRelease(&sFQN);` |
|      16 |  6260 | `		if( zName == 0 ){` |
|     ! 0 |  6261 | `			goto OutOfMem;` |
|       - |  6262 | `		}` |
|      16 |  6263 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6264 | `	}else{` |
|   38762 |  6265 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   38762 |  6266 | `		if( zName == 0 ){` |
|     ! 0 |  6267 | `			goto OutOfMem;` |
|       - |  6268 | `		}` |
|   38762 |  6269 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6270 | `	}` |
|   38776 |  6271 | `	if( pGen->pIn < pEnd ){` |
|       - |  6272 | `		/* Collect function arguments */` |
|   26908 |  6273 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   26908 |  6274 | `		if( rc == SXERR_ABORT ){` |
|       - |  6275 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6276 | `			return SXERR_ABORT;` |
|       - |  6277 | `		}` |
|   13453 |  6278 | `	}` |
|       - |  6279 | `	/* Point past ')' and parse optional return type ': type' */` |
|   38776 |  6280 | `	pGen->pIn = &pEnd[1];` |
|       - |  6281 | `	{` |
|   38776 |  6282 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   38776 |  6283 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6284 | `			return SXERR_ABORT;` |
|   38776 |  6285 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6286 | `			return SXERR_SYNTAX;` |
|       - |  6287 | `		}` |
|       - |  6288 | `	}` |
|   38772 |  6289 | `	if( bHandleClosure ){` |
|       - |  6290 | `		ph7_vm_func_closure_env sEnv;` |
|     178 |  6291 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     176 |  6292 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      97 |  6293 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 |  6294 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6295 | `				/* Closure,record environment variable */` |
|      16 |  6296 | `				pGen->pIn++;` |
|      16 |  6297 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6298 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6299 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6300 | `						return SXERR_ABORT;` |
|       - |  6301 | `					}` |
|     ! 0 |  6302 | `				}` |
|      16 |  6303 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6304 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 |  6305 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 |  6306 | `					int iFlagsLocal = 0;` |
|      34 |  6307 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 |  6308 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 |  6309 | `						break;` |
|       - |  6310 | `					}` |
|      20 |  6311 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 |  6312 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6313 | `						/* Pass by reference,record that */` |
|     ! 0 |  6314 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6315 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6316 | `							);` |
|     ! 0 |  6317 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6318 | `						pGen->pIn++;` |
|     ! 0 |  6319 | `					}` |
|      18 |  6320 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 |  6321 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6322 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6323 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6324 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6325 | `								return SXERR_ABORT;` |
|       - |  6326 | `							}` |
|       - |  6327 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6328 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6329 | `								pGen->pIn++;` |
|     ! 0 |  6330 | `							}` |
|     ! 0 |  6331 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6332 | `								pGen->pIn++;` |
|     ! 0 |  6333 | `							}` |
|     ! 0 |  6334 | `							break;` |
|       - |  6335 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6336 | `					}else{` |
|       - |  6337 | `						SyString *pNameLocal;` |
|       - |  6338 | `						char *zDup;` |
|       - |  6339 | `						/* Duplicate variable name */` |
|      20 |  6340 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 |  6341 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 |  6342 | `						if( zDup ){` |
|       - |  6343 | `							/* Zero the structure */` |
|      20 |  6344 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 |  6345 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 |  6346 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 |  6347 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 |  6348 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6349 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6350 | `									got_this = 1;` |
|     ! 0 |  6351 | `							}` |
|       - |  6352 | `							/* Save imported variable */` |
|      20 |  6353 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 |  6354 | `						}else{` |
|     ! 0 |  6355 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6356 | `							 return SXERR_ABORT;` |
|       - |  6357 | `						}` |
|       - |  6358 | `					}` |
|      20 |  6359 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 |  6360 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6361 | `						/* Ignore trailing commas */` |
|       7 |  6362 | `						pGen->pIn++;` |
|       1 |  6363 | `					}` |
|       2 |  6364 | `				}` |
|      16 |  6365 | `				if( !got_this ){` |
|       - |  6366 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6367 | `					 * available to the closure environment.` |
|       - |  6368 | `					 */` |
|      16 |  6369 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 |  6370 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 |  6371 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 |  6372 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 |  6373 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 |  6374 | `				}` |
|      16 |  6375 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6376 | `					/* Mark as closure */` |
|      16 |  6377 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 |  6378 | `				}` |
|       7 |  6379 | `		}` |
|      88 |  6380 | `	}` |
|       - |  6381 | `	/* Compile the body */` |
|   38772 |  6382 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   38772 |  6383 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6384 | `		return SXERR_ABORT;` |
|       - |  6385 | `	}` |
|   38772 |  6386 | `	if( ppFunc ){` |
|     178 |  6387 | `		*ppFunc = pFunc;` |
|      88 |  6388 | `	}` |
|   38772 |  6389 | `	rc = SXRET_OK;` |
|   38772 |  6390 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6391 | `		/* Finally register the function */` |
|   38758 |  6392 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   19378 |  6393 | `	}` |
|   38772 |  6394 | `	if( rc == SXRET_OK ){` |
|   38772 |  6395 | `		return SXRET_OK;` |
|       - |  6396 | `	}` |
|       - |  6397 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6398 | `OutOfMem:` |
|       - |  6399 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6400 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6401 | `	 */` |
|     ! 0 |  6402 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6403 | `	return SXERR_ABORT;` |
|   19392 |  6404 |  |
|       - |  6405 | `/*` |
|       - |  6406 | ` * Compile a standard PHP function.` |
|       - |  6407 | ` *  Refer to the block-comment above for more information.` |
|       - |  6408 | ` */` |
|   38610 |  6409 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6410 |  |
|       - |  6411 | `	SyString *pName;` |
|       - |  6412 | `	sxi32 iFlags;` |
|       - |  6413 | `	sxu32 nLine;` |
|       - |  6414 | `	sxi32 rc;` |
|       - |  6415 |  |
|   38612 |  6416 | `	nLine = pGen->pIn->nLine;` |
|   38612 |  6417 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   38612 |  6418 | `	iFlags = 0;` |
|   38612 |  6419 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6420 | `		/* Return by reference,remember that */` |
|       7 |  6421 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6422 | `		/* Jump the '&' token */` |
|       7 |  6423 | `		pGen->pIn++;` |
|       3 |  6424 | `	}` |
|   38612 |  6425 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6426 | `		/* Invalid function name */` |
|       5 |  6427 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6428 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6429 | `			return SXERR_ABORT;` |
|       - |  6430 | `		}` |
|       - |  6431 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6432 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6433 | `			pGen->pIn++;` |
|       1 |  6434 | `		}` |
|       5 |  6435 | `		return SXRET_OK;` |
|       - |  6436 | `	}` |
|   38608 |  6437 | `	pName = &pGen->pIn->sData;` |
|   38608 |  6438 | `	nLine = pGen->pIn->nLine;` |
|       - |  6439 | `	/* Jump the function name */` |
|   38608 |  6440 | `	pGen->pIn++;` |
|   38608 |  6441 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6442 | `		/* Syntax error */` |
|       3 |  6443 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6444 | `		if( rc == SXERR_ABORT ){` |
|       - |  6445 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6446 | `			return SXERR_ABORT;` |
|       - |  6447 | `		}` |
|       - |  6448 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6449 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6450 | `			pGen->pIn++;` |
|     ! 0 |  6451 | `		}` |
|       3 |  6452 | `		return SXRET_OK;` |
|       - |  6453 | `	}` |
|       - |  6454 | `	/* Compile function body */` |
|   38606 |  6455 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   38606 |  6456 | `	return rc;` |
|   19307 |  6457 |  |
|       - |  6458 | `/*` |
|       - |  6459 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6460 | ` * According to the PHP language reference manual` |
|       - |  6461 | ` *  Visibility:` |
|       - |  6462 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6463 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6464 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6465 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6466 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6467 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6468 | ` */` |
|  246142 |  6469 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6470 |  |
|  246144 |  6471 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8824 |  6472 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  237322 |  6473 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   37978 |  6474 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6475 | `	}` |
|       - |  6476 | `	/* Assume public by default */` |
|  199346 |  6477 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  123073 |  6478 |  |
|       - |  6479 | `/*` |
|       - |  6480 | ` * Compile a class constant.` |
|       - |  6481 | ` * According to the PHP language reference manual` |
|       - |  6482 | ` *  Class Constants` |
|       - |  6483 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6484 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6485 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6486 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6487 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6488 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6489 | ` * Symisc eXtension.` |
|       - |  6490 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6491 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6492 | ` *  Example:` |
|       - |  6493 | ` *   class Test{` |
|       - |  6494 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6495 | ` *   };` |
|       - |  6496 | ` *   var_dump(TEST::MyConst);` |
|       - |  6497 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6498 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6499 | ` */` |
|      30 |  6500 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6501 |  |
|      32 |  6502 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6503 | `	SySet *pInstrContainer;` |
|       - |  6504 | `	ph7_class_attr *pCons;` |
|       - |  6505 | `	SyString *pName;` |
|       - |  6506 | `	sxi32 rc;` |
|       - |  6507 | `	/* Extract visibility level */` |
|      32 |  6508 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6509 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6510 | `loop:` |
|       - |  6511 | `	/* Mark as constant */` |
|      32 |  6512 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6513 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6514 | `		/* Invalid constant name */` |
|     ! 0 |  6515 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6516 | `		if( rc == SXERR_ABORT ){` |
|       - |  6517 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6518 | `			return SXERR_ABORT;` |
|       - |  6519 | `		}` |
|     ! 0 |  6520 | `		goto Synchronize;` |
|       - |  6521 | `	}` |
|       - |  6522 | `	/* Peek constant name */` |
|      32 |  6523 | `	pName = &pGen->pIn->sData;` |
|       - |  6524 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6525 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6526 | `		/* Reserved constant name */` |
|     ! 0 |  6527 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6528 | `		if( rc == SXERR_ABORT ){` |
|       - |  6529 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6530 | `			return SXERR_ABORT;` |
|       - |  6531 | `		}` |
|     ! 0 |  6532 | `		goto Synchronize;` |
|       - |  6533 | `	}` |
|       - |  6534 | `	/* Advance the stream cursor */` |
|      32 |  6535 | `	pGen->pIn++;` |
|      32 |  6536 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6537 | `		/* Invalid declaration */` |
|     ! 0 |  6538 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6539 | `		if( rc == SXERR_ABORT ){` |
|       - |  6540 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6541 | `			return SXERR_ABORT;` |
|       - |  6542 | `		}` |
|     ! 0 |  6543 | `		goto Synchronize;` |
|       - |  6544 | `	}` |
|      32 |  6545 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6546 | `	/* Allocate a new class attribute */` |
|      32 |  6547 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6548 | `	if( pCons == 0 ){` |
|     ! 0 |  6549 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6550 | `		return SXERR_ABORT;` |
|       - |  6551 | `	}` |
|       - |  6552 | `	/* Swap bytecode container */` |
|      32 |  6553 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6554 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6555 | `	/* Compile constant value.` |
|       - |  6556 | `	 */` |
|      32 |  6557 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6558 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6559 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6560 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6561 | `			return SXERR_ABORT;` |
|       - |  6562 | `		}` |
|       1 |  6563 | `	}` |
|       - |  6564 | `	/* Emit the done instruction */` |
|      32 |  6565 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6566 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6567 | `	if( rc == SXERR_ABORT ){` |
|       - |  6568 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6569 | `		return SXERR_ABORT;` |
|       - |  6570 | `	}` |
|       - |  6571 | `	/* All done,install the constant */` |
|      32 |  6572 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6573 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6574 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6575 | `		return SXERR_ABORT;` |
|       - |  6576 | `	}` |
|      32 |  6577 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6578 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6579 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6580 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6581 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6582 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6583 | `				pTok--;` |
|     ! 0 |  6584 | `			}` |
|     ! 0 |  6585 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6586 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6587 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6588 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6589 | `				return SXERR_ABORT;` |
|       - |  6590 | `			}` |
|     ! 0 |  6591 | `		}else{` |
|     ! 0 |  6592 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6593 | `				goto loop;` |
|       - |  6594 | `			}` |
|       - |  6595 | `		}` |
|     ! 0 |  6596 | `	}` |
|      32 |  6597 | `	return SXRET_OK;` |
|     ! 0 |  6598 | `Synchronize:` |
|       - |  6599 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6600 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6601 | `		pGen->pIn++;` |
|     ! 0 |  6602 | `	}` |
|     ! 0 |  6603 | `	return SXERR_CORRUPT;` |
|      17 |  6604 |  |
|       - |  6605 | `/*` |
|       - |  6606 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6607 | ` * According to the PHP language reference manual` |
|       - |  6608 | ` *  Properties` |
|       - |  6609 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6610 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6611 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6612 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6613 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6614 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6615 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6616 | ` * Symisc eXtension.` |
|       - |  6617 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6618 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6619 | ` *  Example:` |
|       - |  6620 | ` *   class Test{` |
|       - |  6621 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6622 | ` *   };` |
|       - |  6623 | ` *   var_dump(TEST::myVar);` |
|       - |  6624 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6625 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6626 | ` */` |
|       - |  6627 | `/*` |
|       - |  6628 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6629 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6630 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6631 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6632 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6633 | ` */` |
|  143584 |  6634 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6635 |  |
|  143586 |  6636 | `	SyToken *p = pStart;` |
|  143586 |  6637 | `	if( p >= pEnd ) return 0;` |
|  143586 |  6638 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6639 | `		p++;` |
|      16 |  6640 | `		if( p >= pEnd ) return 0;` |
|       7 |  6641 | `	}` |
|  143586 |  6642 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6643 | `		p++;` |
|       3 |  6644 | `		if( p >= pEnd ) return 0;` |
|       1 |  6645 | `	}` |
|  143586 |  6646 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6647 | `		return 0;` |
|       - |  6648 | `	}` |
|       - |  6649 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6650 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6651 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6652 | `	 * dispatch site. */` |
|  143586 |  6653 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  143568 |  6654 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  143615 |  6655 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3069 |  6656 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  143470 |  6657 | `			return 0;` |
|       - |  6658 | `		}` |
|      49 |  6659 | `	}` |
|     118 |  6660 | `	p++;` |
|       - |  6661 | `	/* Consume optional namespace path */` |
|     120 |  6662 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6663 | `		p += 2;` |
|       1 |  6664 | `	}` |
|       - |  6665 | ``	/* Consume any `\| Type` union alternatives */`` |
|     192 |  6666 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      78 |  6667 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6668 | `		p++;` |
|      14 |  6669 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6670 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6671 | `		p++;` |
|      14 |  6672 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6673 | `			p += 2;` |
|     ! 0 |  6674 | `		}` |
|       2 |  6675 | `	}` |
|     118 |  6676 | `	if( p >= pEnd ) return 0;` |
|     118 |  6677 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   71794 |  6678 |  |
|       - |  6679 |  |
|       - |  6680 | `/*` |
|       - |  6681 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6682 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6683 | ` * if not). Recognized forms:` |
|       - |  6684 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6685 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6686 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6687 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6688 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6689 | ` * on unrecoverable error.` |
|       - |  6690 | ` *` |
|       - |  6691 | ` * When a type is parsed:` |
|       - |  6692 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6693 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6694 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6695 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6696 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6697 | ` */` |
|     116 |  6698 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6699 | `	ph7_gen_state *pGen,` |
|       - |  6700 | `	sxu32 *pnType,` |
|       - |  6701 | `	SyString *pClass,` |
|       - |  6702 | `	sxi32 *piTypeFlags,` |
|       - |  6703 | `	SyString *pTypeText,` |
|       - |  6704 | `	SySet *pAlts` |
|       2 |  6705 | `){` |
|     118 |  6706 | `	sxi32 iFlags = 0;` |
|       - |  6707 | `	sxi32 rc;` |
|     118 |  6708 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6709 | `		return SXRET_OK;` |
|       - |  6710 | `	}` |
|       - |  6711 | `	/* If the first token is '$', there's no type */` |
|     118 |  6712 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6713 | `		return SXRET_OK;` |
|       - |  6714 | `	}` |
|     118 |  6715 | `	rc = GenStateParseUnionTypeDecl(` |
|      58 |  6716 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6717 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6718 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6719 | `		/* bAllowVoid */ 0,` |
|     116 |  6720 | `		pGen->pIn->nLine);` |
|     118 |  6721 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6722 | `		return rc;` |
|       - |  6723 | `	}` |
|       - |  6724 | `	/* Verify next token is '$' (start of property name) */` |
|     118 |  6725 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6726 | `		return SXERR_SYNTAX;` |
|       - |  6727 | `	}` |
|     118 |  6728 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     118 |  6729 | `	return SXRET_OK;` |
|      60 |  6730 |  |
|       - |  6731 |  |
|       - |  6732 | `/*` |
|       - |  6733 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6734 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6735 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6736 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6737 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6738 | ` * by the type parser itself before reaching here.` |
|       - |  6739 | ` *` |
|       - |  6740 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6741 | ` * use in the error message.` |
|       - |  6742 | ` */` |
|     182 |  6743 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6744 | `	sxu32 nType,` |
|       - |  6745 | `	const SyString *pClass,` |
|       - |  6746 | `	const char **pzName,` |
|       - |  6747 | `	sxu32 *pnName)` |
|       2 |  6748 |  |
|       - |  6749 | `	const char *z;` |
|       - |  6750 | `	sxu32 n;` |
|     184 |  6751 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     154 |  6752 | `		return 0;` |
|       - |  6753 | `	}` |
|      32 |  6754 | `	z = pClass->zString;` |
|      32 |  6755 | `	n = pClass->nByte;` |
|      32 |  6756 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       5 |  6757 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6758 | `	}` |
|      28 |  6759 | `	if( n == 5 && SyMemcmpNoCase(z,"mixed",5) == 0 ){` |
|     ! 0 |  6760 | `		*pzName = "mixed"; *pnName = 5; return 1;` |
|       - |  6761 | `	}` |
|      28 |  6762 | `	if( n == 8 && SyMemcmpNoCase(z,"iterable",8) == 0 ){` |
|     ! 0 |  6763 | `		*pzName = "iterable"; *pnName = 8; return 1;` |
|       - |  6764 | `	}` |
|      28 |  6765 | `	return 0;` |
|      93 |  6766 |  |
|       - |  6767 |  |
|       - |  6768 | `/*` |
|       - |  6769 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6770 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6771 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6772 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6773 | `` * unions like `callable\|int`).`` |
|       - |  6774 | ` *` |
|       - |  6775 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6776 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6777 | ` */` |
|     154 |  6778 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6779 | `	ph7_gen_state *pGen,` |
|       - |  6780 | `	ph7_class *pClass,` |
|       - |  6781 | `	const SyString *pPropName,` |
|       - |  6782 | `	sxu32 nType,` |
|       - |  6783 | `	const SyString *pTypeClass,` |
|       - |  6784 | `	const SyString *pTypeText,` |
|       - |  6785 | `	SySet *pUnionAlts,` |
|       - |  6786 | `	sxu32 nLine)` |
|       2 |  6787 |  |
|     156 |  6788 | `	const char *zBad = 0;` |
|     156 |  6789 | `	sxu32 nBad = 0;` |
|       - |  6790 | `	SyString sFallback;` |
|       - |  6791 | `	const SyString *pBad;` |
|       - |  6792 | `	sxi32 rc;` |
|     156 |  6793 | `	int bDisallowed = 0;` |
|     156 |  6794 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6795 | `		bDisallowed = 1;` |
|     155 |  6796 | `	}else if( pUnionAlts ){` |
|       - |  6797 | `		sxu32 i;` |
|      42 |  6798 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      30 |  6799 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      30 |  6800 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6801 | `				bDisallowed = 1;` |
|       3 |  6802 | `				break;` |
|       - |  6803 | `			}` |
|      15 |  6804 | `		}` |
|       7 |  6805 | `	}` |
|     156 |  6806 | `	if( !bDisallowed ){` |
|     152 |  6807 | `		return SXRET_OK;` |
|       - |  6808 | `	}` |
|       - |  6809 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6810 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6811 | `	 * canonical spelling if the type text is unavailable. */` |
|       5 |  6812 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       5 |  6813 | `		pBad = pTypeText;` |
|       3 |  6814 | `	}else{` |
|     ! 0 |  6815 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6816 | `		pBad = &sFallback;` |
|       - |  6817 | `	}` |
|       7 |  6818 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6819 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6820 | `		&pClass->sName,pPropName,pBad);` |
|       5 |  6821 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6822 | `		return SXERR_ABORT;` |
|       - |  6823 | `	}` |
|       5 |  6824 | `	return SXERR_SYNTAX;` |
|      79 |  6825 |  |
|       - |  6826 |  |
|   55872 |  6827 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6828 |  |
|   55874 |  6829 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6830 | `	ph7_class_attr *pAttr;` |
|       - |  6831 | `	SyString *pName;` |
|       - |  6832 | `	sxi32 rc;` |
|   55874 |  6833 | `	sxu32 nType = 0;` |
|       - |  6834 | `	SyString sTypeClass;` |
|       - |  6835 | `	SyString sTypeText;` |
|       - |  6836 | `	SySet aUnionAlts;` |
|   55874 |  6837 | `	sxi32 iTypeFlags = 0;` |
|   55874 |  6838 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   55874 |  6839 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   55874 |  6840 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6841 | `	/* Extract visibility level */` |
|   55874 |  6842 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6843 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   55932 |  6844 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     118 |  6845 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     118 |  6846 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6847 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6848 | `			goto Synchronize;` |
|     118 |  6849 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  6850 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6851 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  6852 | `				&pGen->pIn->sData);` |
|     ! 0 |  6853 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6854 | `				return SXERR_ABORT;` |
|       - |  6855 | `			}` |
|     ! 0 |  6856 | `			goto Synchronize;` |
|     118 |  6857 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6858 | `			return SXERR_ABORT;` |
|       - |  6859 | `		}` |
|      58 |  6860 | `	}` |
|     ! 0 |  6861 | `loop:` |
|   55878 |  6862 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6863 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  6864 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6865 | `			return SXERR_ABORT;` |
|       - |  6866 | `		}` |
|     ! 0 |  6867 | `		goto Synchronize;` |
|       - |  6868 | `	}` |
|   55878 |  6869 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   55878 |  6870 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  6871 | `		/* Invalid attribute name */` |
|     ! 0 |  6872 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  6873 | `		if( rc == SXERR_ABORT ){` |
|       - |  6874 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6875 | `			return SXERR_ABORT;` |
|       - |  6876 | `		}` |
|     ! 0 |  6877 | `		goto Synchronize;` |
|       - |  6878 | `	}` |
|       - |  6879 | `	/* Peek attribute name */` |
|   55878 |  6880 | `	pName = &pGen->pIn->sData;` |
|       - |  6881 | `	/* Advance the stream cursor */` |
|   55878 |  6882 | `	pGen->pIn++;` |
|   55878 |  6883 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  6884 | `		/* Invalid declaration */` |
|       3 |  6885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  6886 | `		if( rc == SXERR_ABORT ){` |
|       - |  6887 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6888 | `			return SXERR_ABORT;` |
|       - |  6889 | `		}` |
|       3 |  6890 | `		goto Synchronize;` |
|       - |  6891 | `	}` |
|       - |  6892 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  6893 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  6894 | `	 * by the type parser. */` |
|   55876 |  6895 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     182 |  6896 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  6897 | `			&sTypeText,` |
|     120 |  6898 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     122 |  6899 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6900 | `			return SXERR_ABORT;` |
|     122 |  6901 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  6902 | `			goto Synchronize;` |
|       - |  6903 | `		}` |
|      60 |  6904 | `	}` |
|       - |  6905 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   55876 |  6906 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  6907 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  6908 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  6909 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6910 | `			return SXERR_ABORT;` |
|       - |  6911 | `		}` |
|       3 |  6912 | `		goto Synchronize;` |
|       - |  6913 | `	}` |
|       - |  6914 | `	/* Allocate a new class attribute */` |
|   55874 |  6915 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   55874 |  6916 | `	if( pAttr == 0 ){` |
|     ! 0 |  6917 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  6918 | `		return SXERR_ABORT;` |
|       - |  6919 | `	}` |
|   55874 |  6920 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     120 |  6921 | `		pAttr->nType = nType;` |
|     120 |  6922 | `		pAttr->sClass = sTypeClass;` |
|     120 |  6923 | `		pAttr->sTypeName = sTypeText;` |
|     120 |  6924 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6925 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  6926 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  6927 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  6928 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  6929 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  6930 | `			sxu32 i;` |
|      32 |  6931 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  6932 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  6933 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  6934 | `			}` |
|       5 |  6935 | `		}` |
|      59 |  6936 | `	}` |
|   55874 |  6937 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  6938 | `		SySet *pInstrContainer;` |
|   17838 |  6939 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  6940 | `		/* Swap bytecode container */` |
|   17838 |  6941 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   17838 |  6942 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  6943 | `		/* Compile attribute value.` |
|       - |  6944 | `		 */` |
|   17838 |  6945 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   17838 |  6946 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  6947 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  6948 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6949 | `				return SXERR_ABORT;` |
|       - |  6950 | `			}` |
|     ! 0 |  6951 | `		}` |
|       - |  6952 | `		/* Emit the done instruction */` |
|   17838 |  6953 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   17838 |  6954 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    8918 |  6955 | `	}` |
|       - |  6956 | `	/* All done,install the attribute */` |
|   55874 |  6957 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   55874 |  6958 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6959 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6960 | `		return SXERR_ABORT;` |
|       - |  6961 | `	}` |
|   55874 |  6962 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6963 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  6964 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  6965 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  6966 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6967 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6968 | `				pTok--;` |
|     ! 0 |  6969 | `			}` |
|     ! 0 |  6970 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6971 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  6972 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6973 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6974 | `				return SXERR_ABORT;` |
|       - |  6975 | `			}` |
|     ! 0 |  6976 | `		}else{` |
|       5 |  6977 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  6978 | `				goto loop;` |
|       - |  6979 | `			}` |
|       - |  6980 | `		}` |
|     ! 0 |  6981 | `	}` |
|   55870 |  6982 | `	SySetRelease(&aUnionAlts);` |
|   55870 |  6983 | `	return SXRET_OK;` |
|       2 |  6984 | `Synchronize:` |
|       - |  6985 | `	/* Synchronize with the first semi-colon */` |
|      11 |  6986 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  6987 | `		pGen->pIn++;` |
|       1 |  6988 | `	}` |
|       5 |  6989 | `	SySetRelease(&aUnionAlts);` |
|       5 |  6990 | `	return SXERR_CORRUPT;` |
|   27938 |  6991 |  |
|       - |  6992 | `/*` |
|       - |  6993 | ` * Compile a class method.` |
|       - |  6994 | ` *` |
|       - |  6995 | ` * Refer to the official documentation for more information` |
|       - |  6996 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  6997 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  6998 | ` * overloading and many more.` |
|       - |  6999 | ` */` |
|  190240 |  7000 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7001 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7002 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7003 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7004 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7005 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7006 | `	)` |
|       2 |  7007 |  |
|  190242 |  7008 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7009 | `	ph7_class_method *pMeth;` |
|       - |  7010 | `	sxi32 iFuncFlags;` |
|       - |  7011 | `	SyString *pName;` |
|       - |  7012 | `	SyToken *pEnd;` |
|       - |  7013 | `	sxi32 rc;` |
|       - |  7014 | `	/* Extract visibility level */` |
|  190242 |  7015 | `	iProtection = GetProtectionLevel(iProtection);` |
|  190242 |  7016 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  190242 |  7017 | `	iFuncFlags = 0;` |
|  190242 |  7018 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7019 | `		/* Invalid method name */` |
|     ! 0 |  7020 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7021 | `		if( rc == SXERR_ABORT ){` |
|       - |  7022 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7023 | `			return SXERR_ABORT;` |
|       - |  7024 | `		}` |
|     ! 0 |  7025 | `		goto Synchronize;` |
|       - |  7026 | `	}` |
|  190242 |  7027 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7028 | `		/* Return by reference,remember that */` |
|     ! 0 |  7029 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7030 | `		/* Jump the '&' token */` |
|     ! 0 |  7031 | `		pGen->pIn++;` |
|     ! 0 |  7032 | `	}` |
|  190242 |  7033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7034 | `		/* Invalid method name */` |
|     ! 0 |  7035 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7036 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7037 | `			return SXERR_ABORT;` |
|       - |  7038 | `		}` |
|     ! 0 |  7039 | `		goto Synchronize;` |
|       - |  7040 | `	}` |
|       - |  7041 | `	/* Peek method name */` |
|  190242 |  7042 | `	pName = &pGen->pIn->sData;` |
|  190242 |  7043 | `	nLine = pGen->pIn->nLine;` |
|       - |  7044 | `	/* Jump the method name */` |
|  190242 |  7045 | `	pGen->pIn++;` |
|  190242 |  7046 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7047 | `		/* Abstract method */` |
|   46740 |  7048 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7049 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7050 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7051 | `				&pClass->sName,pName);` |
|     ! 0 |  7052 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7053 | `				return SXERR_ABORT;` |
|       - |  7054 | `			}` |
|     ! 0 |  7055 | `		}` |
|       - |  7056 | `		/* Assemble method signature only */` |
|   46740 |  7057 | `		doBody = FALSE;` |
|   23369 |  7058 | `	}` |
|  190242 |  7059 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7060 | `		/* Syntax error */` |
|     ! 0 |  7061 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7062 | `		if( rc == SXERR_ABORT ){` |
|       - |  7063 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7064 | `			return SXERR_ABORT;` |
|       - |  7065 | `		}` |
|     ! 0 |  7066 | `		goto Synchronize;` |
|       - |  7067 | `	}` |
|       - |  7068 | `	/* Allocate a new class_method instance */` |
|  190242 |  7069 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  190242 |  7070 | `	if( pMeth == 0 ){` |
|     ! 0 |  7071 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7072 | `		return SXERR_ABORT;` |
|       - |  7073 | `	}` |
|       - |  7074 | `	/* Jump the left parenthesis '(' */` |
|  190242 |  7075 | `	pGen->pIn++;` |
|  190242 |  7076 | `	pEnd = 0; /* cc warning */` |
|       - |  7077 | `	/* Delimit the method signature */` |
|  190242 |  7078 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  190242 |  7079 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7080 | `		/* Syntax error */` |
|       3 |  7081 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7082 | `		if( rc == SXERR_ABORT ){` |
|       - |  7083 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7084 | `			return SXERR_ABORT;` |
|       - |  7085 | `		}` |
|       3 |  7086 | `		goto Synchronize;` |
|       - |  7087 | `	}` |
|       - |  7088 | `	{` |
|  190240 |  7089 | `		int bIsCtor = 0;` |
|  190240 |  7090 | `		int bAbstractCtor = 0;` |
|  276558 |  7091 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  114144 |  7092 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  181441 |  7093 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   17600 |  7094 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7095 | `				bAbstractCtor = 1;` |
|       2 |  7096 | `			}else{` |
|   17598 |  7097 | `				bIsCtor = 1;` |
|       - |  7098 | `			}` |
|    8799 |  7099 | `		}` |
|  190240 |  7100 | `		if( pGen->pIn < pEnd ){` |
|       - |  7101 | `			/* Collect method arguments */` |
|   32244 |  7102 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   32244 |  7103 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7104 | `				return SXERR_ABORT;` |
|       - |  7105 | `			}` |
|   16121 |  7106 | `		}` |
|       - |  7107 | `	}` |
|       - |  7108 | `	/* Point past ')' and parse optional return type ': type' */` |
|  190240 |  7109 | `	pGen->pIn = &pEnd[1];` |
|       - |  7110 | `	{` |
|  190240 |  7111 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  190240 |  7112 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7113 | `			return SXERR_ABORT;` |
|  190240 |  7114 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7115 | `			goto Synchronize;` |
|       - |  7116 | `		}` |
|       - |  7117 | `	}` |
|       - |  7118 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7119 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7120 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7121 | `	{` |
|  190240 |  7122 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7123 | `		sxu32 i;` |
|  248778 |  7124 | `		for( i = 0; i < nArg; i++ ){` |
|   58548 |  7125 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7126 | `			ph7_class_attr *pAttr;` |
|   58548 |  7127 | `			sxi32 iAttrFlags = 0;` |
|   58548 |  7128 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   58512 |  7129 | `				continue;` |
|       - |  7130 | `			}` |
|      38 |  7131 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7132 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7133 | `					"Cannot declare variadic promoted property");` |
|       3 |  7134 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7135 | `					return SXERR_ABORT;` |
|       - |  7136 | `				}` |
|       3 |  7137 | `				goto Synchronize;` |
|       - |  7138 | `			}` |
|       - |  7139 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7140 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7141 | `			 * appear as an alternative of a union type. */` |
|      34 |  7142 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       6 |  7143 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      53 |  7144 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7145 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7146 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7147 | `					nLine);` |
|      36 |  7148 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7149 | `					return SXERR_ABORT;` |
|      36 |  7150 | `				}else if( rc != SXRET_OK ){` |
|       5 |  7151 | `					goto Synchronize;` |
|       - |  7152 | `				}` |
|      15 |  7153 | `			}` |
|       - |  7154 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      32 |  7155 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7156 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7157 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7158 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7159 | `					return SXERR_ABORT;` |
|       - |  7160 | `				}` |
|       3 |  7161 | `				goto Synchronize;` |
|       - |  7162 | `			}` |
|      30 |  7163 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      28 |  7164 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7165 | `			}` |
|      30 |  7166 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7167 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7168 | `			}` |
|      30 |  7169 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7170 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7171 | `			}` |
|      30 |  7172 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      30 |  7173 | `			if( pAttr == 0 ){` |
|     ! 0 |  7174 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7175 | `				return SXERR_ABORT;` |
|       - |  7176 | `			}` |
|      30 |  7177 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      28 |  7178 | `				pAttr->nType = pArg->nType;` |
|      28 |  7179 | `				pAttr->sClass = pArg->sClass;` |
|      28 |  7180 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      28 |  7181 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7182 | `					sxu32 k;` |
|     ! 0 |  7183 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7184 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7185 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7186 | `					}` |
|     ! 0 |  7187 | `				}` |
|      13 |  7188 | `			}` |
|      30 |  7189 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      30 |  7190 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7191 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7192 | `				return SXERR_ABORT;` |
|       - |  7193 | `			}` |
|      16 |  7194 | `		}` |
|       - |  7195 | `	}` |
|  190232 |  7196 | `	if( doBody ){` |
|       - |  7197 | `		/* Compile method body */` |
|  143494 |  7198 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  143494 |  7199 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7200 | `			return SXERR_ABORT;` |
|       - |  7201 | `		}` |
|   71748 |  7202 | `	}else{` |
|       - |  7203 | `		/* Only method signature is allowed */` |
|   46740 |  7204 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7205 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7206 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7207 | `				if( rc == SXERR_ABORT ){` |
|       - |  7208 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7209 | `					return SXERR_ABORT;` |
|       - |  7210 | `				}` |
|     ! 0 |  7211 | `				return SXERR_CORRUPT;` |
|       - |  7212 | `			}` |
|       - |  7213 | `	}` |
|       - |  7214 | `	/* All done,install the method */` |
|  190232 |  7215 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  190232 |  7216 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7217 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7218 | `		return SXERR_ABORT;` |
|       - |  7219 | `	}` |
|  190232 |  7220 | `	return SXRET_OK;` |
|       5 |  7221 | `Synchronize:` |
|       - |  7222 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7223 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      21 |  7224 | `		pGen->pIn++;` |
|       1 |  7225 | `	}` |
|      11 |  7226 | `	return SXERR_CORRUPT;` |
|   95122 |  7227 |  |
|       - |  7228 | `/*` |
|       - |  7229 | ` * Compile an object interface.` |
|       - |  7230 | ` *  According to the PHP language reference manual` |
|       - |  7231 | ` *   Object Interfaces:` |
|       - |  7232 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7233 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7234 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7235 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7236 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7237 | ` */` |
|   11714 |  7238 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  7239 |  |
|   11716 |  7240 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7241 | `	ph7_class *pClass,*pBase;` |
|       - |  7242 | `	SyToken *pEnd,*pTmp;` |
|       - |  7243 | `	SyString *pName;` |
|       - |  7244 | `	sxi32 nKwrd;` |
|       - |  7245 | `	sxi32 rc;` |
|       - |  7246 | `	/* Jump the 'interface' keyword */` |
|   11716 |  7247 | `	pGen->pIn++;` |
|       - |  7248 | `	/* Extract interface name */` |
|   11716 |  7249 | `	pName = &pGen->pIn->sData;` |
|       - |  7250 | `	/* Advance the stream cursor */` |
|   11716 |  7251 | `	pGen->pIn++;` |
|       - |  7252 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7253 | `		SyBlob sFQN;` |
|       - |  7254 | `		SyString sFQNStr;` |
|   11716 |  7255 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   11716 |  7256 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   11716 |  7257 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   11716 |  7258 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   11716 |  7259 | `		SyBlobRelease(&sFQN);` |
|       - |  7260 | `	}` |
|   11716 |  7261 | `	if( pClass == 0 ){` |
|     ! 0 |  7262 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7263 | `		return SXERR_ABORT;` |
|       - |  7264 | `	}` |
|       - |  7265 | `	/* Mark as an interface */` |
|   11716 |  7266 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7267 | `	/* Assume no base class is given */` |
|   11716 |  7268 | `	pBase = 0;` |
|   11716 |  7269 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 |  7270 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 |  7271 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7272 | `			SyBlob sResolved;` |
|       - |  7273 | `			SyString sBaseName;` |
|       - |  7274 | `			sxu32 nRefLine;` |
|       - |  7275 | `			/* Extract base interface */` |
|       8 |  7276 | `			pGen->pIn++;` |
|       8 |  7277 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       8 |  7278 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       8 |  7279 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7280 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7281 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7282 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7283 | `					pName);` |
|     ! 0 |  7284 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7285 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7286 | `					return SXERR_ABORT;` |
|       - |  7287 | `				}` |
|     ! 0 |  7288 | `				return SXRET_OK;` |
|       - |  7289 | `			}` |
|      11 |  7290 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|       6 |  7291 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       8 |  7292 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7293 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7294 | `			/* Only interfaces is allowed */` |
|       8 |  7295 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7296 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7297 | `			}` |
|       8 |  7298 | `			if( pBase == 0 ){` |
|     ! 0 |  7299 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7300 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7301 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7302 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7303 | `					return SXERR_ABORT;` |
|       - |  7304 | `				}` |
|     ! 0 |  7305 | `			}` |
|       8 |  7306 | `			SyBlobRelease(&sResolved);` |
|       3 |  7307 | `		}` |
|       3 |  7308 | `	}` |
|   11716 |  7309 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7310 | `		/* Syntax error */` |
|     ! 0 |  7311 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7312 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7313 | `		if( rc == SXERR_ABORT ){` |
|       - |  7314 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7315 | `			return SXERR_ABORT;` |
|       - |  7316 | `		}` |
|     ! 0 |  7317 | `		return SXRET_OK;` |
|       - |  7318 | `	}` |
|   11716 |  7319 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   11716 |  7320 | `	pEnd = 0; /* cc warning */` |
|       - |  7321 | `	/* Delimit the interface body */` |
|   11716 |  7322 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   11716 |  7323 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7324 | `		/* Syntax error */` |
|     ! 0 |  7325 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7326 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7327 | `		if( rc == SXERR_ABORT ){` |
|       - |  7328 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7329 | `			return SXERR_ABORT;` |
|       - |  7330 | `		}` |
|     ! 0 |  7331 | `		return SXRET_OK;` |
|       - |  7332 | `	}` |
|       - |  7333 | `	/* Swap token stream */` |
|   11716 |  7334 | `	pTmp = pGen->pEnd;` |
|   11716 |  7335 | `	pGen->pEnd = pEnd;` |
|       - |  7336 | `	/* Start the parse process` |
|       - |  7337 | `	 * Note (According to the PHP reference manual):` |
|       - |  7338 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7339 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7340 | `	 */` |
|   29220 |  7341 | `	for(;;){` |
|       - |  7342 | `		/* Jump leading/trailing semi-colons */` |
|  105168 |  7343 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   46728 |  7344 | `			pGen->pIn++;` |
|       2 |  7345 | `		}` |
|   58442 |  7346 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7347 | `			/* End of interface body */` |
|   11714 |  7348 | `			break;` |
|       - |  7349 | `		}` |
|   46730 |  7350 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7351 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7352 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7353 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7354 | `			if( rc == SXERR_ABORT ){` |
|       - |  7355 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7356 | `				return SXERR_ABORT;` |
|       - |  7357 | `			}` |
|     ! 0 |  7358 | `			goto done;` |
|       - |  7359 | `		}` |
|       - |  7360 | `		/* Extract the current keyword */` |
|   46730 |  7361 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   46730 |  7362 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7363 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7364 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7365 | `			const char *zKind = "member";` |
|       3 |  7366 | `			SyString *pMemberName = 0;` |
|       3 |  7367 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7368 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7369 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7370 | `					zKind = "constant";` |
|       3 |  7371 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7372 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7373 | `					}` |
|       1 |  7374 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7375 | `					zKind = "method";` |
|     ! 0 |  7376 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7377 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7378 | `					}` |
|     ! 0 |  7379 | `				}` |
|       1 |  7380 | `			}` |
|       3 |  7381 | `			if( pMemberName ){` |
|       4 |  7382 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7383 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7384 | `			}else{` |
|     ! 0 |  7385 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7386 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7387 | `			}` |
|       3 |  7388 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7389 | `				return SXERR_ABORT;` |
|       - |  7390 | `			}` |
|       3 |  7391 | `			goto done;` |
|       - |  7392 | `		}` |
|   46728 |  7393 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7394 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7395 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7396 | `			if( rc == SXERR_ABORT ){` |
|       - |  7397 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7398 | `				return SXERR_ABORT;` |
|       - |  7399 | `			}` |
|     ! 0 |  7400 | `			goto done;` |
|       - |  7401 | `		}` |
|   46728 |  7402 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7403 | `			/* Advance the stream cursor */` |
|   46724 |  7404 | `			pGen->pIn++;` |
|   46724 |  7405 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7406 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7407 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7408 | `				if( rc == SXERR_ABORT ){` |
|       - |  7409 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7410 | `					return SXERR_ABORT;` |
|       - |  7411 | `				}` |
|     ! 0 |  7412 | `				goto done;` |
|       - |  7413 | `			}` |
|   46724 |  7414 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   46724 |  7415 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7416 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7417 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7418 | `				if( rc == SXERR_ABORT ){` |
|       - |  7419 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7420 | `					return SXERR_ABORT;` |
|       - |  7421 | `				}` |
|     ! 0 |  7422 | `				goto done;` |
|       - |  7423 | `			}` |
|   23361 |  7424 | `		}` |
|   46728 |  7425 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7426 | `			/* Parse constant */` |
|       3 |  7427 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7428 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7429 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7430 | `					return SXERR_ABORT;` |
|       - |  7431 | `				}` |
|     ! 0 |  7432 | `				goto done;` |
|       - |  7433 | `			}` |
|       2 |  7434 | `		}else{` |
|   46726 |  7435 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   46726 |  7436 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7437 | `				/* Static method,record that */` |
|     ! 0 |  7438 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7439 | `				/* Advance the stream cursor */` |
|     ! 0 |  7440 | `				pGen->pIn++;` |
|     ! 0 |  7441 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  7442 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7443 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7444 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7445 | `						if( rc == SXERR_ABORT ){` |
|       - |  7446 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7447 | `							return SXERR_ABORT;` |
|       - |  7448 | `						}` |
|     ! 0 |  7449 | `						goto done;` |
|       - |  7450 | `				}` |
|     ! 0 |  7451 | `			}` |
|       - |  7452 | `			/* Process method signature (no body for interface methods) */` |
|   46726 |  7453 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   46726 |  7454 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7455 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7456 | `					return SXERR_ABORT;` |
|       - |  7457 | `				}` |
|     ! 0 |  7458 | `				goto done;` |
|       - |  7459 | `			}` |
|       - |  7460 | `		}` |
|       2 |  7461 | `	}` |
|       - |  7462 | `	/* Install the interface */` |
|   11714 |  7463 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   11714 |  7464 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7465 | `		/* Inherit from the base interface */` |
|       8 |  7466 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       3 |  7467 | `	}` |
|   11714 |  7468 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7469 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7470 | `		return SXERR_ABORT;` |
|       - |  7471 | `	}` |
|    5856 |  7472 | `done:` |
|       - |  7473 | `	/* Point beyond the interface body */` |
|   11716 |  7474 | `	pGen->pIn  = &pEnd[1];` |
|   11716 |  7475 | `	pGen->pEnd = pTmp;` |
|   11716 |  7476 | `	return PH7_OK;` |
|    5859 |  7477 |  |
|       - |  7478 | `/*` |
|       - |  7479 | ` * Compile a user-defined class.` |
|       - |  7480 | ` * According to the PHP language reference manual` |
|       - |  7481 | ` *  class` |
|       - |  7482 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7483 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7484 | ` *  of the properties and methods belonging to the class.` |
|       - |  7485 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7486 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7487 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7488 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7489 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7490 | ` *  (called "methods").` |
|       - |  7491 | ` */` |
|       - |  7492 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7493 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7494 | `struct TraitUseEntry {` |
|       - |  7495 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7496 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7497 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7498 | `};` |
|       - |  7499 | `/*` |
|       - |  7500 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7501 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7502 | ` */` |
|   41594 |  7503 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7504 |  |
|       - |  7505 | `	ph7_class **apIface;` |
|       - |  7506 | `	sxu32 nIface,i;` |
|       - |  7507 | `	sxi32 rc;` |
|   41596 |  7508 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7509 | `		return SXRET_OK;` |
|       - |  7510 | `	}` |
|   41596 |  7511 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   41596 |  7512 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   50392 |  7513 | `	for(i = 0; i < nIface; i++){` |
|    8798 |  7514 | `		ph7_class *pIface = apIface[i];` |
|       - |  7515 | `		SyHashEntry *pEntry;` |
|    8798 |  7516 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   70186 |  7517 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   61390 |  7518 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7519 | `			ph7_class_method *pImplMeth;` |
|   61390 |  7520 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7521 | `			/* Find the implementing method in the class */` |
|   61390 |  7522 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   61390 |  7523 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7524 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7525 | `			}` |
|       - |  7526 | `			/* Check visibility: interface methods must be implemented as public */` |
|   61376 |  7527 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7528 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7529 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7530 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7531 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7532 | `					return SXERR_ABORT;` |
|       - |  7533 | `				}` |
|       1 |  7534 | `			}` |
|       - |  7535 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7536 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7537 | `			 */` |
|       - |  7538 | `			{` |
|   61376 |  7539 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   61376 |  7540 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   61376 |  7541 | `				int sigError = 0;` |
|   61376 |  7542 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7543 | `					sigError = 1;` |
|   61375 |  7544 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7545 | `					/* Extra parameters must all have default values */` |
|       5 |  7546 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7547 | `					sxu32 k;` |
|       7 |  7548 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7549 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7550 | `							sigError = 1;` |
|       3 |  7551 | `							break;` |
|       - |  7552 | `						}` |
|       2 |  7553 | `					}` |
|       2 |  7554 | `				}` |
|   61376 |  7555 | `				if( sigError ){` |
|       - |  7556 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7557 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7558 | `					sxu32 j;` |
|       5 |  7559 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7560 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7561 | `					/* Build implementing method signature */` |
|       5 |  7562 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7563 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7564 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7565 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7566 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7567 | `					}` |
|       - |  7568 | `					/* Build interface method signature */` |
|       5 |  7569 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7570 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7571 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7572 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7573 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7574 | `					}` |
|       7 |  7575 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7576 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7577 | `						&pClass->sName,pMName,` |
|       4 |  7578 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7579 | `						&pIface->sName,pMName,` |
|       4 |  7580 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7581 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7582 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7583 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7584 | `						return SXERR_ABORT;` |
|       - |  7585 | `					}` |
|       2 |  7586 | `				}` |
|       - |  7587 | `			}` |
|       2 |  7588 | `		}` |
|    4400 |  7589 | `	}` |
|   41596 |  7590 | `	return SXRET_OK;` |
|   20799 |  7591 |  |
|       - |  7592 | `/*` |
|       - |  7593 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7594 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7595 | ` */` |
|   41594 |  7596 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7597 |  |
|       - |  7598 | `	ph7_class_method *pMeth;` |
|       - |  7599 | `	SyHashEntry *pEntry;` |
|       - |  7600 | `	sxu32 nAbstract;` |
|       - |  7601 | `	SyBlob sMsg;` |
|       - |  7602 | `	sxi32 rc;` |
|       - |  7603 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   41596 |  7604 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      22 |  7605 | `		return SXRET_OK;` |
|       - |  7606 | `	}` |
|       - |  7607 | `	/* Count abstract methods */` |
|   41576 |  7608 | `	nAbstract = 0;` |
|   41576 |  7609 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  392818 |  7610 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  351244 |  7611 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  351244 |  7612 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7613 | `			nAbstract++;` |
|       8 |  7614 | `		}` |
|       2 |  7615 | `	}` |
|   41576 |  7616 | `	if( nAbstract == 0 ){` |
|   41562 |  7617 | `		return SXRET_OK;` |
|       - |  7618 | `	}` |
|       - |  7619 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7620 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7621 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7622 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7623 | `		&pClass->sName,nAbstract,` |
|       7 |  7624 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7625 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7626 | `	/* Second pass: list methods with origins */` |
|       - |  7627 | `	{` |
|      15 |  7628 | `		sxu32 nListed = 0;` |
|      15 |  7629 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7630 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7631 | `			ph7_class *pOrigin = 0;` |
|       - |  7632 | `			SyString *pMName;` |
|      19 |  7633 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7634 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7635 | `				continue;` |
|       - |  7636 | `			}` |
|      17 |  7637 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7638 | `			if( nListed > 0 ){` |
|       3 |  7639 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7640 | `			}` |
|       - |  7641 | `			/* Find the origin of this abstract method.` |
|       - |  7642 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7643 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7644 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7645 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7646 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7647 | `			 * class's namespace.` |
|       - |  7648 | `			 */` |
|       - |  7649 | `			{` |
|       - |  7650 | `				ph7_class **apIface;` |
|       - |  7651 | `				ph7_class **apTrait;` |
|       - |  7652 | `				ph7_class *pWalk;` |
|       - |  7653 | `				sxu32 i;` |
|       - |  7654 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7655 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7656 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7657 | `				 */` |
|      17 |  7658 | `				if( pClass->pBase ){` |
|       9 |  7659 | `					pWalk = pClass->pBase;` |
|      17 |  7660 | `					while( pWalk ){` |
|       - |  7661 | `						ph7_class_method *pParentMeth;` |
|      11 |  7662 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7663 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7664 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7665 | `							 * in this class's ancestor chain.` |
|       - |  7666 | `							 */` |
|      11 |  7667 | `							int fromIface = 0;` |
|      11 |  7668 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7669 | `							while( pAnc ){` |
|       - |  7670 | `								ph7_class **apPI;` |
|       - |  7671 | `								sxu32 j;` |
|      13 |  7672 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7673 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7674 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7675 | `										fromIface = 1;` |
|       9 |  7676 | `										break;` |
|       - |  7677 | `									}` |
|     ! 0 |  7678 | `								}` |
|      13 |  7679 | `								if( fromIface ) break;` |
|       5 |  7680 | `								pAnc = pAnc->pBase;` |
|       1 |  7681 | `							}` |
|      11 |  7682 | `							if( !fromIface ){` |
|       3 |  7683 | `								pOrigin = pWalk;` |
|       3 |  7684 | `								break;` |
|       - |  7685 | `							}` |
|       4 |  7686 | `						}` |
|       9 |  7687 | `						pWalk = pWalk->pBase;` |
|       1 |  7688 | `					}` |
|       4 |  7689 | `				}` |
|       - |  7690 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7691 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7692 | `				 */` |
|      17 |  7693 | `				if( !pOrigin ){` |
|      15 |  7694 | `					pWalk = pClass;` |
|      37 |  7695 | `					while( pWalk && !pOrigin ){` |
|      23 |  7696 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7697 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7698 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7699 | `							ph7_class *pDeepest = 0;` |
|      25 |  7700 | `							while( pIface ){` |
|      13 |  7701 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7702 | `									pDeepest = pIface;` |
|       6 |  7703 | `								}` |
|      13 |  7704 | `								pIface = pIface->pBase;` |
|       1 |  7705 | `							}` |
|      13 |  7706 | `							if( pDeepest ){` |
|      13 |  7707 | `								pOrigin = pDeepest;` |
|      13 |  7708 | `								break;` |
|       - |  7709 | `							}` |
|     ! 0 |  7710 | `						}` |
|      23 |  7711 | `						pWalk = pWalk->pBase;` |
|       1 |  7712 | `					}` |
|       7 |  7713 | `				}` |
|       - |  7714 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7715 | `				if( !pOrigin ){` |
|       3 |  7716 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7717 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7718 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7719 | `							pOrigin = pClass;` |
|       3 |  7720 | `							break;` |
|       - |  7721 | `						}` |
|     ! 0 |  7722 | `					}` |
|       1 |  7723 | `				}` |
|       - |  7724 | `			}` |
|      17 |  7725 | `			if( pOrigin ){` |
|      17 |  7726 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7727 | `			}else{` |
|       - |  7728 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7729 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7730 | `			}` |
|      17 |  7731 | `			nListed++;` |
|       1 |  7732 | `		}` |
|       - |  7733 | `	}` |
|      15 |  7734 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7735 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7736 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7737 | `	SyBlobRelease(&sMsg);` |
|      15 |  7738 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7739 | `		return SXERR_ABORT;` |
|       - |  7740 | `	}` |
|      15 |  7741 | `	return SXRET_OK;` |
|   20799 |  7742 |  |
|       - |  7743 | `/*` |
|       - |  7744 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  7745 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  7746 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  7747 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  7748 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  7749 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  7750 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  7751 | ` */` |
|   32530 |  7752 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       2 |  7753 |  |
|   32532 |  7754 | `	int isAbsolute = 0;` |
|   32532 |  7755 | `	SyToken *pStart = pGen->pIn;` |
|       - |  7756 | `	SyBlob sName;` |
|   32532 |  7757 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      28 |  7758 | `		isAbsolute = 1;` |
|      28 |  7759 | `		pGen->pIn++;` |
|      13 |  7760 | `	}` |
|   32532 |  7761 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       7 |  7762 | `		pGen->pIn = pStart;` |
|       7 |  7763 | `		return SXERR_INVALID;` |
|       - |  7764 | `	}` |
|   32526 |  7765 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   32526 |  7766 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   32526 |  7767 | `	pGen->pIn++;` |
|   48804 |  7768 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   16282 |  7769 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  7770 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  7771 | `		pGen->pIn++;` |
|      13 |  7772 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  7773 | `		pGen->pIn++;` |
|       1 |  7774 | `	}` |
|   32526 |  7775 | `	if( isAbsolute ){` |
|      25 |  7776 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      13 |  7777 | `	}else{` |
|       - |  7778 | `		SyString sRaw;` |
|   32502 |  7779 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   32502 |  7780 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  7781 | `	}` |
|   32526 |  7782 | `	SyBlobRelease(&sName);` |
|   32526 |  7783 | `	return SXRET_OK;` |
|   16267 |  7784 |  |
|       - |  7785 | `/*` |
|       - |  7786 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  7787 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  7788 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  7789 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  7790 | ` * either direction cannot run unbounded.` |
|       - |  7791 | ` */` |
|       - |  7792 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    8802 |  7793 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       2 |  7794 |  |
|       - |  7795 | `	ph7_class **apParent;` |
|       - |  7796 | `	sxu32 n;` |
|   11766 |  7797 | `	while( pInterface ){` |
|    8810 |  7798 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  7799 | `			return FALSE;` |
|       - |  7800 | `		}` |
|   11736 |  7801 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    5852 |  7802 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    5848 |  7803 | `			return TRUE;` |
|       - |  7804 | `		}` |
|    2964 |  7805 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    2964 |  7806 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  7807 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  7808 | `				return TRUE;` |
|       - |  7809 | `			}` |
|     ! 0 |  7810 | `		}` |
|    2964 |  7811 | `		pInterface = pInterface->pBase;` |
|    2964 |  7812 | `		iDepth++;` |
|       2 |  7813 | `	}` |
|    2958 |  7814 | `	return FALSE;` |
|    4403 |  7815 |  |
|    8802 |  7816 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       2 |  7817 |  |
|    8804 |  7818 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       2 |  7819 |  |
|       - |  7820 | `/*` |
|       - |  7821 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  7822 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  7823 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  7824 | ` */` |
|    5846 |  7825 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       2 |  7826 |  |
|    5852 |  7827 | `	while( pBase ){` |
|      10 |  7828 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  7829 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  7830 | `			return TRUE;` |
|       - |  7831 | `		}` |
|      10 |  7832 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  7833 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  7834 | `			return TRUE;` |
|       - |  7835 | `		}` |
|       5 |  7836 | `		pBase = pBase->pBase;` |
|       1 |  7837 | `	}` |
|    5844 |  7838 | `	return FALSE;` |
|    2925 |  7839 |  |
|   41610 |  7840 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  7841 |  |
|   41612 |  7842 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7843 | `	ph7_class *pClass,*pBase;` |
|       - |  7844 | `	SyToken *pEnd,*pTmp;` |
|       - |  7845 | `	sxi32 iProtection;` |
|       - |  7846 | `	SySet aInterfaces;` |
|       - |  7847 | `	SySet aUseEntries;` |
|       - |  7848 | `	sxi32 iAttrflags;` |
|       - |  7849 | `	SyString *pName;` |
|       - |  7850 | `	sxi32 nKwrd;` |
|       - |  7851 | `	sxi32 rc;` |
|       - |  7852 | `	/* Jump the 'class' keyword */` |
|   41612 |  7853 | `	pGen->pIn++;` |
|   41612 |  7854 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7855 | `		/* Syntax error */` |
|     ! 0 |  7856 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  7857 | `		if( rc == SXERR_ABORT ){` |
|       - |  7858 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7859 | `			return SXERR_ABORT;` |
|       - |  7860 | `		}` |
|       - |  7861 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  7862 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  7863 | `			pGen->pIn++;` |
|     ! 0 |  7864 | `		}` |
|     ! 0 |  7865 | `		return SXRET_OK;` |
|       - |  7866 | `	}` |
|       - |  7867 | `	/* Extract class name */` |
|   41612 |  7868 | `	pName = &pGen->pIn->sData;` |
|       - |  7869 | `	/* Advance the stream cursor */` |
|   41612 |  7870 | `	pGen->pIn++;` |
|       - |  7871 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7872 | `		SyBlob sFQN;` |
|       - |  7873 | `		SyString sFQNStr;` |
|   41612 |  7874 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   41612 |  7875 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   41612 |  7876 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   41612 |  7877 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   41612 |  7878 | `		SyBlobRelease(&sFQN);` |
|       - |  7879 | `	}` |
|   41612 |  7880 | `	if( pClass == 0 ){` |
|     ! 0 |  7881 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7882 | `		return SXERR_ABORT;` |
|       - |  7883 | `	}` |
|       - |  7884 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   41612 |  7885 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   41612 |  7886 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  7887 | `	/* Assume a standalone class */` |
|   41612 |  7888 | `	pBase = 0;` |
|   41612 |  7889 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   32280 |  7890 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   32280 |  7891 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  7892 | `			SyBlob sResolved;` |
|       - |  7893 | `			SyString sBaseName;` |
|       - |  7894 | `			sxu32 nRefLine;` |
|   23484 |  7895 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   23484 |  7896 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   23484 |  7897 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   23484 |  7898 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  7899 | `				SyBlobRelease(&sResolved);` |
|       4 |  7900 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7901 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  7902 | `					pName);` |
|       3 |  7903 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  7904 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7905 | `					return SXERR_ABORT;` |
|       - |  7906 | `				}` |
|       3 |  7907 | `				return SXRET_OK;` |
|       - |  7908 | `			}` |
|   35222 |  7909 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   23480 |  7910 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   23482 |  7911 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7912 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7913 | `			/* Interfaces are not allowed */` |
|   23482 |  7914 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  7915 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7916 | `			}` |
|   23482 |  7917 | `			if( pBase == 0 ){` |
|     ! 0 |  7918 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7919 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  7920 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7921 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7922 | `					return SXERR_ABORT;` |
|       - |  7923 | `				}` |
|     ! 0 |  7924 | `			}else{` |
|   23482 |  7925 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  7926 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7927 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  7928 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7929 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  7930 | `						return SXERR_ABORT;` |
|       - |  7931 | `					}` |
|     ! 0 |  7932 | `				}` |
|       - |  7933 | `			}` |
|   23482 |  7934 | `			SyBlobRelease(&sResolved);` |
|   11740 |  7935 | `		}` |
|   32278 |  7936 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  7937 | `			ph7_class *pInterface;` |
|       - |  7938 | `			/* Interface implementation */` |
|    8804 |  7939 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4401 |  7940 | `			for(;;){` |
|       - |  7941 | `				SyBlob sResolved;` |
|       - |  7942 | `				SyString sIntName;` |
|       - |  7943 | `				sxu32 nRefLine;` |
|    8804 |  7944 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    8804 |  7945 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    8804 |  7946 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7947 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7948 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7949 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  7950 | `						pName);` |
|     ! 0 |  7951 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7952 | `						return SXERR_ABORT;` |
|       - |  7953 | `					}` |
|     ! 0 |  7954 | `					break;` |
|       - |  7955 | `				}` |
|   17606 |  7956 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    8802 |  7957 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    8804 |  7958 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  7959 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7960 | `				/* Only interfaces are allowed */` |
|    8804 |  7961 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7962 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  7963 | `				}` |
|    8804 |  7964 | `				if( pInterface == 0 ){` |
|     ! 0 |  7965 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7966 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  7967 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7968 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  7969 | `						return SXERR_ABORT;` |
|       - |  7970 | `					}` |
|     ! 0 |  7971 | `				}else{` |
|       - |  7972 | `					/* Reject user classes that try to implement Throwable` |
|       - |  7973 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  7974 | `					 * unless they already extend Exception or Error.` |
|       - |  7975 | `					 * Exception and Error themselves are compiled from the` |
|       - |  7976 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  7977 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    8804 |  7978 | `					SyString *pFqn = &pClass->sName;` |
|    8804 |  7979 | `					int bIsExceptionOrError =` |
|    7319 |  7980 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   14662 |  7981 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    7346 |  7982 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    2924 |  7983 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14646 |  7984 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    8769 |  7985 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    2921 |  7986 | `						!bIsExceptionOrError ){` |
|      10 |  7987 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7988 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  7989 | `							&pClass->sName);` |
|       7 |  7990 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7991 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  7992 | `							return SXERR_ABORT;` |
|       - |  7993 | `						}` |
|       - |  7994 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  7995 | `						 * check does not produce a duplicate fatal. */` |
|       4 |  7996 | `					}else{` |
|    8798 |  7997 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  7998 | `					}` |
|       - |  7999 | `				}` |
|    8804 |  8000 | `				SyBlobRelease(&sResolved);` |
|    8804 |  8001 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4403 |  8002 | `					break;` |
|       - |  8003 | `				}` |
|     ! 0 |  8004 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8005 | `			}` |
|    4401 |  8006 | `		}` |
|   16138 |  8007 | `	}` |
|   41610 |  8008 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8009 | `		/* Syntax error */` |
|     ! 0 |  8010 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8011 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8012 | `		if( rc == SXERR_ABORT ){` |
|       - |  8013 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8014 | `			return SXERR_ABORT;` |
|       - |  8015 | `		}` |
|     ! 0 |  8016 | `		return SXRET_OK;` |
|       - |  8017 | `	}` |
|   41610 |  8018 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   41610 |  8019 | `	pEnd = 0; /* cc warning */` |
|       - |  8020 | `	/* Delimit the class body */` |
|   41610 |  8021 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   41610 |  8022 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8023 | `		/* Syntax error */` |
|     ! 0 |  8024 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8025 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8026 | `		if( rc == SXERR_ABORT ){` |
|       - |  8027 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8028 | `			return SXERR_ABORT;` |
|       - |  8029 | `		}` |
|     ! 0 |  8030 | `		return SXRET_OK;` |
|       - |  8031 | `	}` |
|       - |  8032 | `	/* Swap token stream */` |
|   41610 |  8033 | `	pTmp = pGen->pEnd;` |
|   41610 |  8034 | `	pGen->pEnd = pEnd;` |
|       - |  8035 | `	/* Set the inherited flags */` |
|   41610 |  8036 | `	pClass->iFlags = iFlags;` |
|       - |  8037 | `	/* Start the parse process */` |
|   92547 |  8038 | `	for(;;){` |
|       - |  8039 | `		/* Jump leading/trailing semi-colons */` |
|  296908 |  8040 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   55926 |  8041 | `			pGen->pIn++;` |
|       2 |  8042 | `		}` |
|  240984 |  8043 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8044 | `			/* End of class body */` |
|   41596 |  8045 | `			break;` |
|       - |  8046 | `		}` |
|  199390 |  8047 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8048 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8049 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8050 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8051 | `			if( rc == SXERR_ABORT ){` |
|       - |  8052 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8053 | `				return SXERR_ABORT;` |
|       - |  8054 | `			}` |
|     ! 0 |  8055 | `			goto done;` |
|       - |  8056 | `		}` |
|       - |  8057 | `		/* Assume public visibility */` |
|  199390 |  8058 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  199390 |  8059 | `		iAttrflags = 0;` |
|  199390 |  8060 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8061 | `			/* Extract the current keyword */` |
|  199390 |  8062 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  199390 |  8063 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8064 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8065 | `				TraitUseEntry sUse;` |
|      44 |  8066 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  8067 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  8068 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  8069 | `				for(;;){` |
|       - |  8070 | `					ph7_class *pTrait;` |
|       - |  8071 | `					SyString *pTraitName;` |
|      52 |  8072 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8073 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8074 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8075 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8076 | `							return SXERR_ABORT;` |
|       - |  8077 | `						}` |
|     ! 0 |  8078 | `						break;` |
|       - |  8079 | `					}` |
|      52 |  8080 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8081 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8082 | `						SyBlob sResolved;` |
|      52 |  8083 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  8084 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  8085 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  8086 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  8087 | `						SyBlobRelease(&sResolved);` |
|       - |  8088 | `					}` |
|       - |  8089 | `					/* Only traits are allowed */` |
|      52 |  8090 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8091 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8092 | `					}` |
|      52 |  8093 | `					if( pTrait == 0 ){` |
|     ! 0 |  8094 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8095 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8096 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8097 | `							return SXERR_ABORT;` |
|       - |  8098 | `						}` |
|     ! 0 |  8099 | `					}else{` |
|      52 |  8100 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8101 | `					}` |
|      52 |  8102 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  8103 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  8104 | `						break;` |
|       - |  8105 | `					}` |
|       9 |  8106 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  8107 | `				}` |
|       - |  8108 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  8109 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8110 | `					SyToken *pBlock;` |
|       9 |  8111 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  8112 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  8113 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  8114 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  8115 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  8116 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  8117 | `					}else{` |
|     ! 0 |  8118 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8119 | `					}` |
|       4 |  8120 | `				}` |
|      44 |  8121 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8122 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  8123 | `				continue;` |
|       - |  8124 | `			}` |
|  199348 |  8125 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  196310 |  8126 | `				iProtection = nKwrd;` |
|  196310 |  8127 | `				pGen->pIn++; /* Jump the visibility token */` |
|  196308 |  8128 | `				if( pGen->pIn >= pGen->pEnd` |
|  196310 |  8129 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8130 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8131 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8132 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8133 | `					if( rc == SXERR_ABORT ){` |
|       - |  8134 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8135 | `						return SXERR_ABORT;` |
|       - |  8136 | `					}` |
|     ! 0 |  8137 | `					goto done;` |
|       - |  8138 | `				}` |
|  196310 |  8139 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8140 | `					/* Attribute declaration (untyped) */` |
|   55736 |  8141 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   55736 |  8142 | `					if( rc != SXRET_OK ){` |
|       3 |  8143 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8144 | `							return SXERR_ABORT;` |
|       - |  8145 | `						}` |
|       3 |  8146 | `						goto done;` |
|       - |  8147 | `					}` |
|   55734 |  8148 | `					continue;` |
|       - |  8149 | `				}` |
|  140576 |  8150 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8151 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     106 |  8152 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     106 |  8153 | `					if( rc != SXRET_OK ){` |
|       3 |  8154 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8155 | `							return SXERR_ABORT;` |
|       - |  8156 | `						}` |
|       3 |  8157 | `						goto done;` |
|       - |  8158 | `					}` |
|     104 |  8159 | `					continue;` |
|       - |  8160 | `				}` |
|       - |  8161 | `				/* Extract the keyword */` |
|  140472 |  8162 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   70235 |  8163 | `			}` |
|  143510 |  8164 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8165 | `				/* Process constant declaration */` |
|      30 |  8166 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  8167 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8168 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8169 | `						return SXERR_ABORT;` |
|       - |  8170 | `					}` |
|     ! 0 |  8171 | `					goto done;` |
|       - |  8172 | `				}` |
|      16 |  8173 | `			}else{` |
|  143482 |  8174 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8175 | `					/* Static method or attribute,record that */` |
|    2958 |  8176 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2958 |  8177 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2958 |  8178 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8179 | `						/* Extract the keyword */` |
|    2952 |  8180 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2952 |  8181 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8182 | `							iProtection = nKwrd;` |
|     ! 0 |  8183 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8184 | `						}` |
|    1475 |  8185 | `					}` |
|    2956 |  8186 | `					if( pGen->pIn >= pGen->pEnd` |
|    2958 |  8187 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8188 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8189 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8190 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8191 | `						if( rc == SXERR_ABORT ){` |
|       - |  8192 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8193 | `							return SXERR_ABORT;` |
|       - |  8194 | `						}` |
|     ! 0 |  8195 | `						goto done;` |
|       - |  8196 | `					}` |
|    2958 |  8197 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8198 | `						/* Attribute declaration */` |
|       5 |  8199 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8200 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8201 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8202 | `								return SXERR_ABORT;` |
|       - |  8203 | `							}` |
|     ! 0 |  8204 | `							goto done;` |
|       - |  8205 | `						}` |
|       5 |  8206 | `						continue;` |
|       - |  8207 | `					}` |
|    2954 |  8208 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8209 | `						/* Typed static attribute declaration */` |
|      10 |  8210 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 |  8211 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8212 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8213 | `								return SXERR_ABORT;` |
|       - |  8214 | `							}` |
|     ! 0 |  8215 | `							goto done;` |
|       - |  8216 | `						}` |
|      10 |  8217 | `						continue;` |
|       - |  8218 | `					}` |
|       - |  8219 | `					/* Extract the keyword */` |
|    2946 |  8220 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  141998 |  8221 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8222 | `					/* Abstract method,record that */` |
|      12 |  8223 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8224 | `					/* Mark the whole class as abstract */` |
|      12 |  8225 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8226 | `					/* Advance the stream cursor */` |
|      12 |  8227 | `					pGen->pIn++;` |
|      12 |  8228 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8229 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8230 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8231 | `							iProtection = nKwrd;` |
|      10 |  8232 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8233 | `						}` |
|       5 |  8234 | `					}` |
|      12 |  8235 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8236 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8237 | `							/* Static method */` |
|     ! 0 |  8238 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8239 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8240 | `					}` |
|      12 |  8241 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8242 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8243 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8244 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8245 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8246 | `							if( rc == SXERR_ABORT ){` |
|       - |  8247 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8248 | `								return SXERR_ABORT;` |
|       - |  8249 | `							}` |
|     ! 0 |  8250 | `							goto done;` |
|       - |  8251 | `					}` |
|      12 |  8252 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  140521 |  8253 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8254 | `					/* final method ,record that */` |
|       5 |  8255 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  8256 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  8257 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8258 | `						/* Extract the keyword */` |
|       5 |  8259 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8260 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8261 | `							iProtection = nKwrd;` |
|       5 |  8262 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8263 | `						}` |
|       2 |  8264 | `					}` |
|       5 |  8265 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8266 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8267 | `							/* Static method */` |
|     ! 0 |  8268 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8269 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8270 | `					}` |
|       5 |  8271 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8272 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8273 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8274 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8275 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8276 | `							if( rc == SXERR_ABORT ){` |
|       - |  8277 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8278 | `								return SXERR_ABORT;` |
|       - |  8279 | `							}` |
|     ! 0 |  8280 | `							goto done;` |
|       - |  8281 | `					}` |
|       5 |  8282 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8283 | `				}` |
|  143470 |  8284 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8285 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8286 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8287 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8288 | `						if( rc == SXERR_ABORT ){` |
|       - |  8289 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8290 | `							return SXERR_ABORT;` |
|       - |  8291 | `						}` |
|     ! 0 |  8292 | `						goto done;` |
|       - |  8293 | `				}` |
|  143470 |  8294 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8295 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8296 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8297 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8298 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8299 | `						if( rc == SXERR_ABORT ){` |
|       - |  8300 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8301 | `							return SXERR_ABORT;` |
|       - |  8302 | `						}` |
|     ! 0 |  8303 | `						goto done;` |
|       - |  8304 | `					}` |
|       - |  8305 | `					/* Attribute declaration */` |
|       7 |  8306 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8307 | `				}else{` |
|       - |  8308 | `					/* Process method declaration */` |
|  143464 |  8309 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8310 | `				}` |
|  143470 |  8311 | `				if( rc != SXRET_OK ){` |
|      11 |  8312 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8313 | `						return SXERR_ABORT;` |
|       - |  8314 | `					}` |
|      11 |  8315 | `					goto done;` |
|       - |  8316 | `				}` |
|       - |  8317 | `			}` |
|   71745 |  8318 | `		}else{` |
|       - |  8319 | `			/* Attribute declaration */` |
|     ! 0 |  8320 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8321 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8322 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8323 | `					return SXERR_ABORT;` |
|       - |  8324 | `				}` |
|     ! 0 |  8325 | `				goto done;` |
|       - |  8326 | `			}` |
|       - |  8327 | `		}` |
|       2 |  8328 | `	}` |
|       - |  8329 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8330 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8331 | `	 */` |
|       - |  8332 | `	{` |
|       - |  8333 | `		TraitUseEntry *apUse;` |
|       - |  8334 | `		sxu32 nU;` |
|   41596 |  8335 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   41638 |  8336 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  8337 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  8338 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  8339 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  8340 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8341 | `			sxu32 nT;` |
|      44 |  8342 | `			if( !hasResolution ){` |
|       - |  8343 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  8344 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  8345 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  8346 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8347 | `						break;` |
|       - |  8348 | `					}` |
|      22 |  8349 | `				}` |
|      19 |  8350 | `			}else{` |
|       - |  8351 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8352 | `				 * then use the block to resolve method conflicts.` |
|       - |  8353 | `				 */` |
|       - |  8354 | `				SyToken *pR;` |
|      19 |  8355 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  8356 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8357 | `					ph7_class_attr *pAR;` |
|       - |  8358 | `					SyHashEntry *pER;` |
|       - |  8359 | `					SyString *pNR;` |
|      11 |  8360 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  8361 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8362 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8363 | `						pNR = &pAR->sName;` |
|     ! 0 |  8364 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8365 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8366 | `						}` |
|     ! 0 |  8367 | `					}` |
|      11 |  8368 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  8369 | `				}` |
|       - |  8370 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  8371 | `				pR = pUse->pResolvStart;` |
|      21 |  8372 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8373 | `					SyString sTrait,sMethod;` |
|       - |  8374 | `					ph7_class *pSrcTrait;` |
|       - |  8375 | `					ph7_class_method *pMeth;` |
|       - |  8376 | `					sxi32 nRKwrd;` |
|      33 |  8377 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8378 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8379 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8380 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8381 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8382 | `					sMethod = pR->sData;` |
|      13 |  8383 | `					pR++;` |
|      13 |  8384 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8385 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8386 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8387 | `							sTrait = sMethod;` |
|       7 |  8388 | `							pR++;` |
|       7 |  8389 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8390 | `							sMethod = pR->sData;` |
|       7 |  8391 | `							pR++;` |
|       3 |  8392 | `						}` |
|       3 |  8393 | `					}` |
|      13 |  8394 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8395 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8396 | `						continue;` |
|       - |  8397 | `					}` |
|      13 |  8398 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8399 | `					pR++;` |
|      13 |  8400 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8401 | `						pSrcTrait = 0;` |
|       7 |  8402 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8403 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8404 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8405 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8406 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8407 | `								break;` |
|       - |  8408 | `							}` |
|       2 |  8409 | `						}` |
|       5 |  8410 | `						if( pSrcTrait ){` |
|       5 |  8411 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8412 | `							if( pMeth ){` |
|       5 |  8413 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8414 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8415 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8416 | `								}` |
|       2 |  8417 | `							}` |
|       2 |  8418 | `						}` |
|       2 |  8419 | `					}` |
|      29 |  8420 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8421 | `				}` |
|       - |  8422 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8423 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8424 | `					ph7_class_method *pMR;` |
|       - |  8425 | `					SyHashEntry *pER;` |
|       - |  8426 | `					SyString *pNR;` |
|      11 |  8427 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8428 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8429 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8430 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8431 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8432 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8433 | `						}` |
|       1 |  8434 | `					}` |
|       6 |  8435 | `				}` |
|       - |  8436 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8437 | `				pR = pUse->pResolvStart;` |
|      21 |  8438 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8439 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8440 | `					ph7_class *pSrcTrait;` |
|       - |  8441 | `					ph7_class_method *pMeth;` |
|      21 |  8442 | `					int hasQual = 0;` |
|       - |  8443 | `					sxi32 nRKwrd;` |
|      33 |  8444 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8445 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8446 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8447 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8448 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8449 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8450 | `					sMethod = pR->sData;` |
|      13 |  8451 | `					pR++;` |
|      13 |  8452 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8453 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8454 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8455 | `							sTrait = sMethod;` |
|       7 |  8456 | `							hasQual = 1;` |
|       7 |  8457 | `							pR++;` |
|       7 |  8458 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8459 | `							sMethod = pR->sData;` |
|       7 |  8460 | `							pR++;` |
|       3 |  8461 | `						}` |
|       3 |  8462 | `					}` |
|      13 |  8463 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8464 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8465 | `						continue;` |
|       - |  8466 | `					}` |
|      13 |  8467 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8468 | `					pR++;` |
|      13 |  8469 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8470 | `						sxi32 iNewVis = -1;` |
|       9 |  8471 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8472 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8473 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8474 | `								iNewVis = nAK;` |
|       7 |  8475 | `								pR++;` |
|       3 |  8476 | `							}` |
|       3 |  8477 | `						}` |
|       9 |  8478 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8479 | `							sAlias = pR->sData;` |
|       7 |  8480 | `							pR++;` |
|       3 |  8481 | `						}` |
|       9 |  8482 | `						pMeth = 0;` |
|       9 |  8483 | `						if( hasQual ){` |
|       3 |  8484 | `							pSrcTrait = 0;` |
|       5 |  8485 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8486 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8487 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8488 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8489 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8490 | `									break;` |
|       - |  8491 | `								}` |
|       2 |  8492 | `							}` |
|       3 |  8493 | `							if( pSrcTrait ){` |
|       3 |  8494 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8495 | `							}` |
|       2 |  8496 | `						}else{` |
|       7 |  8497 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8498 | `						}` |
|       9 |  8499 | `						if( pMeth ){` |
|       9 |  8500 | `							if( sAlias.nByte > 0 ){` |
|       - |  8501 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8502 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8503 | `								 */` |
|       - |  8504 | `								ph7_class_method *pAlias;` |
|       - |  8505 | `								char *zAliasDup;` |
|       7 |  8506 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8507 | `								if( pAlias ){` |
|       7 |  8508 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8509 | `									if( iNewVis >= 0 ){` |
|       5 |  8510 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8511 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8512 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8513 | `									}` |
|       7 |  8514 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8515 | `									if( zAliasDup ){` |
|       7 |  8516 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8517 | `									}` |
|       4 |  8518 | `								}` |
|       6 |  8519 | `							}else if( iNewVis >= 0 ){` |
|       - |  8520 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8521 | `								ph7_class_method *pCopy;` |
|       3 |  8522 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8523 | `								if( pCopy ){` |
|       3 |  8524 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8525 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8526 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8527 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8528 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8529 | `									/* Replace the method in the class hash */` |
|       3 |  8530 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8531 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8532 | `								}` |
|       1 |  8533 | `							}` |
|       4 |  8534 | `						}` |
|       4 |  8535 | `						SXUNUSED(hasQual);` |
|       4 |  8536 | `					}` |
|      17 |  8537 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8538 | `				}` |
|       - |  8539 | `			}` |
|      44 |  8540 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  8541 | `		}` |
|       - |  8542 | `	}` |
|       - |  8543 | `	/* Install the class */` |
|   41596 |  8544 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   41596 |  8545 | `	if( rc == SXRET_OK ){` |
|       - |  8546 | `		ph7_class **apInterface;` |
|       - |  8547 | `		sxu32 n;` |
|   41596 |  8548 | `		if( pBase ){` |
|       - |  8549 | `			/* Inherit from base class and mark as a subclass */` |
|   23482 |  8550 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   11740 |  8551 | `		}` |
|   41596 |  8552 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   50392 |  8553 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8554 | `			/* Implements one or more interface */` |
|    8798 |  8555 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    8798 |  8556 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8557 | `				break;` |
|       - |  8558 | `			}` |
|    4400 |  8559 | `		}` |
|       - |  8560 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   41596 |  8561 | `		if( rc == SXRET_OK ){` |
|   41596 |  8562 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   41596 |  8563 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8564 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8565 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8566 | `				return SXERR_ABORT;` |
|       - |  8567 | `			}` |
|   20797 |  8568 | `		}` |
|       - |  8569 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   41596 |  8570 | `		if( rc == SXRET_OK ){` |
|   41596 |  8571 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   41596 |  8572 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8573 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8574 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8575 | `				return SXERR_ABORT;` |
|       - |  8576 | `			}` |
|   20797 |  8577 | `		}` |
|   20797 |  8578 | `	}` |
|   41596 |  8579 | `	SySetRelease(&aUseEntries);` |
|   41596 |  8580 | `	SySetRelease(&aInterfaces);` |
|   41596 |  8581 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8582 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8583 | `		return SXERR_ABORT;` |
|       - |  8584 | `	}` |
|   20797 |  8585 | `done:` |
|       - |  8586 | `	/* Point beyond the class body */` |
|   41610 |  8587 | `	pGen->pIn = &pEnd[1];` |
|   41610 |  8588 | `	pGen->pEnd = pTmp;` |
|   41610 |  8589 | `	return PH7_OK;` |
|   20807 |  8590 |  |
|       - |  8591 | `/*` |
|       - |  8592 | ` * Compile a user-defined abstract class.` |
|       - |  8593 | ` *  According to the PHP language reference manual` |
|       - |  8594 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8595 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8596 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8597 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8598 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8599 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8600 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8601 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8602 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8603 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8604 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8605 | ` *   could differ.` |
|       - |  8606 | ` */` |
|      18 |  8607 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8608 |  |
|       - |  8609 | `	sxi32 rc;` |
|      20 |  8610 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      20 |  8611 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      20 |  8612 | `	return rc;` |
|       2 |  8613 |  |
|       - |  8614 | `/*` |
|       - |  8615 | ` * Compile a user-defined final class.` |
|       - |  8616 | ` *  According to the PHP language reference manual` |
|       - |  8617 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8618 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8619 | ` *    final then it cannot be extended.` |
|       - |  8620 | ` */` |
|       2 |  8621 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8622 |  |
|       - |  8623 | `	sxi32 rc;` |
|       3 |  8624 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8625 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8626 | `	return rc;` |
|       1 |  8627 |  |
|       - |  8628 | `/*` |
|       - |  8629 | ` * Compile a user-defined trait.` |
|       - |  8630 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8631 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8632 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8633 | ` */` |
|      54 |  8634 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8635 |  |
|      56 |  8636 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8637 | `	ph7_class *pClass;` |
|       - |  8638 | `	SyToken *pEnd,*pTmp;` |
|       - |  8639 | `	sxi32 iProtection;` |
|       - |  8640 | `	sxi32 iAttrflags;` |
|       - |  8641 | `	SyString *pName;` |
|       - |  8642 | `	sxi32 nKwrd;` |
|       - |  8643 | `	sxi32 rc;` |
|       - |  8644 | `	/* Jump the 'trait' keyword */` |
|      56 |  8645 | `	pGen->pIn++;` |
|      56 |  8646 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8647 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8648 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8649 | `			return SXERR_ABORT;` |
|       - |  8650 | `		}` |
|     ! 0 |  8651 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8652 | `			pGen->pIn++;` |
|     ! 0 |  8653 | `		}` |
|     ! 0 |  8654 | `		return SXRET_OK;` |
|       - |  8655 | `	}` |
|       - |  8656 | `	/* Extract trait name */` |
|      56 |  8657 | `	pName = &pGen->pIn->sData;` |
|      56 |  8658 | `	pGen->pIn++;` |
|       - |  8659 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8660 | `		SyBlob sFQN;` |
|       - |  8661 | `		SyString sFQNStr;` |
|      56 |  8662 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8663 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8664 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8665 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8666 | `		SyBlobRelease(&sFQN);` |
|       - |  8667 | `	}` |
|      56 |  8668 | `	if( pClass == 0 ){` |
|     ! 0 |  8669 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8670 | `		return SXERR_ABORT;` |
|       - |  8671 | `	}` |
|       - |  8672 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8673 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8674 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8675 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8676 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8677 | `			return SXERR_ABORT;` |
|       - |  8678 | `		}` |
|     ! 0 |  8679 | `		return SXRET_OK;` |
|       - |  8680 | `	}` |
|      56 |  8681 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8682 | `	pEnd = 0;` |
|      56 |  8683 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8684 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8685 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8686 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8687 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8688 | `			return SXERR_ABORT;` |
|       - |  8689 | `		}` |
|     ! 0 |  8690 | `		return SXRET_OK;` |
|       - |  8691 | `	}` |
|       - |  8692 | `	/* Swap token stream */` |
|      56 |  8693 | `	pTmp = pGen->pEnd;` |
|      56 |  8694 | `	pGen->pEnd = pEnd;` |
|       - |  8695 | `	/* Mark as trait */` |
|      56 |  8696 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8697 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8698 | `	for(;;){` |
|     154 |  8699 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8700 | `			pGen->pIn++;` |
|       2 |  8701 | `		}` |
|     130 |  8702 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8703 | `			break;` |
|       - |  8704 | `		}` |
|      76 |  8705 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8706 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8707 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8708 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8709 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8710 | `				return SXERR_ABORT;` |
|       - |  8711 | `			}` |
|     ! 0 |  8712 | `			goto done;` |
|       - |  8713 | `		}` |
|      76 |  8714 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8715 | `		iAttrflags = 0;` |
|      76 |  8716 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8717 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8718 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8719 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8720 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8721 | `				for(;;){` |
|       - |  8722 | `					ph7_class *pUsedTrait;` |
|       - |  8723 | `					SyString *pUsedName;` |
|       5 |  8724 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8725 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8726 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8727 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8728 | `							return SXERR_ABORT;` |
|       - |  8729 | `						}` |
|     ! 0 |  8730 | `						break;` |
|       - |  8731 | `					}` |
|       5 |  8732 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8733 | `					{` |
|       - |  8734 | `						SyBlob sResolved;` |
|       5 |  8735 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8736 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8737 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8738 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8739 | `						SyBlobRelease(&sResolved);` |
|       - |  8740 | `					}` |
|       5 |  8741 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8742 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8743 | `					}` |
|       5 |  8744 | `					if( pUsedTrait == 0 ){` |
|       4 |  8745 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8746 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8747 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8748 | `							return SXERR_ABORT;` |
|       - |  8749 | `						}` |
|       2 |  8750 | `					}else{` |
|       3 |  8751 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8752 | `					}` |
|       5 |  8753 | `					pGen->pIn++;` |
|       5 |  8754 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8755 | `						break;` |
|       - |  8756 | `					}` |
|     ! 0 |  8757 | `					pGen->pIn++;` |
|     ! 0 |  8758 | `				}` |
|       5 |  8759 | `				continue;` |
|       - |  8760 | `			}` |
|      72 |  8761 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8762 | `				iProtection = nKwrd;` |
|      68 |  8763 | `				pGen->pIn++;` |
|      66 |  8764 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8765 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8766 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8767 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8768 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8769 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8770 | `						return SXERR_ABORT;` |
|       - |  8771 | `					}` |
|     ! 0 |  8772 | `					goto done;` |
|       - |  8773 | `				}` |
|      68 |  8774 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8775 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8776 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8777 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8778 | `							return SXERR_ABORT;` |
|       - |  8779 | `						}` |
|     ! 0 |  8780 | `						goto done;` |
|       - |  8781 | `					}` |
|      11 |  8782 | `					continue;` |
|       - |  8783 | `				}` |
|      58 |  8784 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8785 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8786 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8787 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8788 | `							return SXERR_ABORT;` |
|       - |  8789 | `						}` |
|     ! 0 |  8790 | `						goto done;` |
|       - |  8791 | `					}` |
|       5 |  8792 | `					continue;` |
|       - |  8793 | `				}` |
|      53 |  8794 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8795 | `			}` |
|      57 |  8796 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8797 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8798 | `					"Traits cannot have constants");` |
|     ! 0 |  8799 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8800 | `					return SXERR_ABORT;` |
|       - |  8801 | `				}` |
|     ! 0 |  8802 | `				goto done;` |
|     ! 0 |  8803 | `			}else{` |
|      57 |  8804 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8805 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8806 | `					pGen->pIn++;` |
|       5 |  8807 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8808 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8809 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8810 | `							iProtection = nKwrd;` |
|     ! 0 |  8811 | `							pGen->pIn++;` |
|     ! 0 |  8812 | `						}` |
|       1 |  8813 | `					}` |
|       4 |  8814 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8815 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8816 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8817 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8818 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8819 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8820 | `							return SXERR_ABORT;` |
|       - |  8821 | `						}` |
|     ! 0 |  8822 | `						goto done;` |
|       - |  8823 | `					}` |
|       5 |  8824 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8825 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8826 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8827 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8828 | `								return SXERR_ABORT;` |
|       - |  8829 | `							}` |
|     ! 0 |  8830 | `							goto done;` |
|       - |  8831 | `						}` |
|       3 |  8832 | `						continue;` |
|       - |  8833 | `					}` |
|       3 |  8834 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8835 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8836 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8837 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8838 | `								return SXERR_ABORT;` |
|       - |  8839 | `							}` |
|     ! 0 |  8840 | `							goto done;` |
|       - |  8841 | `						}` |
|     ! 0 |  8842 | `						continue;` |
|       - |  8843 | `					}` |
|       3 |  8844 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  8845 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  8846 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  8847 | `					pGen->pIn++;` |
|       5 |  8848 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  8849 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8850 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8851 | `							iProtection = nKwrd;` |
|       5 |  8852 | `							pGen->pIn++;` |
|       2 |  8853 | `						}` |
|       2 |  8854 | `					}` |
|       5 |  8855 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8856 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8857 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8858 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  8859 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8860 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8861 | `							return SXERR_ABORT;` |
|       - |  8862 | `						}` |
|     ! 0 |  8863 | `						goto done;` |
|       - |  8864 | `					}` |
|       5 |  8865 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8866 | `				}` |
|      55 |  8867 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8868 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8869 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  8870 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8871 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8872 | `						return SXERR_ABORT;` |
|       - |  8873 | `					}` |
|     ! 0 |  8874 | `					goto done;` |
|       - |  8875 | `				}` |
|      55 |  8876 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  8877 | `					pGen->pIn++;` |
|     ! 0 |  8878 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8879 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8880 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8881 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8882 | `							return SXERR_ABORT;` |
|       - |  8883 | `						}` |
|     ! 0 |  8884 | `						goto done;` |
|       - |  8885 | `					}` |
|     ! 0 |  8886 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8887 | `				}else{` |
|      55 |  8888 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8889 | `				}` |
|      55 |  8890 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8891 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8892 | `						return SXERR_ABORT;` |
|       - |  8893 | `					}` |
|     ! 0 |  8894 | `					goto done;` |
|       - |  8895 | `				}` |
|       - |  8896 | `			}` |
|      28 |  8897 | `		}else{` |
|     ! 0 |  8898 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8899 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8900 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8901 | `					return SXERR_ABORT;` |
|       - |  8902 | `				}` |
|     ! 0 |  8903 | `				goto done;` |
|       - |  8904 | `			}` |
|       - |  8905 | `		}` |
|       1 |  8906 | `	}` |
|       - |  8907 | `	/* Install the trait */` |
|      56 |  8908 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  8909 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8910 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8911 | `		return SXERR_ABORT;` |
|       - |  8912 | `	}` |
|      27 |  8913 | `done:` |
|       - |  8914 | `	/* Point beyond the trait body */` |
|      56 |  8915 | `	pGen->pIn = &pEnd[1];` |
|      56 |  8916 | `	pGen->pEnd = pTmp;` |
|      56 |  8917 | `	return PH7_OK;` |
|      29 |  8918 |  |
|       - |  8919 | `/*` |
|       - |  8920 | ` * Compile a user-defined class.` |
|       - |  8921 | ` *  According to the PHP language reference manual` |
|       - |  8922 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  8923 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  8924 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  8925 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  8926 | ` *   and functions (called "methods").` |
|       - |  8927 | ` */` |
|   41590 |  8928 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  8929 |  |
|       - |  8930 | `	sxi32 rc;` |
|   41592 |  8931 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   41592 |  8932 | `	return rc;` |
|       2 |  8933 |  |
|       - |  8934 | `/*` |
|       - |  8935 | ` * Exception handling.` |
|       - |  8936 | ` *  According to the PHP language reference manual` |
|       - |  8937 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  8938 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  8939 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  8940 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  8941 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  8942 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  8943 | ` *    (or re-thrown) within a catch block.` |
|       - |  8944 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  8945 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  8946 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  8947 | ` *    been defined with set_exception_handler().` |
|       - |  8948 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  8949 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  8950 | ` */` |
|       - |  8951 | `/*` |
|       - |  8952 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  8953 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  8954 | ` * indicates failure.` |
|       - |  8955 | ` */` |
|    8872 |  8956 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  8957 |  |
|    8874 |  8958 | `	sxi32 rc = SXRET_OK;` |
|    8874 |  8959 | `	if( pRoot->pOp ){` |
|    8866 |  8960 | `		switch( pRoot->pOp->iOp ){` |
|    4432 |  8961 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  8962 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  8963 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  8964 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  8965 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  8966 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    8866 |  8967 | `			break;` |
|     ! 0 |  8968 | `		default:` |
|       - |  8969 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  8970 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  8971 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  8972 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8973 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  8974 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  8975 | `				rc = SXERR_INVALID;` |
|     ! 0 |  8976 | `			}` |
|     ! 0 |  8977 | `			break;` |
|       - |  8978 | `		}` |
|    4442 |  8979 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  8980 | `		/* Unexpected expression */` |
|     ! 0 |  8981 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8982 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  8983 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  8984 | `			rc = SXERR_INVALID;` |
|     ! 0 |  8985 | `		}` |
|     ! 0 |  8986 | `	}` |
|    8874 |  8987 | `	return rc;` |
|       2 |  8988 |  |
|       - |  8989 | `/*` |
|       - |  8990 | ` * Compile a 'throw' statement.` |
|       - |  8991 | ` * throw: This is how you trigger an exception.` |
|       - |  8992 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  8993 | ` */` |
|    8836 |  8994 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  8995 |  |
|    8838 |  8996 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8997 | `	GenBlock *pBlock;` |
|       - |  8998 | `	sxu32 nIdx;` |
|       - |  8999 | `	sxi32 rc;` |
|    8838 |  9000 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9001 | `	/* Compile the expression */` |
|    8838 |  9002 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8838 |  9003 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9004 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9005 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9006 | `			return SXERR_ABORT;` |
|       - |  9007 | `		}` |
|     ! 0 |  9008 | `		return SXRET_OK;` |
|       - |  9009 | `	}` |
|    8838 |  9010 | `	pBlock = pGen->pCurrent;` |
|       - |  9011 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   41018 |  9012 | `	while(pBlock->pParent){` |
|   41014 |  9013 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8834 |  9014 | `			break;` |
|       - |  9015 | `		}` |
|       - |  9016 | `		/* Point to the parent block */` |
|   32182 |  9017 | `		pBlock = pBlock->pParent;` |
|       2 |  9018 | `	}` |
|       - |  9019 | `	/* Emit the throw instruction */` |
|    8838 |  9020 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9021 | `	/* Emit the jump */` |
|    8838 |  9022 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8838 |  9023 | `	return SXRET_OK;` |
|    4420 |  9024 |  |
|       - |  9025 | `/*` |
|       - |  9026 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9027 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9028 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9029 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9030 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9031 | ` */` |
|      36 |  9032 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9033 |  |
|      38 |  9034 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9035 | `	GenBlock *pBlock;` |
|       - |  9036 | `	sxu32 nIdx;` |
|       - |  9037 | `	sxi32 rc;` |
|      18 |  9038 | `	(void)iCompileFlag;` |
|      38 |  9039 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9040 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9041 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9042 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9043 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9044 | `			return SXERR_ABORT;` |
|       - |  9045 | `		}` |
|     ! 0 |  9046 | `		return SXRET_OK;` |
|       - |  9047 | `	}` |
|      38 |  9048 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9049 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9050 | `		return SXERR_ABORT;` |
|       - |  9051 | `	}` |
|      38 |  9052 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9053 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9054 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9055 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9056 | `			return SXERR_ABORT;` |
|       - |  9057 | `		}` |
|     ! 0 |  9058 | `		return SXRET_OK;` |
|       - |  9059 | `	}` |
|       - |  9060 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9061 | `	pBlock = pGen->pCurrent;` |
|      60 |  9062 | `	while( pBlock->pParent ){` |
|      49 |  9063 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9064 | `			break;` |
|       - |  9065 | `		}` |
|      23 |  9066 | `		pBlock = pBlock->pParent;` |
|       1 |  9067 | `	}` |
|      38 |  9068 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9069 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9070 | `	return SXRET_OK;` |
|      20 |  9071 |  |
|       - |  9072 | `/*` |
|       - |  9073 | ` * Compile a 'catch' block.` |
|       - |  9074 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9075 | ` * an object containing the exception information.` |
|       - |  9076 | ` */` |
|     214 |  9077 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  9078 |  |
|     216 |  9079 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9080 | `	ph7_exception_block sCatch;` |
|       - |  9081 | `	SySet *pInstrContainer;` |
|       - |  9082 | `	SyString sClassName;` |
|       - |  9083 | `	GenBlock *pCatch;` |
|       - |  9084 | `	SyToken *pToken;` |
|       - |  9085 | `	SyString *pName;` |
|       - |  9086 | `	char *zDup;` |
|       - |  9087 | `	sxi32 rc;` |
|     216 |  9088 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9089 | `	/* Zero the structure */` |
|     216 |  9090 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9091 | `	/* Initialize fields */` |
|     216 |  9092 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     216 |  9093 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     216 |  9094 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9095 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9096 | `			pToken = pGen->pIn;` |
|     ! 0 |  9097 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9098 | `				pToken--;` |
|     ! 0 |  9099 | `			}` |
|     ! 0 |  9100 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9101 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9102 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9103 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9104 | `				return SXERR_ABORT;` |
|       - |  9105 | `			}` |
|     ! 0 |  9106 | `			return SXERR_INVALID;` |
|       - |  9107 | `	}` |
|       - |  9108 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     216 |  9109 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     120 |  9110 | `	for(;;){` |
|       - |  9111 | `		SyBlob sResolved;` |
|     242 |  9112 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     242 |  9113 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       5 |  9114 | `			SyBlobRelease(&sResolved);` |
|       5 |  9115 | `			pToken = pGen->pIn;` |
|       5 |  9116 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9117 | `				pToken--;` |
|     ! 0 |  9118 | `			}` |
|       7 |  9119 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9120 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9121 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  9122 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9123 | `				return SXERR_ABORT;` |
|       - |  9124 | `			}` |
|       5 |  9125 | `			return SXERR_INVALID;` |
|       - |  9126 | `		}` |
|       - |  9127 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9128 | `		 * transient SyBlob allocation. */` |
|     356 |  9129 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     236 |  9130 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     238 |  9131 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     238 |  9132 | `		SyBlobRelease(&sResolved);` |
|     238 |  9133 | `		if( zDup == 0 ){` |
|     ! 0 |  9134 | `			goto Mem;` |
|       - |  9135 | `		}` |
|     238 |  9136 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     238 |  9137 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9138 | `			goto Mem;` |
|       - |  9139 | `		}` |
|       - |  9140 | `		/* Check for '\|' (multi-catch separator) */` |
|     249 |  9141 | `		if( pGen->pIn < pGen->pEnd &&` |
|     236 |  9142 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      28 |  9143 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9144 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9145 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9146 | `			continue;` |
|       - |  9147 | `		}` |
|     212 |  9148 | `		break;` |
|     ! 0 |  9149 | `	}` |
|     315 |  9150 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     212 |  9151 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9152 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9153 | `			pToken = pGen->pIn;` |
|     ! 0 |  9154 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9155 | `				pToken--;` |
|     ! 0 |  9156 | `			}` |
|     ! 0 |  9157 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9158 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9159 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9160 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9161 | `				return SXERR_ABORT;` |
|       - |  9162 | `			}` |
|     ! 0 |  9163 | `			return SXERR_INVALID;` |
|       - |  9164 | `	}` |
|     212 |  9165 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9166 | `	/* Duplicate instance name */` |
|     212 |  9167 | `	pName = &pGen->pIn->sData;` |
|     212 |  9168 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     212 |  9169 | `	if( zDup == 0 ){` |
|     ! 0 |  9170 | `		goto Mem;` |
|       - |  9171 | `	}` |
|     212 |  9172 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     212 |  9173 | `	pGen->pIn++;` |
|     212 |  9174 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9175 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9176 | `		pToken = pGen->pIn;` |
|     ! 0 |  9177 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9178 | `			pToken--;` |
|     ! 0 |  9179 | `		}` |
|     ! 0 |  9180 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9181 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9182 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9183 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9184 | `			return SXERR_ABORT;` |
|       - |  9185 | `		}` |
|     ! 0 |  9186 | `		return SXERR_INVALID;` |
|       - |  9187 | `	}` |
|       - |  9188 | `	/* Compile the block */` |
|     212 |  9189 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9190 | `	/* Create the catch block */` |
|     212 |  9191 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     212 |  9192 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9193 | `		return SXERR_ABORT;` |
|       - |  9194 | `	}` |
|       - |  9195 | `	/* Swap bytecode container */` |
|     212 |  9196 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     212 |  9197 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9198 | `	/* Compile the block */` |
|     212 |  9199 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9200 | `	/* Fix forward jumps now the destination is resolved  */` |
|     212 |  9201 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9202 | `	/* Emit the DONE instruction */` |
|     212 |  9203 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9204 | `	/* Leave the block */` |
|     212 |  9205 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9206 | `	/* Restore the default container */` |
|     212 |  9207 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9208 | `	/* Install the catch block */` |
|     212 |  9209 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     212 |  9210 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9211 | `		goto Mem;` |
|       - |  9212 | `	}` |
|     212 |  9213 | `	return SXRET_OK;` |
|     ! 0 |  9214 | `Mem:` |
|     ! 0 |  9215 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9216 | `	return SXERR_ABORT;` |
|     109 |  9217 |  |
|       - |  9218 | `/*` |
|       - |  9219 | ` * Compile a 'try' block.` |
|       - |  9220 | ` * A function using an exception should be in a "try" block.` |
|       - |  9221 | ` * If the exception does not trigger, the code will continue` |
|       - |  9222 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9223 | ` * is "thrown".` |
|       - |  9224 | ` */` |
|     218 |  9225 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  9226 |  |
|       - |  9227 | `	ph7_exception *pException;` |
|     220 |  9228 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9229 | `	GenBlock *pTry;` |
|       - |  9230 | `	sxu32 nJmpIdx;` |
|       - |  9231 | `	sxi32 rc;` |
|       - |  9232 | `	/* Create the exception container */` |
|     220 |  9233 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     220 |  9234 | `	if( pException == 0 ){` |
|     ! 0 |  9235 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9236 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9237 | `		return SXERR_ABORT;` |
|       - |  9238 | `	}` |
|       - |  9239 | `	/* Zero the structure */` |
|     220 |  9240 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9241 | `	/* Initialize fields */` |
|     220 |  9242 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     220 |  9243 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     220 |  9244 | `	pException->iHasFinally = 0;` |
|     220 |  9245 | `	pException->iFinallyDone = 0;` |
|     220 |  9246 | `	pException->pVm = pGen->pVm;` |
|       - |  9247 | `	/* Create the try block */` |
|     220 |  9248 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     220 |  9249 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9250 | `		return SXERR_ABORT;` |
|       - |  9251 | `	}` |
|       - |  9252 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     220 |  9253 | `	pTry->pUserData = pException;` |
|       - |  9254 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     220 |  9255 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9256 | `	/* Fix the jump later when the destination is resolved */` |
|     220 |  9257 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     220 |  9258 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9259 | `	/* Compile the block */` |
|     220 |  9260 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 |  9261 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9262 | `		return SXERR_ABORT;` |
|       - |  9263 | `	}` |
|       - |  9264 | `	/* Fix forward jumps now the destination is resolved */` |
|     220 |  9265 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9266 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     220 |  9267 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9268 | `	/* Leave the block */` |
|     220 |  9269 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9270 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     220 |  9271 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     216 |  9272 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9273 | `		/* Compile one or more catch blocks */` |
|     210 |  9274 | `		for(;;){` |
|     420 |  9275 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     323 |  9276 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     105 |  9277 | `					break;` |
|       - |  9278 | `			}` |
|     216 |  9279 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     216 |  9280 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9281 | `				return SXERR_ABORT;` |
|       - |  9282 | `			}` |
|       2 |  9283 | `		}` |
|     103 |  9284 | `	}` |
|       - |  9285 | `	/* Compile optional finally block */` |
|     220 |  9286 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      94 |  9287 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9288 | `		SySet *pInstrContainer;` |
|       - |  9289 | `		GenBlock *pFinBlock;` |
|      32 |  9290 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9291 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  9292 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  9293 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9294 | `			return SXERR_ABORT;` |
|       - |  9295 | `		}` |
|       - |  9296 | `		/* Swap bytecode container */` |
|      32 |  9297 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  9298 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9299 | `		/* Compile the finally body */` |
|      32 |  9300 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  9301 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9302 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9303 | `			return SXERR_ABORT;` |
|       - |  9304 | `		}` |
|       - |  9305 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  9306 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9307 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  9308 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9309 | `		/* Leave the block */` |
|      32 |  9310 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9311 | `		/* Restore the default container */` |
|      32 |  9312 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  9313 | `		pException->iHasFinally = 1;` |
|      15 |  9314 | `	}` |
|       - |  9315 | `	/* Must have at least one catch or finally */` |
|     220 |  9316 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  9317 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9318 | `			"Cannot use try without catch or finally");` |
|       7 |  9319 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9320 | `			return SXERR_ABORT;` |
|       - |  9321 | `		}` |
|       3 |  9322 | `	}` |
|     220 |  9323 | `	return SXRET_OK;` |
|     111 |  9324 |  |
|       - |  9325 | `/*` |
|       - |  9326 | ` * Compile a switch block.` |
|       - |  9327 | ` *  (See block-comment below for more information)` |
|       - |  9328 | ` */` |
|     108 |  9329 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  9330 |  |
|     110 |  9331 | `	sxi32 rc = SXRET_OK;` |
|     110 |  9332 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9333 | `		/* Unexpected token */` |
|     ! 0 |  9334 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9335 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9336 | `			return SXERR_ABORT;` |
|       - |  9337 | `		}` |
|     ! 0 |  9338 | `		pGen->pIn++;` |
|     ! 0 |  9339 | `	}` |
|     110 |  9340 | `	pGen->pIn++;` |
|       - |  9341 | `	/* First instruction to execute in this block. */` |
|     110 |  9342 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9343 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9344 | `	 * or the '}' token */` |
|     182 |  9345 | `	for(;;){` |
|     366 |  9346 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9347 | `			/* No more input to process */` |
|     ! 0 |  9348 | `			break;` |
|       - |  9349 | `		}` |
|     366 |  9350 | `		rc = SXRET_OK;` |
|     366 |  9351 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  9352 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  9353 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9354 | `					/* Unexpected token */` |
|     ! 0 |  9355 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9356 | `						&pGen->pIn->sData);` |
|     ! 0 |  9357 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9358 | `						return SXERR_ABORT;` |
|       - |  9359 | `					}` |
|       - |  9360 | `					/* FALL THROUGH */` |
|     ! 0 |  9361 | `				}` |
|      28 |  9362 | `				rc = SXERR_EOF;` |
|      28 |  9363 | `				break;` |
|       - |  9364 | `			}` |
|      23 |  9365 | `		}else{` |
|       - |  9366 | `			sxi32 nKwrd;` |
|       - |  9367 | `			/* Extract the keyword */` |
|     298 |  9368 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  9369 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  9370 | `				break;` |
|       - |  9371 | `			}` |
|     218 |  9372 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9373 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9374 | `					/* Unexpected token */` |
|     ! 0 |  9375 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9376 | `						&pGen->pIn->sData);` |
|     ! 0 |  9377 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9378 | `						return SXERR_ABORT;` |
|       - |  9379 | `					}` |
|       - |  9380 | `					/* FALL THROUGH */` |
|     ! 0 |  9381 | `				}` |
|       - |  9382 | `				/* Block compiled */` |
|       3 |  9383 | `				break;` |
|       - |  9384 | `			}` |
|       - |  9385 | `		}` |
|       - |  9386 | `		/* Compile block */` |
|     258 |  9387 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  9388 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9389 | `			return SXERR_ABORT;` |
|       - |  9390 | `		}` |
|       2 |  9391 | `	}` |
|     110 |  9392 | `	return rc;` |
|      56 |  9393 |  |
|       - |  9394 | `/*` |
|       - |  9395 | ` * Compile a case eXpression.` |
|       - |  9396 | ` *  (See block-comment below for more information)` |
|       - |  9397 | ` */` |
|      88 |  9398 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  9399 |  |
|       - |  9400 | `	SySet *pInstrContainer;` |
|       - |  9401 | `	SyToken *pEnd,*pTmp;` |
|      90 |  9402 | `	sxi32 iNest = 0;` |
|       - |  9403 | `	sxi32 rc;` |
|       - |  9404 | `	/* Delimit the expression */` |
|      90 |  9405 | `	pEnd = pGen->pIn;` |
|     186 |  9406 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  9407 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9408 | `			/* Increment nesting level */` |
|       3 |  9409 | `			iNest++;` |
|     185 |  9410 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9411 | `			/* Decrement nesting level */` |
|       3 |  9412 | `			iNest--;` |
|     183 |  9413 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  9414 | `			break;` |
|       - |  9415 | `		}` |
|      98 |  9416 | `		pEnd++;` |
|       2 |  9417 | `	}` |
|      90 |  9418 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9419 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9420 | `		if( rc == SXERR_ABORT ){` |
|       - |  9421 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9422 | `			return SXERR_ABORT;` |
|       - |  9423 | `		}` |
|     ! 0 |  9424 | `	}` |
|       - |  9425 | `	/* Swap token stream */` |
|      90 |  9426 | `	pTmp = pGen->pEnd;` |
|      90 |  9427 | `	pGen->pEnd = pEnd;` |
|      90 |  9428 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  9429 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9430 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9431 | `	/* Emit the done instruction */` |
|      90 |  9432 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9433 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9434 | `	/* Update token stream */` |
|      90 |  9435 | `	pGen->pIn  = pEnd;` |
|      90 |  9436 | `	pGen->pEnd = pTmp;` |
|      90 |  9437 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9438 | `		return SXERR_ABORT;` |
|       - |  9439 | `	}` |
|      90 |  9440 | `	return SXRET_OK;` |
|      46 |  9441 |  |
|       - |  9442 | `/*` |
|       - |  9443 | ` * Compile the smart switch statement.` |
|       - |  9444 | ` * According to the PHP language reference manual` |
|       - |  9445 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9446 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9447 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9448 | ` *  This is exactly what the switch statement is for.` |
|       - |  9449 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9450 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9451 | ` *  of the outer loop, use continue 2.` |
|       - |  9452 | ` *  Note that switch/case does loose comparision.` |
|       - |  9453 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9454 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9455 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9456 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9457 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9458 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9459 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9460 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9461 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9462 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9463 | ` *  list for the next case.` |
|       - |  9464 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9465 | ` *  or floating-point numbers and strings.` |
|       - |  9466 | ` */` |
|      28 |  9467 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9468 |  |
|       - |  9469 | `	GenBlock *pSwitchBlock;` |
|       - |  9470 | `	SyToken *pTmp,*pEnd;` |
|       - |  9471 | `	ph7_switch *pSwitch;` |
|       - |  9472 | `	sxu32 nToken;` |
|       - |  9473 | `	sxu32 nLine;` |
|       - |  9474 | `	sxi32 rc;` |
|      30 |  9475 | `	nLine = pGen->pIn->nLine;` |
|       - |  9476 | `	/* Jump the 'switch' keyword */` |
|      30 |  9477 | `	pGen->pIn++;` |
|      30 |  9478 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9479 | `		/* Syntax error */` |
|     ! 0 |  9480 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9481 | `		if( rc == SXERR_ABORT ){` |
|       - |  9482 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9483 | `			return SXERR_ABORT;` |
|       - |  9484 | `		}` |
|     ! 0 |  9485 | `		goto Synchronize;` |
|       - |  9486 | `	}` |
|       - |  9487 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9488 | `	pGen->pIn++;` |
|      30 |  9489 | `	pEnd = 0; /* cc warning */` |
|       - |  9490 | `	/* Create the loop block */` |
|      44 |  9491 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9492 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9493 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9494 | `		return SXERR_ABORT;` |
|       - |  9495 | `	}` |
|       - |  9496 | `	/* Delimit the condition */` |
|      30 |  9497 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9498 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9499 | `		/* Empty expression */` |
|     ! 0 |  9500 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9501 | `		if( rc == SXERR_ABORT ){` |
|       - |  9502 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9503 | `			return SXERR_ABORT;` |
|       - |  9504 | `		}` |
|     ! 0 |  9505 | `	}` |
|       - |  9506 | `	/* Swap token streams */` |
|      30 |  9507 | `	pTmp = pGen->pEnd;` |
|      30 |  9508 | `	pGen->pEnd = pEnd;` |
|       - |  9509 | `	/* Compile the expression */` |
|      30 |  9510 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9511 | `	if( rc == SXERR_ABORT ){` |
|       - |  9512 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9513 | `		return SXERR_ABORT;` |
|       - |  9514 | `	}` |
|       - |  9515 | `	/* Update token stream */` |
|      30 |  9516 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9517 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9518 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9519 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9520 | `			return SXERR_ABORT;` |
|       - |  9521 | `		}` |
|     ! 0 |  9522 | `		pGen->pIn++;` |
|     ! 0 |  9523 | `	}` |
|      30 |  9524 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9525 | `	pGen->pEnd = pTmp;` |
|      30 |  9526 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9527 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9528 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9529 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9530 | `				pTmp--;` |
|     ! 0 |  9531 | `			}` |
|       - |  9532 | `			/* Unexpected token */` |
|     ! 0 |  9533 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9534 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9535 | `				return SXERR_ABORT;` |
|       - |  9536 | `			}` |
|     ! 0 |  9537 | `			goto Synchronize;` |
|       - |  9538 | `	}` |
|       - |  9539 | `	/* Set the delimiter token */` |
|      30 |  9540 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9541 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9542 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9543 | `	}else{` |
|      28 |  9544 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9545 | `	}` |
|      30 |  9546 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9547 | `	/* Create the switch blocks container */` |
|      30 |  9548 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9549 | `	if( pSwitch == 0 ){` |
|       - |  9550 | `		/* Abort compilation */` |
|     ! 0 |  9551 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9552 | `		return SXERR_ABORT;` |
|       - |  9553 | `	}` |
|       - |  9554 | `	/* Zero the structure */` |
|      30 |  9555 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9556 | `	/* Initialize fields */` |
|      30 |  9557 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9558 | `	/* Emit the switch instruction */` |
|      30 |  9559 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9560 | `	/* Compile case blocks */` |
|      96 |  9561 | `	for(;;){` |
|       - |  9562 | `		sxu32 nKwrd;` |
|     112 |  9563 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9564 | `			/* No more input to process */` |
|     ! 0 |  9565 | `			break;` |
|       - |  9566 | `		}` |
|     112 |  9567 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9568 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9569 | `				/* Unexpected token */` |
|     ! 0 |  9570 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9571 | `					&pGen->pIn->sData);` |
|     ! 0 |  9572 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9573 | `					return SXERR_ABORT;` |
|       - |  9574 | `				}` |
|       - |  9575 | `				/* FALL THROUGH */` |
|     ! 0 |  9576 | `			}` |
|       - |  9577 | `			/* Block compiled */` |
|     ! 0 |  9578 | `			break;` |
|       - |  9579 | `		}` |
|       - |  9580 | `		/* Extract the keyword */` |
|     112 |  9581 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9582 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9583 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9584 | `				/* Unexpected token */` |
|     ! 0 |  9585 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9586 | `					&pGen->pIn->sData);` |
|     ! 0 |  9587 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9588 | `					return SXERR_ABORT;` |
|       - |  9589 | `				}` |
|       - |  9590 | `				/* FALL THROUGH */` |
|     ! 0 |  9591 | `			}` |
|       - |  9592 | `			/* Block compiled */` |
|       3 |  9593 | `			break;` |
|       - |  9594 | `		}` |
|     110 |  9595 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9596 | `			/*` |
|       - |  9597 | `			 * Accroding to the PHP language reference manual` |
|       - |  9598 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9599 | `			 *  that wasn't matched by the other cases.` |
|       - |  9600 | `			 */` |
|      22 |  9601 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9602 | `				/* Default case already compiled */` |
|     ! 0 |  9603 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9604 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9605 | `					return SXERR_ABORT;` |
|       - |  9606 | `				}` |
|     ! 0 |  9607 | `			}` |
|      22 |  9608 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9609 | `			/* Compile the default block */` |
|      22 |  9610 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9611 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9612 | `				return SXERR_ABORT;` |
|      22 |  9613 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9614 | `				break;` |
|       1 |  9615 | `			}` |
|      91 |  9616 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9617 | `			ph7_case_expr sCase;` |
|       - |  9618 | `			/* Standard case block */` |
|      90 |  9619 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9620 | `			/* initialize the structure */` |
|      90 |  9621 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9622 | `			/* Compile the case expression */` |
|      90 |  9623 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9624 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9625 | `				return SXERR_ABORT;` |
|       - |  9626 | `			}` |
|       - |  9627 | `			/* Compile the case block */` |
|      90 |  9628 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9629 | `			/* Insert in the switch container */` |
|      90 |  9630 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9631 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9632 | `				return SXERR_ABORT;` |
|      90 |  9633 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9634 | `				break;` |
|       - |  9635 | `			}` |
|      42 |  9636 | `		}else{` |
|       - |  9637 | `			/* Unexpected token */` |
|     ! 0 |  9638 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9639 | `				&pGen->pIn->sData);` |
|     ! 0 |  9640 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9641 | `				return SXERR_ABORT;` |
|       - |  9642 | `			}` |
|     ! 0 |  9643 | `			break;` |
|       - |  9644 | `		}` |
|       2 |  9645 | `	}` |
|       - |  9646 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9647 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9648 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9649 | `	/* Release the loop block */` |
|      30 |  9650 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9651 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9652 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9653 | `		pGen->pIn++;` |
|      14 |  9654 | `	}` |
|       - |  9655 | `	/* Statement successfully compiled */` |
|      30 |  9656 | `	return SXRET_OK;` |
|     ! 0 |  9657 | `Synchronize:` |
|       - |  9658 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9659 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9660 | `		pGen->pIn++;` |
|     ! 0 |  9661 | `	}` |
|     ! 0 |  9662 | `	return SXRET_OK;` |
|      16 |  9663 |  |
|       - |  9664 | `/*` |
|       - |  9665 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9666 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9667 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9668 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9669 | ` */` |
|       - |  9670 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9671 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9672 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9673 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9674 |  |
|       - |  9675 | `/*` |
|       - |  9676 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9677 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9678 | ` * patched entries from the pending set.` |
|       - |  9679 | ` */` |
| 2128384 |  9680 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       2 |  9681 |  |
| 2128386 |  9682 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9683 | `	sxu32 nTarget;` |
|       - |  9684 | `	sxu32 *aIdx;` |
|       - |  9685 | `	sxu32 i;` |
| 2128386 |  9686 | `	if( nCur <= nBaseline ){` |
| 2128296 |  9687 | `		return;` |
|       - |  9688 | `	}` |
|      92 |  9689 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      92 |  9690 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     190 |  9691 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     100 |  9692 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     100 |  9693 | `		if( pInstr ){` |
|     100 |  9694 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9695 | `		}` |
|      51 |  9696 | `	}` |
|      92 |  9697 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1064194 |  9698 |  |
|       - |  9699 |  |
|       - |  9700 | `/*` |
|       - |  9701 | ` * Generate bytecode for a given expression tree.` |
|       - |  9702 | ` * If something goes wrong while generating bytecode` |
|       - |  9703 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9704 | ` * this function takes care of generating the appropriate` |
|       - |  9705 | ` * error message.` |
|       - |  9706 | ` */` |
| 2868058 |  9707 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9708 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9709 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9710 | `	sxi32 iFlags /* Control flags */` |
|       - |  9711 | `	)` |
|       2 |  9712 |  |
|       - |  9713 | `	VmInstr *pInstr;` |
|       - |  9714 | `	sxu32 nJmpIdx;` |
| 2868060 |  9715 | `	sxi32 iP1 = 0;` |
| 2868060 |  9716 | `	sxu32 iP2 = 0;` |
| 2868060 |  9717 | `	void *p3  = 0;` |
|       - |  9718 | `	sxi32 iVmOp;` |
|       - |  9719 | `	sxi32 rc;` |
| 2868060 |  9720 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 2868060 |  9721 | `	sxu32 nRhsNsBase = 0;` |
| 2868060 |  9722 | `	if( pNode->xCode ){` |
|       - |  9723 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9724 | `		/* Compile node */` |
| 1776254 |  9725 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1776254 |  9726 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1776254 |  9727 | `		RE_SWAP_DELIMITER(pGen);` |
| 1776254 |  9728 | `		return rc;` |
|       - |  9729 | `	}` |
| 1091808 |  9730 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9731 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9732 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9733 | `		return SXERR_ABORT;` |
|       - |  9734 | `	}` |
| 1091808 |  9735 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1091808 |  9736 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9737 | `		sxu32 nJmp = 0;` |
|       - |  9738 | `		sxu32 nNcNsBase;` |
|       - |  9739 | `		VmInstr *pInstrFix;` |
|       - |  9740 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9741 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9742 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9743 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9744 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9745 | `		if( pNode->pRight ){` |
|      47 |  9746 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9747 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9748 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9749 | `				return rc;` |
|       - |  9750 | `			}` |
|      47 |  9751 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - |  9752 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9753 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9754 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9755 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9756 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9757 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9758 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9759 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9760 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9761 | `				pInstrFix->iP2 = 3;` |
|       9 |  9762 | `			}` |
|      23 |  9763 | `		}` |
|       - |  9764 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9765 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9766 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9767 | `		if( pNode->pLeft ){` |
|      47 |  9768 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9769 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9770 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9771 | `				return rc;` |
|       - |  9772 | `			}` |
|      47 |  9773 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      23 |  9774 | `		}` |
|       - |  9775 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9776 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9777 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9778 | `		if( nJmp > 0 ){` |
|      47 |  9779 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9780 | `			if( pInstrFix ){` |
|      47 |  9781 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9782 | `			}` |
|      23 |  9783 | `		}` |
|      47 |  9784 | `		return SXRET_OK;` |
|       - |  9785 | `	}` |
| 1091762 |  9786 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9787 | `		sxu32 nJz,nJmp;` |
|       - |  9788 | `		sxu32 nTernaryNsBase;` |
|       - |  9789 | `		/* Ternary operator require special handling */` |
|       - |  9790 | `		/* Phase#1: Compile the condition */` |
|    2052 |  9791 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2052 |  9792 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2052 |  9793 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9794 | `			return rc;` |
|       - |  9795 | `		}` |
|       - |  9796 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - |  9797 | `		 * compiling the condition must short-circuit to the end of the` |
|       - |  9798 | `		 * condition expression, not leak past the ternary. */` |
|    2052 |  9799 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2052 |  9800 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2052 |  9801 | `		if( pNode->pLeft ){` |
|       - |  9802 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9803 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1984 |  9804 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9805 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1984 |  9806 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    1984 |  9807 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1984 |  9808 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9809 | `				return rc;` |
|       - |  9810 | `			}` |
|    1984 |  9811 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     993 |  9812 | `		}else{` |
|       - |  9813 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9814 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9815 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9816 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9817 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9818 | `		}` |
|       - |  9819 | `		/* Phase#4: Emit the unconditional jump */` |
|    2052 |  9820 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9821 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2052 |  9822 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2052 |  9823 | `		if( pInstr ){` |
|    2052 |  9824 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1025 |  9825 | `		}` |
|    2052 |  9826 | `		if( !pNode->pLeft ){` |
|       - |  9827 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9828 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9829 | `		}` |
|       - |  9830 | `		/* Phase#6: Compile the 'else' expression */` |
|    2052 |  9831 | `		if( pNode->pRight ){` |
|    2052 |  9832 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2052 |  9833 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2052 |  9834 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9835 | `				return rc;` |
|       - |  9836 | `			}` |
|    2052 |  9837 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1025 |  9838 | `		}` |
|    2052 |  9839 | `		if( nJmp > 0 ){` |
|       - |  9840 | `			/* Phase#7: Fix the unconditional jump */` |
|    2052 |  9841 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2052 |  9842 | `			if( pInstr ){` |
|    2052 |  9843 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1025 |  9844 | `			}` |
|    1025 |  9845 | `		}` |
|       - |  9846 | `		/* All done */` |
|    2052 |  9847 | `		return SXRET_OK;` |
|       - |  9848 | `	}` |
| 1089712 |  9849 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - |  9850 | `	/* Generate code for the left tree */` |
| 1089712 |  9851 | `	if( pNode->pLeft ){` |
| 1089674 |  9852 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1089674 |  9853 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - |  9854 | `			ph7_expr_node **apNode;` |
|  346008 |  9855 | `			int hasSpread = 0;` |
|  346008 |  9856 | `			int hasNamed = 0;` |
|       - |  9857 | `			sxi32 nArgs;` |
|       - |  9858 | `			sxi32 n;` |
|       - |  9859 | `			/* Recurse and generate bytecodes for function arguments */` |
|  346008 |  9860 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  346008 |  9861 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - |  9862 | `			/* Validate: no positional arguments after named arguments */` |
|       - |  9863 | `			{` |
|  346008 |  9864 | `				int seenNamed = 0;` |
|  685076 |  9865 | `				for( n = 0; n < nArgs; ++n ){` |
|  339072 |  9866 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     176 |  9867 | `						seenNamed = 1;` |
|     176 |  9868 | `						hasNamed = 1;` |
|  338985 |  9869 | `					}else if( seenNamed && !(apNode[n]->iFlags & EXPR_NODE_SPREAD) ){` |
|       3 |  9870 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - |  9871 | `							"Cannot use positional argument after named argument");` |
|       3 |  9872 | `						return SXERR_SYNTAX;` |
|       - |  9873 | `					}` |
|  169536 |  9874 | `				}` |
|       - |  9875 | `			}` |
|       - |  9876 | `			/* Read-only load */` |
|  346006 |  9877 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  685072 |  9878 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  339068 |  9879 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  339068 |  9880 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  339068 |  9881 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9882 | `					return rc;` |
|       - |  9883 | `				}` |
|       - |  9884 | `				/* Each argument is an independent nullsafe scope. */` |
|  339068 |  9885 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  339068 |  9886 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - |  9887 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 |  9888 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 |  9889 | `					hasSpread = 1;` |
|       9 |  9890 | `				}` |
|  169535 |  9891 | `			}` |
|       - |  9892 | `			/* Total number of given arguments */` |
|  346006 |  9893 | `			iP1 = nArgs;` |
|  346006 |  9894 | `			iP2 = hasSpread;` |
|       - |  9895 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - |  9896 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  346006 |  9897 | `			if( hasNamed ){` |
|      94 |  9898 | `				sxu32 nStrBytes = 0;` |
|       - |  9899 | `				char *zBuf;` |
|     278 |  9900 | `				for( n = 0; n < nArgs; ++n ){` |
|     186 |  9901 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9902 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      86 |  9903 | `					}` |
|      94 |  9904 | `				}` |
|       - |  9905 | `				{` |
|      94 |  9906 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      94 |  9907 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      92 |  9908 | `					&pGen->pVm->sAllocator, mapSize);` |
|      94 |  9909 | `				if( pMap ){` |
|      94 |  9910 | `					SyZero(pMap, mapSize);` |
|      94 |  9911 | `					pMap->bHasNamed = 1;` |
|      94 |  9912 | `					pMap->nTotal = (sxu32)nArgs;` |
|      94 |  9913 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      94 |  9914 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     278 |  9915 | `					for( n = 0; n < nArgs; ++n ){` |
|     186 |  9916 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9917 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     174 |  9918 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     174 |  9919 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     174 |  9920 | `							zBuf += nb;` |
|      86 |  9921 | `						}` |
|       - |  9922 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      94 |  9923 | `					}` |
|      94 |  9924 | `					p3 = (void *)pMap;` |
|      46 |  9925 | `				}` |
|       - |  9926 | `				}` |
|      46 |  9927 | `			}` |
|       - |  9928 | `			/* Remove stale flags now */` |
|  346006 |  9929 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  173002 |  9930 | `		}` |
| 1089672 |  9931 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1089672 |  9932 | `		if( rc != SXRET_OK ){` |
|      31 |  9933 | `			return rc;` |
|       - |  9934 | `		}` |
| 1089642 |  9935 | `		if( !bIsChainOp ){` |
|       - |  9936 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - |  9937 | `			 * target the end of that LHS chain, which is right here. */` |
|  509394 |  9938 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  254696 |  9939 | `		}` |
| 1089642 |  9940 | `		if( iVmOp == PH7_OP_CALL ){` |
|  346006 |  9941 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  346006 |  9942 | `			if( pInstr ){` |
|  346006 |  9943 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  345104 |  9944 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - |  9945 | `					sxu32 nQual;` |
|  345104 |  9946 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - |  9947 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - |  9948 | `					 * so the later NEW handler (if any) can see it. */` |
|  345104 |  9949 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - |  9950 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - |  9951 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - |  9952 | `					 * imports — class imports must NOT affect function` |
|       - |  9953 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - |  9954 | `					 * before NEW; we store the original literal index in the` |
|       - |  9955 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - |  9956 | `					 * the unqualified name and re-qualify with class imports. */` |
|  345104 |  9957 | `					if( bAbsolute ){` |
|      20 |  9958 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      11 |  9959 | `					}else{` |
|  345086 |  9960 | `						int fromImport = 0;` |
|  345086 |  9961 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  345086 |  9962 | `						pInstr->iP2 = (sxi32)nQual;` |
|  345086 |  9963 | `						if( nQual != nOrig ){` |
|       - |  9964 | `							/* Store original literal index in CALL's iP2 so the` |
|       - |  9965 | `							 * NEW handler can recover the unqualified name. */` |
|      74 |  9966 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      74 |  9967 | `							if( !fromImport ){` |
|       - |  9968 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      64 |  9969 | `								if( p3 == 0 ){` |
|      64 |  9970 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 |  9971 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      64 |  9972 | `									if( pMap ){` |
|      64 |  9973 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      64 |  9974 | `										p3 = (void *)pMap;` |
|      31 |  9975 | `									}` |
|      31 |  9976 | `								}` |
|      64 |  9977 | `								if( p3 ){` |
|      64 |  9978 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 |  9979 | `								}` |
|      31 |  9980 | `							}` |
|      36 |  9981 | `						}` |
|       2 |  9982 | `					}` |
|  173455 |  9983 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - |  9984 | `					/* Method call,flag that */` |
|     748 |  9985 | `					pInstr->iP2 = 1;` |
|     373 |  9986 | `				}` |
|  173004 |  9987 | `			}` |
|  916640 |  9988 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - |  9989 | `			ph7_expr_node **apNode;` |
|       - |  9990 | `			sxi32 n;` |
|       - |  9991 | `			/* Recurse and generate bytecodes for array index */` |
|   74846 |  9992 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  135030 |  9993 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   60186 |  9994 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   60186 |  9995 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   60186 |  9996 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9997 | `					return rc;` |
|       - |  9998 | `				}` |
|       - |  9999 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   60186 | 10000 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   30094 | 10001 | `			}` |
|   74846 | 10002 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   60186 | 10003 | `				iP1 = 1; /* Node have an index associated with it */` |
|   30092 | 10004 | `			}` |
|   74846 | 10005 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10006 | `				/* Create an empty entry when the desired index is not found */` |
|   29580 | 10007 | `				iP2 = 1;` |
|   14791 | 10008 | `			}` |
|  706216 | 10009 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10010 | `			/* POP the left node */` |
|      32 | 10011 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10012 | `		}` |
|  544820 | 10013 | `	}` |
| 1089680 | 10014 | `	rc = SXRET_OK;` |
| 1089680 | 10015 | `	nJmpIdx = 0;` |
|       - | 10016 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10017 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10018 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1089680 | 10019 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     270 | 10020 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     270 | 10021 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     270 | 10022 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     270 | 10023 | `			int isSpecial = 0;` |
|     270 | 10024 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     182 | 10025 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     182 | 10026 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     195 | 10027 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     160 | 10028 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      83 | 10029 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      90 | 10030 | `					isSpecial = 1;` |
|      44 | 10031 | `				}` |
|     112 | 10032 | `			}` |
|     314 | 10033 | `			pInstr->iP1 = 0;` |
|     314 | 10034 | `			if( !isSpecial ){` |
|     138 | 10035 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10036 | `			}` |
|       - | 10037 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10038 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     226 | 10039 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     138 | 10040 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     138 | 10041 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10042 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 10043 | `					return SXRET_OK;` |
|       - | 10044 | `				}` |
|      47 | 10045 | `			}` |
|      91 | 10046 | `		}` |
|     167 | 10047 | `	}` |
|       - | 10048 | `	/* Generate code for the right tree */` |
| 1089602 | 10049 | `	if( pNode->pRight ){` |
|  602196 | 10050 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10051 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9176 | 10052 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  597609 | 10053 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10054 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3068 | 10055 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  591489 | 10056 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10057 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      84 | 10058 | `			iVmOp = 0; /* No binary operator to emit */` |
|      84 | 10059 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  589964 | 10060 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10061 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10062 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10063 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10064 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10065 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10066 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     100 | 10067 | `			sxu32 nNsJmp = 0;` |
|     100 | 10068 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     100 | 10069 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  589825 | 10070 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  244596 | 10071 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  122297 | 10072 | `		}` |
|  602196 | 10073 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  602196 | 10074 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  602196 | 10075 | `		if( !bIsChainOp ){` |
|       - | 10076 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10077 | `			 * operator instruction is emitted. */` |
|  442838 | 10078 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  221418 | 10079 | `		}` |
|  602196 | 10080 | `		if( iVmOp == PH7_OP_STORE ){` |
|  241494 | 10081 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  241468 | 10082 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10083 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10084 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10085 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10086 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10087 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10088 | `				 */` |
|      54 | 10089 | `				iVmOp = 0;` |
|  241468 | 10090 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  241442 | 10091 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10092 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   67412 | 10093 | `					iP2 = 1;` |
|   33707 | 10094 | `				}else{` |
|  174032 | 10095 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10096 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   29518 | 10097 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   29518 | 10098 | `						iP1 = pInstr->iP1;` |
|   14760 | 10099 | `					}else{` |
|  144516 | 10100 | `						p3 = pInstr->p3;` |
|       - | 10101 | `					}` |
|       - | 10102 | `					/* POP the last dynamic load instruction */` |
|  174032 | 10103 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10104 | `				}` |
|  120722 | 10105 | `			}` |
|  481450 | 10106 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 10107 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 10108 | `			if( pInstr ){` |
|      48 | 10109 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10110 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10111 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10112 | `					 */` |
|      15 | 10113 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10114 | `					iP1 = pInstr->iP1;` |
|      15 | 10115 | `					iP2 = pInstr->iP2;` |
|      15 | 10116 | `					p3  = pInstr->p3;` |
|       8 | 10117 | `				}else{` |
|      34 | 10118 | `					p3 = pInstr->p3;` |
|       - | 10119 | `				}` |
|      23 | 10120 | `			}` |
|      23 | 10121 | `		}` |
|  301097 | 10122 | `	}` |
| 1089602 | 10123 | `	if( iVmOp > 0 ){` |
| 1089438 | 10124 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11934 | 10125 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10126 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8764 | 10127 | `				iP1 = 1;` |
|    4383 | 10128 | `			}` |
| 1083472 | 10129 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10130 | `			/* Namespace-qualify the class name for NEW */ {` |
|   15392 | 10131 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   15392 | 10132 | `				VmInstr *pCallInstr = 0;` |
|   15392 | 10133 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   15376 | 10134 | `					pCallInstr = pPeek;` |
|   15376 | 10135 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7687 | 10136 | `				}` |
|   15392 | 10137 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   15390 | 10138 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10139 | `					sxu32 nLitForClass;` |
|       - | 10140 | `					/* If the CALL handler already qualified the name using` |
|       - | 10141 | `					 * function imports, recover the original unqualified` |
|       - | 10142 | `					 * literal so we can re-qualify with class imports. */` |
|   15390 | 10143 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 | 10144 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 | 10145 | `					}else{` |
|   15358 | 10146 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10147 | `					}` |
|   15390 | 10148 | `					pPeek->iP1 = 0;` |
|   15390 | 10149 | `					if( !bAbsolute ){` |
|   15374 | 10150 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7688 | 10151 | `					}else{` |
|      18 | 10152 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10153 | `					}` |
|    7694 | 10154 | `				}` |
|       - | 10155 | `			}` |
|   15392 | 10156 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   15392 | 10157 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10158 | `				VmInstr *pPrev;` |
|   15376 | 10159 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   15376 | 10160 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10161 | `					/* Pop the call instruction, preserve named-arg map */` |
|   15376 | 10162 | `					iP1 = pInstr->iP1;` |
|   15376 | 10163 | `					if( pInstr->p3 ){` |
|      38 | 10164 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      18 | 10165 | `					}` |
|   15376 | 10166 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7687 | 10167 | `				}` |
|    7689 | 10168 | `			}` |
| 1069811 | 10169 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10170 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10171 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|      88 | 10172 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      88 | 10173 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      88 | 10174 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      88 | 10175 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|      88 | 10176 | `				int isSpecialIs = 0;` |
|      88 | 10177 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      84 | 10178 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      84 | 10179 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      87 | 10180 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      79 | 10181 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      42 | 10182 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 10183 | `						isSpecialIs = 1;` |
|       5 | 10184 | `					}` |
|      42 | 10185 | `				}` |
|      90 | 10186 | `				pInstr->iP1 = 0;` |
|      90 | 10187 | `				if( !isSpecialIs && !bAbsolute ){` |
|      68 | 10188 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      33 | 10189 | `				}` |
|      44 | 10190 | `			}` |
| 1062076 | 10191 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10192 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10193 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10194 | `			 * should not trigger constant lookup. */` |
|  159360 | 10195 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  159360 | 10196 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  159322 | 10197 | `				pInstr->iP1 = 0;` |
|   79660 | 10198 | `			}` |
|  159360 | 10199 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10200 | `				/* Static member access,remember that */` |
|     192 | 10201 | `				iP1 = 1;` |
|     192 | 10202 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     192 | 10203 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      32 | 10204 | `					p3 = pInstr->p3;` |
|      32 | 10205 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      15 | 10206 | `				}` |
|      95 | 10207 | `			}` |
|   79679 | 10208 | `		}` |
|       - | 10209 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1089436 | 10210 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  544717 | 10211 | `	}` |
| 1089600 | 10212 | `	if( nJmpIdx > 0 ){` |
|       - | 10213 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   12324 | 10214 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   12324 | 10215 | `		if( pInstr ){` |
|   12324 | 10216 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6161 | 10217 | `		}` |
|    6161 | 10218 | `	}` |
| 1089600 | 10219 | `	return rc;` |
| 1434012 | 10220 |  |
|       - | 10221 | `/*` |
|       - | 10222 | ` * Compile a PHP expression.` |
|       - | 10223 | ` * According to the PHP language reference manual:` |
|       - | 10224 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10225 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10226 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10227 | ` *  is "anything that has a value".` |
|       - | 10228 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10229 | ` * function takes care of generating the appropriate error` |
|       - | 10230 | ` * message.` |
|       - | 10231 | ` */` |
|  770924 | 10232 | `static sxi32 PH7_CompileExpr(` |
|       - | 10233 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10234 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10235 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10236 | `	)` |
|       2 | 10237 |  |
|       - | 10238 | `	ph7_expr_node *pRoot;` |
|       - | 10239 | `	SySet sExprNode;` |
|       - | 10240 | `	SyToken *pEnd;` |
|       - | 10241 | `	sxi32 nExpr;` |
|       - | 10242 | `	sxi32 iNest;` |
|       - | 10243 | `	sxi32 rc;` |
|       - | 10244 | `	sxu32 nNullsafeBase;` |
|       - | 10245 | `	/* Initialize worker variables */` |
|  770926 | 10246 | `	nExpr = 0;` |
|  770926 | 10247 | `	pRoot = 0;` |
|       - | 10248 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10249 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  770926 | 10250 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  770926 | 10251 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  770926 | 10252 | `	SySetAlloc(&sExprNode,0x10);` |
|  770926 | 10253 | `	rc = SXRET_OK;` |
|       - | 10254 | `	/* Delimit the expression */` |
|  770926 | 10255 | `	pEnd = pGen->pIn;` |
|  770926 | 10256 | `	iNest = 0;` |
| 5159558 | 10257 | `	while( pEnd < pGen->pEnd ){` |
| 4897378 | 10258 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10259 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     330 | 10260 | `			iNest++;` |
| 4897214 | 10261 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     338 | 10262 | `			iNest--;` |
| 4896882 | 10263 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  508960 | 10264 | `			if( iNest <= 0 ){` |
|  508746 | 10265 | `				break;` |
|       - | 10266 | `			}` |
|     107 | 10267 | `		}` |
| 4388634 | 10268 | `		pEnd++;` |
|       2 | 10269 | `	}` |
|  770926 | 10270 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   17868 | 10271 | `		SyToken *pEnd2 = pGen->pIn;` |
|   17868 | 10272 | `		iNest = 0;` |
|       - | 10273 | `		/* Stop at the first comma */` |
|   35784 | 10274 | `		while( pEnd2 < pEnd ){` |
|   17922 | 10275 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      16 | 10276 | `				iNest++;` |
|   17915 | 10277 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      16 | 10278 | `				iNest--;` |
|   17901 | 10279 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      13 | 10280 | `				if( iNest <= 0 ){` |
|       5 | 10281 | `					break;` |
|       - | 10282 | `				}` |
|       4 | 10283 | `			}` |
|   17918 | 10284 | `			pEnd2++;` |
|       2 | 10285 | `		}` |
|   17868 | 10286 | `		if( pEnd2 <pEnd ){` |
|       5 | 10287 | `			pEnd = pEnd2;` |
|       2 | 10288 | `		}` |
|    8933 | 10289 | `	}` |
|  770926 | 10290 | `	if( pEnd > pGen->pIn ){` |
|  770916 | 10291 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10292 | `		/* Swap delimiter */` |
|  770916 | 10293 | `		pGen->pEnd = pEnd;` |
|       - | 10294 | `		/* Try to get an expression tree */` |
|  770916 | 10295 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  770916 | 10296 | `		if( rc == SXRET_OK && pRoot ){` |
|  770734 | 10297 | `			rc = SXRET_OK;` |
|  770734 | 10298 | `			if( xTreeValidator ){` |
|       - | 10299 | `				/* Call the upper layer validator callback */` |
|   21740 | 10300 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   10869 | 10301 | `			}` |
|  770734 | 10302 | `			if( rc != SXERR_ABORT ){` |
|       - | 10303 | `				/* Generate code for the given tree */` |
|  770734 | 10304 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10305 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10306 | `				 * expression so they short-circuit to its end. */` |
|  770734 | 10307 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  385366 | 10308 | `			}` |
|  770734 | 10309 | `			nExpr = 1;` |
|  385366 | 10310 | `		}` |
|       - | 10311 | `		/* Release the whole tree */` |
|  770916 | 10312 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10313 | `		/* Synchronize token stream */` |
|  770916 | 10314 | `		pGen->pEnd = pTmp;` |
|  770916 | 10315 | `		pGen->pIn  = pEnd;` |
|  770916 | 10316 | `		if( rc == SXERR_ABORT ){` |
|      11 | 10317 | `			SySetRelease(&sExprNode);` |
|      11 | 10318 | `			return SXERR_ABORT;` |
|       - | 10319 | `		}` |
|  385452 | 10320 | `	}` |
|  770916 | 10321 | `	SySetRelease(&sExprNode);` |
|  770916 | 10322 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  385464 | 10323 |  |
|       - | 10324 | `/*` |
|       - | 10325 | ` * Return a pointer to the node construct handler associated` |
|       - | 10326 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10327 | ` */` |
|  193962 | 10328 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 10329 |  |
|  193964 | 10330 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10331 | `		/* Numeric literal: Either real or integer */` |
|  102676 | 10332 | `		return PH7_CompileNumLiteral;` |
|   91290 | 10333 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10334 | `		/* Double quoted string */` |
|   17112 | 10335 | `		return PH7_CompileString;` |
|   74180 | 10336 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10337 | `		/* Single quoted string */` |
|   74068 | 10338 | `		return PH7_CompileSimpleString;` |
|     114 | 10339 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10340 | `		/* Heredoc */` |
|      66 | 10341 | `		return PH7_CompileHereDoc;` |
|      50 | 10342 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10343 | `		/* Nowdoc */` |
|      44 | 10344 | `		return PH7_CompileNowDoc;` |
|       7 | 10345 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10346 | `		/* Backtick quoted string */` |
|       5 | 10347 | `		return PH7_CompileBacktic;` |
|       - | 10348 | `	}` |
|       3 | 10349 | `	return 0;` |
|   96983 | 10350 |  |
|       - | 10351 | `/*` |
|       - | 10352 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10353 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10354 | ` * in write context" parse error.` |
|       - | 10355 | ` */` |
|    6454 | 10356 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       2 | 10357 |  |
|       - | 10358 | `	sxi32 rc;` |
|    6456 | 10359 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6454 | 10360 | `		return SXRET_OK;` |
|       - | 10361 | `	}` |
|       5 | 10362 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10363 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10364 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10365 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3229 | 10366 |  |
|       - | 10367 | `/*` |
|       - | 10368 | ` * Compile an unset() statement.` |
|       - | 10369 | ` * unset($var, $arr[$key], ...);` |
|       - | 10370 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10371 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10372 | ` * parent array before extracting the element to unset.` |
|       - | 10373 | ` */` |
|    2798 | 10374 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 10375 |  |
|    2800 | 10376 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2800 | 10377 | `	sxu32 nIdx = 0;` |
|       - | 10378 | `	SyString sName;` |
|       - | 10379 | `	sxi32 rc;` |
|       - | 10380 | `	/* Jump the 'unset' keyword */` |
|    2800 | 10381 | `	pGen->pIn++;` |
|       - | 10382 | `	/* Save delimiter */` |
|    2800 | 10383 | `	pTmp = pGen->pEnd;` |
|       - | 10384 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2800 | 10385 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2800 | 10386 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10387 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10388 | `		SyToken *pClose;` |
|    2800 | 10389 | `		pGen->pIn++;   /* Skip '(' */` |
|    2800 | 10390 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2800 | 10391 | `		pEnd = pClose; /* Stop at ')' */` |
|    1399 | 10392 | `	}` |
|    2800 | 10393 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10394 | `	/* Resolve the 'unset' builtin name once */` |
|    2800 | 10395 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     336 | 10396 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     336 | 10397 | `		if( pObj == 0 ){` |
|     ! 0 | 10398 | `			return SXERR_ABORT;` |
|       - | 10399 | `		}` |
|     336 | 10400 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     336 | 10401 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     167 | 10402 | `	}` |
|       - | 10403 | `	/* Compile each comma-separated argument */` |
|    9256 | 10404 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6458 | 10405 | `		if( pGen->pIn < pNext ){` |
|    6458 | 10406 | `			pGen->pEnd = pNext;` |
|    6458 | 10407 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10408 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,` |
|       - | 10409 | `				GenStateUnsetValidator);` |
|    6458 | 10410 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10411 | `				return SXERR_ABORT;` |
|       - | 10412 | `			}` |
|    6458 | 10413 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10414 | `				/* Emit call for this single argument */` |
|    6456 | 10415 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6456 | 10416 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6456 | 10417 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3227 | 10418 | `			}` |
|    3228 | 10419 | `		}` |
|       - | 10420 | `		/* Jump trailing commas */` |
|   10118 | 10421 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3662 | 10422 | `			pNext++;` |
|       2 | 10423 | `		}` |
|    6458 | 10424 | `		pGen->pIn = pNext;` |
|       2 | 10425 | `	}` |
|       - | 10426 | `	/* Skip past the closing ')' if present */` |
|    2800 | 10427 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2800 | 10428 | `		pGen->pIn++;` |
|    1399 | 10429 | `	}` |
|       - | 10430 | `	/* Restore token stream */` |
|    2800 | 10431 | `	pGen->pEnd = pTmp;` |
|    2800 | 10432 | `	return SXRET_OK;` |
|    1401 | 10433 |  |
|       - | 10434 | `/*` |
|       - | 10435 | ` * PHP Language construct table.` |
|       - | 10436 | ` */` |
|       - | 10437 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10438 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10439 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10440 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10441 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10442 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10443 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10444 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10445 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10446 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10447 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10448 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10449 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10450 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10451 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10452 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10453 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10454 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10455 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10456 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10457 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10458 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10459 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10460 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10461 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10462 | `};` |
|       - | 10463 | `/*` |
|       - | 10464 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10465 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10466 | ` */` |
|  462190 | 10467 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10468 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10469 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10470 | `	)` |
|       2 | 10471 |  |
|  462192 | 10472 | `	sxu32 n = 0;` |
| 1958491 | 10473 | `	for(;;){` |
| 3916984 | 10474 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   53518 | 10475 | `			break;` |
|       - | 10476 | `		}` |
| 3863468 | 10477 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  408676 | 10478 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10479 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10480 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10481 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10482 | `					return 0;` |
|       - | 10483 | `				}` |
|     ! 0 | 10484 | `			}` |
|  408674 | 10485 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 10486 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 10487 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10488 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10489 | `				return 0;` |
|       - | 10490 | `			}` |
|       - | 10491 | `			/* Return a pointer to the handler.` |
|       - | 10492 | `			*/` |
|  408676 | 10493 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10494 | `		}` |
| 3454794 | 10495 | `		n++;` |
|       2 | 10496 | `	}` |
|   53518 | 10497 | `	if( pLookahed ){` |
|   53518 | 10498 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   11716 | 10499 | `			return PH7_CompileClassInterface;` |
|   41804 | 10500 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   41592 | 10501 | `			return PH7_CompileClass;` |
|     214 | 10502 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 | 10503 | `			return PH7_CompileTrait;` |
|     158 | 10504 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      21 | 10505 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      20 | 10506 | `				return PH7_CompileAbstractClass;` |
|     140 | 10507 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 10508 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10509 | `				return PH7_CompileFinalClass;` |
|       - | 10510 | `		}` |
|      69 | 10511 | `	}` |
|       - | 10512 | `	/* Not a language construct */` |
|     140 | 10513 | `	return 0;` |
|  231097 | 10514 |  |
|       - | 10515 | `/*` |
|       - | 10516 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10517 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10518 | ` */` |
|     138 | 10519 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 10520 |  |
|       - | 10521 | `	int rc;` |
|     140 | 10522 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     140 | 10523 | `	if( rc == FALSE ){` |
|      44 | 10524 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 | 10525 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10526 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10527 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10528 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10529 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10530 | `			*/` |
|       - | 10531 | `			){` |
|      38 | 10532 | `				rc = TRUE;` |
|      18 | 10533 | `		}` |
|      22 | 10534 | `	}` |
|     140 | 10535 | `	return rc;` |
|       2 | 10536 |  |
|       - | 10537 | `/*` |
|       - | 10538 | ` * Compile a PHP chunk.` |
|       - | 10539 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10540 | ` * takes care of generating the appropriate error message.` |
|       - | 10541 | ` */` |
|  624026 | 10542 | `static sxi32 GenStateCompileChunk(` |
|       - | 10543 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10544 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10545 | `	)` |
|       2 | 10546 |  |
|       - | 10547 | `	ProcLangConstruct xCons;` |
|       - | 10548 | `	sxi32 rc;` |
|  624028 | 10549 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  369837 | 10550 | `	for(;;){` |
|  739676 | 10551 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10552 | `			/* No more input to process */` |
|   12414 | 10553 | `			break;` |
|       - | 10554 | `		}` |
|  727264 | 10555 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10556 | `			/* Compile block */` |
|      16 | 10557 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      16 | 10558 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10559 | `				break;` |
|       - | 10560 | `			}` |
|       9 | 10561 | `		}else{` |
|  727250 | 10562 | `			xCons = 0;` |
|  727250 | 10563 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  462192 | 10564 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10565 | `				/* Try to extract a language construct handler */` |
|  462192 | 10566 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  462192 | 10567 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10568 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10569 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10570 | `						&pGen->pIn->sData);` |
|       9 | 10571 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10572 | `						break;` |
|       - | 10573 | `					}` |
|       - | 10574 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10575 | `					 * this erroneous statement.` |
|       - | 10576 | `					 */` |
|       9 | 10577 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10578 | `				}` |
|  496155 | 10579 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   43350 | 10580 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10581 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10582 | `				xCons = PH7_CompileLabel;` |
|      56 | 10583 | `			}` |
|  727250 | 10584 | `			if( xCons == 0 ){` |
|       - | 10585 | `				/* Assume an expression an try to compile it */` |
|  265078 | 10586 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  265078 | 10587 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10588 | `					/* Pop l-value */` |
|  264928 | 10589 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  132463 | 10590 | `				}` |
|  132540 | 10591 | `			}else{` |
|       - | 10592 | `				/* Go compile the sucker */` |
|  462174 | 10593 | `				rc = xCons(&(*pGen));` |
|       - | 10594 | `			}` |
|  727250 | 10595 | `			if( rc == SXERR_ABORT ){` |
|       - | 10596 | `				/* Request to abort compilation */` |
|      11 | 10597 | `				break;` |
|       - | 10598 | `			}` |
|       - | 10599 | `		}` |
|       - | 10600 | `		/* Ignore trailing semi-colons ';' */` |
| 1212682 | 10601 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  485430 | 10602 | `			pGen->pIn++;` |
|       2 | 10603 | `		}` |
|  727254 | 10604 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10605 | `			/* Compile a single statement and return */` |
|  611606 | 10606 | `			break;` |
|       - | 10607 | `		}` |
|       - | 10608 | `		/* LOOP ONE */` |
|       - | 10609 | `		/* LOOP TWO */` |
|       - | 10610 | `		/* LOOP THREE */` |
|       - | 10611 | `		/* LOOP FOUR */` |
|       2 | 10612 | `	}` |
|       - | 10613 | `	/* Return compilation status */` |
|  624028 | 10614 | `	return rc;` |
|       2 | 10615 |  |
|       - | 10616 | `/*` |
|       - | 10617 | ` * Compile a Raw PHP chunk.` |
|       - | 10618 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10619 | ` * takes care of generating the appropriate error message.` |
|       - | 10620 | ` */` |
|   12424 | 10621 | `static sxi32 PH7_CompilePHP(` |
|       - | 10622 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10623 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10624 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10625 | `	)` |
|       2 | 10626 |  |
|   12426 | 10627 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10628 | `	sxi32 rc;` |
|       - | 10629 | `	/* Reset the token set */` |
|   12426 | 10630 | `	SySetReset(&(*pTokenSet));` |
|       - | 10631 | `	/* Mark as the default token set */` |
|   12426 | 10632 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10633 | `	/* Advance the stream cursor */` |
|   12426 | 10634 | `	pGen->pRawIn++;` |
|       - | 10635 | `	/* Tokenize the PHP chunk first */` |
|   12426 | 10636 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10637 | `	/* Point to the head and tail of the token stream. */` |
|   12426 | 10638 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   12426 | 10639 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   12426 | 10640 | `	if( is_expr ){` |
|     ! 0 | 10641 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10642 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10643 | `			/* A simple expression,compile it */` |
|     ! 0 | 10644 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10645 | `		}` |
|       - | 10646 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10647 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10648 | `		return SXRET_OK;` |
|       - | 10649 | `	}` |
|   12426 | 10650 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10651 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10652 | `		/*` |
|       - | 10653 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10654 | `		 * According to the PHP reference manual:` |
|       - | 10655 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10656 | `		 *  immediately follow` |
|       - | 10657 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10658 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10659 | `		 * Symisc extension:` |
|       - | 10660 | `		 *   This short syntax works with all PHP opening` |
|       - | 10661 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10662 | `		 *   only short tag.` |
|       - | 10663 | `		 */` |
|       - | 10664 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10665 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10666 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 10667 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 10668 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 10669 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 10670 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 10671 | `		}` |
|       3 | 10672 | `		return SXRET_OK;` |
|       - | 10673 | `	}` |
|       - | 10674 | `	/* Compile the PHP chunk */` |
|   12424 | 10675 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 10676 | `	/* Fix exceptions jumps */` |
|   12424 | 10677 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10678 | `	/* Fix gotos now, the jump destination is resolved */` |
|   12424 | 10679 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 10680 | `		rc = SXERR_ABORT;` |
|       1 | 10681 | `	}` |
|       - | 10682 | `	/* Reset container */` |
|   12424 | 10683 | `	SySetReset(&pGen->aGoto);` |
|   12424 | 10684 | `	SySetReset(&pGen->aLabel);` |
|   12424 | 10685 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 10686 | `	/* Compilation result */` |
|   12424 | 10687 | `	return rc;` |
|    6214 | 10688 |  |
|       - | 10689 | `/*` |
|       - | 10690 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 10691 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 10692 | ` * This is the only compile interface exported from this file.` |
|       - | 10693 | ` */` |
|   14764 | 10694 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 10695 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 10696 | `	SyString *pScript,  /* Script to compile */` |
|       - | 10697 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 10698 | `	)` |
|       2 | 10699 |  |
|       - | 10700 | `	SySet aPhpToken,aRawToken;` |
|       - | 10701 | `	ph7_gen_state *pCodeGen;` |
|       - | 10702 | `	ph7_value *pRawObj;` |
|       - | 10703 | `	sxu32 nObjIdx;` |
|       - | 10704 | `	sxi32 nRawObj;` |
|       - | 10705 | `	int is_expr;` |
|       - | 10706 | `	sxi32 rc;` |
|   14766 | 10707 | `	if( pScript->nByte < 1 ){` |
|       - | 10708 | `		/* Nothing to compile */` |
|     ! 0 | 10709 | `		return PH7_OK;` |
|       - | 10710 | `	}` |
|       - | 10711 | `	/* Initialize the tokens containers */` |
|   14766 | 10712 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14766 | 10713 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14766 | 10714 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14766 | 10715 | `	is_expr = 0;` |
|   14766 | 10716 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 10717 | `		SyToken sTmp;` |
|       - | 10718 | `		/* PHP only: -*/` |
|    2948 | 10719 | `		sTmp.nLine = 1;` |
|    2948 | 10720 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2948 | 10721 | `		sTmp.pUserData = 0;` |
|    2948 | 10722 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2948 | 10723 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2948 | 10724 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 10725 | `			/* A simple PHP expression */` |
|     ! 0 | 10726 | `			is_expr = 1;` |
|     ! 0 | 10727 | `		}` |
|    1475 | 10728 | `	}else{` |
|       - | 10729 | `		/* Tokenize raw text */` |
|   11820 | 10730 | `		SySetAlloc(&aRawToken,32);` |
|   11820 | 10731 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 10732 | `	}` |
|   14766 | 10733 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 10734 | `	/* Process high-level tokens */` |
|   14766 | 10735 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14766 | 10736 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14766 | 10737 | `	rc = PH7_OK;` |
|   14766 | 10738 | `	if( is_expr ){` |
|       - | 10739 | `		/* Compile the expression */` |
|     ! 0 | 10740 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 10741 | `		goto cleanup;` |
|       - | 10742 | `	}` |
|   14766 | 10743 | `	nObjIdx = 0;` |
|       - | 10744 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 10745 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 10746 | `	 * preventing namespace bleeding across include()d files. */` |
|   14766 | 10747 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 10748 | `	/* Start the compilation process */` |
|   13295 | 10749 | `	for(;;){` |
|   39004 | 10750 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14754 | 10751 | `			break; /* No more tokens to process */` |
|       - | 10752 | `		}` |
|   24252 | 10753 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 10754 | `			/* Compile the PHP chunk */` |
|   12426 | 10755 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   12426 | 10756 | `			if( rc == SXERR_ABORT ){` |
|      13 | 10757 | `				break;` |
|       - | 10758 | `			}` |
|   12414 | 10759 | `			continue;` |
|       - | 10760 | `		}` |
|       - | 10761 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11828 | 10762 | `		nRawObj = 0;` |
|   23654 | 10763 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 10764 | `			/* Consume the raw chunk without any processing */` |
|   11828 | 10765 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11828 | 10766 | `			if( pRawObj == 0 ){` |
|     ! 0 | 10767 | `				rc = SXERR_MEM;` |
|     ! 0 | 10768 | `				break;` |
|       - | 10769 | `			}` |
|       - | 10770 | `			/* Mark as constant and emit the load constant instruction */` |
|   11828 | 10771 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11828 | 10772 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11828 | 10773 | `			++nRawObj;` |
|   11828 | 10774 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 10775 | `		}` |
|   11828 | 10776 | `		if( nRawObj > 0 ){` |
|       - | 10777 | `			/* Emit the consume instruction */` |
|   11828 | 10778 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5913 | 10779 | `		}` |
|    7384 | 10780 | `	}` |
|    7382 | 10781 | `cleanup:` |
|   14766 | 10782 | `	SySetRelease(&aRawToken);` |
|   14766 | 10783 | `	SySetRelease(&aPhpToken);` |
|   14766 | 10784 | `	return rc;` |
|    7384 | 10785 |  |
|       - | 10786 | `/*` |
|       - | 10787 | ` * Utility routines.Initialize the code generator.` |
|       - | 10788 | ` */` |
|    2918 | 10789 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 10790 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10791 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10792 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10793 | `	)` |
|       2 | 10794 |  |
|    2920 | 10795 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10796 | `	/* Zero the structure */` |
|    2920 | 10797 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 10798 | `	/* Initial state */` |
|    2920 | 10799 | `	pGen->pVm  = &(*pVm);` |
|    2920 | 10800 | `	pGen->xErr = xErr;` |
|    2920 | 10801 | `	pGen->pErrData = pErrData;` |
|    2920 | 10802 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2920 | 10803 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2920 | 10804 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    2920 | 10805 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2920 | 10806 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 10807 | `	/* Error log buffer */` |
|    2920 | 10808 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 10809 | `	/* General purpose working buffer */` |
|    2920 | 10810 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 10811 | `	/* Namespace state */` |
|    2920 | 10812 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2920 | 10813 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2920 | 10814 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2920 | 10815 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10816 | `	/* Create the global scope */` |
|    2920 | 10817 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 10818 | `	/* Point to the global scope */` |
|    2920 | 10819 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2920 | 10820 | `	return SXRET_OK;` |
|       2 | 10821 |  |
|       - | 10822 | `/*` |
|       - | 10823 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 10824 | ` */` |
|   17376 | 10825 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 10826 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10827 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10828 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10829 | `	)` |
|       2 | 10830 |  |
|   17378 | 10831 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10832 | `	GenBlock *pBlock,*pParent;` |
|       - | 10833 | `	/* Reset state */` |
|   17378 | 10834 | `	SySetReset(&pGen->aLabel);` |
|   17378 | 10835 | `	SySetReset(&pGen->aGoto);` |
|   17378 | 10836 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   17378 | 10837 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   17378 | 10838 | `	SyBlobRelease(&pGen->sWorker);` |
|   17378 | 10839 | `	SyBlobRelease(&pGen->sNamespace);` |
|   17378 | 10840 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   17378 | 10841 | `	SyHashRelease(&pGen->hUseImports);` |
|   17378 | 10842 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   17378 | 10843 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   17378 | 10844 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   17378 | 10845 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   17378 | 10846 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10847 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 10848 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 10849 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 10850 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 10851 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 10852 | `	 * number of unique names, which is acceptable. */` |
|       - | 10853 | `	/* Point to the global scope */` |
|   17378 | 10854 | `	pBlock = pGen->pCurrent;` |
|   17378 | 10855 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 10856 | `		pParent = pBlock->pParent;` |
|     ! 0 | 10857 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 10858 | `		pBlock = pParent;` |
|     ! 0 | 10859 | `	}` |
|   17378 | 10860 | `	pGen->xErr = xErr;` |
|   17378 | 10861 | `	pGen->pErrData = pErrData;` |
|   17378 | 10862 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   17378 | 10863 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   17378 | 10864 | `	pGen->pIn = pGen->pEnd = 0;` |
|   17378 | 10865 | `	pGen->nErr = 0;` |
|   17378 | 10866 | `	return SXRET_OK;` |
|       2 | 10867 |  |
|       - | 10868 | `/*` |
|       - | 10869 | ` * Generate a compile-time error message.` |
|       - | 10870 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 10871 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 10872 | ` * abort compilation immediately.` |
|       - | 10873 | ` */` |
|     562 | 10874 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 10875 |  |
|     564 | 10876 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     564 | 10877 | `	const char *zErr = "Error";` |
|       - | 10878 | `	SyString *pFile;` |
|       - | 10879 | `	va_list ap;` |
|       - | 10880 | `	sxi32 rc;` |
|       - | 10881 | `	/* Reset the working buffer */` |
|     564 | 10882 | `	SyBlobReset(pWorker);` |
|       - | 10883 | `	/* Peek the processed file path if available */` |
|     564 | 10884 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     564 | 10885 | `	if( nErrType == E_ERROR ){` |
|       - | 10886 | `		/* Increment the error counter */` |
|     462 | 10887 | `		pGen->nErr++;` |
|     462 | 10888 | `		if( pGen->nErr > 15 ){` |
|       - | 10889 | `			/* Error count limit reached */` |
|       5 | 10890 | `			if( pGen->xErr ){` |
|       5 | 10891 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 10892 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 10893 | `				if( pFile ){` |
|       5 | 10894 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 10895 | `				}` |
|       5 | 10896 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 10897 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 10898 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 10899 | `				}` |
|       2 | 10900 | `			}` |
|       - | 10901 | `			/* Abort immediately */` |
|       5 | 10902 | `			return SXERR_ABORT;` |
|       - | 10903 | `		}` |
|     228 | 10904 | `	}` |
|     560 | 10905 | `	if( pGen->xErr == 0 ){` |
|       - | 10906 | `		/* No available error consumer,return immediately */` |
|       3 | 10907 | `		return SXRET_OK;` |
|       - | 10908 | `	}` |
|     557 | 10909 | `	switch(nErrType){` |
|     455 | 10910 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 10911 | `	case E_WARNING: zErr = "Warning";     break;` |
|      69 | 10912 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 10913 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 10914 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 10915 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 10916 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 10917 | `	default:` |
|     ! 0 | 10918 | `		break;` |
|       - | 10919 | `	}` |
|     557 | 10920 | `	rc = SXRET_OK;` |
|       - | 10921 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     557 | 10922 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     557 | 10923 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     557 | 10924 | `	va_start(ap,zFormat);` |
|     557 | 10925 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     557 | 10926 | `	va_end(ap);` |
|     557 | 10927 | `	if( pFile ){` |
|     557 | 10928 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     278 | 10929 | `	}` |
|       - | 10930 | `	/* Append a new line */` |
|     557 | 10931 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     557 | 10932 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 10933 | `		/* Consume the generated error message */` |
|     557 | 10934 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     278 | 10935 | `	}` |
|     557 | 10936 | `	return rc;` |
|     283 | 10937 |  |
|       - | 10938 |  |
