# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4757/6052 lines (78.60%)

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
|    3104 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3106 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    8747 |   131 | `	for(;;){` |
|   17496 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2998 |   133 | `			iCount--; /* Decrement nesting level */` |
|    2998 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    2972 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   14526 |   140 | `		pBlock = pBlock->pParent;` |
|   14526 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1554 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  605984 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  605986 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  605986 |   162 | `	pBlock->pUserData   = pUserData;` |
|  605986 |   163 | `	pBlock->pGen        = pGen;` |
|  605986 |   164 | `	pBlock->iFlags      = iType;` |
|  605986 |   165 | `	pBlock->pParent     = 0;` |
|  605986 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  605986 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  605986 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  603146 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  603148 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  603148 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  603148 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  603148 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  603148 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  603148 |   200 | `	pGen->pCurrent = pBlock;` |
|  603148 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  291892 |   203 | `		*ppBlock = pBlock;` |
|  145945 |   204 | `	}` |
|  603148 |   205 | `	return SXRET_OK;` |
|  301575 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  603138 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  603140 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  603140 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  603140 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  603138 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  603140 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  603140 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  603140 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  603140 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  603138 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  603140 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  603140 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  603140 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  603140 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  603140 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  603140 |   244 | `	return SXRET_OK;` |
|  301571 |   245 |  |
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
|  183670 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  183672 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  183672 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  183672 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  183672 |   265 | `	return rc;` |
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
|  429036 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  429038 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  786886 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  357850 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  139336 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  218516 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   34848 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  183670 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  183670 |   298 | `		if( pInstr ){` |
|  183670 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  183670 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  183670 |   302 | `			aFix[n].nJumpType = -1;` |
|   91834 |   303 | `		}` |
|   91836 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  429038 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  163816 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  163818 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  163964 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  163816 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  163948 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  163816 |   358 | `	return SXRET_OK;` |
|   81910 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  533316 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  533318 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  533318 |   367 | `	if( pEntry == 0 ){` |
|  263134 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  270186 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  270186 |   371 | `	return SXRET_OK;` |
|  266660 |   372 |  |
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
|  263132 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  263134 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  263134 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  131566 |   387 | `	}` |
|  263134 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|   93714 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|   93716 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|   93716 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   93716 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|   93716 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|   93716 |   408 | `	return pObj;` |
|   46859 |   409 |  |
|       - |   410 | `/*` |
|       - |   411 | ` * Implementation of the PHP language constructs.` |
|       - |   412 | ` */` |
|       - |   413 | `/* Forward declaration */` |
|       - |   414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |   415 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd);` |
|       - |   416 | `/* Forward decl: union type parser is defined later in this file. */` |
|       - |   417 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |   418 | `	ph7_gen_state *pGen,` |
|       - |   419 | `	sxu32 *pnType,` |
|       - |   420 | `	SyString *pClass,` |
|       - |   421 | `	SySet *pAlts,` |
|       - |   422 | `	sxi32 *piTypeFlags,` |
|       - |   423 | `	SyString *pTypeText,` |
|       - |   424 | `	int iNullableFlag,` |
|       - |   425 | `	int iUnionFlag,` |
|       - |   426 | `	int bAllowVoid,` |
|       - |   427 | `	sxu32 nLine` |
|       - |   428 | `);` |
|       - |   429 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |   430 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |   431 | `/*` |
|       - |   432 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |   433 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |   434 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |   435 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |   436 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |   437 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |   438 | ` * inputs like a thousand-digit number.` |
|       - |   439 | ` */` |
|       - |   440 | `#define GEN_NUM_SCRATCH 128` |
|       - |   441 | `/*` |
|       - |   442 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |   443 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |   444 | ` *   base  2 => 0 or 1` |
|       - |   445 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |   446 | ` *              decimal scan in the lexer)` |
|       - |   447 | ` */` |
|    1076 |   448 | `static int GenStateIsBaseDigit(int c, int base)` |
|       2 |   449 |  |
|    1078 |   450 | `	if( base == 16 ){ return SyisHex(c); }` |
|     980 |   451 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     702 |   452 | `	return SyisDigit(c);` |
|     540 |   453 |  |
|       - |   454 | `/*` |
|       - |   455 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |   456 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |   457 | ` * the exact wording PHP uses:` |
|       - |   458 | ` *` |
|       - |   459 | ` *   syntax error, unexpected identifier "X"` |
|       - |   460 | ` *` |
|       - |   461 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |   462 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |   463 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |   464 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |   465 | ` * no forward rescan needed.` |
|       - |   466 | ` *` |
|       - |   467 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |   468 | ` * returns 0 when it is well-formed.` |
|       - |   469 | ` */` |
|   94238 |   470 | `static int GenStateFindBadNumericSeparator(` |
|       - |   471 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   472 |  |
|   94240 |   473 | `	const char *z = pRaw->zString;` |
|   94240 |   474 | `	sxu32 n = pRaw->nByte;` |
|   94240 |   475 | `	int base = 10;` |
|       - |   476 | `	sxu32 i, start;` |
|   94240 |   477 | `	if( n < 2 ) return 0;` |
|    8466 |   478 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   479 | `		base = 16;` |
|    8431 |   480 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   481 | `		base = 2;` |
|     139 |   482 | `	}` |
|   31346 |   483 | `	for( i = 0; i < n; ++i ){` |
|   22896 |   484 | `		if( z[i] != '_' ) continue;` |
|     814 |   485 | `		if( i > 0 && i + 1 < n` |
|     543 |   486 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     540 |   487 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |   488 | `			continue; /* well-placed separator */` |
|       - |   489 | `		}` |
|       - |   490 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   491 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      15 |   492 | `		start = i;` |
|      20 |   493 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |   494 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       5 |   495 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |   496 | `		}` |
|      15 |   497 | `		*pBadStart = &z[start];` |
|      15 |   498 | `		*pBadLen = n - start;` |
|      15 |   499 | `		return 1;` |
|     ! 0 |   500 | `	}` |
|    8452 |   501 | `	return 0;` |
|   47121 |   502 |  |
|       - |   503 | `/*` |
|       - |   504 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   505 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   506 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   507 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   508 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   509 | ` * so callers can bail from the current construct).` |
|       - |   510 | ` */` |
|   94238 |   511 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   512 |  |
|   94240 |   513 | `	const char *zBad = 0;` |
|   94240 |   514 | `	sxu32 nBad = 0;` |
|       - |   515 | `	SyString sBad;` |
|       - |   516 | `	sxi32 rc;` |
|   94240 |   517 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   94226 |   518 | `		return SXRET_OK;` |
|       - |   519 | `	}` |
|      15 |   520 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   521 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   522 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   523 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   524 | `		return SXERR_ABORT;` |
|       - |   525 | `	}` |
|      15 |   526 | `	return SXERR_SYNTAX;` |
|   47121 |   527 |  |
|       - |   528 | `/*` |
|       - |   529 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |   530 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |   531 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |   532 | ` *` |
|       - |   533 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |   534 | ` * and *pzAlloc is set to NULL.` |
|       - |   535 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |   536 | ` * and *pzAlloc is set to NULL.` |
|       - |   537 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |   538 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |   539 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |   540 | ` *` |
|       - |   541 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |   542 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |   543 | ` */` |
|   94224 |   544 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   545 | `	SyMemBackend *pAlloc,` |
|       - |   546 | `	const SyString *pToken,` |
|       - |   547 | `	char *zScratch, sxu32 nScratch,` |
|       - |   548 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   549 |  |
|       - |   550 | `	sxu32 i, j;` |
|   94226 |   551 | `	int hasUnderscore = 0;` |
|       - |   552 | `	char *zBuf;` |
|   94226 |   553 | `	*pzAlloc = 0;` |
|  200814 |   554 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  106842 |   555 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   53296 |   556 | `	}` |
|   94226 |   557 | `	if( !hasUnderscore ){` |
|   93974 |   558 | `		SyStringDupPtr(pOut, pToken);` |
|   93974 |   559 | `		return SXRET_OK;` |
|       - |   560 | `	}` |
|     253 |   561 | `	if( pToken->nByte <= nScratch ){` |
|     251 |   562 | `		zBuf = zScratch;` |
|     126 |   563 | `	}else{` |
|       3 |   564 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |   565 | `		if( zBuf == 0 ){` |
|     ! 0 |   566 | `			return SXERR_ABORT;` |
|       - |   567 | `		}` |
|       3 |   568 | `		*pzAlloc = zBuf;` |
|       - |   569 | `	}` |
|     253 |   570 | `	j = 0;` |
|    2895 |   571 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |   572 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |   573 | `	}` |
|     253 |   574 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |   575 | `	return SXRET_OK;` |
|   47114 |   576 |  |
|       - |   577 | `/*` |
|       - |   578 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |   579 | ` * Notes on the integer type.` |
|       - |   580 | ` *  According to the PHP language reference manual` |
|       - |   581 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |   582 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |   583 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |   584 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |   585 | ` * Symisc eXtension to the integer type.` |
|       - |   586 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |   587 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |   588 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |   589 | ` *  [i.e: either 32bit or 64bit].` |
|       - |   590 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |   591 | ` *  documentation.` |
|       - |   592 | ` */` |
|   94210 |   593 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   594 |  |
|   94212 |   595 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   94212 |   596 | `	sxu32 nIdx = 0;` |
|       - |   597 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   94212 |   598 | `	char *zAlloc = 0;` |
|       - |   599 | `	SyString sNum;` |
|       - |   600 | `	sxi32 rc;` |
|   47105 |   601 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   94212 |   602 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   94212 |   603 | `	if( rc != SXRET_OK ){` |
|      11 |   604 | `		return rc;` |
|       - |   605 | `	}` |
|  141302 |   606 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   47100 |   607 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   94202 |   608 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   609 | `		return SXERR_ABORT;` |
|       - |   610 | `	}` |
|   94202 |   611 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   612 | `		ph7_value *pObj;` |
|       - |   613 | `		sxi64 iValue;` |
|   93716 |   614 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   93716 |   615 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   93716 |   616 | `		if( pObj == 0 ){` |
|     ! 0 |   617 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   618 | `			return SXERR_ABORT;` |
|       - |   619 | `		}` |
|   93716 |   620 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   46859 |   621 | `	}else{` |
|       - |   622 | `		/* Real number */` |
|       - |   623 | `		ph7_value *pObj;` |
|       - |   624 | `		/* Reserve a new constant */` |
|     488 |   625 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     488 |   626 | `		if( pObj == 0 ){` |
|     ! 0 |   627 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   628 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   629 | `			return SXERR_ABORT;` |
|       - |   630 | `		}` |
|     488 |   631 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     488 |   632 | `		PH7_MemObjToReal(pObj);` |
|       - |   633 | `	}` |
|   94202 |   634 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   635 | `	/* Emit the load constant instruction */` |
|   94202 |   636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   637 | `	/* Node successfully compiled */` |
|   94202 |   638 | `	return SXRET_OK;` |
|   47107 |   639 |  |
|       - |   640 | `/*` |
|       - |   641 | ` * Compile a single quoted string.` |
|       - |   642 | ` * According to the PHP language reference manual:` |
|       - |   643 | ` *` |
|       - |   644 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   645 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   646 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   647 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   648 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   649 | ` *` |
|       - |   650 | ` */` |
|   60904 |   651 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   652 |  |
|   60906 |   653 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   654 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   655 | `	ph7_value *pObj;` |
|       - |   656 | `	sxu32 nIdx;` |
|   60906 |   657 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   658 | `	/* Delimit the string */` |
|   60906 |   659 | `	zIn  = pStr->zString;` |
|   60906 |   660 | `	zEnd = &zIn[pStr->nByte];` |
|   60906 |   661 | `	if( zIn >= zEnd ){` |
|       - |   662 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   663 | `		 * rather than reserving a new object each time. */` |
|     144 |   664 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     144 |   665 | `		return SXRET_OK;` |
|       - |   666 | `	}` |
|   60764 |   667 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   668 | `		/* Already processed,emit the load constant instruction` |
|       - |   669 | `		 * and return.` |
|       - |   670 | `		 */` |
|   17886 |   671 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17886 |   672 | `		return SXRET_OK;` |
|       - |   673 | `	}` |
|       - |   674 | `	/* Reserve a new constant */` |
|   42880 |   675 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   42880 |   676 | `	if( pObj == 0 ){` |
|     ! 0 |   677 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   678 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   679 | `		return SXERR_ABORT;` |
|       - |   680 | `	}` |
|   42880 |   681 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   682 | `	/* Compile the node */` |
|   42920 |   683 | `	for(;;){` |
|   85842 |   684 | `		if( zIn >= zEnd ){` |
|       - |   685 | `			/* End of input */` |
|   42880 |   686 | `			break;` |
|       - |   687 | `		}` |
|   42964 |   688 | `		zCur = zIn;` |
|  682248 |   689 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  639286 |   690 | `			zIn++;` |
|       2 |   691 | `		}` |
|   42964 |   692 | `		if( zIn > zCur ){` |
|       - |   693 | `			/* Append raw contents*/` |
|   42944 |   694 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   21471 |   695 | `		}` |
|   42964 |   696 | `		zIn++;` |
|   42964 |   697 | `		if( zIn < zEnd ){` |
|     105 |   698 | `			if( zIn[0] == '\\' ){` |
|       - |   699 | `				/* A literal backslash */` |
|      23 |   700 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |   701 | `			}else if( zIn[0] == '\'' ){` |
|       - |   702 | `				/* A single quote */` |
|      11 |   703 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   704 | `			}else{` |
|       - |   705 | `				/* verbatim copy */` |
|      73 |   706 | `				zIn--;` |
|      73 |   707 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |   708 | `				zIn++;` |
|       - |   709 | `			}` |
|      52 |   710 | `		}` |
|       - |   711 | `		/* Advance the stream cursor */` |
|   42964 |   712 | `		zIn++;` |
|       2 |   713 | `	}` |
|       - |   714 | `	/* Emit the load constant instruction */` |
|   42880 |   715 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   42880 |   716 | `	if( pStr->nByte < 1024 ){` |
|       - |   717 | `		/* Install in the literal table */` |
|   42880 |   718 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   21439 |   719 | `	}` |
|       - |   720 | `	/* Node successfully compiled */` |
|   42880 |   721 | `	return SXRET_OK;` |
|   30454 |   722 |  |
|       - |   723 | `/*` |
|       - |   724 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   725 | ` *` |
|       - |   726 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   727 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   728 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   729 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   730 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   731 | ` *` |
|       - |   732 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   733 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   734 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   735 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   736 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   737 | ` *     whitespace.` |
|       - |   738 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   739 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   740 | ` */` |
|     106 |   741 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       2 |   742 |  |
|     108 |   743 | `	SyString *pIn = &pGen->pIn->sData;` |
|     108 |   744 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   745 | `	const char *zPrefix;` |
|       - |   746 | `	const char *z, *zEnd;` |
|       - |   747 | `	char *zBuf, *zDst;` |
|     108 |   748 | `	if( nIndent == 0 ){` |
|       - |   749 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      64 |   750 | `		*pOut = *pIn;` |
|      64 |   751 | `		return SXRET_OK;` |
|       - |   752 | `	}` |
|       - |   753 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   754 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   755 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   756 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   757 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   758 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      46 |   759 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      46 |   760 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   761 | `		zPrefix += 2;` |
|     ! 0 |   762 | `	}else{` |
|      46 |   763 | `		zPrefix += 1;` |
|       - |   764 | `	}` |
|       - |   765 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      46 |   766 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      46 |   767 | `	if( zBuf == 0 ){` |
|     ! 0 |   768 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   769 | `		return SXERR_ABORT;` |
|       - |   770 | `	}` |
|      46 |   771 | `	zDst = zBuf;` |
|      46 |   772 | `	z = pIn->zString;` |
|      46 |   773 | `	zEnd = z + pIn->nByte;` |
|     128 |   774 | `	while( z < zEnd ){` |
|      70 |   775 | `		const char *zLine = z;` |
|       - |   776 | `		sxu32 nLine;` |
|       - |   777 | `		int bEmpty;` |
|     798 |   778 | `		while( z < zEnd && z[0] != '\n' ){` |
|     730 |   779 | `			z++;` |
|       2 |   780 | `		}` |
|      70 |   781 | `		nLine = (sxu32)(z - zLine);` |
|      70 |   782 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      70 |   783 | `		if( !bEmpty ){` |
|       - |   784 | `			sxu32 i;` |
|      66 |   785 | `			if( nLine < nIndent ){` |
|     ! 0 |   786 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   787 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   788 | `					nIndent);` |
|     ! 0 |   789 | `				return SXERR_ABORT;` |
|       - |   790 | `			}` |
|     268 |   791 | `			for( i = 0; i < nIndent; i++ ){` |
|     212 |   792 | `				if( zLine[i] != zPrefix[i] ){` |
|       9 |   793 | `					unsigned char c = (unsigned char)zLine[i];` |
|       9 |   794 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |   795 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   796 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |   797 | `					}else{` |
|       7 |   798 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   799 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   800 | `							nIndent);` |
|       - |   801 | `					}` |
|       9 |   802 | `					return SXERR_ABORT;` |
|       - |   803 | `				}` |
|     103 |   804 | `			}` |
|      57 |   805 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   806 | `			zDst += nLine - nIndent;` |
|      33 |   807 | `		}else if( nLine == 1 ){` |
|       - |   808 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   809 | `			*zDst++ = '\r';` |
|     ! 0 |   810 | `		}` |
|      61 |   811 | `		if( z < zEnd ){` |
|      25 |   812 | `			*zDst++ = '\n';` |
|      25 |   813 | `			z++;` |
|      12 |   814 | `		}` |
|       1 |   815 | `	}` |
|      37 |   816 | `	pOut->zString = zBuf;` |
|      37 |   817 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   818 | `	return SXRET_OK;` |
|      55 |   819 |  |
|       - |   820 | `/*` |
|       - |   821 | ` * Compile a nowdoc string.` |
|       - |   822 | ` * According to the PHP language reference manual:` |
|       - |   823 | ` *` |
|       - |   824 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   825 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   826 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   827 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   828 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   829 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   830 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   831 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   832 | ` *  of the closing identifier.` |
|       - |   833 | ` */` |
|      42 |   834 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   835 |  |
|       - |   836 | `	SyString sStripped;` |
|       - |   837 | `	SyString *pStr;` |
|       - |   838 | `	ph7_value *pObj;` |
|       - |   839 | `	sxu32 nIdx;` |
|       - |   840 | `	sxi32 rc;` |
|      44 |   841 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      44 |   842 | `	if( rc != SXRET_OK ){` |
|       5 |   843 | `		return rc;` |
|       - |   844 | `	}` |
|      40 |   845 | `	pStr = &sStripped;` |
|      40 |   846 | `	nIdx = 0; /* Prevent compiler warning */` |
|      40 |   847 | `	if( pStr->nByte <= 0 ){` |
|       - |   848 | `		/* Empty string,load NULL */` |
|       7 |   849 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   850 | `		return SXRET_OK;` |
|       - |   851 | `	}` |
|       - |   852 | `	/* Reserve a new constant */` |
|      34 |   853 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      34 |   854 | `	if( pObj == 0 ){` |
|     ! 0 |   855 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   856 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   857 | `		return SXERR_ABORT;` |
|       - |   858 | `	}` |
|       - |   859 | `	/* No processing is done here, simply a memcpy() operation */` |
|      34 |   860 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   861 | `	/* Emit the load constant instruction */` |
|      34 |   862 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   863 | `	/* Node successfully compiled */` |
|      34 |   864 | `	return SXRET_OK;` |
|      23 |   865 |  |
|       - |   866 | `/*` |
|       - |   867 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |   868 | ` * According to the PHP language reference manual` |
|       - |   869 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |   870 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |   871 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |   872 | ` *  property in a string with a minimum of effort.` |
|       - |   873 | ` *  Simple syntax` |
|       - |   874 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |   875 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |   876 | ` *   the end of the name.` |
|       - |   877 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |   878 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |   879 | ` *   as to simple variables.` |
|       - |   880 | ` *  Complex (curly) syntax` |
|       - |   881 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |   882 | ` *   of complex expressions.` |
|       - |   883 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |   884 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |   885 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |   886 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |   887 | ` */` |
|    1932 |   888 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   889 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   890 | `	sxu32 nLine,         /* Line number */` |
|       - |   891 | `	const char *zIn,     /* Raw expression */` |
|       - |   892 | `	const char *zEnd     /* End of the expression */` |
|       - |   893 | `	)` |
|       2 |   894 |  |
|       - |   895 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   896 | `	SySet sToken;` |
|       - |   897 | `	sxi32 rc;` |
|       - |   898 | `	/* Initialize the token set */` |
|    1934 |   899 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   900 | `	/* Preallocate some slots */` |
|    1934 |   901 | `	SySetAlloc(&sToken,0x08);` |
|       - |   902 | `	/* Tokenize the text */` |
|    1934 |   903 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   904 | `	/* Swap delimiter */` |
|    1934 |   905 | `	pTmpIn  = pGen->pIn;` |
|    1934 |   906 | `	pTmpEnd = pGen->pEnd;` |
|    1934 |   907 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1934 |   908 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   909 | `	/* Compile the expression */` |
|    1934 |   910 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   911 | `	/* Restore token stream */` |
|    1934 |   912 | `	pGen->pIn  = pTmpIn;` |
|    1934 |   913 | `	pGen->pEnd = pTmpEnd;` |
|       - |   914 | `	/* Release the token set */` |
|    1934 |   915 | `	SySetRelease(&sToken);` |
|       - |   916 | `	/* Compilation result */` |
|    1934 |   917 | `	return rc;` |
|       2 |   918 |  |
|       - |   919 | `/*` |
|       - |   920 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   921 | ` */` |
|   18024 |   922 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   923 |  |
|       - |   924 | `	ph7_value *pConstObj;` |
|   18026 |   925 | `	sxu32 nIdx = 0;` |
|       - |   926 | `	/* Reserve a new constant */` |
|   18026 |   927 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   18026 |   928 | `	if( pConstObj == 0 ){` |
|     ! 0 |   929 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   930 | `		return 0;` |
|       - |   931 | `	}` |
|   18026 |   932 | `	(*pCount)++;` |
|   18026 |   933 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   934 | `	/* Emit the load constant instruction */` |
|   18026 |   935 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18026 |   936 | `	return pConstObj;` |
|    9014 |   937 |  |
|       - |   938 | `/*` |
|       - |   939 | ` * Compile a double quoted/heredoc string.` |
|       - |   940 | ` * According to the PHP language reference manual` |
|       - |   941 | ` * Heredoc` |
|       - |   942 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |   943 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |   944 | ` *  to close the quotation.` |
|       - |   945 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |   946 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |   947 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |   948 | ` *  Warning` |
|       - |   949 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |   950 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |   951 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |   952 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |   953 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |   954 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |   955 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |   956 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |   957 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |   958 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |   959 | ` * Double quoted` |
|       - |   960 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |   961 | ` *  Escaped characters Sequence 	Meaning` |
|       - |   962 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |   963 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |   964 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |   965 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |   966 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |   967 | ` *  \\ backslash` |
|       - |   968 | ` *  \$ dollar sign` |
|       - |   969 | ` *  \" double-quote` |
|       - |   970 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |   971 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |   972 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |   973 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |   974 | ` * See string parsing for details.` |
|       - |   975 | ` */` |
|   16638 |   976 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |   977 |  |
|   16640 |   978 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |   979 | `	const char *zIn,*zCur,*zEnd;` |
|   16640 |   980 | `	ph7_value *pObj = 0;` |
|       - |   981 | `	sxi32 iCons;` |
|       - |   982 | `	sxi32 rc;` |
|       - |   983 | `	/* Delimit the string */` |
|   16640 |   984 | `	zIn  = pStr->zString;` |
|   16640 |   985 | `	zEnd = &zIn[pStr->nByte];` |
|   16640 |   986 | `	if( zIn >= zEnd ){` |
|       - |   987 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |   988 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |   989 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |   990 | `		 */` |
|     234 |   991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     234 |   992 | `		return SXRET_OK;` |
|       - |   993 | `	}` |
|   16408 |   994 | `	zCur = 0;` |
|       - |   995 | `	/* Compile the node */` |
|   16408 |   996 | `	iCons = 0;` |
|    9169 |   997 | `	for(;;){` |
|   27718 |   998 | `		zCur = zIn;` |
|  143056 |   999 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  117272 |  1000 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      59 |  1001 | `				break;` |
|  117158 |  1002 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1820 |  1003 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     910 |  1004 | `					break;` |
|       - |  1005 | `			}` |
|  115340 |  1006 | `			zIn++;` |
|       2 |  1007 | `		}` |
|   27718 |  1008 | `		if( zIn > zCur ){` |
|   12608 |  1009 | `			if( pObj == 0 ){` |
|   12332 |  1010 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12332 |  1011 | `				if( pObj == 0 ){` |
|     ! 0 |  1012 | `					return SXERR_ABORT;` |
|       - |  1013 | `				}` |
|    6165 |  1014 | `			}` |
|   12608 |  1015 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6303 |  1016 | `		}` |
|   27718 |  1017 | `		if( zIn >= zEnd ){` |
|   16408 |  1018 | `			break;` |
|       - |  1019 | `		}` |
|   11312 |  1020 | `		if( zIn[0] == '\\' ){` |
|    9380 |  1021 | `			const char *zPtr = 0;` |
|       - |  1022 | `			sxu32 n;` |
|    9380 |  1023 | `			zIn++;` |
|    9380 |  1024 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1025 | `				break;` |
|       - |  1026 | `			}` |
|    9380 |  1027 | `			if( pObj == 0 ){` |
|    5696 |  1028 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5696 |  1029 | `				if( pObj == 0 ){` |
|     ! 0 |  1030 | `					return SXERR_ABORT;` |
|       - |  1031 | `				}` |
|    2847 |  1032 | `			}` |
|    9380 |  1033 | `			n = sizeof(char); /* size of conversion */` |
|    9380 |  1034 | `			switch( zIn[0] ){` |
|       3 |  1035 | `			case '$':` |
|       - |  1036 | `				/* Dollar sign */` |
|       7 |  1037 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  1038 | `				break;` |
|      38 |  1039 | `			case '\\':` |
|       - |  1040 | `				/* A literal backslash */` |
|      78 |  1041 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 |  1042 | `				break;` |
|       2 |  1043 | `			case 'a':` |
|       - |  1044 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  1045 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  1046 | `				break;` |
|       2 |  1047 | `			case 'b':` |
|       - |  1048 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  1049 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  1050 | `				break;` |
|       4 |  1051 | `			case 'f':` |
|       - |  1052 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1053 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1054 | `				break;` |
|    4324 |  1055 | `			case 'n':` |
|       - |  1056 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8650 |  1057 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8650 |  1058 | `				break;` |
|      19 |  1059 | `			case 'r':` |
|       - |  1060 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 |  1061 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 |  1062 | `				break;` |
|      24 |  1063 | `			case 't':` |
|       - |  1064 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 |  1065 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 |  1066 | `				break;` |
|       3 |  1067 | `			case 'v':` |
|       - |  1068 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1069 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1070 | `				break;` |
|       1 |  1071 | `			case '\'':` |
|       - |  1072 | `				/* Single quote */` |
|       3 |  1073 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1074 | `				break;` |
|      50 |  1075 | `			case '"':` |
|       - |  1076 | `				/* Double quote */` |
|     102 |  1077 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 |  1078 | `				break;` |
|       5 |  1079 | `			case '0':` |
|       - |  1080 | `				/* NUL byte */` |
|      11 |  1081 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 |  1082 | `				break;` |
|     188 |  1083 | `			case 'x':` |
|     377 |  1084 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1085 | `					int c;` |
|       - |  1086 | `					/* Hex digit */` |
|     363 |  1087 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 |  1088 | `					if( &zIn[2] < zEnd ){` |
|     363 |  1089 | `						c +=  SyHexToint(zIn[2]);` |
|     181 |  1090 | `					}` |
|       - |  1091 | `					/* Output char */` |
|     363 |  1092 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 |  1093 | `					n += sizeof(char) * 2;` |
|     182 |  1094 | `				}else{` |
|       - |  1095 | `					/* Output literal character  */` |
|      15 |  1096 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1097 | `				}` |
|     377 |  1098 | `				break;` |
|      15 |  1099 | `			case 'o':` |
|      31 |  1100 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  1101 | `					/* Octal digit stream */` |
|       - |  1102 | `					int c;` |
|      21 |  1103 | `					c = 0;` |
|      21 |  1104 | `					zIn++;` |
|      61 |  1105 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  1106 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  1107 | `							break;` |
|       - |  1108 | `						}` |
|      41 |  1109 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  1110 | `					}` |
|      21 |  1111 | `					if ( c > 0 ){` |
|      15 |  1112 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  1113 | `					}` |
|      21 |  1114 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  1115 | `				}else{` |
|       - |  1116 | `					/* Output literal character  */` |
|      11 |  1117 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  1118 | `				}` |
|      31 |  1119 | `				break;` |
|      11 |  1120 | `			default:` |
|       - |  1121 | `				/* Output without a slash */` |
|      23 |  1122 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  1123 | `				break;` |
|       - |  1124 | `			}` |
|       - |  1125 | `			/* Advance the stream cursor */` |
|    9380 |  1126 | `			zIn += n;` |
|    9380 |  1127 | `			continue;` |
|       - |  1128 | `		}` |
|    1934 |  1129 | `		if( zIn[0] == '{' ){` |
|       - |  1130 | `			/* Curly syntax */` |
|       - |  1131 | `			const char *zExpr;` |
|     117 |  1132 | `			sxi32 iNest = 1;` |
|     117 |  1133 | `			zIn++;` |
|     117 |  1134 | `			zExpr = zIn;` |
|       - |  1135 | `			/* Synchronize with the next closing curly braces */` |
|    1243 |  1136 | `			while( zIn < zEnd ){` |
|    1243 |  1137 | `				if( zIn[0] == '{' ){` |
|       - |  1138 | `					/* Increment nesting level */` |
|       9 |  1139 | `					iNest++;` |
|    1239 |  1140 | `				}else if(zIn[0] == '}' ){` |
|       - |  1141 | `					/* Decrement nesting level */` |
|     125 |  1142 | `					iNest--;` |
|     125 |  1143 | `					if( iNest <= 0 ){` |
|     117 |  1144 | `						break;` |
|       - |  1145 | `					}` |
|       4 |  1146 | `				}` |
|    1127 |  1147 | `				zIn++;` |
|       1 |  1148 | `			}` |
|       - |  1149 | `			/* Process the expression */` |
|     117 |  1150 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     117 |  1151 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1152 | `				return SXERR_ABORT;` |
|       - |  1153 | `			}` |
|     117 |  1154 | `			if( rc != SXERR_EMPTY ){` |
|     117 |  1155 | `				++iCons;` |
|      58 |  1156 | `			}` |
|     117 |  1157 | `			if( zIn < zEnd ){` |
|       - |  1158 | `				/* Jump the trailing curly */` |
|     117 |  1159 | `				zIn++;` |
|      58 |  1160 | `			}` |
|      59 |  1161 | `		}else{` |
|       - |  1162 | `			/* Simple syntax */` |
|    1818 |  1163 | `			const char *zExpr = zIn;` |
|       - |  1164 | `			/* Assemble variable name */` |
|     914 |  1165 | `			for(;;){` |
|       - |  1166 | `				/* Jump leading dollars */` |
|    3646 |  1167 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1818 |  1168 | `					zIn++;` |
|       2 |  1169 | `				}` |
|     914 |  1170 | `				for(;;){` |
|   10730 |  1171 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7988 |  1172 | `						zIn++;` |
|       2 |  1173 | `					}` |
|    1830 |  1174 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1175 | `						/* UTF-8 stream */` |
|     ! 0 |  1176 | `						zIn++;` |
|     ! 0 |  1177 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1178 | `							zIn++;` |
|     ! 0 |  1179 | `						}` |
|     ! 0 |  1180 | `						continue;` |
|       - |  1181 | `					}` |
|    1830 |  1182 | `					break;` |
|     ! 0 |  1183 | `				}` |
|    1830 |  1184 | `				if( zIn >= zEnd ){` |
|     108 |  1185 | `					break;` |
|       - |  1186 | `				}` |
|    1724 |  1187 | `				if( zIn[0] == '[' ){` |
|       9 |  1188 | `					sxi32 iSquare = 1;` |
|       9 |  1189 | `					zIn++;` |
|      17 |  1190 | `					while( zIn < zEnd ){` |
|      17 |  1191 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1192 | `							iSquare++;` |
|      17 |  1193 | `						}else if (zIn[0] == ']' ){` |
|       9 |  1194 | `							iSquare--;` |
|       9 |  1195 | `							if( iSquare <= 0 ){` |
|       9 |  1196 | `								break;` |
|       - |  1197 | `							}` |
|     ! 0 |  1198 | `						}` |
|       9 |  1199 | `						zIn++;` |
|       1 |  1200 | `					}` |
|       9 |  1201 | `					if( zIn < zEnd ){` |
|       9 |  1202 | `						zIn++;` |
|       4 |  1203 | `					}` |
|       9 |  1204 | `					break;` |
|    1716 |  1205 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1206 | `					sxi32 iCurly = 1;` |
|       6 |  1207 | `					zIn++;` |
|      18 |  1208 | `					while( zIn < zEnd ){` |
|      16 |  1209 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1210 | `							iCurly++;` |
|      16 |  1211 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1212 | `							iCurly--;` |
|       3 |  1213 | `							if( iCurly <= 0 ){` |
|       3 |  1214 | `								break;` |
|       - |  1215 | `							}` |
|     ! 0 |  1216 | `						}` |
|      14 |  1217 | `						zIn++;` |
|       2 |  1218 | `					}` |
|       6 |  1219 | `					if( zIn < zEnd ){` |
|       3 |  1220 | `						zIn++;` |
|       1 |  1221 | `					}` |
|       6 |  1222 | `					break;` |
|    1712 |  1223 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1224 | `					/* Member access operator '->' */` |
|      13 |  1225 | `					zIn += 2;` |
|    1706 |  1226 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1227 | `					/* Static member access operator '::' */` |
|     ! 0 |  1228 | `					zIn += 2;` |
|     ! 0 |  1229 | `				}else{` |
|     851 |  1230 | `					break;` |
|       - |  1231 | `				}` |
|       1 |  1232 | `			}` |
|       - |  1233 | `			/* Process the expression */` |
|    1818 |  1234 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1818 |  1235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1236 | `				return SXERR_ABORT;` |
|       - |  1237 | `			}` |
|    1818 |  1238 | `			if( rc != SXERR_EMPTY ){` |
|    1816 |  1239 | `				++iCons;` |
|     907 |  1240 | `			}` |
|       - |  1241 | `		}` |
|       - |  1242 | `		/* Invalidate the previously used constant */` |
|    1934 |  1243 | `		pObj = 0;` |
|       2 |  1244 | `	}/*for(;;)*/` |
|   16408 |  1245 | `	if( iCons > 1 ){` |
|       - |  1246 | `		/* Concatenate all compiled constants */` |
|    1430 |  1247 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     714 |  1248 | `	}` |
|       - |  1249 | `	/* Node successfully compiled */` |
|   16408 |  1250 | `	return SXRET_OK;` |
|    8321 |  1251 |  |
|       - |  1252 | `/*` |
|       - |  1253 | ` * Compile a double quoted string.` |
|       - |  1254 | ` *  See the block-comment above for more information.` |
|       - |  1255 | ` */` |
|   16578 |  1256 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1257 |  |
|       - |  1258 | `	sxi32 rc;` |
|   16580 |  1259 | `	rc = GenStateCompileString(&(*pGen));` |
|    8289 |  1260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1261 | `	/* Compilation result */` |
|   16580 |  1262 | `	return rc;` |
|       2 |  1263 |  |
|       - |  1264 | `/*` |
|       - |  1265 | ` * Compile a Heredoc string.` |
|       - |  1266 | ` *  See the block-comment above for more information.` |
|       - |  1267 | ` */` |
|      64 |  1268 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1269 |  |
|       - |  1270 | `	SyString sOrig, sStripped;` |
|       - |  1271 | `	sxi32 rc;` |
|      66 |  1272 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      66 |  1273 | `	if( rc != SXRET_OK ){` |
|       5 |  1274 | `		return rc;` |
|       - |  1275 | `	}` |
|       - |  1276 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1277 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1278 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1279 | `	 * unaffected, including on the error path. */` |
|      62 |  1280 | `	sOrig = pGen->pIn->sData;` |
|      62 |  1281 | `	pGen->pIn->sData = sStripped;` |
|      62 |  1282 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 |  1283 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1284 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 |  1285 | `	return rc;` |
|      34 |  1286 |  |
|       - |  1287 | `/*` |
|       - |  1288 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1289 | ` *  Notes on array entries.` |
|       - |  1290 | ` *  According to the PHP language reference manual` |
|       - |  1291 | ` *  An array can be created by the array() language construct.` |
|       - |  1292 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1293 | ` *  array(  key =>  value` |
|       - |  1294 | ` *    , ...` |
|       - |  1295 | ` *    )` |
|       - |  1296 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1297 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1298 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1299 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1300 | ` *  contain integer and string indices.` |
|       - |  1301 | ` *  A value can be any PHP type.` |
|       - |  1302 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1303 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1304 | ` *  is specified, that value will be overwritten.` |
|       - |  1305 | ` */` |
|   16736 |  1306 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1307 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1308 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1309 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1310 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1311 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1312 | `	)` |
|       2 |  1313 |  |
|       - |  1314 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1315 | `	sxi32 rc;` |
|       - |  1316 | `	/* Swap token stream */` |
|   16738 |  1317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1318 | `	/* Compile the expression*/` |
|   16738 |  1319 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1320 | `	/* Restore token stream */` |
|   16738 |  1321 | `	RE_SWAP_DELIMITER(pGen);` |
|   16738 |  1322 | `	return rc;` |
|       2 |  1323 |  |
|       - |  1324 | `/*` |
|       - |  1325 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1326 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1327 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1328 | ` * error message.` |
|       - |  1329 | ` * See the routine responible of compiling the array language construct` |
|       - |  1330 | ` * for more inforation.` |
|       - |  1331 | ` */` |
|      30 |  1332 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1333 |  |
|      32 |  1334 | `	sxi32 rc = SXRET_OK;` |
|      32 |  1335 | `	if( pRoot->pOp ){` |
|      19 |  1336 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1337 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 |  1338 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1339 | `			/* Unexpected expression */` |
|      11 |  1340 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1341 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 |  1342 | `			if( rc != SXERR_ABORT ){` |
|      11 |  1343 | `				rc = SXERR_INVALID;` |
|       5 |  1344 | `			}` |
|       7 |  1345 | `		}` |
|      25 |  1346 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1347 | `		/* Unexpected expression */` |
|       3 |  1348 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1349 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1350 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1351 | `			rc = SXERR_INVALID;` |
|       1 |  1352 | `		}` |
|       1 |  1353 | `	}` |
|      32 |  1354 | `	return rc;` |
|       2 |  1355 |  |
|       - |  1356 | `/*` |
|       - |  1357 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1358 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1359 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1360 | ` */` |
|   24756 |  1361 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1362 |  |
|       - |  1363 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1364 | `	SyToken *pKey,*pCur;` |
|   24758 |  1365 | `	sxi32 iEmitRef = 0;` |
|   24758 |  1366 | `	sxi32 nPair = 0;` |
|       - |  1367 | `	sxi32 iNest;` |
|       - |  1368 | `	sxi32 rc;` |
|   24758 |  1369 | `	xValidator = 0;` |
|   20079 |  1370 | `	for(;;){` |
|       - |  1371 | `		/* Jump leading commas */` |
|   45350 |  1372 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5192 |  1373 | `			pGen->pIn++;` |
|       2 |  1374 | `		}` |
|   40160 |  1375 | `		pCur = pGen->pIn;` |
|   40160 |  1376 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1377 | `			/* No more entry to process */` |
|   24746 |  1378 | `			break;` |
|       - |  1379 | `		}` |
|   15416 |  1380 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1381 | `			continue;` |
|       - |  1382 | `		}` |
|       - |  1383 | `		/* Compile the key if available */` |
|   15416 |  1384 | `		pKey = pCur;` |
|   15416 |  1385 | `		iNest = 0;` |
|   42858 |  1386 | `		while( pCur < pGen->pIn ){` |
|   28664 |  1387 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 |  1388 | `				break;` |
|       - |  1389 | `			}` |
|       - |  1390 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1391 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1392 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1393 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1394 | `			 */` |
|   27448 |  1395 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      76 |  1396 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      76 |  1397 | `				SyToken *pFn = pCur;` |
|      74 |  1398 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 |  1399 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 |  1400 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1401 | `					pFn = &pCur[1];` |
|     ! 0 |  1402 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1403 | `				}` |
|      76 |  1404 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1405 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1406 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1407 | `						pCur++;` |
|     ! 0 |  1408 | `					}` |
|       5 |  1409 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1410 | `						pCur++;` |
|       5 |  1411 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1412 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1413 | `						if( pCur < pGen->pIn ){` |
|       5 |  1414 | `							pCur++;` |
|       2 |  1415 | `						}` |
|       2 |  1416 | `					}` |
|       5 |  1417 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1418 | `						pCur++;` |
|     ! 0 |  1419 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1420 | `							&& pCur->sData.nByte == 1` |
|     ! 0 |  1421 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1422 | `							pCur++;` |
|     ! 0 |  1423 | `						}` |
|     ! 0 |  1424 | `						if( pCur < pGen->pIn` |
|     ! 0 |  1425 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1426 | `							pCur++;` |
|     ! 0 |  1427 | `						}` |
|     ! 0 |  1428 | `					}` |
|       - |  1429 | `					/* The rest of the entry is the arrow function body — no` |
|       - |  1430 | `					 * outer key to extract. Stop the scan here. */` |
|       5 |  1431 | `					pCur = pGen->pIn;` |
|       5 |  1432 | `					break;` |
|       - |  1433 | `				}` |
|       - |  1434 | `				/* Match expression (PHP 8.0): 'match (subject) { ... }'.` |
|       - |  1435 | `				 * The '=>' inside match arms is not an array key/value separator —` |
|       - |  1436 | `				 * it introduces each arm's result expression. Skip past the full` |
|       - |  1437 | `				 * match span so the outer scan sees no false '=>'. */` |
|      72 |  1438 | `				if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1439 | `					pCur++; /* past 'match' */` |
|       3 |  1440 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1441 | `						pCur++;` |
|       3 |  1442 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1443 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1444 | `						if( pCur < pGen->pIn ){` |
|       3 |  1445 | `							pCur++;` |
|       1 |  1446 | `						}` |
|       1 |  1447 | `					}` |
|       3 |  1448 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1449 | `						pCur++;` |
|       3 |  1450 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1451 | `							PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1452 | `						if( pCur < pGen->pIn ){` |
|       3 |  1453 | `							pCur++;` |
|       1 |  1454 | `						}` |
|       1 |  1455 | `					}` |
|       3 |  1456 | `					continue;` |
|       - |  1457 | `				}` |
|      34 |  1458 | `			}` |
|   27442 |  1459 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 |  1460 | `				iNest++;` |
|   27404 |  1461 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - |  1462 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - |  1463 | `				 * parser will shortly detect any syntax error.` |
|       - |  1464 | `				 */` |
|      78 |  1465 | `				iNest--;` |
|      38 |  1466 | `			}` |
|   27442 |  1467 | `			pCur++;` |
|       2 |  1468 | `		}` |
|   15416 |  1469 | `		rc = SXERR_EMPTY;` |
|   15416 |  1470 | `		if( pCur < pGen->pIn ){` |
|    1218 |  1471 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1472 | `				/* Missing value */` |
|      11 |  1473 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 |  1474 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1475 | `					return SXERR_ABORT;` |
|       - |  1476 | `				}` |
|      11 |  1477 | `				return SXRET_OK;` |
|       - |  1478 | `			}` |
|       - |  1479 | `			/* Compile the expression holding the key */` |
|    1208 |  1480 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1481 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1208 |  1482 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1483 | `				return SXERR_ABORT;` |
|       - |  1484 | `			}` |
|    1208 |  1485 | `			pCur++; /* Jump the '=>' operator */` |
|   14803 |  1486 | `		}else if( pKey == pCur ){` |
|       - |  1487 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1488 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1489 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1490 | `		}else{` |
|       - |  1491 | `			/* Reset back the cursor and point to the entry value */` |
|   14200 |  1492 | `			pCur = pKey;` |
|       - |  1493 | `		}` |
|   15406 |  1494 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1495 | `			/* No available key,load NULL */` |
|   14202 |  1496 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7100 |  1497 | `		}` |
|   15406 |  1498 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1499 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 |  1500 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 |  1501 | `			iEmitRef = 1;` |
|      34 |  1502 | `			pCur++; /* Jump the '&' token */` |
|      34 |  1503 | `			if( pCur >= pGen->pIn ){` |
|       - |  1504 | `				/* Missing value */` |
|       3 |  1505 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1506 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1507 | `					return SXERR_ABORT;` |
|       - |  1508 | `				}` |
|       3 |  1509 | `				return SXRET_OK;` |
|       - |  1510 | `			}` |
|      15 |  1511 | `		}` |
|       - |  1512 | `		/* Compile indice value */` |
|   15404 |  1513 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15404 |  1514 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1515 | `			return SXERR_ABORT;` |
|       - |  1516 | `		}` |
|   15404 |  1517 | `		if( iEmitRef ){` |
|       - |  1518 | `			/* Emit the load reference instruction */` |
|      32 |  1519 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1520 | `		}` |
|   15404 |  1521 | `		xValidator = 0;` |
|   15404 |  1522 | `		iEmitRef = 0;` |
|   15404 |  1523 | `		nPair++;` |
|       2 |  1524 | `	}` |
|       - |  1525 | `	/* Emit the load map instruction */` |
|   24746 |  1526 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1527 | `	/* Node successfully compiled */` |
|   24746 |  1528 | `	return SXRET_OK;` |
|   12380 |  1529 |  |
|       - |  1530 | `/*` |
|       - |  1531 | ` * Compile the 'array' language construct.` |
|       - |  1532 | ` *	 According to the PHP language reference manual` |
|       - |  1533 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1534 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1535 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1536 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1537 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1538 | ` */` |
|   24460 |  1539 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1540 |  |
|       - |  1541 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   24462 |  1542 | `	pGen->pIn += 2;` |
|   24462 |  1543 | `	pGen->pEnd--;` |
|   12230 |  1544 | `	SXUNUSED(iCompileFlag);` |
|   24462 |  1545 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1546 |  |
|       - |  1547 | `/*` |
|       - |  1548 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1549 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1550 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1551 | ` */` |
|     296 |  1552 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1553 |  |
|       - |  1554 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     298 |  1555 | `	pGen->pIn++;` |
|     298 |  1556 | `	pGen->pEnd--;` |
|     148 |  1557 | `	SXUNUSED(iCompileFlag);` |
|     298 |  1558 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1559 |  |
|       - |  1560 | `/*` |
|       - |  1561 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1562 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1563 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1564 | ` * error message.` |
|       - |  1565 | ` * See the routine responible of compiling the list language construct` |
|       - |  1566 | ` * for more inforation.` |
|       - |  1567 | ` */` |
|     128 |  1568 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1569 |  |
|     130 |  1570 | `	sxi32 rc = SXRET_OK;` |
|     130 |  1571 | `	if( pRoot->pOp ){` |
|     ! 0 |  1572 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 |  1573 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1574 | `				/* Unexpected expression */` |
|     ! 0 |  1575 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1576 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1577 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1578 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1579 | `				}` |
|     ! 0 |  1580 | `		}` |
|     130 |  1581 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1582 | `		/* Unexpected expression */` |
|       5 |  1583 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1584 | `			"list(): Expecting a variable not an expression");` |
|       5 |  1585 | `		if( rc != SXERR_ABORT ){` |
|       5 |  1586 | `			rc = SXERR_INVALID;` |
|       2 |  1587 | `		}` |
|       2 |  1588 | `	}` |
|     130 |  1589 | `	return rc;` |
|       2 |  1590 |  |
|       - |  1591 | `/*` |
|       - |  1592 | ` * Compile the 'list' language construct.` |
|       - |  1593 | ` *  According to the PHP language reference` |
|       - |  1594 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1595 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1596 | ` *  Description` |
|       - |  1597 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1598 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1599 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1600 | ` *  Parameters` |
|       - |  1601 | ` *   $varname: A variable.` |
|       - |  1602 | ` *  Return Values` |
|       - |  1603 | ` *   The assigned array.` |
|       - |  1604 | ` */` |
|       - |  1605 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1606 | `struct NestedListEntry {` |
|       - |  1607 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1608 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1609 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1610 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1611 | `};` |
|       - |  1612 | `/*` |
|       - |  1613 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1614 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1615 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1616 | ` */` |
|      74 |  1617 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 |  1618 |  |
|       - |  1619 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1620 | `	SyToken *pNext;` |
|       - |  1621 | `	sxi32 nExpr;` |
|       - |  1622 | `	sxi32 rc;` |
|      76 |  1623 | `	nExpr = 0;` |
|      76 |  1624 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 |  1625 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 |  1626 | `		if( pGen->pIn < pNext ){` |
|       - |  1627 | `			/* Check for nested list() */` |
|     144 |  1628 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1629 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1630 | `				/* Record this nested list for post-processing */` |
|       3 |  1631 | `				SyToken *pListEnd = 0;` |
|       3 |  1632 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1633 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1634 | `				}` |
|       3 |  1635 | `				if( pListEnd ){` |
|       - |  1636 | `					struct NestedListEntry sEntry;` |
|       3 |  1637 | `					sEntry.nIndex = nExpr;` |
|       3 |  1638 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1639 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1640 | `					sEntry.isShort = 0;` |
|       3 |  1641 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1642 | `				}` |
|       - |  1643 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1644 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 |  1645 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1646 | `				/* Nested short destructuring [...] */` |
|      13 |  1647 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1648 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1649 | `				if( pBracketEnd ){` |
|       - |  1650 | `					struct NestedListEntry sEntry;` |
|      13 |  1651 | `					sEntry.nIndex = nExpr;` |
|      13 |  1652 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1653 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1654 | `					sEntry.isShort = 1;` |
|      13 |  1655 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1656 | `				}` |
|       - |  1657 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1658 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1659 | `			}else{` |
|       - |  1660 | `				/* Compile the expression holding the variable */` |
|     130 |  1661 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 |  1662 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1663 | `					SySetRelease(&sNested);` |
|     ! 0 |  1664 | `					return SXRET_OK;` |
|       - |  1665 | `				}` |
|       - |  1666 | `			}` |
|      73 |  1667 | `		}else{` |
|       - |  1668 | `			/* Empty entry,load NULL */` |
|      13 |  1669 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1670 | `		}` |
|     156 |  1671 | `		nExpr++;` |
|       - |  1672 | `		/* Advance the stream cursor */` |
|     156 |  1673 | `		pGen->pIn = &pNext[1];` |
|       2 |  1674 | `	}` |
|       - |  1675 | `	/* Emit the LOAD_LIST instruction */` |
|      76 |  1676 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1677 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1678 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1679 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1680 | `	 */` |
|      76 |  1681 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1682 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1683 | `		sxu32 i;` |
|      27 |  1684 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1685 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1686 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1687 | `			ph7_value *pIdx;` |
|       - |  1688 | `			sxu32 nConstIdx;` |
|       - |  1689 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1690 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1691 | `			/* Push the integer index for this nested entry */` |
|      15 |  1692 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1693 | `			if( pIdx == 0 ){` |
|     ! 0 |  1694 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1695 | `				SySetRelease(&sNested);` |
|     ! 0 |  1696 | `				return SXERR_ABORT;` |
|       - |  1697 | `			}` |
|      15 |  1698 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1699 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1700 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1701 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1702 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1703 | `			 */` |
|      15 |  1704 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1705 | `			/* Recursively compile the inner list */` |
|      15 |  1706 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1707 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1708 | `			if( apNested[i].isShort ){` |
|      13 |  1709 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1710 | `			}else{` |
|       3 |  1711 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1712 | `			}` |
|      15 |  1713 | `			pGen->pIn = pSavedIn;` |
|      15 |  1714 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1715 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1716 | `				SySetRelease(&sNested);` |
|     ! 0 |  1717 | `				return SXERR_ABORT;` |
|       - |  1718 | `			}` |
|       - |  1719 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1720 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1721 | `		}` |
|       6 |  1722 | `	}` |
|      76 |  1723 | `	SySetRelease(&sNested);` |
|       - |  1724 | `	/* Node successfully compiled */` |
|      76 |  1725 | `	return SXRET_OK;` |
|      39 |  1726 |  |
|      32 |  1727 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1728 |  |
|       - |  1729 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 |  1730 | `	pGen->pIn += 2;` |
|      34 |  1731 | `	pGen->pEnd--;` |
|      16 |  1732 | `	SXUNUSED(iCompileFlag);` |
|      34 |  1733 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1734 |  |
|      42 |  1735 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1736 |  |
|       - |  1737 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 |  1738 | `	pGen->pIn++;` |
|      44 |  1739 | `	pGen->pEnd--;` |
|      21 |  1740 | `	SXUNUSED(iCompileFlag);` |
|      44 |  1741 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1742 |  |
|       - |  1743 | `/* Forward declarations */` |
|       - |  1744 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1745 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1746 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1747 | `/*` |
|       - |  1748 | ` * Compile an annoynmous function or a closure.` |
|       - |  1749 | ` * According to the PHP language reference` |
|       - |  1750 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1751 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1752 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1753 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1754 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1755 | ` *  Example Anonymous function variable assignment example` |
|       - |  1756 | ` * <?php` |
|       - |  1757 | ` * $greet = function($name)` |
|       - |  1758 | ` * {` |
|       - |  1759 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1760 | ` * };` |
|       - |  1761 | ` * $greet('World');` |
|       - |  1762 | ` * $greet('PHP');` |
|       - |  1763 | ` * ?>` |
|       - |  1764 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1765 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1766 | ` */` |
|     176 |  1767 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1768 |  |
|       - |  1769 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1770 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1771 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1772 | `							  * one thread is allowed to compile the script.` |
|       - |  1773 | `						      */` |
|       - |  1774 | `	ph7_value *pObj;` |
|       - |  1775 | `	SyString sName;` |
|       - |  1776 | `	sxu32 nIdx;` |
|       - |  1777 | `	sxu32 nLen;` |
|       - |  1778 | `	sxi32 rc;` |
|       - |  1779 |  |
|     178 |  1780 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     178 |  1781 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1782 | `		pGen->pIn++;` |
|     ! 0 |  1783 | `	}` |
|       - |  1784 | `	/* Reserve a constant for the lambda */` |
|     178 |  1785 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     178 |  1786 | `	if( pObj == 0 ){` |
|     ! 0 |  1787 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1788 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1789 | `		return SXERR_ABORT;` |
|       - |  1790 | `	}` |
|       - |  1791 | `	/* Generate a unique name */` |
|     178 |  1792 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1793 | `	/* Make sure the generated name is unique */` |
|     178 |  1794 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1795 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1796 | `	}` |
|     178 |  1797 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     178 |  1798 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1799 | `	/* Compile the lambda body */` |
|     178 |  1800 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     178 |  1801 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1802 | `		return SXERR_ABORT;` |
|       - |  1803 | `	}` |
|     178 |  1804 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1805 | `		/* Emit the load closure instruction */` |
|      16 |  1806 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 |  1807 | `	}else{` |
|       - |  1808 | `		/* Emit the load constant instruction */` |
|     164 |  1809 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1810 | `	}` |
|       - |  1811 | `	/* Node successfully compiled */` |
|     178 |  1812 | `	return SXRET_OK;` |
|      90 |  1813 |  |
|       - |  1814 | `/*` |
|       - |  1815 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1816 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1817 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1818 | ` */` |
|     120 |  1819 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1820 | `	ph7_gen_state *pGen,` |
|       - |  1821 | `	ph7_vm_func *pFunc,` |
|       - |  1822 | `	const char *zName,` |
|       - |  1823 | `	sxu32 nByte,` |
|       - |  1824 | `	SyString *aShadow,` |
|       - |  1825 | `	sxu32 nShadow)` |
|       1 |  1826 |  |
|       - |  1827 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1828 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1829 | `	sxu32 n, nEnv;` |
|       - |  1830 | `	char *zDup;` |
|     121 |  1831 | `	if( nByte == 0 ){` |
|     ! 0 |  1832 | `		return SXRET_OK;` |
|       - |  1833 | `	}` |
|     120 |  1834 | `	if( nByte == sizeof("this")-1` |
|      65 |  1835 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1836 | `		return SXRET_OK;` |
|       - |  1837 | `	}` |
|     145 |  1838 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      92 |  1839 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      88 |  1840 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      67 |  1841 | `			return SXRET_OK;` |
|       - |  1842 | `		}` |
|      14 |  1843 | `	}` |
|      53 |  1844 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1845 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1846 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1847 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1848 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1849 | `			return SXRET_OK;` |
|       - |  1850 | `		}` |
|      15 |  1851 | `	}` |
|      53 |  1852 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1853 | `	if( zDup == 0 ){` |
|     ! 0 |  1854 | `		return SXERR_ABORT;` |
|       - |  1855 | `	}` |
|      53 |  1856 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1857 | `	sEnv.iFlags = 0;` |
|      53 |  1858 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1859 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1860 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1861 | `	return SXRET_OK;` |
|      61 |  1862 |  |
|       - |  1863 | `/*` |
|       - |  1864 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1865 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1866 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1867 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1868 | ` */` |
|      14 |  1869 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1870 | `	ph7_gen_state *pGen,` |
|       - |  1871 | `	ph7_vm_func *pFunc,` |
|       - |  1872 | `	const char *zIn,` |
|       - |  1873 | `	const char *zEnd,` |
|       - |  1874 | `	SyString *aShadow,` |
|       - |  1875 | `	sxu32 nShadow)` |
|       1 |  1876 |  |
|       - |  1877 | `	sxi32 rc;` |
|     159 |  1878 | `	while( zIn < zEnd ){` |
|     145 |  1879 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1880 | `			zIn++;` |
|     ! 0 |  1881 | `			if( zIn < zEnd ){` |
|     ! 0 |  1882 | `				zIn++;` |
|     ! 0 |  1883 | `			}` |
|     ! 0 |  1884 | `			continue;` |
|       - |  1885 | `		}` |
|     144 |  1886 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1887 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1888 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1889 | `			const char *zName;` |
|      13 |  1890 | `			zIn++; /* skip '$' */` |
|      13 |  1891 | `			zName = zIn;` |
|      39 |  1892 | `			while( zIn < zEnd ){` |
|      35 |  1893 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1894 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1895 | `					zIn++;` |
|     ! 0 |  1896 | `					while( zIn < zEnd` |
|     ! 0 |  1897 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1898 | `						zIn++;` |
|     ! 0 |  1899 | `					}` |
|     ! 0 |  1900 | `					continue;` |
|       - |  1901 | `				}` |
|      35 |  1902 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1903 | `					break;` |
|       - |  1904 | `				}` |
|      27 |  1905 | `				zIn++;` |
|       1 |  1906 | `			}` |
|      13 |  1907 | `			if( zIn > zName ){` |
|      19 |  1908 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1909 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1910 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1911 | `					return SXERR_ABORT;` |
|       - |  1912 | `				}` |
|       6 |  1913 | `			}` |
|      13 |  1914 | `			continue;` |
|       - |  1915 | `		}` |
|     133 |  1916 | `		zIn++;` |
|       1 |  1917 | `	}` |
|      15 |  1918 | `	return SXRET_OK;` |
|       8 |  1919 |  |
|       - |  1920 | `/*` |
|       - |  1921 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1922 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1923 | ` *   - plain $<id> pairs` |
|       - |  1924 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1925 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1926 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1927 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1928 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1929 | ` *     are never mistakenly captured.` |
|       - |  1930 | ` */` |
|     102 |  1931 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1932 | `	ph7_gen_state *pGen,` |
|       - |  1933 | `	ph7_vm_func *pFunc,` |
|       - |  1934 | `	SyToken *pStart,` |
|       - |  1935 | `	SyToken *pEnd,` |
|       - |  1936 | `	SyString *aShadow,` |
|       - |  1937 | `	sxu32 nShadow)` |
|       1 |  1938 |  |
|     103 |  1939 | `	SyToken *pScan = pStart;` |
|       - |  1940 | `	sxi32 rc;` |
|     371 |  1941 | `	while( pScan < pEnd ){` |
|     269 |  1942 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1943 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1944 | `				pScan->sData.zString,` |
|      14 |  1945 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1946 | `				aShadow,nShadow);` |
|      15 |  1947 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1948 | `				return SXERR_ABORT;` |
|       - |  1949 | `			}` |
|      15 |  1950 | `			pScan++;` |
|      15 |  1951 | `			continue;` |
|       - |  1952 | `		}` |
|     255 |  1953 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      19 |  1954 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      19 |  1955 | `			SyToken *pFnKw = pScan;` |
|      18 |  1956 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  1957 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  1958 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1959 | `				pFnKw = &pScan[1];` |
|     ! 0 |  1960 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1961 | `			}` |
|      19 |  1962 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  1963 | `				SyToken *pInnerSigStart;` |
|       - |  1964 | `				SyToken *pInnerSigEnd;` |
|       - |  1965 | `				SyToken *pInnerBodyEnd;` |
|       - |  1966 | `				SyString *aInnerShadow;` |
|       - |  1967 | `				sxu32 nInnerShadow;` |
|       - |  1968 | `				sxu32 nInnerParamMax;` |
|       - |  1969 | `				SyToken *p;` |
|       - |  1970 | `				int iNestInner;` |
|      19 |  1971 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  1972 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1973 | `					pScan++;` |
|     ! 0 |  1974 | `				}` |
|      19 |  1975 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  1976 | `					pScan++;` |
|     ! 0 |  1977 | `					continue;` |
|       - |  1978 | `				}` |
|      19 |  1979 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  1980 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  1981 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  1982 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  1983 | `					pScan = pEnd;` |
|     ! 0 |  1984 | `					continue;` |
|       - |  1985 | `				}` |
|       - |  1986 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  1987 | `				nInnerParamMax = 0;` |
|      57 |  1988 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  1989 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  1990 | `						nInnerParamMax++;` |
|       6 |  1991 | `					}` |
|      20 |  1992 | `				}` |
|      19 |  1993 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  1994 | `					&pGen->pVm->sAllocator,` |
|      18 |  1995 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  1996 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  1997 | `					return SXERR_ABORT;` |
|       - |  1998 | `				}` |
|      19 |  1999 | `				nInnerShadow = 0;` |
|      25 |  2000 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2001 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2002 | `				}` |
|      57 |  2003 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2004 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2005 | `						continue;` |
|       - |  2006 | `					}` |
|      13 |  2007 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2008 | `						break;` |
|       - |  2009 | `					}` |
|      13 |  2010 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2011 | `						continue;` |
|       - |  2012 | `					}` |
|      13 |  2013 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2014 | `				}` |
|      19 |  2015 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2016 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2017 | `					pScan++;` |
|     ! 0 |  2018 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2019 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2020 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2021 | `						pScan++;` |
|     ! 0 |  2022 | `					}` |
|     ! 0 |  2023 | `					if( pScan < pEnd` |
|     ! 0 |  2024 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2025 | `						pScan++;` |
|     ! 0 |  2026 | `					}` |
|     ! 0 |  2027 | `				}` |
|      19 |  2028 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2029 | `					pScan++; /* past '=>' */` |
|       9 |  2030 | `				}` |
|      19 |  2031 | `				pInnerBodyEnd = pScan;` |
|      19 |  2032 | `				iNestInner = 0;` |
|     131 |  2033 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2034 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2035 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2036 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2037 | `						break;` |
|       - |  2038 | `					}` |
|     113 |  2039 | `					if( pInnerBodyEnd->nType &` |
|       - |  2040 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2041 | `						iNestInner++;` |
|     112 |  2042 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2043 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2044 | `						iNestInner--;` |
|       1 |  2045 | `					}` |
|     113 |  2046 | `					pInnerBodyEnd++;` |
|       1 |  2047 | `				}` |
|       - |  2048 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2049 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2050 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2051 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2052 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2053 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2054 | `				 *` |
|       - |  2055 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2056 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2057 | `				 * range after the '=' sign. */` |
|       - |  2058 | `				{` |
|      19 |  2059 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2060 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2061 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2062 | `						SyToken *pEq = 0;` |
|      13 |  2063 | `						int iNestArg = 0;` |
|      49 |  2064 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2065 | `							if( iNestArg == 0` |
|      39 |  2066 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2067 | `								break;` |
|       - |  2068 | `							}` |
|      37 |  2069 | `							if( pArgEnd->nType &` |
|       - |  2070 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2071 | `								iNestArg++;` |
|      37 |  2072 | `							}else if( pArgEnd->nType &` |
|       - |  2073 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2074 | `								iNestArg--;` |
|     ! 0 |  2075 | `							}` |
|      36 |  2076 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2077 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2078 | `								pEq = pArgEnd;` |
|       3 |  2079 | `							}` |
|      37 |  2080 | `							pArgEnd++;` |
|       1 |  2081 | `						}` |
|      13 |  2082 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2083 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2084 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2085 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2086 | `								return SXERR_ABORT;` |
|       - |  2087 | `							}` |
|       3 |  2088 | `						}` |
|      13 |  2089 | `						pArgStart = pArgEnd;` |
|      12 |  2090 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2091 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2092 | `							pArgStart++;` |
|       1 |  2093 | `						}` |
|       1 |  2094 | `					}` |
|       - |  2095 | `				}` |
|      28 |  2096 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2097 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2098 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2099 | `					return SXERR_ABORT;` |
|       - |  2100 | `				}` |
|      19 |  2101 | `				pScan = pInnerBodyEnd;` |
|      19 |  2102 | `				continue;` |
|       - |  2103 | `			}` |
|     ! 0 |  2104 | `		}` |
|     237 |  2105 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     129 |  2106 | `			pScan++;` |
|     129 |  2107 | `			continue;` |
|       - |  2108 | `		}` |
|       - |  2109 | `		{` |
|       - |  2110 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     109 |  2111 | `			SyToken *pDollar = pScan;` |
|     162 |  2112 | `			while( &pDollar[1] < pEnd` |
|     109 |  2113 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2114 | `				pDollar++;` |
|     ! 0 |  2115 | `			}` |
|     109 |  2116 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2117 | `				break;` |
|       - |  2118 | `			}` |
|     109 |  2119 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2120 | `				pScan = pDollar + 1;` |
|     ! 0 |  2121 | `				continue;` |
|       - |  2122 | `			}` |
|     163 |  2123 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     108 |  2124 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      54 |  2125 | `				aShadow,nShadow);` |
|     109 |  2126 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2127 | `				return SXERR_ABORT;` |
|       - |  2128 | `			}` |
|     109 |  2129 | `			pScan = pDollar + 2;` |
|       - |  2130 | `		}` |
|       1 |  2131 | `	}` |
|     103 |  2132 | `	return SXRET_OK;` |
|      52 |  2133 |  |
|       - |  2134 | `/*` |
|       - |  2135 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2136 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2137 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2138 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2139 | ` * $this is also made available.` |
|       - |  2140 | ` */` |
|      84 |  2141 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2142 |  |
|       - |  2143 | `	ph7_vm_func *pFunc;` |
|       - |  2144 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2145 | `	GenBlock *pBlock;` |
|       - |  2146 | `	SySet *pInstrContainer;` |
|       - |  2147 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2148 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2149 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2150 | `	SyToken *pSavedEnd;` |
|       - |  2151 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2152 | `	char zName[512];` |
|       - |  2153 | `	static int iCnt = 1;` |
|       - |  2154 | `	char *zDup;` |
|       - |  2155 | `	sxu32 nLen;` |
|       - |  2156 | `	sxu32 nLine;` |
|      86 |  2157 | `	sxi32 iFlags = 0;` |
|      86 |  2158 | `	int bStatic = 0;` |
|       - |  2159 | `	sxi32 rc;` |
|       - |  2160 | `	sxu32 n;` |
|      42 |  2161 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2162 |  |
|      86 |  2163 | `	nLine = pGen->pIn->nLine;` |
|       - |  2164 | `	/* Optional 'static' prefix */` |
|      84 |  2165 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      86 |  2166 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2167 | `		bStatic = 1;` |
|       3 |  2168 | `		pGen->pIn++;` |
|       1 |  2169 | `	}` |
|       - |  2170 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      84 |  2171 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      86 |  2172 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2173 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2174 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2175 | `		return SXERR_SYNTAX;` |
|       - |  2176 | `	}` |
|      86 |  2177 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2178 | `	/* Optional '&' — return by reference */` |
|      86 |  2179 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2180 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2181 | `		pGen->pIn++;` |
|     ! 0 |  2182 | `	}` |
|       - |  2183 | `	/* Expect '(' */` |
|      86 |  2184 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2185 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2186 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2187 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2188 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2189 | `		}else{` |
|     ! 0 |  2190 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2191 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2192 | `		}` |
|       3 |  2193 | `		return SXERR_SYNTAX;` |
|       - |  2194 | `	}` |
|      84 |  2195 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2196 | `	/* Delimit the parameter list */` |
|      84 |  2197 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      84 |  2198 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2199 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2200 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2201 | `		return SXERR_SYNTAX;` |
|       - |  2202 | `	}` |
|       - |  2203 | `	/* Allocate the function state */` |
|      82 |  2204 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      82 |  2205 | `	if( pFunc == 0 ){` |
|     ! 0 |  2206 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2207 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2208 | `		return SXERR_ABORT;` |
|       - |  2209 | `	}` |
|       - |  2210 | `	/* Generate a unique lambda name */` |
|      82 |  2211 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     166 |  2212 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      85 |  2213 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 |  2214 | `	}` |
|      82 |  2215 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      82 |  2216 | `	if( zDup == 0 ){` |
|     ! 0 |  2217 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2218 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2219 | `		return SXERR_ABORT;` |
|       - |  2220 | `	}` |
|      82 |  2221 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2222 | `	/* Collect function arguments */` |
|      82 |  2223 | `	if( pGen->pIn < pSigEnd ){` |
|      52 |  2224 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd);` |
|      52 |  2225 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2226 | `			return SXERR_ABORT;` |
|       - |  2227 | `		}` |
|      25 |  2228 | `	}` |
|       - |  2229 | `	/* Point past ')' and parse optional return type */` |
|      82 |  2230 | `	pGen->pIn = &pSigEnd[1];` |
|      82 |  2231 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      82 |  2232 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2233 | `		return SXERR_ABORT;` |
|      82 |  2234 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2235 | `		return SXERR_SYNTAX;` |
|       - |  2236 | `	}` |
|       - |  2237 | `	/* Expect '=>' */` |
|      82 |  2238 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2239 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2240 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2241 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2242 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2243 | `		}else{` |
|     ! 0 |  2244 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2245 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2246 | `		}` |
|       3 |  2247 | `		return SXERR_SYNTAX;` |
|       - |  2248 | `	}` |
|      79 |  2249 | `	pGen->pIn++; /* Jump '=>' */` |
|      79 |  2250 | `	pBodyStart = pGen->pIn;` |
|      79 |  2251 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2252 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2253 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2254 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2255 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      79 |  2256 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2257 | `	{` |
|      79 |  2258 | `		SyString *aShadow = 0;` |
|      79 |  2259 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      79 |  2260 | `		if( nShadow > 0 ){` |
|      49 |  2261 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      48 |  2262 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      49 |  2263 | `			if( aShadow == 0 ){` |
|     ! 0 |  2264 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2265 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2266 | `				return SXERR_ABORT;` |
|       - |  2267 | `			}` |
|     103 |  2268 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      55 |  2269 | `				aShadow[n] = aArgs[n].sName;` |
|      28 |  2270 | `			}` |
|      24 |  2271 | `		}` |
|     118 |  2272 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      39 |  2273 | `			aShadow,nShadow);` |
|      79 |  2274 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2275 | `			return SXERR_ABORT;` |
|       - |  2276 | `		}` |
|       - |  2277 | `	}` |
|       - |  2278 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2279 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2280 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2281 | `	 * $this. */` |
|      79 |  2282 | `	if( !bStatic ){` |
|       - |  2283 | `		char *zThisDup;` |
|      77 |  2284 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      77 |  2285 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2286 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2287 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2288 | `			return SXERR_ABORT;` |
|       - |  2289 | `		}` |
|      77 |  2290 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      77 |  2291 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      77 |  2292 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      77 |  2293 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      77 |  2294 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      38 |  2295 | `	}` |
|       - |  2296 | `	/* Arrow functions are always closures */` |
|      79 |  2297 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2298 | `	/* Compile the body expression as an implicit return */` |
|     118 |  2299 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      39 |  2300 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      79 |  2301 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2302 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2303 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2304 | `		return SXERR_ABORT;` |
|       - |  2305 | `	}` |
|      79 |  2306 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      79 |  2307 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      79 |  2308 | `	pSavedEnd = pGen->pEnd;` |
|      79 |  2309 | `	pGen->pIn = pBodyStart;` |
|      79 |  2310 | `	pGen->pEnd = pBodyEnd;` |
|      79 |  2311 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      79 |  2312 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2313 | `		return SXERR_ABORT;` |
|       - |  2314 | `	}` |
|       - |  2315 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack' */` |
|      79 |  2316 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      79 |  2317 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      79 |  2318 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2319 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      79 |  2320 | `	pGen->pIn = pBodyEnd;` |
|      79 |  2321 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2322 | `	/* Emit the load-closure instruction */` |
|      79 |  2323 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      79 |  2324 | `	return SXRET_OK;` |
|      44 |  2325 |  |
|       - |  2326 | `/*` |
|       - |  2327 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2328 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2329 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2330 | ` * expression's value.` |
|       - |  2331 | ` */` |
|     340 |  2332 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2333 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       2 |  2334 |  |
|       - |  2335 | `	SySet *pInstrContainer;` |
|       - |  2336 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2337 | `	sxi32 rc;` |
|     342 |  2338 | `	pTmpIn  = pGen->pIn;` |
|     342 |  2339 | `	pTmpEnd = pGen->pEnd;` |
|     342 |  2340 | `	pGen->pIn  = pStart;` |
|     342 |  2341 | `	pGen->pEnd = pStop;` |
|     342 |  2342 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     342 |  2343 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|     342 |  2344 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     342 |  2345 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     342 |  2346 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     342 |  2347 | `	pGen->pIn  = pTmpIn;` |
|     342 |  2348 | `	pGen->pEnd = pTmpEnd;` |
|     342 |  2349 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2350 | `		return SXERR_ABORT;` |
|       - |  2351 | `	}` |
|     342 |  2352 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2353 | `		return SXERR_EMPTY;` |
|       - |  2354 | `	}` |
|     342 |  2355 | `	return SXRET_OK;` |
|     172 |  2356 |  |
|       - |  2357 | `/*` |
|       - |  2358 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2359 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2360 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2361 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2362 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2363 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2364 | ` */` |
|       - |  2365 | `/*` |
|       - |  2366 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2367 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2368 | ` * caller can bail out of the current expression.` |
|       - |  2369 | ` */` |
|       2 |  2370 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2371 |  |
|       - |  2372 | `	va_list ap;` |
|       - |  2373 | `	sxi32 rc;` |
|       - |  2374 | `	SyBlob sMsg;` |
|       3 |  2375 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2376 | `	va_start(ap,zFmt);` |
|       3 |  2377 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2378 | `	va_end(ap);` |
|       3 |  2379 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2380 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2381 | `	SyBlobRelease(&sMsg);` |
|       3 |  2382 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2383 | `		return SXERR_ABORT;` |
|       - |  2384 | `	}` |
|       3 |  2385 | `	return SXERR_SYNTAX;` |
|       2 |  2386 |  |
|       - |  2387 | `/*` |
|       - |  2388 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2389 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2390 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2391 | ` */` |
|     342 |  2392 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       2 |  2393 |  |
|     344 |  2394 | `	SyToken *pCur = pStart;` |
|     344 |  2395 | `	int iNest = 0;` |
|     790 |  2396 | `	while( pCur < pEnd ){` |
|     756 |  2397 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      11 |  2398 | `			iNest++;` |
|     751 |  2399 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      11 |  2400 | `			iNest--;` |
|     741 |  2401 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     310 |  2402 | `			return pCur;` |
|       - |  2403 | `		}` |
|     448 |  2404 | `		pCur++;` |
|       2 |  2405 | `	}` |
|      36 |  2406 | `	return pEnd;` |
|     173 |  2407 |  |
|      68 |  2408 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2409 |  |
|       - |  2410 | `	ph7_match *pMatch;` |
|       - |  2411 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      70 |  2412 | `	int bHasDefault = 0;` |
|       - |  2413 | `	sxu32 nLine;` |
|       - |  2414 | `	sxi32 rc;` |
|      34 |  2415 | `	SXUNUSED(iCompileFlag);` |
|      70 |  2416 | `	nLine = pGen->pIn->nLine;` |
|      70 |  2417 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2418 | `	/* Expect '(' */` |
|      70 |  2419 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2420 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2421 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2422 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2423 | `	}` |
|      70 |  2424 | `	pGen->pIn++; /* Jump '(' */` |
|      70 |  2425 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      70 |  2426 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2427 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2428 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2429 | `	}` |
|      70 |  2430 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2431 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2432 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2433 | `	}` |
|       - |  2434 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      70 |  2435 | `	pSavedEnd = pGen->pEnd;` |
|      70 |  2436 | `	pGen->pEnd = pSubjEnd;` |
|      70 |  2437 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      70 |  2438 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2439 | `		return SXERR_ABORT;` |
|       - |  2440 | `	}` |
|      70 |  2441 | `	pGen->pEnd = pSavedEnd;` |
|      70 |  2442 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2443 | `	/* Expect '{' */` |
|      70 |  2444 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2445 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2446 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2447 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2448 | `	}` |
|      70 |  2449 | `	pGen->pIn++; /* Jump '{' */` |
|      70 |  2450 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      70 |  2451 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2452 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2453 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2454 | `	}` |
|       - |  2455 | `	/* Allocate ph7_match container */` |
|      70 |  2456 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      70 |  2457 | `	if( pMatch == 0 ){` |
|     ! 0 |  2458 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2459 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2460 | `		return SXERR_ABORT;` |
|       - |  2461 | `	}` |
|      70 |  2462 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      70 |  2463 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2464 | `	/* Iterate arms */` |
|     244 |  2465 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2466 | `		ph7_match_arm sArm;` |
|       - |  2467 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     180 |  2468 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     180 |  2469 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     180 |  2470 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     180 |  2471 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2472 | `		/* 'default' arm? */` |
|     178 |  2473 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     100 |  2474 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      20 |  2475 | `			if( bHasDefault ){` |
|       3 |  2476 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2477 | `					"Match expressions may only contain one default arm");` |
|       4 |  2478 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2479 | `			}` |
|      18 |  2480 | `			sArm.bDefault = 1;` |
|      18 |  2481 | `			bHasDefault = 1;` |
|      18 |  2482 | `			pGen->pIn++;` |
|      18 |  2483 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2484 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2485 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2486 | `			}` |
|      18 |  2487 | `			pGen->pIn++; /* Jump '=>' */` |
|      10 |  2488 | `		}else{` |
|       - |  2489 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     162 |  2490 | `			pCondStart = pGen->pIn;` |
|     162 |  2491 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2492 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     170 |  2493 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2494 | `				SySet sCondBc;` |
|       9 |  2495 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2496 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2497 | `						"syntax error, empty match condition expression");` |
|       - |  2498 | `				}` |
|       9 |  2499 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2500 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2501 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2502 | `					return SXERR_ABORT;` |
|       - |  2503 | `				}` |
|       9 |  2504 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2505 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2506 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2507 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2508 | `			}` |
|     162 |  2509 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2510 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2511 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2512 | `			}` |
|     160 |  2513 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2514 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2515 | `					"syntax error, empty match condition expression");` |
|       - |  2516 | `			}` |
|       - |  2517 | `			{` |
|       - |  2518 | `				SySet sCondBc;` |
|     160 |  2519 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     160 |  2520 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     160 |  2521 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2522 | `					return SXERR_ABORT;` |
|       - |  2523 | `				}` |
|     160 |  2524 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2525 | `			}` |
|     160 |  2526 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2527 | `		}` |
|       - |  2528 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     176 |  2529 | `		pResStart = pGen->pIn;` |
|     176 |  2530 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     176 |  2531 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2532 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2533 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2534 | `		}` |
|     176 |  2535 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     176 |  2536 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2537 | `			return SXERR_ABORT;` |
|       - |  2538 | `		}` |
|     176 |  2539 | `		pGen->pIn = pResEnd;` |
|     176 |  2540 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     144 |  2541 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      71 |  2542 | `		}` |
|     176 |  2543 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       2 |  2544 | `	}` |
|      66 |  2545 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      66 |  2546 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      66 |  2547 | `	return SXRET_OK;` |
|      36 |  2548 |  |
|       - |  2549 | `/*` |
|       - |  2550 | ` * Compile a backtick quoted string.` |
|       - |  2551 | ` */` |
|       4 |  2552 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  2553 |  |
|       - |  2554 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2555 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2556 | `	 */` |
|       7 |  2557 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2558 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2559 | `		ph7_lib_version()` |
|       - |  2560 | `		);` |
|       - |  2561 | `	/* Load NULL */` |
|       5 |  2562 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2563 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2564 | `	/* Node successfully compiled */` |
|       5 |  2565 | `	return SXRET_OK;` |
|       1 |  2566 |  |
|       - |  2567 | `/*` |
|       - |  2568 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2569 | ` * construct.` |
|       - |  2570 | ` */` |
|      72 |  2571 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2572 |  |
|       - |  2573 | `	SyString *pName;` |
|       - |  2574 | `	sxu32 nKeyID;` |
|       - |  2575 | `	sxi32 rc;` |
|       - |  2576 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 |  2577 | `	pName = &pGen->pIn->sData;` |
|      74 |  2578 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 |  2579 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 |  2580 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2581 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2582 | `		/* Compile arguments one after one */` |
|       9 |  2583 | `		pTmp = pGen->pEnd;` |
|       - |  2584 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2585 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2586 | `		 *  mean that the following expression is valid:` |
|       - |  2587 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2588 | `		 */` |
|       9 |  2589 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2590 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2591 | `			if( pGen->pIn < pNext ){` |
|       9 |  2592 | `				pGen->pEnd = pNext;` |
|       9 |  2593 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2594 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2595 | `					return SXERR_ABORT;` |
|       - |  2596 | `				}` |
|       9 |  2597 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2598 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2599 | `					 * without the overhead of a function call.` |
|       - |  2600 | `					 * This is a very powerful optimization that improve` |
|       - |  2601 | `					 * performance greatly.` |
|       - |  2602 | `					 */` |
|       9 |  2603 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2604 | `				}` |
|       4 |  2605 | `			}` |
|       - |  2606 | `			/* Jump trailing commas */` |
|       9 |  2607 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2608 | `				pNext++;` |
|     ! 0 |  2609 | `			}` |
|       9 |  2610 | `			pGen->pIn = pNext;` |
|       1 |  2611 | `		}` |
|       - |  2612 | `		/* Restore token stream */` |
|       9 |  2613 | `		pGen->pEnd = pTmp;` |
|       5 |  2614 | `	}else{` |
|      66 |  2615 | `		sxi32 nArg = 0;` |
|      66 |  2616 | `		sxu32 nIdx = 0;` |
|      66 |  2617 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 |  2618 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2619 | `			return SXERR_ABORT;` |
|      66 |  2620 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 |  2621 | `			nArg = 1;` |
|      32 |  2622 | `		}` |
|      66 |  2623 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2624 | `			ph7_value *pObj;` |
|       - |  2625 | `			/* Emit the call instruction */` |
|      20 |  2626 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2627 | `			if( pObj == 0 ){` |
|     ! 0 |  2628 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2629 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2630 | `				return SXERR_ABORT;` |
|       - |  2631 | `			}` |
|      20 |  2632 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2633 | `			/* Install in the literal table */` |
|      20 |  2634 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 |  2635 | `		}` |
|       - |  2636 | `		/* Emit the call instruction */` |
|      66 |  2637 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 |  2638 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - |  2639 | `	}` |
|       - |  2640 | `	/* Node successfully compiled */` |
|      74 |  2641 | `	return SXRET_OK;` |
|      38 |  2642 |  |
|       - |  2643 | `/*` |
|       - |  2644 | ` * Compile a node holding a variable declaration.` |
|       - |  2645 | ` * According to the PHP language reference` |
|       - |  2646 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2647 | ` *  The variable name is case-sensitive.` |
|       - |  2648 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2649 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2650 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2651 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2652 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2653 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2654 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2655 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2656 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2657 | ` *  the chapter on Expressions.` |
|       - |  2658 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2659 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2660 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2661 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2662 | ` *  is being assigned (the source variable).` |
|       - |  2663 | ` */` |
|  829406 |  2664 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2665 |  |
|  829408 |  2666 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2667 | `	sxi32 iVv;` |
|       - |  2668 | `	sxi32 iP1;` |
|       - |  2669 | `	void *p3;` |
|       - |  2670 | `	sxi32 rc;` |
|  829408 |  2671 | `	iVv = -1; /* Variable variable counter */` |
| 1658826 |  2672 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  829420 |  2673 | `		pGen->pIn++;` |
|  829420 |  2674 | `		iVv++;` |
|       2 |  2675 | `	}` |
|  829408 |  2676 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2677 | `		/* Invalid variable name */` |
|     ! 0 |  2678 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2679 | `		if( rc == SXERR_ABORT ){` |
|       - |  2680 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2681 | `			return SXERR_ABORT;` |
|       - |  2682 | `		}` |
|     ! 0 |  2683 | `		return SXRET_OK;` |
|       - |  2684 | `	}` |
|  829408 |  2685 | `	p3  = 0;` |
|  829408 |  2686 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2687 | `		/* Dynamic variable creation */` |
|      18 |  2688 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 |  2689 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 |  2690 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2691 | `			/* Empty expression */` |
|       3 |  2692 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2693 | `			return SXRET_OK;` |
|       - |  2694 | `		}` |
|       - |  2695 | `		/* Compile the expression holding the variable name */` |
|      16 |  2696 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2697 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2698 | `			return SXERR_ABORT;` |
|      16 |  2699 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2700 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2701 | `			return SXRET_OK;` |
|       - |  2702 | `		}` |
|       7 |  2703 | `	}else{` |
|       - |  2704 | `		SyHashEntry *pEntry;` |
|       - |  2705 | `		SyString *pName;` |
|  829392 |  2706 | `		char *zName = 0;` |
|       - |  2707 | `		/* Extract variable name */` |
|  829392 |  2708 | `		pName = &pGen->pIn->sData;` |
|       - |  2709 | `		/* Advance the stream cursor */` |
|  829392 |  2710 | `		pGen->pIn++;` |
|  829392 |  2711 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  829392 |  2712 | `		if( pEntry == 0 ){` |
|       - |  2713 | `			/* Duplicate name */` |
|  119242 |  2714 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  119242 |  2715 | `			if( zName == 0 ){` |
|     ! 0 |  2716 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2717 | `				return SXERR_ABORT;` |
|       - |  2718 | `			}` |
|       - |  2719 | `			/* Install in the hashtable */` |
|  119242 |  2720 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   59622 |  2721 | `		}else{` |
|       - |  2722 | `			/* Name already available */` |
|  710152 |  2723 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2724 | `		}` |
|  829392 |  2725 | `		p3 = (void *)zName;` |
|       - |  2726 | `	}` |
|  829404 |  2727 | `	iP1 = 0;` |
|  829404 |  2728 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  318554 |  2729 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2730 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  312232 |  2731 | `			iP1 = 1;` |
|  156115 |  2732 | `		}` |
|  159276 |  2733 | `	}` |
|       - |  2734 | `	/* Emit the load instruction */` |
|  829404 |  2735 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  829416 |  2736 | `	while( iVv > 0 ){` |
|      13 |  2737 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2738 | `		iVv--;` |
|       1 |  2739 | `	}` |
|       - |  2740 | `	/* Node successfully compiled */` |
|  829404 |  2741 | `	return SXRET_OK;` |
|  414705 |  2742 |  |
|       - |  2743 | `/*` |
|       - |  2744 | ` * Load a literal.` |
|       - |  2745 | ` */` |
|  556306 |  2746 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2747 |  |
|  556308 |  2748 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2749 | `	ph7_value *pObj;` |
|       - |  2750 | `	SyString *pStr;` |
|       - |  2751 | `	sxu32 nIdx;` |
|       - |  2752 | `	/* Extract token value */` |
|  556308 |  2753 | `	pStr = &pToken->sData;` |
|       - |  2754 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  556308 |  2755 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  101134 |  2756 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2757 | `			/* NULL constant are always indexed at 0 */` |
|   43022 |  2758 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   43022 |  2759 | `			return SXRET_OK;` |
|   58114 |  2760 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2761 | `			/* TRUE constant are always indexed at 1 */` |
|     512 |  2762 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     512 |  2763 | `			return SXRET_OK;` |
|       2 |  2764 | `		}` |
|  527859 |  2765 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   87764 |  2766 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2767 | `			/* FALSE constant are always indexed at 2 */` |
|   37476 |  2768 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   37476 |  2769 | `			return SXRET_OK;` |
|  456474 |  2770 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   77544 |  2771 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2772 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5680 |  2773 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5680 |  2774 | `			if( pObj == 0 ){` |
|     ! 0 |  2775 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2776 | `				return SXERR_ABORT;` |
|       - |  2777 | `			}` |
|    5680 |  2778 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2779 | `			/* Emit the load constant instruction */` |
|    5680 |  2780 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5680 |  2781 | `			return SXRET_OK;` |
|  426335 |  2782 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   28622 |  2783 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2784 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2785 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2786 | `			if( pObj == 0 ){` |
|     ! 0 |  2787 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2788 | `				return SXERR_ABORT;` |
|       - |  2789 | `			}` |
|       7 |  2790 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2791 | `				SyString sNs;` |
|       7 |  2792 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2793 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2794 | `			}else{` |
|     ! 0 |  2795 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2796 | `			}` |
|       7 |  2797 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2798 | `			return SXRET_OK;` |
|  425473 |  2799 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11950 |  2800 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  419492 |  2801 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   14966 |  2802 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2803 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2804 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2805 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2806 | `				/* Point to the upper block */` |
|      11 |  2807 | `				pBlock = pBlock->pParent;` |
|       1 |  2808 | `			}` |
|      11 |  2809 | `			if( pBlock == 0 ){` |
|       - |  2810 | `				/* Called in the global scope,load NULL */` |
|       5 |  2811 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2812 | `			}else{` |
|       - |  2813 | `				/* Extract the target function/method */` |
|       7 |  2814 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2815 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2816 | `					/* Not a class method,Load null */` |
|       3 |  2817 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2818 | `				}else{` |
|       5 |  2819 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2820 | `					if( pObj == 0 ){` |
|     ! 0 |  2821 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2822 | `						return SXERR_ABORT;` |
|       - |  2823 | `					}` |
|       5 |  2824 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2825 | `					/* Emit the load constant instruction */` |
|       5 |  2826 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2827 | `				}` |
|       - |  2828 | `			}` |
|      11 |  2829 | `			return SXRET_OK;` |
|       - |  2830 | `	}` |
|       - |  2831 | `	/* Query literal table */` |
|  469610 |  2832 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2833 | `		ph7_value *pLitObj;` |
|       - |  2834 | `		/* Unknown literal,install it in the literal table */` |
|  219822 |  2835 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  219822 |  2836 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2837 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2838 | `			return SXERR_ABORT;` |
|       - |  2839 | `		}` |
|  219822 |  2840 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  219822 |  2841 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  109910 |  2842 | `	}` |
|       - |  2843 | `	/* Emit the load constant instruction */` |
|  469610 |  2844 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  469610 |  2845 | `	return SXRET_OK;` |
|  278155 |  2846 |  |
|       - |  2847 | `/*` |
|       - |  2848 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2849 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2850 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2851 | ` * Otherwise, load the simple literal directly.` |
|       - |  2852 | ` */` |
|  556332 |  2853 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2854 |  |
|       - |  2855 | `	sxi32 rc;` |
|  556334 |  2856 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2857 | `		return SXRET_OK;` |
|       - |  2858 | `	}` |
|       - |  2859 | `	/* Check if this is a multi-token namespace path */` |
|  556334 |  2860 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2861 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      28 |  2862 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      28 |  2863 | `		int isAbsolute = 0;` |
|      28 |  2864 | `		SyBlobReset(pWorker);` |
|       - |  2865 | `		/* Check for leading backslash (absolute path) */` |
|      28 |  2866 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      26 |  2867 | `			isAbsolute = 1;` |
|      26 |  2868 | `			pGen->pIn++; /* Skip leading backslash */` |
|      12 |  2869 | `		}` |
|       - |  2870 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      28 |  2871 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2872 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2873 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2874 | `		}` |
|       - |  2875 | `		/* Collect all path components */` |
|     108 |  2876 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     108 |  2877 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      42 |  2878 | `				SyBlobAppend(pWorker,"\\",1);` |
|      22 |  2879 | `			}else{` |
|      68 |  2880 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2881 | `			}` |
|     108 |  2882 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      28 |  2883 | `				pGen->pIn++;` |
|      28 |  2884 | `				break;` |
|       - |  2885 | `			}` |
|      82 |  2886 | `			pGen->pIn++;` |
|       2 |  2887 | `		}` |
|      28 |  2888 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2889 | `			ph7_value *pObj;` |
|       - |  2890 | `			SyString sPath;` |
|       - |  2891 | `			sxu32 nIdx;` |
|      28 |  2892 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2893 | `			/* Install in the literal table */` |
|      28 |  2894 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      16 |  2895 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      16 |  2896 | `				if( pObj == 0 ){` |
|     ! 0 |  2897 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2898 | `					return SXERR_ABORT;` |
|       - |  2899 | `				}` |
|      16 |  2900 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      16 |  2901 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       7 |  2902 | `			}` |
|       - |  2903 | `			/* Emit the load constant instruction.` |
|       - |  2904 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      28 |  2905 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      28 |  2906 | `			return SXRET_OK;` |
|       - |  2907 | `		}` |
|     ! 0 |  2908 | `	}` |
|       - |  2909 | `	/* Single-token literal: load directly */` |
|  556308 |  2910 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  556308 |  2911 | `	return rc;` |
|  278168 |  2912 |  |
|       - |  2913 | `/*` |
|       - |  2914 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2915 | ` */` |
|  556332 |  2916 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2917 |  |
|       - |  2918 | `	sxi32 rc;` |
|  556334 |  2919 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  556334 |  2920 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2921 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2922 | `		return rc;` |
|       - |  2923 | `	}` |
|       - |  2924 | `	/* Node successfully compiled */` |
|  556334 |  2925 | `	return SXRET_OK;` |
|  278168 |  2926 |  |
|       - |  2927 | `/*` |
|       - |  2928 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  2929 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  2930 | ` */` |
|       8 |  2931 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  2932 |  |
|       - |  2933 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  2934 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  2935 | `		pGen->pIn++;` |
|       1 |  2936 | `	}` |
|       9 |  2937 | `	return SXRET_OK;` |
|       1 |  2938 |  |
|       - |  2939 | `/*` |
|       - |  2940 | ` * Check if the given identifier name is reserved or not.` |
|       - |  2941 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  2942 | ` */` |
|      56 |  2943 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 |  2944 |  |
|      58 |  2945 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 |  2946 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  2947 | `			return TRUE;` |
|      24 |  2948 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  2949 | `			return TRUE;` |
|       2 |  2950 | `		}` |
|      43 |  2951 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  2952 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  2953 | `			return TRUE;` |
|       - |  2954 | `		}` |
|     ! 0 |  2955 | `	}` |
|       - |  2956 | `	/* Not a reserved constant */` |
|      50 |  2957 | `	return FALSE;` |
|      30 |  2958 |  |
|       - |  2959 | `/*` |
|       - |  2960 | ` * Compile the 'const' statement.` |
|       - |  2961 | ` * According to the PHP language reference` |
|       - |  2962 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  2963 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  2964 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  2965 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  2966 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2967 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  2968 | ` *  Syntax` |
|       - |  2969 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  2970 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  2971 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  2972 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  2973 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  2974 | ` *  to get a list of all defined constants.` |
|       - |  2975 | ` *` |
|       - |  2976 | ` * Symisc eXtension.` |
|       - |  2977 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  2978 | ` *  would allow only simple scalar value.` |
|       - |  2979 | ` *  Example` |
|       - |  2980 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  2981 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  2982 | ` */` |
|      32 |  2983 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 |  2984 |  |
|       - |  2985 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 |  2986 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2987 | `	SyString *pName;` |
|       - |  2988 | `	sxi32 rc;` |
|      34 |  2989 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 |  2990 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  2991 | `		/* Invalid constant name */` |
|       7 |  2992 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  2993 | `		if( rc == SXERR_ABORT ){` |
|       - |  2994 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2995 | `			return SXERR_ABORT;` |
|       - |  2996 | `		}` |
|       7 |  2997 | `		goto Synchronize;` |
|       - |  2998 | `	}` |
|       - |  2999 | `	/* Peek constant name */` |
|      28 |  3000 | `	pName = &pGen->pIn->sData;` |
|       - |  3001 | `	/* Make sure the constant name isn't reserved */` |
|      28 |  3002 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3003 | `		/* Reserved constant */` |
|       9 |  3004 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3005 | `		if( rc == SXERR_ABORT ){` |
|       - |  3006 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3007 | `			return SXERR_ABORT;` |
|       - |  3008 | `		}` |
|       9 |  3009 | `		goto Synchronize;` |
|       - |  3010 | `	}` |
|      20 |  3011 | `	pGen->pIn++;` |
|      20 |  3012 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3013 | `		/* Invalid statement*/` |
|       5 |  3014 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 |  3015 | `		if( rc == SXERR_ABORT ){` |
|       - |  3016 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3017 | `			return SXERR_ABORT;` |
|       - |  3018 | `		}` |
|       5 |  3019 | `		goto Synchronize;` |
|       - |  3020 | `	}` |
|      15 |  3021 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3022 | `	/* Allocate a new constant value container */` |
|      15 |  3023 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3024 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3025 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3026 | `		return SXERR_ABORT;` |
|       - |  3027 | `	}` |
|      15 |  3028 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3029 | `	/* Swap bytecode container */` |
|      15 |  3030 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3031 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3032 | `	/* Compile constant value */` |
|      15 |  3033 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3034 | `	/* Emit the done instruction */` |
|      15 |  3035 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3036 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3037 | `	if( rc == SXERR_ABORT ){` |
|       - |  3038 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3039 | `		return SXERR_ABORT;` |
|       - |  3040 | `	}` |
|      15 |  3041 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3042 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3043 | `	{` |
|       - |  3044 | `		SyBlob sFQN;` |
|       - |  3045 | `		SyString sFQNStr;` |
|      15 |  3046 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3047 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3048 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3049 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3050 | `		SyBlobRelease(&sFQN);` |
|       - |  3051 | `	}` |
|      15 |  3052 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3053 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3054 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3055 | `	}` |
|      15 |  3056 | `	return SXRET_OK;` |
|       9 |  3057 | `Synchronize:` |
|       - |  3058 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 |  3059 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 |  3060 | `		pGen->pIn++;` |
|       1 |  3061 | `	}` |
|      19 |  3062 | `	return SXRET_OK;` |
|      18 |  3063 |  |
|       - |  3064 | `/*` |
|       - |  3065 | ` * Compile the 'continue' statement.` |
|       - |  3066 | ` * According to the PHP language reference` |
|       - |  3067 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3068 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3069 | ` *  iteration.` |
|       - |  3070 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3071 | ` *  the purposes of continue.` |
|       - |  3072 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3073 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3074 | ` *  Note:` |
|       - |  3075 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3076 | ` */` |
|       - |  3077 | `/*` |
|       - |  3078 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3079 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3080 | ` * break/continue crosses a try boundary.` |
|       - |  3081 | ` *` |
|       - |  3082 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3083 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3084 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3085 | ` */` |
|    2966 |  3086 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3087 |  |
|    2968 |  3088 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17342 |  3089 | `	while( pBlock && pBlock != pTarget ){` |
|   14376 |  3090 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3091 | `			if( pBlock->pUserData ){` |
|       - |  3092 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3093 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3094 | `			}else{` |
|       - |  3095 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3096 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3097 | `				 * exception context from a sub-execution.` |
|       - |  3098 | `				 */` |
|     ! 0 |  3099 | `				break;` |
|       - |  3100 | `			}` |
|       1 |  3101 | `		}` |
|   14376 |  3102 | `		pBlock = pBlock->pParent;` |
|       2 |  3103 | `	}` |
|    2968 |  3104 |  |
|    2882 |  3105 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3106 |  |
|       - |  3107 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3108 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3109 | `	sxu32 nLineLocal;` |
|       - |  3110 | `	sxi32 rc;` |
|    2884 |  3111 | `	nLineLocal = pGen->pIn->nLine;` |
|    2884 |  3112 | `	iLevel = 0;` |
|       - |  3113 | `	/* Jump the 'continue' keyword */` |
|    2884 |  3114 | `	pGen->pIn++;` |
|    2884 |  3115 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3116 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3117 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3118 | `		 */` |
|       - |  3119 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3120 | `		char *zAlloc = 0;` |
|       - |  3121 | `		SyString sNum;` |
|      16 |  3122 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3123 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3124 | `			return SXERR_ABORT;` |
|       - |  3125 | `		}` |
|      16 |  3126 | `		if( rc == SXRET_OK ){` |
|      20 |  3127 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3128 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3129 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3130 | `				return SXERR_ABORT;` |
|       - |  3131 | `			}` |
|      14 |  3132 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3133 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3134 | `		}` |
|      16 |  3135 | `		if( iLevel < 2 ){` |
|       3 |  3136 | `			iLevel = 0;` |
|       1 |  3137 | `		}` |
|      16 |  3138 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3139 | `	}` |
|       - |  3140 | `	/* Point to the target loop */` |
|    2884 |  3141 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2884 |  3142 | `	if( pLoop == 0 ){` |
|       - |  3143 | `		/* Illegal continue */` |
|      11 |  3144 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3145 | `		if( rc == SXERR_ABORT ){` |
|       - |  3146 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3147 | `			return SXERR_ABORT;` |
|       - |  3148 | `		}` |
|       6 |  3149 | `	}else{` |
|    2874 |  3150 | `		sxu32 nInstrIdx = 0;` |
|       - |  3151 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2874 |  3152 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2874 |  3153 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3154 | `			/* According to the PHP language reference manual` |
|       - |  3155 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3156 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3157 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3158 | `			 */` |
|       5 |  3159 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3160 | `			if( rc == SXRET_OK ){` |
|       5 |  3161 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3162 | `			}` |
|       3 |  3163 | `		}else{` |
|       - |  3164 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2870 |  3165 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2870 |  3166 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3167 | `				JumpFixup sJumpFix;` |
|       - |  3168 | `				/* Post-continue */` |
|      14 |  3169 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3170 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3171 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3172 | `			}` |
|       - |  3173 | `		}` |
|       - |  3174 | `	}` |
|    2884 |  3175 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3176 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3177 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3178 | `	}` |
|       - |  3179 | `	/* Statement successfully compiled */` |
|    2884 |  3180 | `	return SXRET_OK;` |
|    1443 |  3181 |  |
|       - |  3182 | `/*` |
|       - |  3183 | ` * Compile the 'break' statement.` |
|       - |  3184 | ` * According to the PHP language reference` |
|       - |  3185 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3186 | ` *  structure.` |
|       - |  3187 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3188 | ` *  enclosing structures are to be broken out of.` |
|       - |  3189 | ` */` |
|     110 |  3190 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 |  3191 |  |
|       - |  3192 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3193 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3194 | `	sxi32 rc;` |
|     112 |  3195 | `	iLevel = 0;` |
|       - |  3196 | `	/* Jump the 'break' keyword */` |
|     112 |  3197 | `	pGen->pIn++;` |
|     112 |  3198 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3199 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3200 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3201 | `		 */` |
|       - |  3202 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3203 | `		char *zAlloc = 0;` |
|       - |  3204 | `		SyString sNum;` |
|      16 |  3205 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3206 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3207 | `			return SXERR_ABORT;` |
|       - |  3208 | `		}` |
|      16 |  3209 | `		if( rc == SXRET_OK ){` |
|      20 |  3210 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3211 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3212 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3213 | `				return SXERR_ABORT;` |
|       - |  3214 | `			}` |
|      14 |  3215 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3216 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3217 | `		}` |
|      16 |  3218 | `		if( iLevel < 2 ){` |
|       3 |  3219 | `			iLevel = 0;` |
|       1 |  3220 | `		}` |
|      16 |  3221 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3222 | `	}` |
|       - |  3223 | `	/* Extract the target loop */` |
|     112 |  3224 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 |  3225 | `	if( pLoop == 0 ){` |
|       - |  3226 | `		/* Illegal break */` |
|      17 |  3227 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 |  3228 | `		if( rc == SXERR_ABORT ){` |
|       - |  3229 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3230 | `			return SXERR_ABORT;` |
|       - |  3231 | `		}` |
|       9 |  3232 | `	}else{` |
|       - |  3233 | `		sxu32 nInstrIdx;` |
|       - |  3234 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 |  3235 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 |  3236 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 |  3237 | `		if( rc == SXRET_OK ){` |
|       - |  3238 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 |  3239 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 |  3240 | `		}` |
|       - |  3241 | `	}` |
|     112 |  3242 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3243 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3244 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3245 | `	}` |
|       - |  3246 | `	/* Statement successfully compiled */` |
|     112 |  3247 | `	return SXRET_OK;` |
|      57 |  3248 |  |
|       - |  3249 | `/*` |
|       - |  3250 | ` * Compile or record a label.` |
|       - |  3251 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3252 | ` * Example` |
|       - |  3253 | ` *  goto LABEL;` |
|       - |  3254 | ` *   echo 'Foo';` |
|       - |  3255 | ` *  LABEL:` |
|       - |  3256 | ` *   echo 'Bar';` |
|       - |  3257 | ` */` |
|     112 |  3258 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 |  3259 |  |
|       - |  3260 | `	GenBlock *pBlock;` |
|       - |  3261 | `	Label sLabel;` |
|       - |  3262 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 |  3263 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 |  3264 | `	if( pBlock ){` |
|       - |  3265 | `		sxi32 rc;` |
|       7 |  3266 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3267 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 |  3268 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3269 | `			return SXERR_ABORT;` |
|       - |  3270 | `		}` |
|       3 |  3271 | `	}else{` |
|     110 |  3272 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3273 | `		char *zDup;` |
|       - |  3274 | `		/* Initialize label fields */` |
|     110 |  3275 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3276 | `		/* Duplicate label name */` |
|     110 |  3277 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 |  3278 | `		if( zDup == 0 ){` |
|     ! 0 |  3279 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3280 | `			return SXERR_ABORT;` |
|       - |  3281 | `		}` |
|     110 |  3282 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 |  3283 | `		sLabel.bRef  = FALSE;` |
|     110 |  3284 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 |  3285 | `		pBlock = pGen->pCurrent;` |
|     218 |  3286 | `		while( pBlock ){` |
|     130 |  3287 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3288 | `				break;` |
|       - |  3289 | `			}` |
|       - |  3290 | `			/* Point to the upper block */` |
|     110 |  3291 | `			pBlock = pBlock->pParent;` |
|       2 |  3292 | `		}` |
|     110 |  3293 | `		if( pBlock ){` |
|      22 |  3294 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3295 | `		}else{` |
|      90 |  3296 | `			sLabel.pFunc = 0;` |
|       - |  3297 | `		}` |
|       - |  3298 | `		/* Insert in label set */` |
|     110 |  3299 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3300 | `	}` |
|     114 |  3301 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 |  3302 | `	return SXRET_OK;` |
|      58 |  3303 |  |
|       - |  3304 | `/*` |
|       - |  3305 | ` * Compile the so hated 'goto' statement.` |
|       - |  3306 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3307 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3308 | ` * a compiler it has to do this.` |
|       - |  3309 | ` * According to the PHP language reference manual` |
|       - |  3310 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3311 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3312 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3313 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3314 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3315 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3316 | ` *   of a multi-level break` |
|       - |  3317 | ` */` |
|     152 |  3318 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 |  3319 |  |
|       - |  3320 | `	JumpFixup sJump;` |
|       - |  3321 | `	sxi32 rc;` |
|     154 |  3322 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 |  3323 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3324 | `		/* Missing label */` |
|     ! 0 |  3325 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3326 | `		if( rc == SXERR_ABORT ){` |
|       - |  3327 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3328 | `			return SXERR_ABORT;` |
|       - |  3329 | `		}` |
|     ! 0 |  3330 | `		return SXRET_OK;` |
|       - |  3331 | `	}` |
|     154 |  3332 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3333 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3334 | `		if( rc == SXERR_ABORT ){` |
|       - |  3335 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3336 | `			return SXERR_ABORT;` |
|       - |  3337 | `		}` |
|       3 |  3338 | `	}else{` |
|     150 |  3339 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3340 | `		GenBlock *pBlock;` |
|       - |  3341 | `		char *zDup;` |
|       - |  3342 | `		/* Prepare the jump destination */` |
|     150 |  3343 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 |  3344 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3345 | `		/* Duplicate label name */` |
|     150 |  3346 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 |  3347 | `		if( zDup == 0 ){` |
|     ! 0 |  3348 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3349 | `			return SXERR_ABORT;` |
|       - |  3350 | `		}` |
|     150 |  3351 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 |  3352 | `		pBlock = pGen->pCurrent;` |
|     312 |  3353 | `		while( pBlock ){` |
|     196 |  3354 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 |  3355 | `				break;` |
|       - |  3356 | `			}` |
|       - |  3357 | `			/* Point to the upper block */` |
|     164 |  3358 | `			pBlock = pBlock->pParent;` |
|       2 |  3359 | `		}` |
|     150 |  3360 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 |  3361 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 |  3362 | `			if( rc == SXERR_ABORT ){` |
|       - |  3363 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3364 | `				return SXERR_ABORT;` |
|       - |  3365 | `			}` |
|       3 |  3366 | `		}` |
|     150 |  3367 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 |  3368 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3369 | `		}else{` |
|     124 |  3370 | `			sJump.pFunc = 0;` |
|       - |  3371 | `		}` |
|       - |  3372 | `		/* Emit the unconditional jump */` |
|     150 |  3373 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 |  3374 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3375 | `		}` |
|       - |  3376 | `	}` |
|     154 |  3377 | `	pGen->pIn++; /* Jump the label name */` |
|     154 |  3378 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3379 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3380 | `	}` |
|       - |  3381 | `	/* Statement successfully compiled */` |
|     154 |  3382 | `	return SXRET_OK;` |
|      78 |  3383 |  |
|       - |  3384 | `/*` |
|       - |  3385 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3386 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3387 | ` * failure.` |
|       - |  3388 | ` */` |
|      20 |  3389 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3390 |  |
|       - |  3391 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3392 | `	sxu32 nRawObj;` |
|      10 |  3393 | `	sxu32 nObjIdx;` |
|       - |  3394 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3395 | `	 * a PHP block.` |
|       - |  3396 | `	 */` |
|      10 |  3397 | `Consume:` |
|      21 |  3398 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3399 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3400 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3401 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3402 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3403 | `			return SXERR_ABORT;` |
|       - |  3404 | `		}` |
|       - |  3405 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3406 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3407 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3408 | `		++nRawObj;` |
|     ! 0 |  3409 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3410 | `	}` |
|      21 |  3411 | `	if( nRawObj > 0 ){` |
|       - |  3412 | `		/* Emit the consume instruction */` |
|     ! 0 |  3413 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3414 | `	}` |
|      21 |  3415 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3416 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3417 | `		/* Reset the token set */` |
|     ! 0 |  3418 | `		SySetReset(pTokenSet);` |
|       - |  3419 | `		/* Tokenize input */` |
|     ! 0 |  3420 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3421 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3422 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3423 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3424 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3425 | `		/* Advance the stream cursor */` |
|     ! 0 |  3426 | `		pGen->pRawIn++;` |
|       - |  3427 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3428 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3429 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3430 | `			sxi32 rc;` |
|       - |  3431 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3432 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3433 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3434 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3435 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3436 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3437 | `				return SXERR_ABORT;` |
|     ! 0 |  3438 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3439 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3440 | `			}` |
|     ! 0 |  3441 | `			goto Consume;` |
|       - |  3442 | `		}` |
|     ! 0 |  3443 | `	}else{` |
|       - |  3444 | `		/* No more chunks to process */` |
|      21 |  3445 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3446 | `		return SXERR_EOF;` |
|       - |  3447 | `	}` |
|     ! 0 |  3448 | `	return SXRET_OK;` |
|      11 |  3449 |  |
|       - |  3450 | `/*` |
|       - |  3451 | ` * Compile a PHP block.` |
|       - |  3452 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3453 | ` * optionally delimited by braces {}.` |
|       - |  3454 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3455 | ` * and this function takes care of generating the appropriate error` |
|       - |  3456 | ` * message.` |
|       - |  3457 | ` */` |
|  312664 |  3458 | `static sxi32 PH7_CompileBlock(` |
|       - |  3459 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3460 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3461 | `	)` |
|       2 |  3462 |  |
|       - |  3463 | `	sxi32 rc;` |
|       - |  3464 | `	sxu32 nLine;` |
|  312666 |  3465 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  311258 |  3466 | `		nLine = pGen->pIn->nLine;` |
|  311258 |  3467 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  311258 |  3468 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3469 | `			return SXERR_ABORT;` |
|       - |  3470 | `		}` |
|  311258 |  3471 | `		pGen->pIn++;` |
|       - |  3472 | `		/* Compile until we hit the closing braces '}' */` |
|  429648 |  3473 | `		for(;;){` |
|  859298 |  3474 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3475 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3476 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3477 | `			 	   return SXERR_ABORT;` |
|       - |  3478 | `				}` |
|      21 |  3479 | `				if( rc == SXERR_EOF ){` |
|       - |  3480 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3481 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3482 | `					break;` |
|       - |  3483 | `				}` |
|     ! 0 |  3484 | `			}` |
|  859278 |  3485 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3486 | `				/* Closing braces found,break immediately*/` |
|  311238 |  3487 | `				pGen->pIn++;` |
|  311238 |  3488 | `				break;` |
|       - |  3489 | `			}` |
|       - |  3490 | `			/* Compile a single statement */` |
|  548042 |  3491 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  548042 |  3492 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3493 | `				return SXERR_ABORT;` |
|       - |  3494 | `			}` |
|       2 |  3495 | `		}` |
|  311258 |  3496 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  157038 |  3497 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3498 | `		pGen->pIn++;` |
|     ! 0 |  3499 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3500 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3501 | `			return SXERR_ABORT;` |
|       - |  3502 | `		}` |
|       - |  3503 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3504 | `		for(;;){` |
|     ! 0 |  3505 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3506 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3507 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3508 | `			 	   return SXERR_ABORT;` |
|       - |  3509 | `				}` |
|     ! 0 |  3510 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3511 | `					/* No more token to process */` |
|     ! 0 |  3512 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3513 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3514 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3515 | `					}` |
|     ! 0 |  3516 | `					break;` |
|       - |  3517 | `				}` |
|     ! 0 |  3518 | `			}` |
|     ! 0 |  3519 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3520 | `				sxi32 nKwrd;` |
|       - |  3521 | `				/* Keyword found */` |
|     ! 0 |  3522 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3523 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3524 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3525 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3526 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3527 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3528 | `						}` |
|     ! 0 |  3529 | `						break;` |
|       - |  3530 | `				}` |
|     ! 0 |  3531 | `			}` |
|       - |  3532 | `			/* Compile a single statement */` |
|     ! 0 |  3533 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3534 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3535 | `				return SXERR_ABORT;` |
|       - |  3536 | `			}` |
|     ! 0 |  3537 | `		}` |
|     ! 0 |  3538 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3539 | `	}else{` |
|       - |  3540 | `		/* Compile a single statement */` |
|    1410 |  3541 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1410 |  3542 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3543 | `			return SXERR_ABORT;` |
|       - |  3544 | `		}` |
|       - |  3545 | `	}` |
|       - |  3546 | `	/* Jump trailing semi-colons ';' */` |
|  312666 |  3547 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3548 | `		pGen->pIn++;` |
|     ! 0 |  3549 | `	}` |
|  312666 |  3550 | `	return SXRET_OK;` |
|  156334 |  3551 |  |
|       - |  3552 | `/*` |
|       - |  3553 | ` * Compile the gentle 'while' statement.` |
|       - |  3554 | ` * According to the PHP language reference` |
|       - |  3555 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3556 | ` *  The basic form of a while statement is:` |
|       - |  3557 | ` *  while (expr)` |
|       - |  3558 | ` *   statement` |
|       - |  3559 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3560 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3561 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3562 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3563 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3564 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3565 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3566 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3567 | ` *  while (expr):` |
|       - |  3568 | ` *    statement` |
|       - |  3569 | ` *   endwhile;` |
|       - |  3570 | ` */` |
|   11454 |  3571 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3572 |  |
|   11456 |  3573 | `	GenBlock *pWhileBlock = 0;` |
|   11456 |  3574 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3575 | `	sxu32 nFalseJump;` |
|       - |  3576 | `	sxu32 nLine;` |
|       - |  3577 | `	sxi32 rc;` |
|   11456 |  3578 | `	nLine = pGen->pIn->nLine;` |
|       - |  3579 | `	/* Jump the 'while' keyword */` |
|   11456 |  3580 | `	pGen->pIn++;` |
|   11456 |  3581 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3582 | `		/* Syntax error */` |
|     ! 0 |  3583 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3584 | `		if( rc == SXERR_ABORT ){` |
|       - |  3585 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3586 | `			return SXERR_ABORT;` |
|       - |  3587 | `		}` |
|     ! 0 |  3588 | `		goto Synchronize;` |
|       - |  3589 | `	}` |
|       - |  3590 | `	/* Jump the left parenthesis '(' */` |
|   11456 |  3591 | `	pGen->pIn++;` |
|       - |  3592 | `	/* Create the loop block */` |
|   11456 |  3593 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11456 |  3594 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3595 | `		return SXERR_ABORT;` |
|       - |  3596 | `	}` |
|       - |  3597 | `	/* Delimit the condition */` |
|   11456 |  3598 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11456 |  3599 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3600 | `		/* Empty expression */` |
|       3 |  3601 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3602 | `		if( rc == SXERR_ABORT ){` |
|       - |  3603 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3604 | `			return SXERR_ABORT;` |
|       - |  3605 | `		}` |
|       1 |  3606 | `	}` |
|       - |  3607 | `	/* Swap token streams */` |
|   11456 |  3608 | `	pTmp = pGen->pEnd;` |
|   11456 |  3609 | `	pGen->pEnd = pEnd;` |
|       - |  3610 | `	/* Compile the expression */` |
|   11456 |  3611 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11456 |  3612 | `	if( rc == SXERR_ABORT ){` |
|       - |  3613 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3614 | `		return SXERR_ABORT;` |
|       - |  3615 | `	}` |
|       - |  3616 | `	/* Update token stream */` |
|   11456 |  3617 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3618 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3619 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3620 | `			return SXERR_ABORT;` |
|       - |  3621 | `		}` |
|     ! 0 |  3622 | `		pGen->pIn++;` |
|     ! 0 |  3623 | `	}` |
|       - |  3624 | `	/* Synchronize pointers */` |
|   11456 |  3625 | `	pGen->pIn  = &pEnd[1];` |
|   11456 |  3626 | `	pGen->pEnd = pTmp;` |
|       - |  3627 | `	/* Emit the false jump */` |
|   11456 |  3628 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3629 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11456 |  3630 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3631 | `	/* Compile the loop body */` |
|   11456 |  3632 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11456 |  3633 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3634 | `		return SXERR_ABORT;` |
|       - |  3635 | `	}` |
|       - |  3636 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11456 |  3637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3638 | `	/* Fix all jumps now the destination is resolved */` |
|   11456 |  3639 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3640 | `	/* Release the loop block */` |
|   11456 |  3641 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3642 | `	/* Statement successfully compiled */` |
|   11456 |  3643 | `	return SXRET_OK;` |
|     ! 0 |  3644 | `Synchronize:` |
|       - |  3645 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3646 | `	 * compiling this erroneous block.` |
|       - |  3647 | `	 */` |
|     ! 0 |  3648 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3649 | `		pGen->pIn++;` |
|     ! 0 |  3650 | `	}` |
|     ! 0 |  3651 | `	return SXRET_OK;` |
|    5729 |  3652 |  |
|       - |  3653 | `/*` |
|       - |  3654 | ` * Compile the ugly do..while() statement.` |
|       - |  3655 | ` * According to the PHP language reference` |
|       - |  3656 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3657 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3658 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3659 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3660 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3661 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3662 | ` *  would end immediately).` |
|       - |  3663 | ` *  There is just one syntax for do-while loops:` |
|       - |  3664 | ` *  <?php` |
|       - |  3665 | ` *  $i = 0;` |
|       - |  3666 | ` *  do {` |
|       - |  3667 | ` *   echo $i;` |
|       - |  3668 | ` *  } while ($i > 0);` |
|       - |  3669 | ` * ?>` |
|       - |  3670 | ` */` |
|       2 |  3671 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3672 |  |
|       3 |  3673 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3674 | `	GenBlock *pDoBlock = 0;` |
|       - |  3675 | `	sxu32 nLine;` |
|       - |  3676 | `	sxi32 rc;` |
|       3 |  3677 | `	nLine = pGen->pIn->nLine;` |
|       - |  3678 | `	/* Jump the 'do' keyword */` |
|       3 |  3679 | `	pGen->pIn++;` |
|       - |  3680 | `	/* Create the loop block */` |
|       3 |  3681 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3682 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3683 | `		return SXERR_ABORT;` |
|       - |  3684 | `	}` |
|       - |  3685 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3686 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3687 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3688 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3689 | `		return SXERR_ABORT;` |
|       - |  3690 | `	}` |
|       3 |  3691 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3692 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3693 | `	}` |
|       3 |  3694 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3695 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3696 | `			/* Missing 'while' statement */` |
|       3 |  3697 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3698 | `			if( rc == SXERR_ABORT ){` |
|       - |  3699 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3700 | `				return SXERR_ABORT;` |
|       - |  3701 | `			}` |
|       3 |  3702 | `			goto Synchronize;` |
|       - |  3703 | `	}` |
|       - |  3704 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3705 | `	pGen->pIn++;` |
|     ! 0 |  3706 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3707 | `		/* Syntax error */` |
|     ! 0 |  3708 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3709 | `		if( rc == SXERR_ABORT ){` |
|       - |  3710 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3711 | `			return SXERR_ABORT;` |
|       - |  3712 | `		}` |
|     ! 0 |  3713 | `		goto Synchronize;` |
|       - |  3714 | `	}` |
|       - |  3715 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3716 | `	pGen->pIn++;` |
|       - |  3717 | `	/* Delimit the condition */` |
|     ! 0 |  3718 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3719 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3720 | `		/* Empty expression */` |
|     ! 0 |  3721 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3722 | `		if( rc == SXERR_ABORT ){` |
|       - |  3723 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3724 | `			return SXERR_ABORT;` |
|       - |  3725 | `		}` |
|     ! 0 |  3726 | `		goto Synchronize;` |
|       - |  3727 | `	}` |
|       - |  3728 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3729 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3730 | `		JumpFixup *aPost;` |
|       - |  3731 | `		VmInstr *pInstr;` |
|       - |  3732 | `		sxu32 nJumpDest;` |
|       - |  3733 | `		sxu32 n;` |
|     ! 0 |  3734 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3735 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3736 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3737 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3738 | `			if( pInstr ){` |
|       - |  3739 | `				/* Fix */` |
|     ! 0 |  3740 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3741 | `			}` |
|     ! 0 |  3742 | `		}` |
|     ! 0 |  3743 | `	}` |
|       - |  3744 | `	/* Swap token streams */` |
|     ! 0 |  3745 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3746 | `	pGen->pEnd = pEnd;` |
|       - |  3747 | `	/* Compile the expression */` |
|     ! 0 |  3748 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3749 | `	if( rc == SXERR_ABORT ){` |
|       - |  3750 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3751 | `		return SXERR_ABORT;` |
|       - |  3752 | `	}` |
|       - |  3753 | `	/* Update token stream */` |
|     ! 0 |  3754 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3755 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3756 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3757 | `			return SXERR_ABORT;` |
|       - |  3758 | `		}` |
|     ! 0 |  3759 | `		pGen->pIn++;` |
|     ! 0 |  3760 | `	}` |
|     ! 0 |  3761 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3762 | `	pGen->pEnd = pTmp;` |
|       - |  3763 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3764 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3765 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3766 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3767 | `	/* Release the loop block */` |
|     ! 0 |  3768 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3769 | `	/* Statement successfully compiled */` |
|     ! 0 |  3770 | `	return SXRET_OK;` |
|       1 |  3771 | `Synchronize:` |
|       - |  3772 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3773 | `	 * compiling this erroneous block.` |
|       - |  3774 | `	 */` |
|       3 |  3775 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3776 | `		pGen->pIn++;` |
|     ! 0 |  3777 | `	}` |
|       3 |  3778 | `	return SXRET_OK;` |
|       2 |  3779 |  |
|       - |  3780 | `/*` |
|       - |  3781 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3782 | ` * According to the PHP language reference` |
|       - |  3783 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3784 | ` *  The syntax of a for loop is:` |
|       - |  3785 | ` *  for (expr1; expr2; expr3)` |
|       - |  3786 | ` *   statement` |
|       - |  3787 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3788 | ` *  the beginning of the loop.` |
|       - |  3789 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3790 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3791 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3792 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3793 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3794 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3795 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3796 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3797 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3798 | ` *  of using the for truth expression.` |
|       - |  3799 | ` */` |
|   11458 |  3800 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3801 |  |
|   11460 |  3802 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11460 |  3803 | `	GenBlock *pForBlock = 0;` |
|       - |  3804 | `	sxu32 nFalseJump;` |
|       - |  3805 | `	sxu32 nLine;` |
|       - |  3806 | `	sxi32 rc;` |
|   11460 |  3807 | `	nLine = pGen->pIn->nLine;` |
|       - |  3808 | `	/* Jump the 'for' keyword */` |
|   11460 |  3809 | `	pGen->pIn++;` |
|   11460 |  3810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3811 | `		/* Syntax error */` |
|     ! 0 |  3812 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3813 | `		if( rc == SXERR_ABORT ){` |
|       - |  3814 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3815 | `			return SXERR_ABORT;` |
|       - |  3816 | `		}` |
|     ! 0 |  3817 | `		return SXRET_OK;` |
|       - |  3818 | `	}` |
|       - |  3819 | `	/* Jump the left parenthesis '(' */` |
|   11460 |  3820 | `	pGen->pIn++;` |
|       - |  3821 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11460 |  3822 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11460 |  3823 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3824 | `		/* Empty expression */` |
|     ! 0 |  3825 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3826 | `		if( rc == SXERR_ABORT ){` |
|       - |  3827 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3828 | `			return SXERR_ABORT;` |
|       - |  3829 | `		}` |
|       - |  3830 | `		/* Synchronize */` |
|     ! 0 |  3831 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3832 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3833 | `			pGen->pIn++;` |
|     ! 0 |  3834 | `		}` |
|     ! 0 |  3835 | `		return SXRET_OK;` |
|       - |  3836 | `	}` |
|       - |  3837 | `	/* Swap token streams */` |
|   11460 |  3838 | `	pTmp = pGen->pEnd;` |
|   11460 |  3839 | `	pGen->pEnd = pEnd;` |
|       - |  3840 | `	/* Compile initialization expressions if available */` |
|   11460 |  3841 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3842 | `	/* Pop operand lvalues */` |
|   11460 |  3843 | `	if( rc == SXERR_ABORT ){` |
|       - |  3844 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3845 | `		return SXERR_ABORT;` |
|   11460 |  3846 | `	}else if( rc != SXERR_EMPTY ){` |
|   11458 |  3847 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5728 |  3848 | `	}` |
|   11460 |  3849 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3850 | `		/* Syntax error */` |
|     ! 0 |  3851 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3852 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3853 | `		if( rc == SXERR_ABORT ){` |
|       - |  3854 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3855 | `			return SXERR_ABORT;` |
|       - |  3856 | `		}` |
|     ! 0 |  3857 | `		return SXRET_OK;` |
|       - |  3858 | `	}` |
|       - |  3859 | `	/* Jump the trailing ';' */` |
|   11460 |  3860 | `	pGen->pIn++;` |
|       - |  3861 | `	/* Create the loop block */` |
|   11460 |  3862 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11460 |  3863 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3864 | `		return SXERR_ABORT;` |
|       - |  3865 | `	}` |
|       - |  3866 | `	/* Deffer continue jumps */` |
|   11460 |  3867 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3868 | `	/* Compile the condition */` |
|   11460 |  3869 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11460 |  3870 | `	if( rc == SXERR_ABORT ){` |
|       - |  3871 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3872 | `		return SXERR_ABORT;` |
|   11460 |  3873 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3874 | `		/* Emit the false jump */` |
|   11458 |  3875 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3876 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11458 |  3877 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5728 |  3878 | `	}` |
|   11460 |  3879 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3880 | `		/* Syntax error */` |
|       5 |  3881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3882 | `			"for: Expected ';' after conditionals expressions");` |
|       5 |  3883 | `		if( rc == SXERR_ABORT ){` |
|       - |  3884 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3885 | `			return SXERR_ABORT;` |
|       - |  3886 | `		}` |
|       5 |  3887 | `		return SXRET_OK;` |
|       - |  3888 | `	}` |
|       - |  3889 | `	/* Jump the trailing ';' */` |
|   11456 |  3890 | `	pGen->pIn++;` |
|       - |  3891 | `	/* Save the post condition stream */` |
|   11456 |  3892 | `	pPostStart = pGen->pIn;` |
|       - |  3893 | `	/* Compile the loop body */` |
|   11456 |  3894 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11456 |  3895 | `	pGen->pEnd = pTmp;` |
|   11456 |  3896 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11456 |  3897 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3898 | `		return SXERR_ABORT;` |
|       - |  3899 | `	}` |
|       - |  3900 | `	/* Fix post-continue jumps */` |
|   11456 |  3901 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3902 | `		JumpFixup *aPost;` |
|       - |  3903 | `		VmInstr *pInstr;` |
|       - |  3904 | `		sxu32 nJumpDest;` |
|       - |  3905 | `		sxu32 n;` |
|      14 |  3906 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3907 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3908 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3909 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3910 | `			if( pInstr ){` |
|       - |  3911 | `				/* Fix jump */` |
|      14 |  3912 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3913 | `			}` |
|       8 |  3914 | `		}` |
|       6 |  3915 | `	}` |
|       - |  3916 | `	/* compile the post-expressions if available */` |
|   11456 |  3917 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3918 | `		pPostStart++;` |
|     ! 0 |  3919 | `	}` |
|   11456 |  3920 | `	if( pPostStart < pEnd ){` |
|       - |  3921 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11456 |  3922 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11456 |  3923 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11456 |  3924 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3925 | `			/* Syntax error */` |
|     ! 0 |  3926 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  3927 | `			if( rc == SXERR_ABORT ){` |
|       - |  3928 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3929 | `				return SXERR_ABORT;` |
|       - |  3930 | `			}` |
|     ! 0 |  3931 | `			return SXRET_OK;` |
|       - |  3932 | `		}` |
|   11456 |  3933 | `		RE_SWAP_DELIMITER(pGen);` |
|   11456 |  3934 | `		if( rc == SXERR_ABORT ){` |
|       - |  3935 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3936 | `			return SXERR_ABORT;` |
|   11456 |  3937 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  3938 | `			/* Pop operand lvalue */` |
|   11456 |  3939 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5727 |  3940 | `		}` |
|    5727 |  3941 | `	}` |
|       - |  3942 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11456 |  3943 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  3944 | `	/* Fix all jumps now the destination is resolved */` |
|   11456 |  3945 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3946 | `	/* Release the loop block */` |
|   11456 |  3947 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3948 | `	/* Statement successfully compiled */` |
|   11456 |  3949 | `	return SXRET_OK;` |
|    5731 |  3950 |  |
|       - |  3951 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  3952 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  3953 | ` * are allowed.` |
|       - |  3954 | ` */` |
|    6092 |  3955 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  3956 |  |
|    6094 |  3957 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6094 |  3958 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  3959 | `		/* Unexpected expression */` |
|     ! 0 |  3960 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  3961 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  3962 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  3963 | `			rc = SXERR_INVALID;` |
|     ! 0 |  3964 | `		}` |
|     ! 0 |  3965 | `	}` |
|    6094 |  3966 | `	return rc;` |
|       2 |  3967 |  |
|       - |  3968 | `/*` |
|       - |  3969 | ` * Compile the 'foreach' statement.` |
|       - |  3970 | ` * According to the PHP language reference` |
|       - |  3971 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  3972 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  3973 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  3974 | ` *  is a minor but useful extension of the first:` |
|       - |  3975 | ` *  foreach (array_expression as $value)` |
|       - |  3976 | ` *    statement` |
|       - |  3977 | ` *  foreach (array_expression as $key => $value)` |
|       - |  3978 | ` *   statement` |
|       - |  3979 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  3980 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  3981 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  3982 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  3983 | ` *  to the variable $key on each loop.` |
|       - |  3984 | ` *  Note:` |
|       - |  3985 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  3986 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  3987 | ` *  Note:` |
|       - |  3988 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  3989 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  3990 | ` *  or after the foreach without resetting it.` |
|       - |  3991 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  3992 | ` *  of copying the value.` |
|       - |  3993 | ` */` |
|    3102 |  3994 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  3995 |  |
|    3104 |  3996 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3104 |  3997 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3104 |  3998 | `	GenBlock *pForeachBlock = 0;` |
|       - |  3999 | `	ph7_foreach_info *pInfo;` |
|       - |  4000 | `	sxu32 nFalseJump;` |
|       - |  4001 | `	VmInstr *pInstr;` |
|       - |  4002 | `	sxu32 nLine;` |
|       - |  4003 | `	sxi32 rc;` |
|    3104 |  4004 | `	nLine = pGen->pIn->nLine;` |
|       - |  4005 | `	/* Jump the 'foreach' keyword */` |
|    3104 |  4006 | `	pGen->pIn++;` |
|    3104 |  4007 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4008 | `		/* Syntax error */` |
|     ! 0 |  4009 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4010 | `		if( rc == SXERR_ABORT ){` |
|       - |  4011 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4012 | `			return SXERR_ABORT;` |
|       - |  4013 | `		}` |
|     ! 0 |  4014 | `		goto Synchronize;` |
|       - |  4015 | `	}` |
|       - |  4016 | `	/* Jump the left parenthesis '(' */` |
|    3104 |  4017 | `	pGen->pIn++;` |
|       - |  4018 | `	/* Create the loop block */` |
|    3104 |  4019 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3104 |  4020 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4021 | `		return SXERR_ABORT;` |
|       - |  4022 | `	}` |
|       - |  4023 | `	/* Delimit the expression */` |
|    3104 |  4024 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3104 |  4025 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4026 | `		/* Empty expression */` |
|     ! 0 |  4027 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4028 | `		if( rc == SXERR_ABORT ){` |
|       - |  4029 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4030 | `			return SXERR_ABORT;` |
|       - |  4031 | `		}` |
|       - |  4032 | `		/* Synchronize */` |
|     ! 0 |  4033 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4034 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4035 | `			pGen->pIn++;` |
|     ! 0 |  4036 | `		}` |
|     ! 0 |  4037 | `		return SXRET_OK;` |
|       - |  4038 | `	}` |
|       - |  4039 | `	/* Compile the array expression */` |
|    3104 |  4040 | `	pCur = pGen->pIn;` |
|   20828 |  4041 | `	while( pCur < pEnd ){` |
|   20828 |  4042 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3114 |  4043 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3114 |  4044 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4045 | `				/* Break with the first 'as' found */` |
|    3104 |  4046 | `				break;` |
|       - |  4047 | `			}` |
|       5 |  4048 | `		}` |
|       - |  4049 | `		/* Advance the stream cursor */` |
|   17726 |  4050 | `		pCur++;` |
|       2 |  4051 | `	}` |
|    3104 |  4052 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4053 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4054 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4055 | `		if( rc == SXERR_ABORT ){` |
|       - |  4056 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4057 | `			return SXERR_ABORT;` |
|       - |  4058 | `		}` |
|     ! 0 |  4059 | `		goto Synchronize;` |
|       - |  4060 | `	}` |
|       - |  4061 | `	/* Swap token streams */` |
|    3104 |  4062 | `	pTmp = pGen->pEnd;` |
|    3104 |  4063 | `	pGen->pEnd = pCur;` |
|    3104 |  4064 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3104 |  4065 | `	if( rc == SXERR_ABORT ){` |
|       - |  4066 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4067 | `		return SXERR_ABORT;` |
|       - |  4068 | `	}` |
|       - |  4069 | `	/* Update token stream */` |
|    3104 |  4070 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4071 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4072 | `		if( rc == SXERR_ABORT ){` |
|       - |  4073 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4074 | `			return SXERR_ABORT;` |
|       - |  4075 | `		}` |
|     ! 0 |  4076 | `		pGen->pIn++;` |
|     ! 0 |  4077 | `	}` |
|    3104 |  4078 | `	pCur++; /* Jump the 'as' keyword */` |
|    3104 |  4079 | `	pGen->pIn = pCur;` |
|    3104 |  4080 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4081 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4082 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4083 | `			return SXERR_ABORT;` |
|       - |  4084 | `		}` |
|     ! 0 |  4085 | `	}` |
|       - |  4086 | `	/* Create the foreach context */` |
|    3104 |  4087 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3104 |  4088 | `	if( pInfo == 0 ){` |
|     ! 0 |  4089 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4090 | `		return SXERR_ABORT;` |
|       - |  4091 | `	}` |
|       - |  4092 | `	/* Zero the structure */` |
|    3104 |  4093 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4094 | `	/* Initialize structure fields */` |
|    3104 |  4095 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4096 | `	/* Check if we have a key field */` |
|    9358 |  4097 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6256 |  4098 | `		pCur++;` |
|       2 |  4099 | `	}` |
|    3104 |  4100 | `	if( pCur < pEnd ){` |
|       - |  4101 | `		/* Compile the expression holding the key name */` |
|    3002 |  4102 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4103 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4104 | `			if( rc == SXERR_ABORT ){` |
|       - |  4105 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4106 | `				return SXERR_ABORT;` |
|       - |  4107 | `			}` |
|     ! 0 |  4108 | `		}else{` |
|    3002 |  4109 | `			pGen->pEnd = pCur;` |
|    3002 |  4110 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3002 |  4111 | `			if( rc == SXERR_ABORT ){` |
|       - |  4112 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4113 | `				return SXERR_ABORT;` |
|       - |  4114 | `			}` |
|    3002 |  4115 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3002 |  4116 | `			if( pInstr->p3 ){` |
|       - |  4117 | `				/* Record key name */` |
|    3002 |  4118 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1500 |  4119 | `			}` |
|    3002 |  4120 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4121 | `		}` |
|    3002 |  4122 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1500 |  4123 | `	}` |
|    3104 |  4124 | `	pGen->pEnd = pEnd;` |
|    3104 |  4125 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4126 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4127 | `		if( rc == SXERR_ABORT ){` |
|       - |  4128 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4129 | `			return SXERR_ABORT;` |
|       - |  4130 | `		}` |
|     ! 0 |  4131 | `		goto Synchronize;` |
|       - |  4132 | `	}` |
|    3104 |  4133 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4134 | `		pGen->pIn++;` |
|       - |  4135 | `		/* Pass by reference  */` |
|      11 |  4136 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4137 | `	}` |
|       - |  4138 | `	/* Check if the value target is list() */` |
|    3104 |  4139 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4140 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4141 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4142 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4143 | `		 */` |
|       - |  4144 | `		static int iForeachListCnt = 0;` |
|       - |  4145 | `		char zTmp[128];` |
|       - |  4146 | `		sxu32 nLen;` |
|       - |  4147 | `		char *zDup;` |
|      10 |  4148 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4149 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4150 | `		if( zDup == 0 ){` |
|     ! 0 |  4151 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4152 | `			return SXERR_ABORT;` |
|       - |  4153 | `		}` |
|      10 |  4154 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4155 | `		/* Save list() token boundaries */` |
|      10 |  4156 | `		pListStart = pGen->pIn;` |
|       - |  4157 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4158 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4159 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4160 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4161 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4162 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4163 | `				return SXERR_ABORT;` |
|       - |  4164 | `			}` |
|       3 |  4165 | `			goto Synchronize;` |
|       - |  4166 | `		}` |
|       7 |  4167 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4168 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4169 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4170 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4171 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4172 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4173 | `				return SXERR_ABORT;` |
|       - |  4174 | `			}` |
|     ! 0 |  4175 | `			goto Synchronize;` |
|       - |  4176 | `		}` |
|       7 |  4177 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4178 | `		pListEnd = pGen->pIn;` |
|       7 |  4179 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3099 |  4180 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4181 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4182 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4183 | `		 */` |
|       - |  4184 | `		static int iForeachShortListCnt = 0;` |
|       - |  4185 | `		char zTmp[128];` |
|       - |  4186 | `		sxu32 nLen;` |
|       - |  4187 | `		char *zDup;` |
|       3 |  4188 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4189 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4190 | `		if( zDup == 0 ){` |
|     ! 0 |  4191 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4192 | `			return SXERR_ABORT;` |
|       - |  4193 | `		}` |
|       3 |  4194 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4195 | `		/* Save [...] token boundaries */` |
|       3 |  4196 | `		pListStart = pGen->pIn;` |
|       - |  4197 | `		/* Advance past [...] */` |
|       3 |  4198 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4199 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4200 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4201 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4202 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4203 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4204 | `				return SXERR_ABORT;` |
|       - |  4205 | `			}` |
|     ! 0 |  4206 | `			goto Synchronize;` |
|       - |  4207 | `		}` |
|       3 |  4208 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4209 | `		pListEnd = pGen->pIn;` |
|       3 |  4210 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4211 | `	}else{` |
|       - |  4212 | `		/* Compile the expression holding the value name */` |
|    3094 |  4213 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3094 |  4214 | `		if( rc == SXERR_ABORT ){` |
|       - |  4215 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4216 | `			return SXERR_ABORT;` |
|       - |  4217 | `		}` |
|    3094 |  4218 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3094 |  4219 | `		if( pInstr->p3 ){` |
|       - |  4220 | `			/* Record value name */` |
|    3094 |  4221 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1546 |  4222 | `		}` |
|       - |  4223 | `	}` |
|       - |  4224 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3102 |  4225 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4226 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3102 |  4227 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4228 | `	/* Record the first instruction to execute */` |
|    3102 |  4229 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4230 | `	/* Emit the FOREACH_STEP instruction */` |
|    3102 |  4231 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4232 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3102 |  4233 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4234 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3102 |  4235 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4236 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4237 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4238 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4239 | `		 */` |
|       9 |  4240 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4241 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4242 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4243 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4244 | `		 */` |
|       9 |  4245 | `		pSavedIn = pGen->pIn;` |
|       9 |  4246 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4247 | `		pGen->pIn = pListStart;` |
|       9 |  4248 | `		pGen->pEnd = pListEnd;` |
|       9 |  4249 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4250 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4251 | `		}else{` |
|       7 |  4252 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4253 | `		}` |
|       9 |  4254 | `		pGen->pIn = pSavedIn;` |
|       9 |  4255 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4256 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4257 | `			return SXERR_ABORT;` |
|       - |  4258 | `		}` |
|       - |  4259 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4260 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4261 | `	}` |
|       - |  4262 | `	/* Compile the loop body */` |
|    3102 |  4263 | `	pGen->pIn = &pEnd[1];` |
|    3102 |  4264 | `	pGen->pEnd = pTmp;` |
|    3102 |  4265 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3102 |  4266 | `	if( rc == SXERR_ABORT ){` |
|       - |  4267 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4268 | `		return SXERR_ABORT;` |
|       - |  4269 | `	}` |
|       - |  4270 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3102 |  4271 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4272 | `	/* Fix all jumps now the destination is resolved */` |
|    3102 |  4273 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4274 | `	/* Release the loop block */` |
|    3102 |  4275 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4276 | `	/* Statement successfully compiled */` |
|    3102 |  4277 | `	return SXRET_OK;` |
|       1 |  4278 | `Synchronize:` |
|       - |  4279 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4280 | `	 * compiling this erroneous block.` |
|       - |  4281 | `	 */` |
|       3 |  4282 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4283 | `		pGen->pIn++;` |
|     ! 0 |  4284 | `	}` |
|       3 |  4285 | `	return SXRET_OK;` |
|    1553 |  4286 |  |
|       - |  4287 | `/*` |
|       - |  4288 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4289 | ` * According to the PHP language reference` |
|       - |  4290 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4291 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4292 | ` *  that is similar to that of C:` |
|       - |  4293 | ` *  if (expr)` |
|       - |  4294 | ` *   statement` |
|       - |  4295 | ` *  else construct:` |
|       - |  4296 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4297 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4298 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4299 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4300 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4301 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4302 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4303 | ` *  elseif` |
|       - |  4304 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4305 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4306 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4307 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4308 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4309 | ` *   <?php` |
|       - |  4310 | ` *    if ($a > $b) {` |
|       - |  4311 | ` *     echo "a is bigger than b";` |
|       - |  4312 | ` *    } elseif ($a == $b) {` |
|       - |  4313 | ` *     echo "a is equal to b";` |
|       - |  4314 | ` *    } else {` |
|       - |  4315 | ` *     echo "a is smaller than b";` |
|       - |  4316 | ` *    }` |
|       - |  4317 | ` *    ?>` |
|       - |  4318 | ` */` |
|  113728 |  4319 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4320 |  |
|  113730 |  4321 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  113730 |  4322 | `	GenBlock *pCondBlock = 0;` |
|       - |  4323 | `	sxu32 nJumpIdx;` |
|       - |  4324 | `	sxu32 nKeyID;` |
|       - |  4325 | `	sxi32 rc;` |
|       - |  4326 | `	/* Jump the 'if' keyword */` |
|  113730 |  4327 | `	pGen->pIn++;` |
|  113730 |  4328 | `	pToken = pGen->pIn;` |
|       - |  4329 | `	/* Create the conditional block */` |
|  113730 |  4330 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  113730 |  4331 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4332 | `		return SXERR_ABORT;` |
|       - |  4333 | `	}` |
|       - |  4334 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   62553 |  4335 | `	for(;;){` |
|  125108 |  4336 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4337 | `			/* Syntax error */` |
|     ! 0 |  4338 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4339 | `				pToken--;` |
|     ! 0 |  4340 | `			}` |
|     ! 0 |  4341 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4342 | `			if( rc == SXERR_ABORT ){` |
|       - |  4343 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4344 | `				return SXERR_ABORT;` |
|       - |  4345 | `			}` |
|     ! 0 |  4346 | `			goto Synchronize;` |
|       - |  4347 | `		}` |
|       - |  4348 | `		/* Jump the left parenthesis '(' */` |
|  125108 |  4349 | `		pToken++;` |
|       - |  4350 | `		/* Delimit the condition */` |
|  125108 |  4351 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  125108 |  4352 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4353 | `			/* Syntax error */` |
|     ! 0 |  4354 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4355 | `				pToken--;` |
|     ! 0 |  4356 | `			}` |
|     ! 0 |  4357 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4358 | `			if( rc == SXERR_ABORT ){` |
|       - |  4359 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4360 | `				return SXERR_ABORT;` |
|       - |  4361 | `			}` |
|     ! 0 |  4362 | `			goto Synchronize;` |
|       - |  4363 | `		}` |
|       - |  4364 | `		/* Swap token streams */` |
|  125108 |  4365 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4366 | `		/* Compile the condition */` |
|  125108 |  4367 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4368 | `		/* Update token stream */` |
|  125108 |  4369 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4370 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4371 | `			pGen->pIn++;` |
|     ! 0 |  4372 | `		}` |
|  125108 |  4373 | `		pGen->pIn  = &pEnd[1];` |
|  125108 |  4374 | `		pGen->pEnd = pTmp;` |
|  125108 |  4375 | `		if( rc == SXERR_ABORT ){` |
|       - |  4376 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4377 | `			return SXERR_ABORT;` |
|       - |  4378 | `		}` |
|       - |  4379 | `		/* Emit the false jump */` |
|  125108 |  4380 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4381 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  125108 |  4382 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4383 | `		/* Compile the body */` |
|  125108 |  4384 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  125108 |  4385 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4386 | `			return SXERR_ABORT;` |
|       - |  4387 | `		}` |
|  125108 |  4388 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   33658 |  4389 | `			break;` |
|       - |  4390 | `		}` |
|       - |  4391 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   57796 |  4392 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   57796 |  4393 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   37180 |  4394 | `			break;` |
|       - |  4395 | `		}` |
|       - |  4396 | `		/* Emit the unconditional jump */` |
|   20618 |  4397 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4398 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   20618 |  4399 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   20618 |  4400 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14916 |  4401 | `			pToken = &pGen->pIn[1];` |
|   14916 |  4402 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5706 |  4403 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4621 |  4404 | `					break;` |
|       - |  4405 | `			}` |
|    5678 |  4406 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2838 |  4407 | `		}` |
|   11380 |  4408 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4409 | `		/* Synchronize cursors */` |
|   11380 |  4410 | `		pToken = pGen->pIn;` |
|       - |  4411 | `		/* Fix the false jump */` |
|   11380 |  4412 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4413 | `	} /* For(;;) */` |
|       - |  4414 | `	/* Fix the false jump */` |
|  113730 |  4415 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  113730 |  4416 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   46416 |  4417 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4418 | `			/* Compile the else block */` |
|    9240 |  4419 | `			pGen->pIn++;` |
|    9240 |  4420 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9240 |  4421 | `			if( rc == SXERR_ABORT ){` |
|       - |  4422 |  |
|     ! 0 |  4423 | `				return SXERR_ABORT;` |
|       - |  4424 | `			}` |
|    4619 |  4425 | `	}` |
|  113730 |  4426 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4427 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  113730 |  4428 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4429 | `	/* Release the conditional block */` |
|  113730 |  4430 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4431 | `	/* Statement successfully compiled */` |
|  113730 |  4432 | `	return SXRET_OK;` |
|     ! 0 |  4433 | `Synchronize:` |
|       - |  4434 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4435 | `	 */` |
|     ! 0 |  4436 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4437 | `		pGen->pIn++;` |
|     ! 0 |  4438 | `	}` |
|     ! 0 |  4439 | `	return SXRET_OK;` |
|   56866 |  4440 |  |
|       - |  4441 | `/*` |
|       - |  4442 | ` * Compile the global construct.` |
|       - |  4443 | ` * According to the PHP language reference` |
|       - |  4444 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4445 | ` *  to be used in that function.` |
|       - |  4446 | ` *  Example #1 Using global` |
|       - |  4447 | ` *  <?php` |
|       - |  4448 | ` *   $a = 1;` |
|       - |  4449 | ` *   $b = 2;` |
|       - |  4450 | ` *   function Sum()` |
|       - |  4451 | ` *   {` |
|       - |  4452 | ` *    global $a, $b;` |
|       - |  4453 | ` *    $b = $a + $b;` |
|       - |  4454 | ` *   }` |
|       - |  4455 | ` *   Sum();` |
|       - |  4456 | ` *   echo $b;` |
|       - |  4457 | ` *  ?>` |
|       - |  4458 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4459 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4460 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4461 | ` */` |
|      30 |  4462 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4463 |  |
|      32 |  4464 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4465 | `	sxi32 nExpr;` |
|       - |  4466 | `	sxi32 rc;` |
|       - |  4467 | `	/* Jump the 'global' keyword */` |
|      32 |  4468 | `	pGen->pIn++;` |
|      32 |  4469 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4470 | `		/* Nothing to process */` |
|     ! 0 |  4471 | `		return SXRET_OK;` |
|       - |  4472 | `	}` |
|      32 |  4473 | `	pTmp = pGen->pEnd;` |
|      32 |  4474 | `	nExpr = 0;` |
|      64 |  4475 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      34 |  4476 | `		if( pGen->pIn < pNext ){` |
|      34 |  4477 | `			pGen->pEnd = pNext;` |
|      34 |  4478 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4479 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4480 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4481 | `					return SXERR_ABORT;` |
|       - |  4482 | `				}` |
|     ! 0 |  4483 | `			}else{` |
|      34 |  4484 | `				pGen->pIn++;` |
|      34 |  4485 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4486 | `					/* Emit a warning */` |
|     ! 0 |  4487 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4488 | `				}else{` |
|      34 |  4489 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      34 |  4490 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4491 | `						return SXERR_ABORT;` |
|      34 |  4492 | `					}else if(rc != SXERR_EMPTY ){` |
|      34 |  4493 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      34 |  4494 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4495 | `							/* Variable name, not a constant */` |
|      34 |  4496 | `							pLast->iP1 = 0;` |
|      16 |  4497 | `						}` |
|      34 |  4498 | `						nExpr++;` |
|      16 |  4499 | `					}` |
|       - |  4500 | `				}` |
|       - |  4501 | `			}` |
|      16 |  4502 | `		}` |
|       - |  4503 | `		/* Next expression in the stream */` |
|      34 |  4504 | `		pGen->pIn = pNext;` |
|       - |  4505 | `		/* Jump trailing commas */` |
|      36 |  4506 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4507 | `			pGen->pIn++;` |
|       1 |  4508 | `		}` |
|       2 |  4509 | `	}` |
|       - |  4510 | `	/* Restore token stream */` |
|      32 |  4511 | `	pGen->pEnd = pTmp;` |
|      32 |  4512 | `	if( nExpr > 0 ){` |
|       - |  4513 | `		/* Emit the uplink instruction */` |
|      32 |  4514 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      15 |  4515 | `	}` |
|      32 |  4516 | `	return SXRET_OK;` |
|      17 |  4517 |  |
|       - |  4518 | `/*` |
|       - |  4519 | ` * Compile the return statement.` |
|       - |  4520 | ` * According to the PHP language reference` |
|       - |  4521 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4522 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4523 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4524 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4525 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4526 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4527 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4528 | ` *  from within the main script file, then script execution end.` |
|       - |  4529 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4530 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4531 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4532 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4533 | ` */` |
|  165332 |  4534 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4535 |  |
|  165334 |  4536 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4537 | `	sxi32 rc;` |
|       - |  4538 | `	/* Jump the 'return' keyword */` |
|  165334 |  4539 | `	pGen->pIn++;` |
|  165334 |  4540 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4541 | `		/* Compile the expression */` |
|  165312 |  4542 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  165312 |  4543 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4544 | `			return SXERR_ABORT;` |
|  165312 |  4545 | `		}else if(rc != SXERR_EMPTY ){` |
|  165312 |  4546 | `			nRet = 1;` |
|   82655 |  4547 | `		}` |
|   82655 |  4548 | `	}` |
|       - |  4549 | `	/* Emit the done instruction */` |
|  165334 |  4550 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  165334 |  4551 | `	return SXRET_OK;` |
|   82668 |  4552 |  |
|       - |  4553 | `/*` |
|       - |  4554 | ` * Compile a yield expression.` |
|       - |  4555 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4556 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4557 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4558 | ` */` |
|      34 |  4559 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  4560 |  |
|       - |  4561 | `	SyToken *pTmp, *pSplit;` |
|      36 |  4562 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      36 |  4563 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4564 | `	sxi32 rc;` |
|      17 |  4565 | `	(void)iCompileFlag;` |
|       - |  4566 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      36 |  4567 | `	pGen->pIn++;` |
|       - |  4568 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4569 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      36 |  4570 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4571 | `		/* Bare yield — no value */` |
|     ! 0 |  4572 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4573 | `		return SXRET_OK;` |
|       - |  4574 | `	}` |
|       - |  4575 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      36 |  4576 | `	pSplit = 0;` |
|       - |  4577 | `	{` |
|      36 |  4578 | `		SyToken *pCur = pGen->pIn;` |
|      36 |  4579 | `		sxi32 nNest = 0;` |
|      84 |  4580 | `		while( pCur < pGen->pEnd ){` |
|      56 |  4581 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4582 | `				nNest++;` |
|      56 |  4583 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4584 | `				nNest--;` |
|      56 |  4585 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 |  4586 | `				pSplit = pCur;` |
|       7 |  4587 | `				break;` |
|       - |  4588 | `			}` |
|      50 |  4589 | `			pCur++;` |
|       2 |  4590 | `		}` |
|       - |  4591 | `	}` |
|      36 |  4592 | `	pTmp = pGen->pEnd;` |
|      36 |  4593 | `	if( pSplit ){` |
|       - |  4594 | `		/* yield $key => $value */` |
|       7 |  4595 | `		pGen->pEnd = pSplit;` |
|       7 |  4596 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4597 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4598 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 |  4599 | `		pGen->pEnd = pTmp;` |
|       7 |  4600 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4601 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4602 | `		iP1 = 1;` |
|       7 |  4603 | `		iP2 = 1;` |
|       4 |  4604 | `	}else{` |
|       - |  4605 | `		/* yield $value */` |
|      30 |  4606 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      30 |  4607 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      30 |  4608 | `		if( rc != SXERR_EMPTY ){` |
|      30 |  4609 | `			iP1 = 1;` |
|      14 |  4610 | `		}` |
|       - |  4611 | `	}` |
|      36 |  4612 | `	pGen->pEnd = pTmp;` |
|      36 |  4613 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      36 |  4614 | `	return SXRET_OK;` |
|      19 |  4615 |  |
|       - |  4616 | `/*` |
|       - |  4617 | ` * Compile the die/exit language construct.` |
|       - |  4618 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4619 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4620 | ` */` |
|      88 |  4621 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 |  4622 |  |
|      90 |  4623 | `	sxi32 nExpr = 0;` |
|       - |  4624 | `	sxi32 rc;` |
|       - |  4625 | `	/* Jump the die/exit keyword */` |
|      90 |  4626 | `	pGen->pIn++;` |
|      90 |  4627 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4628 | `		/* Compile the expression */` |
|      90 |  4629 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 |  4630 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4631 | `			return SXERR_ABORT;` |
|      90 |  4632 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 |  4633 | `			nExpr = 1;` |
|      44 |  4634 | `		}` |
|      44 |  4635 | `	}` |
|       - |  4636 | `	/* Emit the HALT instruction */` |
|      90 |  4637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 |  4638 | `	return SXRET_OK;` |
|      46 |  4639 |  |
|       - |  4640 | `/*` |
|       - |  4641 | ` * Compile the 'echo' language construct.` |
|       - |  4642 | ` */` |
|   11578 |  4643 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4644 |  |
|   11580 |  4645 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4646 | `	sxi32 rc;` |
|       - |  4647 | `	/* Jump the 'echo' keyword */` |
|   11580 |  4648 | `	pGen->pIn++;` |
|       - |  4649 | `	/* Compile arguments one after one */` |
|   11580 |  4650 | `	pTmp = pGen->pEnd;` |
|   24008 |  4651 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   12430 |  4652 | `		if( pGen->pIn < pNext ){` |
|   12430 |  4653 | `			pGen->pEnd = pNext;` |
|   12430 |  4654 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   12430 |  4655 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4656 | `				return SXERR_ABORT;` |
|   12430 |  4657 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4658 | `				/* Emit the consume instruction */` |
|   12406 |  4659 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6202 |  4660 | `			}` |
|    6214 |  4661 | `		}` |
|       - |  4662 | `		/* Jump trailing commas */` |
|   13280 |  4663 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     852 |  4664 | `			pNext++;` |
|       2 |  4665 | `		}` |
|   12430 |  4666 | `		pGen->pIn = pNext;` |
|       2 |  4667 | `	}` |
|       - |  4668 | `	/* Restore token stream */` |
|   11580 |  4669 | `	pGen->pEnd = pTmp;` |
|   11580 |  4670 | `	return SXRET_OK;` |
|    5791 |  4671 |  |
|       - |  4672 | `/*` |
|       - |  4673 | ` * Compile the static statement.` |
|       - |  4674 | ` * According to the PHP language reference` |
|       - |  4675 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4676 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4677 | ` *  when program execution leaves this scope.` |
|       - |  4678 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4679 | ` * Symisc eXtension.` |
|       - |  4680 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4681 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4682 | ` *  Example` |
|       - |  4683 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4684 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4685 | ` */` |
|       2 |  4686 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 |  4687 |  |
|       - |  4688 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4689 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4690 | `	GenBlock *pBlock;` |
|       - |  4691 | `	SyString *pName;` |
|       - |  4692 | `	char *zDup;` |
|       - |  4693 | `	sxu32 nLine;` |
|       - |  4694 | `	sxi32 rc;` |
|       - |  4695 | `	/* Jump the static keyword */` |
|       3 |  4696 | `	nLine = pGen->pIn->nLine;` |
|       3 |  4697 | `	pGen->pIn++;` |
|       - |  4698 | `	/* Extract the enclosing function if any */` |
|       3 |  4699 | `	pBlock = pGen->pCurrent;` |
|       5 |  4700 | `	while( pBlock ){` |
|       5 |  4701 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 |  4702 | `			break;` |
|       - |  4703 | `		}` |
|       - |  4704 | `		/* Point to the upper block */` |
|       3 |  4705 | `		pBlock = pBlock->pParent;` |
|       1 |  4706 | `	}` |
|       3 |  4707 | `	if( pBlock == 0 ){` |
|       - |  4708 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4709 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4710 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4711 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4712 | `				return SXERR_ABORT;` |
|       - |  4713 | `			}` |
|     ! 0 |  4714 | `			goto Synchronize;` |
|       - |  4715 | `		}` |
|       - |  4716 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4717 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4718 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4719 | `			return SXERR_ABORT;` |
|     ! 0 |  4720 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4721 | `			/* Emit the POP instruction */` |
|     ! 0 |  4722 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4723 | `		}` |
|     ! 0 |  4724 | `		return SXRET_OK;` |
|       - |  4725 | `	}` |
|       3 |  4726 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4727 | `	/* Make sure we are dealing with a valid statement */` |
|       3 |  4728 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 |  4729 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4730 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4731 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4732 | `				return SXERR_ABORT;` |
|       - |  4733 | `			}` |
|       3 |  4734 | `			goto Synchronize;` |
|       - |  4735 | `	}` |
|     ! 0 |  4736 | `	pGen->pIn++;` |
|       - |  4737 | `	/* Extract variable name */` |
|     ! 0 |  4738 | `	pName = &pGen->pIn->sData;` |
|     ! 0 |  4739 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 |  4740 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4741 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4742 | `		goto Synchronize;` |
|       - |  4743 | `	}` |
|       - |  4744 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 |  4745 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 |  4746 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4747 | `	/* Duplicate variable name */` |
|     ! 0 |  4748 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 |  4749 | `	if( zDup == 0 ){` |
|     ! 0 |  4750 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4751 | `		return SXERR_ABORT;` |
|       - |  4752 | `	}` |
|     ! 0 |  4753 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4754 | `	/* Check if we have an expression to compile */` |
|     ! 0 |  4755 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4756 | `		SySet *pInstrContainer;` |
|       - |  4757 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4758 | `		 * Static variable can take any complex expression including function` |
|       - |  4759 | `		 * call as their initialization value.` |
|       - |  4760 | `		 * Example:` |
|       - |  4761 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4762 | `		 */` |
|     ! 0 |  4763 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4764 | `		/* Swap bytecode container */` |
|     ! 0 |  4765 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 |  4766 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4767 | `		/* Compile the expression */` |
|     ! 0 |  4768 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4769 | `		/* Emit the done instruction */` |
|     ! 0 |  4770 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4771 | `		/* Restore default bytecode container */` |
|     ! 0 |  4772 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  4773 | `	}` |
|       - |  4774 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 |  4775 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 |  4776 | `	return SXRET_OK;` |
|       1 |  4777 | `Synchronize:` |
|       - |  4778 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4779 | `	 * statement.` |
|       - |  4780 | `	 */` |
|       5 |  4781 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4782 | `		pGen->pIn++;` |
|       1 |  4783 | `	}` |
|       3 |  4784 | `	return SXRET_OK;` |
|       2 |  4785 |  |
|       - |  4786 | `/*` |
|       - |  4787 | ` * Compile the var statement.` |
|       - |  4788 | ` * Symisc Extension:` |
|       - |  4789 | ` *      var statement can be used outside of a class definition.` |
|       - |  4790 | ` */` |
|       4 |  4791 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4792 |  |
|       - |  4793 | `	sxu32 nLine;` |
|       - |  4794 | `	sxi32 rc;` |
|       5 |  4795 | `	nLine = pGen->pIn->nLine;` |
|       - |  4796 | `	/* Jump the 'var' keyword */` |
|       5 |  4797 | `	pGen->pIn++;` |
|       5 |  4798 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4799 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4800 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4801 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4802 | `			pGen->pIn++;` |
|     ! 0 |  4803 | `		}` |
|     ! 0 |  4804 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4805 | `			return SXERR_ABORT;` |
|       - |  4806 | `		}` |
|     ! 0 |  4807 | `	}else{` |
|       - |  4808 | `		/* Compile the expression */` |
|       5 |  4809 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4810 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4811 | `			return SXERR_ABORT;` |
|       5 |  4812 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4813 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4814 | `		}` |
|       - |  4815 | `	}` |
|       5 |  4816 | `	return SXRET_OK;` |
|       3 |  4817 |  |
|       - |  4818 | `/*` |
|       - |  4819 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4820 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4821 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4822 | ` */` |
|       - |  4823 | `/*` |
|       - |  4824 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4825 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4826 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4827 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4828 | ` *` |
|       - |  4829 | ` * Resolution order:` |
|       - |  4830 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4831 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4832 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4833 | ` *` |
|       - |  4834 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4835 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4836 | ` * Returns the (possibly new) literal index.` |
|       - |  4837 | ` */` |
|  339256 |  4838 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 |  4839 |  |
|       - |  4840 | `	ph7_value *pLit;` |
|       - |  4841 | `	const char *zLit;` |
|       - |  4842 | `	SyString sQualified;` |
|       - |  4843 | `	sxu32 nLit;` |
|       - |  4844 | `	sxu32 k;` |
|       - |  4845 | `	sxu32 nNewIdx;` |
|       - |  4846 | `	int hasNsSep;` |
|       - |  4847 | `	SyHashEntry *pImport;` |
|       - |  4848 | `	ph7_value *pNew;` |
|  339258 |  4849 | `	if( pFromImport ){` |
|  324318 |  4850 | `		*pFromImport = 0;` |
|  162158 |  4851 | `	}` |
|  339258 |  4852 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  339258 |  4853 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4854 | `		return nOrigIdx;` |
|       - |  4855 | `	}` |
|  339258 |  4856 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  339258 |  4857 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4858 | `	/* Skip if already qualified (contains backslash) */` |
|  339258 |  4859 | `	hasNsSep = 0;` |
| 3647272 |  4860 | `	for( k = 0; k < nLit; k++ ){` |
| 3308052 |  4861 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1654009 |  4862 | `	}` |
|  339258 |  4863 | `	if( hasNsSep ){` |
|      38 |  4864 | `		return nOrigIdx;` |
|       - |  4865 | `	}` |
|       - |  4866 | `	/* Check use imports first (works even outside namespaces) */` |
|  339222 |  4867 | `	SyBlobReset(&pGen->sWorker);` |
|  339222 |  4868 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  339222 |  4869 | `	if( pImport ){` |
|      38 |  4870 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4871 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4872 | `		if( pFromImport ){` |
|      18 |  4873 | `			*pFromImport = 1;` |
|       8 |  4874 | `		}` |
|      20 |  4875 | `	}else{` |
|  339186 |  4876 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  339098 |  4877 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4878 | `		}` |
|       - |  4879 | `		/* Prepend current namespace */` |
|      90 |  4880 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      90 |  4881 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      90 |  4882 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4883 | `	}` |
|       - |  4884 | `	/* Look up or create a new literal for the qualified name */` |
|     126 |  4885 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     126 |  4886 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 |  4887 | `		return nNewIdx; /* Already interned */` |
|       - |  4888 | `	}` |
|      74 |  4889 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      74 |  4890 | `	if( pNew == 0 ){` |
|     ! 0 |  4891 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4892 | `	}` |
|      74 |  4893 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      74 |  4894 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      74 |  4895 | `	return nNewIdx;` |
|  169630 |  4896 |  |
|       - |  4897 | `/*` |
|       - |  4898 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4899 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4900 | ` */` |
|   28762 |  4901 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4902 |  |
|       - |  4903 | `	SyHashEntry *pImport;` |
|       - |  4904 | `	/* Check use imports first */` |
|   28764 |  4905 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   28764 |  4906 | `	if( pImport ){` |
|      12 |  4907 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 |  4908 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 |  4909 | `		return;` |
|       - |  4910 | `	}` |
|       - |  4911 | `	/* Prepend current namespace if active */` |
|   28754 |  4912 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4913 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4914 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4915 | `	}` |
|   28754 |  4916 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   14383 |  4917 |  |
|       - |  4918 | `/*` |
|       - |  4919 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4920 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4921 | ` * The caller must release pOut when done.` |
|       - |  4922 | ` */` |
|   48974 |  4923 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4924 |  |
|   48976 |  4925 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      54 |  4926 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      54 |  4927 | `		SyBlobAppend(pOut,"\\",1);` |
|      26 |  4928 | `	}` |
|   48976 |  4929 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   48976 |  4930 |  |
|       - |  4931 | `/*` |
|       - |  4932 | ` * Compile a namespace statement` |
|       - |  4933 | ` * According to the PHP language reference manual` |
|       - |  4934 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  4935 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  4936 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  4937 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  4938 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  4939 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  4940 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  4941 | ` *  programming world.` |
|       - |  4942 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  4943 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  4944 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  4945 | ` *  classes/functions/constants.` |
|       - |  4946 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  4947 | ` *  readability of source code.` |
|       - |  4948 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  4949 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  4950 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  4951 | ` *       class MyClass {}` |
|       - |  4952 | ` *       function myfunction() {}` |
|       - |  4953 | ` *       const MYCONST = 1;` |
|       - |  4954 | ` *       $a = new MyClass;` |
|       - |  4955 | ` *       $c = new \my\name\MyClass;` |
|       - |  4956 | ` *       $a = strlen('hi');` |
|       - |  4957 | ` *       $d = namespace\MYCONST;` |
|       - |  4958 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  4959 | ` *       echo constant($d);` |
|       - |  4960 | ` * NOTE` |
|       - |  4961 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  4962 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  4963 | ` */` |
|       - |  4964 | `/*` |
|       - |  4965 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  4966 | ` */` |
|      14 |  4967 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 |  4968 |  |
|      15 |  4969 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 |  4970 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 |  4971 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 |  4972 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 |  4973 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 |  4974 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  4975 | `	return "token";` |
|       8 |  4976 |  |
|     100 |  4977 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 |  4978 |  |
|       - |  4979 | `	sxu32 nLine;` |
|       - |  4980 | `	sxi32 rc;` |
|     102 |  4981 | `	nLine = pGen->pIn->nLine;` |
|     102 |  4982 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  4983 | `	/* Reset namespace and clear previous use imports */` |
|     102 |  4984 | `	SyBlobReset(&pGen->sNamespace);` |
|     102 |  4985 | `	SyHashRelease(&pGen->hUseImports);` |
|     102 |  4986 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  4987 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     102 |  4988 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  4989 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     102 |  4990 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  4991 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4992 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  4993 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  4994 | `		return SXRET_OK;` |
|       - |  4995 | `	}` |
|     102 |  4996 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  4997 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  4998 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  4999 | `		return SXRET_OK;` |
|       - |  5000 | `	}` |
|     102 |  5001 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5002 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5003 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5004 | `		return SXRET_OK;` |
|       - |  5005 | `	}` |
|       - |  5006 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     240 |  5007 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     140 |  5008 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5009 | `			/* Append backslash separator */` |
|      21 |  5010 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 |  5011 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 |  5012 | `			}` |
|      11 |  5013 | `		}else{` |
|       - |  5014 | `			/* Append identifier */` |
|     120 |  5015 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5016 | `		}` |
|     140 |  5017 | `		pGen->pIn++;` |
|       2 |  5018 | `	}` |
|       - |  5019 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5020 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5021 | `	{` |
|     102 |  5022 | `		char *zNsDup = 0;` |
|     102 |  5023 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     149 |  5024 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      98 |  5025 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      49 |  5026 | `		}` |
|     102 |  5027 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5028 | `	}` |
|     102 |  5029 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 |  5030 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5031 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5032 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 |  5033 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5034 | `			return SXERR_ABORT;` |
|       - |  5035 | `		}` |
|       2 |  5036 | `	}` |
|     102 |  5037 | `	return SXRET_OK;` |
|      52 |  5038 |  |
|       - |  5039 | `/*` |
|       - |  5040 | ` * Compile the 'use' statement` |
|       - |  5041 | ` * According to the PHP language reference manual` |
|       - |  5042 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5043 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5044 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5045 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5046 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5047 | ` *  a function or constant is not supported.` |
|       - |  5048 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5049 | ` * NOTE` |
|       - |  5050 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5051 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5052 | ` */` |
|      66 |  5053 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 |  5054 |  |
|       - |  5055 | `	sxu32 nLine;` |
|       - |  5056 | `	sxi32 rc;` |
|       - |  5057 | `	SyBlob sPath;` |
|       - |  5058 | `	SyString sAlias;` |
|       - |  5059 | `	SyToken *pLast;` |
|       - |  5060 | `	char *zDup;` |
|       - |  5061 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5062 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5063 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 |  5064 | `	nLine = pGen->pIn->nLine;` |
|      68 |  5065 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5066 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 |  5067 | `	iUseType = 0;` |
|      68 |  5068 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5069 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5070 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5071 | `			iUseType = 1;` |
|      16 |  5072 | `			pGen->pIn++;` |
|      23 |  5073 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5074 | `			iUseType = 2;` |
|      16 |  5075 | `			pGen->pIn++;` |
|       7 |  5076 | `		}` |
|      14 |  5077 | `	}` |
|       - |  5078 | `	/* Select target hash tables based on import type */` |
|      68 |  5079 | `	switch( iUseType ){` |
|       7 |  5080 | `		case 1:` |
|      16 |  5081 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5082 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5083 | `			break;` |
|       7 |  5084 | `		case 2:` |
|      16 |  5085 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5086 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5087 | `			break;` |
|      19 |  5088 | `		default:` |
|      40 |  5089 | `			pGenHash = &pGen->hUseImports;` |
|      40 |  5090 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 |  5091 | `			break;` |
|       - |  5092 | `	}` |
|      68 |  5093 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5094 | `	/* Process one or more use declarations separated by commas */` |
|      34 |  5095 | `	for(;;){` |
|      70 |  5096 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5097 | `			break;` |
|       - |  5098 | `		}` |
|      70 |  5099 | `		SyBlobReset(&sPath);` |
|      70 |  5100 | `		pLast = 0;` |
|       - |  5101 | `		/* Collect the full namespace path */` |
|     254 |  5102 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 |  5103 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 |  5104 | `				pLast = pGen->pIn;` |
|     126 |  5105 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 |  5106 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5107 | `				}` |
|     126 |  5108 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 |  5109 | `			}` |
|     186 |  5110 | `			pGen->pIn++;` |
|       2 |  5111 | `		}` |
|      70 |  5112 | `		if( pLast == 0 ){` |
|       - |  5113 | `			/* Empty path */` |
|       5 |  5114 | `			break;` |
|       - |  5115 | `		}` |
|       - |  5116 | `		/* Default alias is the last component of the path */` |
|      66 |  5117 | `		sAlias = pLast->sData;` |
|       - |  5118 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 |  5119 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 |  5120 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5121 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5122 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5123 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5124 | `				pGen->pIn++;` |
|       8 |  5125 | `			}` |
|       8 |  5126 | `		}` |
|       - |  5127 | `		/* Check for duplicate import alias (per-type) */` |
|      66 |  5128 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 |  5129 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5130 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5131 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 |  5132 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5133 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5134 | `				return SXERR_ABORT;` |
|       - |  5135 | `			}` |
|       2 |  5136 | `		}` |
|       - |  5137 | `		/* Register the import: alias -> FQN.` |
|       - |  5138 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5139 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5140 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 |  5141 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 |  5142 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 |  5143 | `		if( zDup ){` |
|      66 |  5144 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 |  5145 | `			if( pVmHash ){` |
|       - |  5146 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5147 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 |  5148 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 |  5149 | `				if( zAliasDup ){` |
|      38 |  5150 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 |  5151 | `				}` |
|      18 |  5152 | `			}` |
|      66 |  5153 | `			if( iUseType == 2 ){` |
|       - |  5154 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5155 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5156 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5157 | `				if( zAliasDup ){` |
|       - |  5158 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5159 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5160 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5161 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5162 | `					if( azPair ){` |
|      16 |  5163 | `						azPair[0] = zAliasDup;` |
|      16 |  5164 | `						azPair[1] = zDup;` |
|      16 |  5165 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5166 | `					}` |
|       7 |  5167 | `				}` |
|       7 |  5168 | `			}` |
|      32 |  5169 | `		}` |
|       - |  5170 | `		/* Check for comma (multiple use declarations) */` |
|      66 |  5171 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5172 | `			pGen->pIn++;` |
|       2 |  5173 | `		}else{` |
|      33 |  5174 | `			break;` |
|       - |  5175 | `		}` |
|       1 |  5176 | `	}` |
|      68 |  5177 | `	SyBlobRelease(&sPath);` |
|      68 |  5178 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5179 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5180 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5181 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5182 | `			return SXERR_ABORT;` |
|       - |  5183 | `		}` |
|       1 |  5184 | `	}` |
|      68 |  5185 | `	return SXRET_OK;` |
|      35 |  5186 |  |
|       - |  5187 | `/*` |
|       - |  5188 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5189 | ` *` |
|       - |  5190 | ` * According to the PHP language reference manual.` |
|       - |  5191 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5192 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5193 | ` *  declare (directive)` |
|       - |  5194 | ` *   statement` |
|       - |  5195 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5196 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5197 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5198 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5199 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5200 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5201 | ` * <?php` |
|       - |  5202 | ` * // these are the same:` |
|       - |  5203 | ` * // you can use this:` |
|       - |  5204 | ` * declare(ticks=1) {` |
|       - |  5205 | ` *   // entire script here` |
|       - |  5206 | ` * }` |
|       - |  5207 | ` * // or you can use this:` |
|       - |  5208 | ` * declare(ticks=1);` |
|       - |  5209 | ` * // entire script here` |
|       - |  5210 | ` * ?>` |
|       - |  5211 | ` *` |
|       - |  5212 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5213 | ` */` |
|       8 |  5214 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 |  5215 |  |
|       9 |  5216 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 |  5217 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - |  5218 | `	sxi32 rc;` |
|       9 |  5219 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 |  5220 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5221 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5222 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5223 | `			return SXERR_ABORT;` |
|       - |  5224 | `		}` |
|       5 |  5225 | `		goto Synchro;` |
|       - |  5226 | `	}` |
|       5 |  5227 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - |  5228 | `	/* Delimit the directive */` |
|       5 |  5229 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 |  5230 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  5231 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5232 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5233 | `			return SXERR_ABORT;` |
|       - |  5234 | `		}` |
|     ! 0 |  5235 | `		return SXRET_OK;` |
|       - |  5236 | `	}` |
|       - |  5237 | `	/* Update the cursor */` |
|       5 |  5238 | `	pGen->pIn = &pEnd[1];` |
|       5 |  5239 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 |  5240 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5241 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5242 | `			return SXERR_ABORT;` |
|       - |  5243 | `		}` |
|     ! 0 |  5244 | `	}` |
|       - |  5245 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 |  5246 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - |  5247 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5248 | `		ph7_lib_version()` |
|       - |  5249 | `		);` |
|       - |  5250 | `	/*All done */` |
|       5 |  5251 | `	return SXRET_OK;` |
|       2 |  5252 | `Synchro:` |
|       - |  5253 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5254 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5255 | `		pGen->pIn++;` |
|       1 |  5256 | `	}` |
|       5 |  5257 | `	return SXRET_OK;` |
|       5 |  5258 |  |
|       - |  5259 | `/*` |
|       - |  5260 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5261 | ` * as follows:` |
|       - |  5262 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5263 | ` * {` |
|       - |  5264 | ` *   return "Making a cup of $type.\n";` |
|       - |  5265 | ` * }` |
|       - |  5266 | ` * Symisc eXtension.` |
|       - |  5267 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5268 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5269 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5270 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5271 | ` *      {` |
|       - |  5272 | ` *       var_dump($a);` |
|       - |  5273 | ` *      }` |
|       - |  5274 | ` *     //call test without args` |
|       - |  5275 | ` *      test();` |
|       - |  5276 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5277 | ` *      Example:` |
|       - |  5278 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5279 | ` * 3 -) Function overloading!!` |
|       - |  5280 | ` *      Example:` |
|       - |  5281 | ` *      function foo($a) {` |
|       - |  5282 | ` *   	  return $a.PHP_EOL;` |
|       - |  5283 | ` *	    }` |
|       - |  5284 | ` *	    function foo($a, $b) {` |
|       - |  5285 | ` *   	  return $a + $b;` |
|       - |  5286 | ` *	    }` |
|       - |  5287 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5288 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5289 | ` *      // Same arg` |
|       - |  5290 | ` *	   function foo(string $a)` |
|       - |  5291 | ` *	   {` |
|       - |  5292 | ` *	     echo "a is a string\n";` |
|       - |  5293 | ` *	     var_dump($a);` |
|       - |  5294 | ` *	   }` |
|       - |  5295 | ` *	  function foo(int $a)` |
|       - |  5296 | ` *	  {` |
|       - |  5297 | ` *	    echo "a is integer\n";` |
|       - |  5298 | ` *	    var_dump($a);` |
|       - |  5299 | ` *	  }` |
|       - |  5300 | ` *	  function foo(array $a)` |
|       - |  5301 | ` *	  {` |
|       - |  5302 | ` * 	    echo "a is an array\n";` |
|       - |  5303 | ` * 	    var_dump($a);` |
|       - |  5304 | ` *	  }` |
|       - |  5305 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5306 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5307 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5308 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5309 | ` * introduced by the PH7 engine.` |
|       - |  5310 | ` */` |
|   45434 |  5311 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5312 |  |
|       - |  5313 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5314 | `	SySet *pInstrContainer;` |
|       - |  5315 | `	sxi32 rc;` |
|       - |  5316 | `	/* Swap token stream */` |
|   45436 |  5317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   45436 |  5318 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   45436 |  5319 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5320 | `	/* Compile the expression holding the argument value */` |
|   45436 |  5321 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5322 | `	/* Emit the done instruction */` |
|   45436 |  5323 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   45436 |  5324 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   45436 |  5325 | `	RE_SWAP_DELIMITER(pGen);` |
|   45436 |  5326 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5327 | `		return SXERR_ABORT;` |
|       - |  5328 | `	}` |
|   45436 |  5329 | `	return SXRET_OK;` |
|   22719 |  5330 |  |
|       - |  5331 | `/*` |
|       - |  5332 | ` * Collect function arguments one after one.` |
|       - |  5333 | ` * According to the PHP language reference manual.` |
|       - |  5334 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5335 | ` * list of expressions.` |
|       - |  5336 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5337 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5338 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5339 | ` * for more information.` |
|       - |  5340 | ` * Example #1 Passing arrays to functions` |
|       - |  5341 | ` * <?php` |
|       - |  5342 | ` * function takes_array($input)` |
|       - |  5343 | ` * {` |
|       - |  5344 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5345 | ` * }` |
|       - |  5346 | ` * ?>` |
|       - |  5347 | ` * Making arguments be passed by reference` |
|       - |  5348 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5349 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5350 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5351 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5352 | ` * to the argument name in the function definition:` |
|       - |  5353 | ` * Example #2 Passing function parameters by reference` |
|       - |  5354 | ` * <?php` |
|       - |  5355 | ` * function add_some_extra(&$string)` |
|       - |  5356 | ` * {` |
|       - |  5357 | ` *   $string .= 'and something extra.';` |
|       - |  5358 | ` * }` |
|       - |  5359 | ` * $str = 'This is a string, ';` |
|       - |  5360 | ` * add_some_extra($str);` |
|       - |  5361 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5362 | ` * ?>` |
|       - |  5363 | ` *` |
|       - |  5364 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5365 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5366 | ` * on these extension.` |
|       - |  5367 | ` */` |
|   54704 |  5368 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 |  5369 |  |
|       - |  5370 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5371 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5372 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5373 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5374 | `	sxi32 rc;` |
|       - |  5375 |  |
|   54706 |  5376 | `	pIn = pGen->pIn;` |
|   54706 |  5377 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5378 | `	/* Process arguments one after one */` |
|   69184 |  5379 | `	for(;;){` |
|  138370 |  5380 | `		if( pIn >= pEnd ){` |
|       - |  5381 | `			/* No more arguments to process */` |
|   54698 |  5382 | `			break;` |
|       - |  5383 | `		}` |
|   83674 |  5384 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   83674 |  5385 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   83674 |  5386 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   83674 |  5387 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5388 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  112122 |  5389 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   71717 |  5390 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   58333 |  5391 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   56884 |  5392 | `			sxu32 nLineLocal = pIn->nLine;` |
|   56884 |  5393 | `			sxi32 iTFlags = 0;` |
|   56884 |  5394 | `			pGen->pIn = pIn;` |
|   56884 |  5395 | `			rc = GenStateParseUnionTypeDecl(` |
|   28441 |  5396 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   28441 |  5397 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5398 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5399 | `				/* bAllowVoid */ 0,` |
|   28441 |  5400 | `						nLineLocal);` |
|   56884 |  5401 | `			pIn = pGen->pIn;` |
|   56884 |  5402 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5403 | `				return SXERR_ABORT;` |
|   56884 |  5404 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5405 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5406 | `				return SXERR_SYNTAX;` |
|   56882 |  5407 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5408 | `				if( pIn < pEnd ){` |
|       7 |  5409 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5410 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5411 | `						&pIn->sData);` |
|       3 |  5412 | `				}else{` |
|     ! 0 |  5413 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5414 | `						"syntax error, unexpected end of file");` |
|       - |  5415 | `				}` |
|       5 |  5416 | `				return SXERR_SYNTAX;` |
|       - |  5417 | `			}` |
|   56878 |  5418 | `			sArg.iFlags \|= iTFlags;` |
|   28438 |  5419 | `		}` |
|   83668 |  5420 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5421 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5422 | `			return rc;` |
|       - |  5423 | `		}` |
|   83668 |  5424 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5425 | `			/* Pass by reference,record that */` |
|    2866 |  5426 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2866 |  5427 | `			pIn++;` |
|    1432 |  5428 | `		}` |
|   83668 |  5429 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5430 | `			/* Variadic parameter: ...$args */` |
|      38 |  5431 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      38 |  5432 | `			pIn++;` |
|      18 |  5433 | `		}` |
|   83668 |  5434 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5435 | `			/* Invalid argument */` |
|     ! 0 |  5436 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5437 | `			return rc;` |
|       - |  5438 | `		}` |
|   83668 |  5439 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5440 | `		/* Copy argument name */` |
|   83668 |  5441 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   83668 |  5442 | `		if( zDup == 0 ){` |
|     ! 0 |  5443 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5444 | `			return SXERR_ABORT;` |
|       - |  5445 | `		}` |
|   83668 |  5446 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   83668 |  5447 | `		pIn++;` |
|   83668 |  5448 | `		if( pIn < pEnd ){` |
|   51700 |  5449 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5450 | `				SyToken *pDefend;` |
|   45438 |  5451 | `				sxi32 iNest = 0;` |
|   45438 |  5452 | `				pIn++; /* Jump the equal sign */` |
|   45438 |  5453 | `				pDefend = pIn;` |
|       - |  5454 | `				/* Process the default value associated with this argument */` |
|   96548 |  5455 | `				while( pDefend < pEnd ){` |
|   73822 |  5456 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   22712 |  5457 | `						break;` |
|       - |  5458 | `					}` |
|   51112 |  5459 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5460 | `						/* Increment nesting level */` |
|    2840 |  5461 | `						iNest++;` |
|   49693 |  5462 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5463 | `						/* Decrement nesting level */` |
|    2840 |  5464 | `						iNest--;` |
|    1419 |  5465 | `					}` |
|   51112 |  5466 | `					pDefend++;` |
|       2 |  5467 | `				}` |
|   45438 |  5468 | `				if( pIn >= pDefend ){` |
|       3 |  5469 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5470 | `					return rc;` |
|       - |  5471 | `				}` |
|       - |  5472 | `				/* Process default value */` |
|   45436 |  5473 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   45436 |  5474 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5475 | `					return rc;` |
|       - |  5476 | `				}` |
|       - |  5477 | `				/* Point beyond the default value */` |
|   45436 |  5478 | `				pIn = pDefend;` |
|   22717 |  5479 | `			}` |
|   51698 |  5480 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5481 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5482 | `				return rc;` |
|       - |  5483 | `			}` |
|   51698 |  5484 | `			pIn++; /* Jump the trailing comma */` |
|   25848 |  5485 | `		}` |
|       - |  5486 | `		/* Append argument signature */` |
|   83666 |  5487 | `		if( sArg.nType > 0 ){` |
|   56842 |  5488 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5489 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5690 |  5490 | `				int marker = 'o';` |
|    5690 |  5491 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5690 |  5492 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2846 |  5493 | `			}else{` |
|       - |  5494 | `				int c;` |
|   51154 |  5495 | `				c = 'n'; /* cc warning */` |
|       - |  5496 | `				/* Type leading character */` |
|   51154 |  5497 | `				switch(sArg.nType){` |
|     ! 0 |  5498 | `				case MEMOBJ_HASHMAP:` |
|       - |  5499 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5500 | `					c = 'h';` |
|     ! 0 |  5501 | `					break;` |
|    7110 |  5502 | `				case MEMOBJ_INT:` |
|       - |  5503 | `					/* Integer */` |
|   14222 |  5504 | `					c = 'i';` |
|   14222 |  5505 | `					break;` |
|     ! 0 |  5506 | `				case MEMOBJ_BOOL:` |
|       - |  5507 | `					/* Bool */` |
|     ! 0 |  5508 | `					c = 'b';` |
|     ! 0 |  5509 | `					break;` |
|     ! 0 |  5510 | `				case MEMOBJ_REAL:` |
|       - |  5511 | `					/* Float */` |
|     ! 0 |  5512 | `					c = 'f';` |
|     ! 0 |  5513 | `					break;` |
|   18459 |  5514 | `				case MEMOBJ_STRING:` |
|       - |  5515 | `					/* String */` |
|   36920 |  5516 | `					c = 's';` |
|   36920 |  5517 | `					break;` |
|       7 |  5518 | `				case MEMOBJ_OBJ:` |
|       - |  5519 | `					/* Object */` |
|      16 |  5520 | `					c = 'o';` |
|      14 |  5521 | `					break;` |
|     ! 0 |  5522 | `				default:` |
|     ! 0 |  5523 | `					break;` |
|       - |  5524 | `				}` |
|   51154 |  5525 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5526 | `			}` |
|   28422 |  5527 | `		}else{` |
|       - |  5528 | `			/* No type is associated with this parameter which mean` |
|       - |  5529 | `			 * that this function is not condidate for overloading.` |
|       - |  5530 | `			 */` |
|   26826 |  5531 | `			SyBlobRelease(&sSig);` |
|       - |  5532 | `		}` |
|       - |  5533 | `		/* Save in the argument set */` |
|   83666 |  5534 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5535 | `	}` |
|   54698 |  5536 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5537 | `		/* Save function signature */` |
|   34128 |  5538 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   17063 |  5539 | `	}` |
|   54698 |  5540 | `	return SXRET_OK;` |
|   27354 |  5541 |  |
|       - |  5542 | `/*` |
|       - |  5543 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5544 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5545 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5546 | ` */` |
|  151690 |  5547 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5548 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5549 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5550 | `	)` |
|       2 |  5551 |  |
|       - |  5552 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5553 | `	GenBlock *pBlock;` |
|       - |  5554 | `	sxu32 nGotoOfft;` |
|       - |  5555 | `	sxi32 rc;` |
|       - |  5556 | `	/* Attach the new function */` |
|  151692 |  5557 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  151692 |  5558 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5559 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5560 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5561 | `		return SXERR_ABORT;` |
|       - |  5562 | `	}` |
|  151692 |  5563 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5564 | `	/* Swap bytecode containers */` |
|  151692 |  5565 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  151692 |  5566 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5567 | `	/* Compile the body */` |
|  151692 |  5568 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5569 | `	/* Fix exception jumps now the destination is resolved */` |
|  151692 |  5570 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5571 | `	/* Emit the final return if not yet done */` |
|  151692 |  5572 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5573 | `	/* Fix gotos jumps now the destination is resolved */` |
|  151692 |  5574 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5575 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5576 | `	}` |
|  151692 |  5577 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5578 | `	/* Restore the default container */` |
|  151692 |  5579 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5580 | `	/* Leave function block */` |
|  151692 |  5581 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  151692 |  5582 | `	if( rc == SXERR_ABORT ){` |
|       - |  5583 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5584 | `		return SXERR_ABORT;` |
|       - |  5585 | `	}` |
|       - |  5586 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5587 | `	{` |
|  151692 |  5588 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5589 | `		sxu32 i;` |
| 3148484 |  5590 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2996812 |  5591 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5592 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5593 | `				break;` |
|       - |  5594 | `			}` |
| 1498398 |  5595 | `		}` |
|       - |  5596 | `	}` |
|       - |  5597 | `	/* All done, function body compiled */` |
|  151692 |  5598 | `	return SXRET_OK;` |
|   75847 |  5599 |  |
|       - |  5600 | `/*` |
|       - |  5601 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5602 | ` * According to the PHP language reference manual.` |
|       - |  5603 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5604 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5605 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5606 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5607 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5608 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5609 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5610 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5611 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5612 | ` *` |
|       - |  5613 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5614 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5615 | ` * on these extension.` |
|       - |  5616 | ` */` |
|       - |  5617 | `/*` |
|       - |  5618 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5619 | ` */` |
|      62 |  5620 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5621 |  |
|       - |  5622 | `	sxu32 i;` |
|     190 |  5623 | `	for( i = 0; i < n; i++ ){` |
|     168 |  5624 | `		int a = zA[i], b = zB[i];` |
|     168 |  5625 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     168 |  5626 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     168 |  5627 | `		if( a != b ) return a - b;` |
|      65 |  5628 | `	}` |
|      24 |  5629 | `	return 0;` |
|      33 |  5630 |  |
|       - |  5631 | `/*` |
|       - |  5632 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5633 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5634 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5635 | ` */` |
|       - |  5636 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5637 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5638 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5639 |  |
|       - |  5640 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5641 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5642 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5643 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5644 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5645 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5646 |  |
|       - |  5647 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5648 | `struct PhlTypeAtom {` |
|       - |  5649 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5650 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5651 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5652 | `	sxu32 nCanon;` |
|       - |  5653 | `};` |
|       - |  5654 |  |
|       - |  5655 | `/*` |
|       - |  5656 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5657 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5658 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5659 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5660 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5661 | ` * already be consumed by the caller.` |
|       - |  5662 | ` */` |
|   57158 |  5663 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5664 |  |
|   57160 |  5665 | `	SyToken *pIn = pGen->pIn;` |
|   57160 |  5666 | `	SyZero(pOut, sizeof(*pOut));` |
|   57160 |  5667 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   57160 |  5668 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5669 | `		return SXERR_SYNTAX;` |
|       - |  5670 | `	}` |
|       - |  5671 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   57160 |  5672 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5673 | `		pIn++;` |
|       8 |  5674 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5675 | `			return SXERR_SYNTAX;` |
|       - |  5676 | `		}` |
|       3 |  5677 | `	}` |
|   57160 |  5678 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5679 | `		return SXERR_SYNTAX;` |
|       - |  5680 | `	}` |
|   57160 |  5681 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   51416 |  5682 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   51416 |  5683 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5684 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   51409 |  5685 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       8 |  5686 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   51399 |  5687 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   14344 |  5688 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   44225 |  5689 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   37004 |  5690 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   18553 |  5691 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      22 |  5692 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      42 |  5693 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5694 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5695 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5696 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5697 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5698 | `			pOut->sClass = pIn->sData;` |
|       4 |  5699 | `		}else{` |
|       3 |  5700 | `			return SXERR_SYNTAX;` |
|       - |  5701 | `		}` |
|   51414 |  5702 | `		pIn++;` |
|   25708 |  5703 | `	}else{` |
|       - |  5704 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5705 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    5746 |  5706 | `		SyString *pT = &pIn->sData;` |
|    5746 |  5707 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5708 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5709 | `			pIn++;` |
|    5741 |  5710 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      12 |  5711 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      12 |  5712 | `			pIn++;` |
|    5731 |  5713 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5714 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5715 | `			pIn++;` |
|       2 |  5716 | `		}else{` |
|       - |  5717 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    5724 |  5718 | `			SyToken *pFirst = pIn;` |
|    5724 |  5719 | `			SyToken *pLast = pIn;` |
|    5724 |  5720 | `			pOut->nType = SXU32_HIGH;` |
|    5724 |  5721 | `			pOut->sClass = pIn->sData;` |
|    5724 |  5722 | `			pIn++;` |
|    8586 |  5723 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    5727 |  5724 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  5725 | `				pLast = &pIn[1];` |
|       3 |  5726 | `				pIn += 2;` |
|       1 |  5727 | `			}` |
|    5724 |  5728 | `			if( pLast != pFirst ){` |
|       3 |  5729 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  5730 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  5731 | `				pOut->sClass.zString = zFirst;` |
|       3 |  5732 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  5733 | `			}` |
|       - |  5734 | `		}` |
|       - |  5735 | `	}` |
|   57158 |  5736 | `	pGen->pIn = pIn;` |
|   57158 |  5737 | `	return SXRET_OK;` |
|   28581 |  5738 |  |
|       - |  5739 |  |
|       - |  5740 | `/*` |
|       - |  5741 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  5742 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  5743 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  5744 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  5745 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  5746 | ` */` |
|   57066 |  5747 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  5748 |  |
|       - |  5749 | `	int i;` |
|   57068 |  5750 | `	int nNonNull = 0;` |
|  114210 |  5751 | `	for( i = 0; i < nAtoms; i++ ){` |
|   57144 |  5752 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   57134 |  5753 | `			nNonNull++;` |
|   28566 |  5754 | `		}` |
|   28573 |  5755 | `	}` |
|   57068 |  5756 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  5757 | `		/* Shorthand: ?T */` |
|      52 |  5758 | `		for( i = 0; i < nAtoms; i++ ){` |
|      52 |  5759 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      52 |  5760 | `			SyBlobAppend(pBlob, "?", 1);` |
|      52 |  5761 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5762 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  5763 | `			}else{` |
|      42 |  5764 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  5765 | `			}` |
|      52 |  5766 | `			return;` |
|     ! 0 |  5767 | `		}` |
|     ! 0 |  5768 | `	}` |
|       - |  5769 | `	{` |
|   57018 |  5770 | `		int bFirst = 1;` |
|       - |  5771 | `		/* 1) Classes in declaration order */` |
|  114104 |  5772 | `		for( i = 0; i < nAtoms; i++ ){` |
|   57088 |  5773 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    5718 |  5774 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    5718 |  5775 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    5718 |  5776 | `				bFirst = 0;` |
|    2858 |  5777 | `			}` |
|   28545 |  5778 | `		}` |
|       - |  5779 | `		/* 2) Built-ins in canonical order */` |
|       - |  5780 | `		{` |
|       - |  5781 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  5782 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  5783 | `			int k;` |
|  399114 |  5784 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  633170 |  5785 | `				for( i = 0; i < nAtoms; i++ ){` |
|  342432 |  5786 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   51360 |  5787 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   51360 |  5788 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   51360 |  5789 | `						bFirst = 0;` |
|   51360 |  5790 | `						break;` |
|       - |  5791 | `					}` |
|  145538 |  5792 | `				}` |
|  171050 |  5793 | `			}` |
|       - |  5794 | `		}` |
|       - |  5795 | `		/* 3) null suffix */` |
|   57018 |  5796 | `		if( bNullable ){` |
|       6 |  5797 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  5798 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  5799 | `		}` |
|       - |  5800 | `	}` |
|   28535 |  5801 |  |
|       - |  5802 |  |
|       - |  5803 | `/*` |
|       - |  5804 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  5805 | ` *` |
|       - |  5806 | ` * Outputs:` |
|       - |  5807 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  5808 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  5809 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  5810 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  5811 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  5812 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  5813 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  5814 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  5815 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  5816 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  5817 | ` *` |
|       - |  5818 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  5819 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  5820 | ` */` |
|   57076 |  5821 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  5822 | `	ph7_gen_state *pGen,` |
|       - |  5823 | `	sxu32 *pnType,` |
|       - |  5824 | `	SyString *pClass,` |
|       - |  5825 | `	SySet *pAlts,` |
|       - |  5826 | `	sxi32 *piTypeFlags,` |
|       - |  5827 | `	SyString *pTypeText,` |
|       - |  5828 | `	int iNullableFlag,` |
|       - |  5829 | `	int iUnionFlag,` |
|       - |  5830 | `	int bAllowVoid,` |
|       - |  5831 | `	sxu32 nLine` |
|       2 |  5832 | `){` |
|       - |  5833 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   57078 |  5834 | `	int nAtoms = 0;` |
|   57078 |  5835 | `	int bShortNullable = 0;` |
|   57078 |  5836 | `	int bExplicitNull = 0;` |
|       - |  5837 | `	sxi32 rc;` |
|   57078 |  5838 | `	*pnType = 0;` |
|   57078 |  5839 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   57078 |  5840 | `	*piTypeFlags = 0;` |
|   57078 |  5841 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  5842 |  |
|   57078 |  5843 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5844 | `		return SXRET_OK;` |
|       - |  5845 | `	}` |
|       - |  5846 | ``	/* Optional `?` shorthand prefix */`` |
|   57076 |  5847 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      48 |  5848 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      48 |  5849 | `		bShortNullable = 1;` |
|      48 |  5850 | `		pGen->pIn++;` |
|      48 |  5851 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5852 | `			return SXERR_SYNTAX;` |
|       - |  5853 | `		}` |
|      23 |  5854 | `	}` |
|       - |  5855 | `	/* First atom is mandatory */` |
|   57078 |  5856 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   57078 |  5857 | `	if( rc != SXRET_OK ){` |
|       3 |  5858 | `		return rc;` |
|       - |  5859 | `	}` |
|   57076 |  5860 | `	nAtoms = 1;` |
|       - |  5861 | ``	/* Subsequent atoms separated by `\|` */`` |
|   85736 |  5862 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   57201 |  5863 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      86 |  5864 | `		if( bShortNullable ){` |
|       - |  5865 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  5866 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  5867 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  5868 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  5869 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  5870 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  5871 | `		}` |
|      84 |  5872 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  5873 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5874 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  5875 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  5876 | `		}` |
|      84 |  5877 | ``		pGen->pIn++; /* skip `\|` */`` |
|      84 |  5878 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      84 |  5879 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  5880 | `			return rc;` |
|       - |  5881 | `		}` |
|      84 |  5882 | `		nAtoms++;` |
|       2 |  5883 | `	}` |
|       - |  5884 | `	/* Validation pass.` |
|       - |  5885 | `	 *` |
|       - |  5886 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  5887 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  5888 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  5889 | `	 */` |
|       - |  5890 | `	{` |
|       - |  5891 | `		int i, j;` |
|   57074 |  5892 | `		int bHasNonNull = 0;` |
|  114222 |  5893 | `		for( i = 0; i < nAtoms; i++ ){` |
|   57156 |  5894 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      12 |  5895 | `				if( nAtoms > 1 ){` |
|       3 |  5896 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5897 | `						"Void can only be used as a standalone type");` |
|       3 |  5898 | `					return SXERR_SYNTAX;` |
|       - |  5899 | `				}` |
|      10 |  5900 | `				if( !bAllowVoid ){` |
|     ! 0 |  5901 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5902 | `						"void cannot be used here");` |
|     ! 0 |  5903 | `					return SXERR_SYNTAX;` |
|       - |  5904 | `				}` |
|      10 |  5905 | `				if( bShortNullable ){` |
|     ! 0 |  5906 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5907 | `						"Void type cannot be nullable");` |
|     ! 0 |  5908 | `					return SXERR_SYNTAX;` |
|       - |  5909 | `				}` |
|       4 |  5910 | `			}` |
|   57154 |  5911 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  5912 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  5913 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  5914 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  5915 | `				 * does not return), and folding them would mislead any` |
|       - |  5916 | `				 * future return-enforcement work. */` |
|       3 |  5917 | `				if( nAtoms > 1 ){` |
|       3 |  5918 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5919 | `						"never can only be used as a standalone type");` |
|       3 |  5920 | `					return SXERR_SYNTAX;` |
|       - |  5921 | `				}` |
|     ! 0 |  5922 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5923 | `					"never type is not yet implemented");` |
|     ! 0 |  5924 | `				return SXERR_SYNTAX;` |
|       - |  5925 | `			}` |
|   57152 |  5926 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  5927 | `				bExplicitNull = 1;` |
|       7 |  5928 | `			}else{` |
|   57142 |  5929 | `				bHasNonNull = 1;` |
|       - |  5930 | `			}` |
|       - |  5931 | `			/* Duplicate detection */` |
|   57264 |  5932 | `			for( j = 0; j < i; j++ ){` |
|     116 |  5933 | `				int bDup = 0;` |
|     116 |  5934 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  5935 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5936 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  5937 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  5938 | `								aAtoms[j].sClass.zString,` |
|      12 |  5939 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  5940 | `							bDup = 1;` |
|     ! 0 |  5941 | `						}` |
|       8 |  5942 | `					}else{` |
|       3 |  5943 | `						bDup = 1;` |
|       - |  5944 | `					}` |
|       7 |  5945 | `				}` |
|     116 |  5946 | `				if( bDup ){` |
|       - |  5947 | `					const char *zName;` |
|       - |  5948 | `					sxu32 nName;` |
|       3 |  5949 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  5950 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  5951 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  5952 | `					}else{` |
|       3 |  5953 | `						zName = aAtoms[i].zCanon;` |
|       3 |  5954 | `						nName = aAtoms[i].nCanon;` |
|       - |  5955 | `					}` |
|       4 |  5956 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  5957 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  5958 | `					return SXERR_SYNTAX;` |
|       - |  5959 | `				}` |
|      58 |  5960 | `			}` |
|   28576 |  5961 | `		}` |
|   57068 |  5962 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  5963 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5964 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  5965 | `			return SXERR_SYNTAX;` |
|       - |  5966 | `		}` |
|       - |  5967 | `	}` |
|       - |  5968 | `	/* Compute nullability flag */` |
|   57068 |  5969 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      56 |  5970 | `		*piTypeFlags \|= iNullableFlag;` |
|      27 |  5971 | `	}` |
|       - |  5972 | `	/* Build canonical type text */` |
|   57068 |  5973 | `	if( pTypeText ){` |
|       - |  5974 | `		SyBlob sBlob;` |
|   57068 |  5975 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   85579 |  5976 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   28533 |  5977 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   57068 |  5978 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   85589 |  5979 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   57058 |  5980 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   57060 |  5981 | `			if( zDup ){` |
|   57060 |  5982 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   28529 |  5983 | `			}` |
|   28529 |  5984 | `		}` |
|   57068 |  5985 | `		SyBlobRelease(&sBlob);` |
|   28533 |  5986 | `	}` |
|       - |  5987 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  5988 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  5989 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  5990 | `	{` |
|   57068 |  5991 | `		int nNonNull = 0;` |
|   57068 |  5992 | `		int iNonNullIdx = -1;` |
|       - |  5993 | `		int i;` |
|  114210 |  5994 | `		for( i = 0; i < nAtoms; i++ ){` |
|   57144 |  5995 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   57134 |  5996 | `				nNonNull++;` |
|   57134 |  5997 | `				iNonNullIdx = i;` |
|   28566 |  5998 | `			}` |
|   28573 |  5999 | `		}` |
|   57068 |  6000 | `		if( nNonNull <= 1 ){` |
|       - |  6001 | `			/* Fast path: store as single type. */` |
|   57018 |  6002 | `			if( iNonNullIdx >= 0 ){` |
|   57018 |  6003 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   57018 |  6004 | `				if( pA->nType == SXU32_HIGH ){` |
|    8555 |  6005 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    2851 |  6006 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    5704 |  6007 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    5704 |  6008 | `					*pnType = SXU32_HIGH;` |
|    5704 |  6009 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   54167 |  6010 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      10 |  6011 | `					*pnType = MEMOBJ_VOID;` |
|       6 |  6012 | `				}else{` |
|       - |  6013 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6014 | `					 * pass above rejects it as not-yet-implemented. */` |
|   51308 |  6015 | `					*pnType = pA->nType;` |
|       - |  6016 | `				}` |
|   28508 |  6017 | `			}` |
|   28510 |  6018 | `		}else{` |
|       - |  6019 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      52 |  6020 | `			*piTypeFlags \|= iUnionFlag;` |
|     172 |  6021 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6022 | `				ph7_type_alt sAlt;` |
|     122 |  6023 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     118 |  6024 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     118 |  6025 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      38 |  6026 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      12 |  6027 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      26 |  6028 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      26 |  6029 | `					sAlt.nType = SXU32_HIGH;` |
|      26 |  6030 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      14 |  6031 | `				}else{` |
|      94 |  6032 | `					sAlt.nType = aAtoms[i].nType;` |
|      94 |  6033 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6034 | `				}` |
|     118 |  6035 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      60 |  6036 | `			}` |
|       - |  6037 | `		}` |
|       - |  6038 | `	}` |
|   57068 |  6039 | `	return SXRET_OK;` |
|   28540 |  6040 |  |
|       - |  6041 |  |
|       - |  6042 | `/*` |
|       - |  6043 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6044 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6045 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6046 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6047 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6048 | `` *          and union types `: T\|U`.`` |
|       - |  6049 | ` */` |
|  174526 |  6050 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6051 |  |
|  174528 |  6052 | `	sxi32 iFlags = 0;` |
|       - |  6053 | `	sxi32 rc;` |
|       - |  6054 | `	sxu32 nLine;` |
|  174528 |  6055 | `	pFunc->nReturnType = 0;` |
|  174528 |  6056 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  174528 |  6057 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  174528 |  6058 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  174438 |  6059 | `		return SXRET_OK;` |
|       - |  6060 | `	}` |
|      92 |  6061 | `	pGen->pIn++; /* Skip ':' */` |
|      92 |  6062 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6063 | `		return SXRET_OK;` |
|       - |  6064 | `	}` |
|      92 |  6065 | `	nLine = pGen->pIn->nLine;` |
|      92 |  6066 | `	rc = GenStateParseUnionTypeDecl(` |
|      45 |  6067 | `		pGen,` |
|      45 |  6068 | `		&pFunc->nReturnType,` |
|      45 |  6069 | `		&pFunc->sReturnClass,` |
|      45 |  6070 | `		&pFunc->aReturnUnion,` |
|       - |  6071 | `		&iFlags,` |
|      45 |  6072 | `		&pFunc->sReturnTypeName,` |
|       - |  6073 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6074 | `		/* iUnionFlag */ 0,` |
|       - |  6075 | `		/* bAllowVoid */ 1,` |
|      45 |  6076 | `		nLine);` |
|      45 |  6077 | `	(void)iFlags;` |
|      92 |  6078 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6079 | `		return SXERR_ABORT;` |
|       - |  6080 | `	}` |
|      92 |  6081 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6082 | `		/* Error already reported */` |
|     ! 0 |  6083 | `		return SXERR_SYNTAX;` |
|       - |  6084 | `	}` |
|      92 |  6085 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6086 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6087 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6088 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6089 | `				&pGen->pIn->sData);` |
|       3 |  6090 | `		}else{` |
|     ! 0 |  6091 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6092 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6093 | `		}` |
|       5 |  6094 | `		return SXERR_SYNTAX;` |
|       - |  6095 | `	}` |
|      88 |  6096 | `	return SXRET_OK;` |
|   87265 |  6097 |  |
|       - |  6098 |  |
|   37722 |  6099 | `static sxi32 GenStateCompileFunc(` |
|       - |  6100 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6101 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6102 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6103 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6104 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6105 | `	)` |
|       2 |  6106 |  |
|       - |  6107 | `	ph7_vm_func *pFunc;` |
|       - |  6108 | `	SyToken *pEnd;` |
|       - |  6109 | `	sxu32 nLine;` |
|       - |  6110 | `	char *zName;` |
|       - |  6111 | `	sxi32 rc;` |
|       - |  6112 | `	/* Extract line number */` |
|   37724 |  6113 | `	nLine = pGen->pIn->nLine;` |
|       - |  6114 | `	/* Jump the left parenthesis '(' */` |
|   37724 |  6115 | `	pGen->pIn++;` |
|       - |  6116 | `	/* Delimit the function signature */` |
|   37724 |  6117 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   37724 |  6118 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6119 | `		/* Syntax error */` |
|       7 |  6120 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6121 | `		if( rc == SXERR_ABORT ){` |
|       - |  6122 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6123 | `			return SXERR_ABORT;` |
|       - |  6124 | `		}` |
|       7 |  6125 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6126 | `		return SXRET_OK;` |
|       - |  6127 | `	}` |
|       - |  6128 | `	/* Create the function state */` |
|   37718 |  6129 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   37718 |  6130 | `	if( pFunc == 0 ){` |
|     ! 0 |  6131 | `		goto OutOfMem;` |
|       - |  6132 | `	}` |
|       - |  6133 | `	/* Build the function name, prepending namespace if active */` |
|   37725 |  6134 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6135 | `		SyBlob sFQN;` |
|       - |  6136 | `		sxu32 nLen;` |
|      16 |  6137 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6138 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6139 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6140 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6141 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6142 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6143 | `		SyBlobRelease(&sFQN);` |
|      16 |  6144 | `		if( zName == 0 ){` |
|     ! 0 |  6145 | `			goto OutOfMem;` |
|       - |  6146 | `		}` |
|      16 |  6147 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6148 | `	}else{` |
|   37704 |  6149 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   37704 |  6150 | `		if( zName == 0 ){` |
|     ! 0 |  6151 | `			goto OutOfMem;` |
|       - |  6152 | `		}` |
|   37704 |  6153 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6154 | `	}` |
|   37718 |  6155 | `	if( pGen->pIn < pEnd ){` |
|       - |  6156 | `		/* Collect function arguments */` |
|   26172 |  6157 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   26172 |  6158 | `		if( rc == SXERR_ABORT ){` |
|       - |  6159 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6160 | `			return SXERR_ABORT;` |
|       - |  6161 | `		}` |
|   13085 |  6162 | `	}` |
|       - |  6163 | `	/* Point past ')' and parse optional return type ': type' */` |
|   37718 |  6164 | `	pGen->pIn = &pEnd[1];` |
|       - |  6165 | `	{` |
|   37718 |  6166 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   37718 |  6167 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6168 | `			return SXERR_ABORT;` |
|   37718 |  6169 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6170 | `			return SXERR_SYNTAX;` |
|       - |  6171 | `		}` |
|       - |  6172 | `	}` |
|   37714 |  6173 | `	if( bHandleClosure ){` |
|       - |  6174 | `		ph7_vm_func_closure_env sEnv;` |
|     178 |  6175 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     176 |  6176 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      97 |  6177 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 |  6178 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6179 | `				/* Closure,record environment variable */` |
|      16 |  6180 | `				pGen->pIn++;` |
|      16 |  6181 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6182 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6183 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6184 | `						return SXERR_ABORT;` |
|       - |  6185 | `					}` |
|     ! 0 |  6186 | `				}` |
|      16 |  6187 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6188 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 |  6189 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 |  6190 | `					int iFlagsLocal = 0;` |
|      34 |  6191 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 |  6192 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 |  6193 | `						break;` |
|       - |  6194 | `					}` |
|      20 |  6195 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 |  6196 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6197 | `						/* Pass by reference,record that */` |
|     ! 0 |  6198 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6199 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6200 | `							);` |
|     ! 0 |  6201 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6202 | `						pGen->pIn++;` |
|     ! 0 |  6203 | `					}` |
|      18 |  6204 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 |  6205 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6206 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6207 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6208 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6209 | `								return SXERR_ABORT;` |
|       - |  6210 | `							}` |
|       - |  6211 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6212 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6213 | `								pGen->pIn++;` |
|     ! 0 |  6214 | `							}` |
|     ! 0 |  6215 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6216 | `								pGen->pIn++;` |
|     ! 0 |  6217 | `							}` |
|     ! 0 |  6218 | `							break;` |
|       - |  6219 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6220 | `					}else{` |
|       - |  6221 | `						SyString *pNameLocal;` |
|       - |  6222 | `						char *zDup;` |
|       - |  6223 | `						/* Duplicate variable name */` |
|      20 |  6224 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 |  6225 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 |  6226 | `						if( zDup ){` |
|       - |  6227 | `							/* Zero the structure */` |
|      20 |  6228 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 |  6229 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 |  6230 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 |  6231 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 |  6232 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6233 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6234 | `									got_this = 1;` |
|     ! 0 |  6235 | `							}` |
|       - |  6236 | `							/* Save imported variable */` |
|      20 |  6237 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 |  6238 | `						}else{` |
|     ! 0 |  6239 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6240 | `							 return SXERR_ABORT;` |
|       - |  6241 | `						}` |
|       - |  6242 | `					}` |
|      20 |  6243 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 |  6244 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6245 | `						/* Ignore trailing commas */` |
|       7 |  6246 | `						pGen->pIn++;` |
|       1 |  6247 | `					}` |
|       2 |  6248 | `				}` |
|      16 |  6249 | `				if( !got_this ){` |
|       - |  6250 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6251 | `					 * available to the closure environment.` |
|       - |  6252 | `					 */` |
|      16 |  6253 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 |  6254 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 |  6255 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 |  6256 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 |  6257 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 |  6258 | `				}` |
|      16 |  6259 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6260 | `					/* Mark as closure */` |
|      16 |  6261 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 |  6262 | `				}` |
|       7 |  6263 | `		}` |
|      88 |  6264 | `	}` |
|       - |  6265 | `	/* Compile the body */` |
|   37714 |  6266 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   37714 |  6267 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6268 | `		return SXERR_ABORT;` |
|       - |  6269 | `	}` |
|   37714 |  6270 | `	if( ppFunc ){` |
|     178 |  6271 | `		*ppFunc = pFunc;` |
|      88 |  6272 | `	}` |
|   37714 |  6273 | `	rc = SXRET_OK;` |
|   37714 |  6274 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6275 | `		/* Finally register the function */` |
|   37700 |  6276 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   18849 |  6277 | `	}` |
|   37714 |  6278 | `	if( rc == SXRET_OK ){` |
|   37714 |  6279 | `		return SXRET_OK;` |
|       - |  6280 | `	}` |
|       - |  6281 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6282 | `OutOfMem:` |
|       - |  6283 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6284 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6285 | `	 */` |
|     ! 0 |  6286 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6287 | `	return SXERR_ABORT;` |
|   18863 |  6288 |  |
|       - |  6289 | `/*` |
|       - |  6290 | ` * Compile a standard PHP function.` |
|       - |  6291 | ` *  Refer to the block-comment above for more information.` |
|       - |  6292 | ` */` |
|   37552 |  6293 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6294 |  |
|       - |  6295 | `	SyString *pName;` |
|       - |  6296 | `	sxi32 iFlags;` |
|       - |  6297 | `	sxu32 nLine;` |
|       - |  6298 | `	sxi32 rc;` |
|       - |  6299 |  |
|   37554 |  6300 | `	nLine = pGen->pIn->nLine;` |
|   37554 |  6301 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   37554 |  6302 | `	iFlags = 0;` |
|   37554 |  6303 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6304 | `		/* Return by reference,remember that */` |
|       7 |  6305 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6306 | `		/* Jump the '&' token */` |
|       7 |  6307 | `		pGen->pIn++;` |
|       3 |  6308 | `	}` |
|   37554 |  6309 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6310 | `		/* Invalid function name */` |
|       5 |  6311 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6312 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6313 | `			return SXERR_ABORT;` |
|       - |  6314 | `		}` |
|       - |  6315 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6316 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6317 | `			pGen->pIn++;` |
|       1 |  6318 | `		}` |
|       5 |  6319 | `		return SXRET_OK;` |
|       - |  6320 | `	}` |
|   37550 |  6321 | `	pName = &pGen->pIn->sData;` |
|   37550 |  6322 | `	nLine = pGen->pIn->nLine;` |
|       - |  6323 | `	/* Jump the function name */` |
|   37550 |  6324 | `	pGen->pIn++;` |
|   37550 |  6325 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6326 | `		/* Syntax error */` |
|       3 |  6327 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6328 | `		if( rc == SXERR_ABORT ){` |
|       - |  6329 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6330 | `			return SXERR_ABORT;` |
|       - |  6331 | `		}` |
|       - |  6332 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6333 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6334 | `			pGen->pIn++;` |
|     ! 0 |  6335 | `		}` |
|       3 |  6336 | `		return SXRET_OK;` |
|       - |  6337 | `	}` |
|       - |  6338 | `	/* Compile function body */` |
|   37548 |  6339 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   37548 |  6340 | `	return rc;` |
|   18778 |  6341 |  |
|       - |  6342 | `/*` |
|       - |  6343 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6344 | ` * According to the PHP language reference manual` |
|       - |  6345 | ` *  Visibility:` |
|       - |  6346 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6347 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6348 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6349 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6350 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6351 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6352 | ` */` |
|  174014 |  6353 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6354 |  |
|  174016 |  6355 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8584 |  6356 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  165434 |  6357 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   19910 |  6358 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6359 | `	}` |
|       - |  6360 | `	/* Assume public by default */` |
|  145526 |  6361 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   87009 |  6362 |  |
|       - |  6363 | `/*` |
|       - |  6364 | ` * Compile a class constant.` |
|       - |  6365 | ` * According to the PHP language reference manual` |
|       - |  6366 | ` *  Class Constants` |
|       - |  6367 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6368 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6369 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6370 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6371 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6372 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6373 | ` * Symisc eXtension.` |
|       - |  6374 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6375 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6376 | ` *  Example:` |
|       - |  6377 | ` *   class Test{` |
|       - |  6378 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6379 | ` *   };` |
|       - |  6380 | ` *   var_dump(TEST::MyConst);` |
|       - |  6381 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6382 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6383 | ` */` |
|      30 |  6384 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6385 |  |
|      32 |  6386 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6387 | `	SySet *pInstrContainer;` |
|       - |  6388 | `	ph7_class_attr *pCons;` |
|       - |  6389 | `	SyString *pName;` |
|       - |  6390 | `	sxi32 rc;` |
|       - |  6391 | `	/* Extract visibility level */` |
|      32 |  6392 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6393 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6394 | `loop:` |
|       - |  6395 | `	/* Mark as constant */` |
|      32 |  6396 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6397 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6398 | `		/* Invalid constant name */` |
|     ! 0 |  6399 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6400 | `		if( rc == SXERR_ABORT ){` |
|       - |  6401 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6402 | `			return SXERR_ABORT;` |
|       - |  6403 | `		}` |
|     ! 0 |  6404 | `		goto Synchronize;` |
|       - |  6405 | `	}` |
|       - |  6406 | `	/* Peek constant name */` |
|      32 |  6407 | `	pName = &pGen->pIn->sData;` |
|       - |  6408 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6409 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6410 | `		/* Reserved constant name */` |
|     ! 0 |  6411 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6412 | `		if( rc == SXERR_ABORT ){` |
|       - |  6413 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6414 | `			return SXERR_ABORT;` |
|       - |  6415 | `		}` |
|     ! 0 |  6416 | `		goto Synchronize;` |
|       - |  6417 | `	}` |
|       - |  6418 | `	/* Advance the stream cursor */` |
|      32 |  6419 | `	pGen->pIn++;` |
|      32 |  6420 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6421 | `		/* Invalid declaration */` |
|     ! 0 |  6422 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6423 | `		if( rc == SXERR_ABORT ){` |
|       - |  6424 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6425 | `			return SXERR_ABORT;` |
|       - |  6426 | `		}` |
|     ! 0 |  6427 | `		goto Synchronize;` |
|       - |  6428 | `	}` |
|      32 |  6429 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6430 | `	/* Allocate a new class attribute */` |
|      32 |  6431 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6432 | `	if( pCons == 0 ){` |
|     ! 0 |  6433 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6434 | `		return SXERR_ABORT;` |
|       - |  6435 | `	}` |
|       - |  6436 | `	/* Swap bytecode container */` |
|      32 |  6437 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6438 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6439 | `	/* Compile constant value.` |
|       - |  6440 | `	 */` |
|      32 |  6441 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6442 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6443 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6444 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6445 | `			return SXERR_ABORT;` |
|       - |  6446 | `		}` |
|       1 |  6447 | `	}` |
|       - |  6448 | `	/* Emit the done instruction */` |
|      32 |  6449 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6450 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6451 | `	if( rc == SXERR_ABORT ){` |
|       - |  6452 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6453 | `		return SXERR_ABORT;` |
|       - |  6454 | `	}` |
|       - |  6455 | `	/* All done,install the constant */` |
|      32 |  6456 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6457 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6458 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6459 | `		return SXERR_ABORT;` |
|       - |  6460 | `	}` |
|      32 |  6461 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6462 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6463 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6464 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6465 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6466 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6467 | `				pTok--;` |
|     ! 0 |  6468 | `			}` |
|     ! 0 |  6469 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6470 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6471 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6472 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6473 | `				return SXERR_ABORT;` |
|       - |  6474 | `			}` |
|     ! 0 |  6475 | `		}else{` |
|     ! 0 |  6476 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6477 | `				goto loop;` |
|       - |  6478 | `			}` |
|       - |  6479 | `		}` |
|     ! 0 |  6480 | `	}` |
|      32 |  6481 | `	return SXRET_OK;` |
|     ! 0 |  6482 | `Synchronize:` |
|       - |  6483 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6484 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6485 | `		pGen->pIn++;` |
|     ! 0 |  6486 | `	}` |
|     ! 0 |  6487 | `	return SXERR_CORRUPT;` |
|      17 |  6488 |  |
|       - |  6489 | `/*` |
|       - |  6490 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6491 | ` * According to the PHP language reference manual` |
|       - |  6492 | ` *  Properties` |
|       - |  6493 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6494 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6495 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6496 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6497 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6498 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6499 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6500 | ` * Symisc eXtension.` |
|       - |  6501 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6502 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6503 | ` *  Example:` |
|       - |  6504 | ` *   class Test{` |
|       - |  6505 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6506 | ` *   };` |
|       - |  6507 | ` *   var_dump(TEST::myVar);` |
|       - |  6508 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6509 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6510 | ` */` |
|       - |  6511 | `/*` |
|       - |  6512 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6513 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6514 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6515 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6516 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6517 | ` */` |
|  114056 |  6518 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6519 |  |
|  114058 |  6520 | `	SyToken *p = pStart;` |
|  114058 |  6521 | `	if( p >= pEnd ) return 0;` |
|  114058 |  6522 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6523 | `		p++;` |
|      16 |  6524 | `		if( p >= pEnd ) return 0;` |
|       7 |  6525 | `	}` |
|  114058 |  6526 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6527 | `		p++;` |
|       3 |  6528 | `		if( p >= pEnd ) return 0;` |
|       1 |  6529 | `	}` |
|  114058 |  6530 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6531 | `		return 0;` |
|       - |  6532 | `	}` |
|       - |  6533 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6534 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6535 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6536 | `	 * dispatch site. */` |
|  114058 |  6537 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  114044 |  6538 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  114087 |  6539 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    2979 |  6540 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  113954 |  6541 | `			return 0;` |
|       - |  6542 | `		}` |
|      45 |  6543 | `	}` |
|     106 |  6544 | `	p++;` |
|       - |  6545 | `	/* Consume optional namespace path */` |
|     108 |  6546 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6547 | `		p += 2;` |
|       1 |  6548 | `	}` |
|       - |  6549 | ``	/* Consume any `\| Type` union alternatives */`` |
|     174 |  6550 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      72 |  6551 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6552 | `		p++;` |
|      14 |  6553 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6554 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6555 | `		p++;` |
|      14 |  6556 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6557 | `			p += 2;` |
|     ! 0 |  6558 | `		}` |
|       2 |  6559 | `	}` |
|     106 |  6560 | `	if( p >= pEnd ) return 0;` |
|     106 |  6561 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   57030 |  6562 |  |
|       - |  6563 |  |
|       - |  6564 | `/*` |
|       - |  6565 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6566 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6567 | ` * if not). Recognized forms:` |
|       - |  6568 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6569 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6570 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6571 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6572 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6573 | ` * on unrecoverable error.` |
|       - |  6574 | ` *` |
|       - |  6575 | ` * When a type is parsed:` |
|       - |  6576 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6577 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6578 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6579 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6580 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6581 | ` */` |
|     104 |  6582 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6583 | `	ph7_gen_state *pGen,` |
|       - |  6584 | `	sxu32 *pnType,` |
|       - |  6585 | `	SyString *pClass,` |
|       - |  6586 | `	sxi32 *piTypeFlags,` |
|       - |  6587 | `	SyString *pTypeText,` |
|       - |  6588 | `	SySet *pAlts` |
|       2 |  6589 | `){` |
|     106 |  6590 | `	sxi32 iFlags = 0;` |
|       - |  6591 | `	sxi32 rc;` |
|     106 |  6592 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6593 | `		return SXRET_OK;` |
|       - |  6594 | `	}` |
|       - |  6595 | `	/* If the first token is '$', there's no type */` |
|     106 |  6596 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6597 | `		return SXRET_OK;` |
|       - |  6598 | `	}` |
|     106 |  6599 | `	rc = GenStateParseUnionTypeDecl(` |
|      52 |  6600 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6601 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6602 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6603 | `		/* bAllowVoid */ 0,` |
|     104 |  6604 | `		pGen->pIn->nLine);` |
|     106 |  6605 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6606 | `		return rc;` |
|       - |  6607 | `	}` |
|       - |  6608 | `	/* Verify next token is '$' (start of property name) */` |
|     106 |  6609 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6610 | `		return SXERR_SYNTAX;` |
|       - |  6611 | `	}` |
|     106 |  6612 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     106 |  6613 | `	return SXRET_OK;` |
|      54 |  6614 |  |
|       - |  6615 |  |
|   37252 |  6616 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6617 |  |
|   37254 |  6618 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6619 | `	ph7_class_attr *pAttr;` |
|       - |  6620 | `	SyString *pName;` |
|       - |  6621 | `	sxi32 rc;` |
|   37254 |  6622 | `	sxu32 nType = 0;` |
|       - |  6623 | `	SyString sTypeClass;` |
|       - |  6624 | `	SyString sTypeText;` |
|       - |  6625 | `	SySet aUnionAlts;` |
|   37254 |  6626 | `	sxi32 iTypeFlags = 0;` |
|   37254 |  6627 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   37254 |  6628 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   37254 |  6629 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6630 | `	/* Extract visibility level */` |
|   37254 |  6631 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6632 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   37306 |  6633 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     106 |  6634 | `		SyToken *pTypeTok = pGen->pIn;` |
|       - |  6635 | `		/* A leading '?' is part of the type, look past it when sniffing the` |
|       - |  6636 | `		 * type keyword for the disallowed list. */` |
|     111 |  6637 | `		if( (pTypeTok->nType & PH7_TK_OP) && pTypeTok->sData.nByte == 1` |
|      16 |  6638 | `		 && pTypeTok->sData.zString[0] == '?' && pTypeTok + 1 < pGen->pEnd ){` |
|      16 |  6639 | `			pTypeTok = pTypeTok + 1;` |
|       7 |  6640 | `		}` |
|       - |  6641 | `		/* Reject disallowed standalone property types up front (when there's` |
|       - |  6642 | ``		 * no `\|` ahead): void, callable, never, mixed, iterable. The union`` |
|       - |  6643 | `		 * parser also rejects void/never inside unions; here we only catch` |
|       - |  6644 | `		 * the simple single-token form so the existing single-type error` |
|       - |  6645 | `		 * messages stay intact. */` |
|     106 |  6646 | `		if( pTypeTok->nType & PH7_TK_ID ){` |
|      14 |  6647 | `			SyString *pT = &pTypeTok->sData;` |
|      14 |  6648 | `			SyToken *pAfter = pTypeTok + 1;` |
|      26 |  6649 | `			int bSingle = (pAfter >= pGen->pEnd` |
|      12 |  6650 | `				\|\| (pAfter->nType & PH7_TK_DOLLAR)` |
|      22 |  6651 | `				\|\| !((pAfter->nType & PH7_TK_OP) && pAfter->sData.nByte == 1` |
|       4 |  6652 | `					&& pAfter->sData.zString[0] == '\|'));` |
|      15 |  6653 | `			if( bSingle && (` |
|       8 |  6654 | `				 (pT->nByte == 4 && SyMemcmpNoCase(pT->zString,"void",4) == 0)` |
|       8 |  6655 | `			 \|\| (pT->nByte == 5 && SyMemcmpNoCase(pT->zString,"never",5) == 0)` |
|       8 |  6656 | `			 \|\| (pT->nByte == 5 && SyMemcmpNoCase(pT->zString,"mixed",5) == 0)` |
|       8 |  6657 | `			 \|\| (pT->nByte == 8 && SyMemcmpNoCase(pT->zString,"callable",8) == 0)` |
|       8 |  6658 | `			 \|\| (pT->nByte == 8 && SyMemcmpNoCase(pT->zString,"iterable",8) == 0)) ){` |
|     ! 0 |  6659 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  6660 | `					"Property cannot have type %z",pT);` |
|     ! 0 |  6661 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  6662 | `					return SXERR_ABORT;` |
|       - |  6663 | `				}` |
|     ! 0 |  6664 | `				goto Synchronize;` |
|       - |  6665 | `			}` |
|       6 |  6666 | `		}` |
|     106 |  6667 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     106 |  6668 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6669 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6670 | `			goto Synchronize;` |
|     106 |  6671 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  6672 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6673 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  6674 | `				&pGen->pIn->sData);` |
|     ! 0 |  6675 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6676 | `				return SXERR_ABORT;` |
|       - |  6677 | `			}` |
|     ! 0 |  6678 | `			goto Synchronize;` |
|     106 |  6679 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6680 | `			return SXERR_ABORT;` |
|       - |  6681 | `		}` |
|      52 |  6682 | `	}` |
|     ! 0 |  6683 | `loop:` |
|   37258 |  6684 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6685 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  6686 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6687 | `			return SXERR_ABORT;` |
|       - |  6688 | `		}` |
|     ! 0 |  6689 | `		goto Synchronize;` |
|       - |  6690 | `	}` |
|   37258 |  6691 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   37258 |  6692 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  6693 | `		/* Invalid attribute name */` |
|     ! 0 |  6694 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  6695 | `		if( rc == SXERR_ABORT ){` |
|       - |  6696 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6697 | `			return SXERR_ABORT;` |
|       - |  6698 | `		}` |
|     ! 0 |  6699 | `		goto Synchronize;` |
|       - |  6700 | `	}` |
|       - |  6701 | `	/* Peek attribute name */` |
|   37258 |  6702 | `	pName = &pGen->pIn->sData;` |
|       - |  6703 | `	/* Advance the stream cursor */` |
|   37258 |  6704 | `	pGen->pIn++;` |
|   37258 |  6705 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  6706 | `		/* Invalid declaration */` |
|       3 |  6707 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  6708 | `		if( rc == SXERR_ABORT ){` |
|       - |  6709 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6710 | `			return SXERR_ABORT;` |
|       - |  6711 | `		}` |
|       3 |  6712 | `		goto Synchronize;` |
|       - |  6713 | `	}` |
|       - |  6714 | `	/* Allocate a new class attribute */` |
|   37256 |  6715 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   37256 |  6716 | `	if( pAttr == 0 ){` |
|     ! 0 |  6717 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  6718 | `		return SXERR_ABORT;` |
|       - |  6719 | `	}` |
|   37256 |  6720 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     110 |  6721 | `		pAttr->nType = nType;` |
|     110 |  6722 | `		pAttr->sClass = sTypeClass;` |
|     110 |  6723 | `		pAttr->sTypeName = sTypeText;` |
|     110 |  6724 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6725 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  6726 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  6727 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  6728 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  6729 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  6730 | `			sxu32 i;` |
|      32 |  6731 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  6732 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  6733 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  6734 | `			}` |
|       5 |  6735 | `		}` |
|      54 |  6736 | `	}` |
|   37256 |  6737 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  6738 | `		SySet *pInstrContainer;` |
|   11624 |  6739 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  6740 | `		/* Swap bytecode container */` |
|   11624 |  6741 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   11624 |  6742 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  6743 | `		/* Compile attribute value.` |
|       - |  6744 | `		 */` |
|   11624 |  6745 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   11624 |  6746 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  6747 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  6748 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6749 | `				return SXERR_ABORT;` |
|       - |  6750 | `			}` |
|     ! 0 |  6751 | `		}` |
|       - |  6752 | `		/* Emit the done instruction */` |
|   11624 |  6753 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   11624 |  6754 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5811 |  6755 | `	}` |
|       - |  6756 | `	/* All done,install the attribute */` |
|   37256 |  6757 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   37256 |  6758 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6759 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6760 | `		return SXERR_ABORT;` |
|       - |  6761 | `	}` |
|   37256 |  6762 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6763 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  6764 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  6765 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  6766 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6767 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6768 | `				pTok--;` |
|     ! 0 |  6769 | `			}` |
|     ! 0 |  6770 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6771 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  6772 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6773 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6774 | `				return SXERR_ABORT;` |
|       - |  6775 | `			}` |
|     ! 0 |  6776 | `		}else{` |
|       5 |  6777 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  6778 | `				goto loop;` |
|       - |  6779 | `			}` |
|       - |  6780 | `		}` |
|     ! 0 |  6781 | `	}` |
|   37252 |  6782 | `	SySetRelease(&aUnionAlts);` |
|   37252 |  6783 | `	return SXRET_OK;` |
|       1 |  6784 | `Synchronize:` |
|       - |  6785 | `	/* Synchronize with the first semi-colon */` |
|       5 |  6786 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 |  6787 | `		pGen->pIn++;` |
|       1 |  6788 | `	}` |
|       3 |  6789 | `	SySetRelease(&aUnionAlts);` |
|       3 |  6790 | `	return SXERR_CORRUPT;` |
|   18628 |  6791 |  |
|       - |  6792 | `/*` |
|       - |  6793 | ` * Compile a class method.` |
|       - |  6794 | ` *` |
|       - |  6795 | ` * Refer to the official documentation for more information` |
|       - |  6796 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  6797 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  6798 | ` * overloading and many more.` |
|       - |  6799 | ` */` |
|  136732 |  6800 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  6801 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6802 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  6803 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  6804 | `	int doBody,          /* TRUE to process method body */` |
|       - |  6805 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  6806 | `	)` |
|       2 |  6807 |  |
|  136734 |  6808 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6809 | `	ph7_class_method *pMeth;` |
|       - |  6810 | `	sxi32 iFuncFlags;` |
|       - |  6811 | `	SyString *pName;` |
|       - |  6812 | `	SyToken *pEnd;` |
|       - |  6813 | `	sxi32 rc;` |
|       - |  6814 | `	/* Extract visibility level */` |
|  136734 |  6815 | `	iProtection = GetProtectionLevel(iProtection);` |
|  136734 |  6816 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  136734 |  6817 | `	iFuncFlags = 0;` |
|  136734 |  6818 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  6819 | `		/* Invalid method name */` |
|     ! 0 |  6820 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  6821 | `		if( rc == SXERR_ABORT ){` |
|       - |  6822 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6823 | `			return SXERR_ABORT;` |
|       - |  6824 | `		}` |
|     ! 0 |  6825 | `		goto Synchronize;` |
|       - |  6826 | `	}` |
|  136734 |  6827 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6828 | `		/* Return by reference,remember that */` |
|     ! 0 |  6829 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6830 | `		/* Jump the '&' token */` |
|     ! 0 |  6831 | `		pGen->pIn++;` |
|     ! 0 |  6832 | `	}` |
|  136734 |  6833 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6834 | `		/* Invalid method name */` |
|     ! 0 |  6835 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  6836 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6837 | `			return SXERR_ABORT;` |
|       - |  6838 | `		}` |
|     ! 0 |  6839 | `		goto Synchronize;` |
|       - |  6840 | `	}` |
|       - |  6841 | `	/* Peek method name */` |
|  136734 |  6842 | `	pName = &pGen->pIn->sData;` |
|  136734 |  6843 | `	nLine = pGen->pIn->nLine;` |
|       - |  6844 | `	/* Jump the method name */` |
|  136734 |  6845 | `	pGen->pIn++;` |
|  136734 |  6846 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  6847 | `		/* Abstract method */` |
|   22754 |  6848 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  6849 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6850 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  6851 | `				&pClass->sName,pName);` |
|     ! 0 |  6852 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6853 | `				return SXERR_ABORT;` |
|       - |  6854 | `			}` |
|     ! 0 |  6855 | `		}` |
|       - |  6856 | `		/* Assemble method signature only */` |
|   22754 |  6857 | `		doBody = FALSE;` |
|   11376 |  6858 | `	}` |
|  136734 |  6859 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6860 | `		/* Syntax error */` |
|     ! 0 |  6861 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  6862 | `		if( rc == SXERR_ABORT ){` |
|       - |  6863 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6864 | `			return SXERR_ABORT;` |
|       - |  6865 | `		}` |
|     ! 0 |  6866 | `		goto Synchronize;` |
|       - |  6867 | `	}` |
|       - |  6868 | `	/* Allocate a new class_method instance */` |
|  136734 |  6869 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  136734 |  6870 | `	if( pMeth == 0 ){` |
|     ! 0 |  6871 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6872 | `		return SXERR_ABORT;` |
|       - |  6873 | `	}` |
|       - |  6874 | `	/* Jump the left parenthesis '(' */` |
|  136734 |  6875 | `	pGen->pIn++;` |
|  136734 |  6876 | `	pEnd = 0; /* cc warning */` |
|       - |  6877 | `	/* Delimit the method signature */` |
|  136734 |  6878 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  136734 |  6879 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6880 | `		/* Syntax error */` |
|       3 |  6881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  6882 | `		if( rc == SXERR_ABORT ){` |
|       - |  6883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6884 | `			return SXERR_ABORT;` |
|       - |  6885 | `		}` |
|       3 |  6886 | `		goto Synchronize;` |
|       - |  6887 | `	}` |
|  136732 |  6888 | `	if( pGen->pIn < pEnd ){` |
|       - |  6889 | `		/* Collect method arguments */` |
|   28486 |  6890 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   28486 |  6891 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6892 | `			return SXERR_ABORT;` |
|       - |  6893 | `		}` |
|   14242 |  6894 | `	}` |
|       - |  6895 | `	/* Point past ')' and parse optional return type ': type' */` |
|  136732 |  6896 | `	pGen->pIn = &pEnd[1];` |
|       - |  6897 | `	{` |
|  136732 |  6898 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  136732 |  6899 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6900 | `			return SXERR_ABORT;` |
|  136732 |  6901 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  6902 | `			goto Synchronize;` |
|       - |  6903 | `		}` |
|       - |  6904 | `	}` |
|  136732 |  6905 | `	if( doBody ){` |
|       - |  6906 | `		/* Compile method body */` |
|  113980 |  6907 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  113980 |  6908 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6909 | `			return SXERR_ABORT;` |
|       - |  6910 | `		}` |
|   56991 |  6911 | `	}else{` |
|       - |  6912 | `		/* Only method signature is allowed */` |
|   22754 |  6913 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  6914 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  6915 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  6916 | `				if( rc == SXERR_ABORT ){` |
|       - |  6917 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  6918 | `					return SXERR_ABORT;` |
|       - |  6919 | `				}` |
|     ! 0 |  6920 | `				return SXERR_CORRUPT;` |
|       - |  6921 | `			}` |
|       - |  6922 | `	}` |
|       - |  6923 | `	/* All done,install the method */` |
|  136732 |  6924 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  136732 |  6925 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6926 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6927 | `		return SXERR_ABORT;` |
|       - |  6928 | `	}` |
|  136732 |  6929 | `	return SXRET_OK;` |
|       1 |  6930 | `Synchronize:` |
|       - |  6931 | `	/* Synchronize with the first semi-colon */` |
|       7 |  6932 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 |  6933 | `		pGen->pIn++;` |
|       1 |  6934 | `	}` |
|       3 |  6935 | `	return SXERR_CORRUPT;` |
|   68368 |  6936 |  |
|       - |  6937 | `/*` |
|       - |  6938 | ` * Compile an object interface.` |
|       - |  6939 | ` *  According to the PHP language reference manual` |
|       - |  6940 | ` *   Object Interfaces:` |
|       - |  6941 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  6942 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  6943 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  6944 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  6945 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  6946 | ` */` |
|    8552 |  6947 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  6948 |  |
|    8554 |  6949 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6950 | `	ph7_class *pClass,*pBase;` |
|       - |  6951 | `	SyToken *pEnd,*pTmp;` |
|       - |  6952 | `	SyString *pName;` |
|       - |  6953 | `	sxi32 nKwrd;` |
|       - |  6954 | `	sxi32 rc;` |
|       - |  6955 | `	/* Jump the 'interface' keyword */` |
|    8554 |  6956 | `	pGen->pIn++;` |
|       - |  6957 | `	/* Extract interface name */` |
|    8554 |  6958 | `	pName = &pGen->pIn->sData;` |
|       - |  6959 | `	/* Advance the stream cursor */` |
|    8554 |  6960 | `	pGen->pIn++;` |
|       - |  6961 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  6962 | `		SyBlob sFQN;` |
|       - |  6963 | `		SyString sFQNStr;` |
|    8554 |  6964 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8554 |  6965 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8554 |  6966 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8554 |  6967 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8554 |  6968 | `		SyBlobRelease(&sFQN);` |
|       - |  6969 | `	}` |
|    8554 |  6970 | `	if( pClass == 0 ){` |
|     ! 0 |  6971 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6972 | `		return SXERR_ABORT;` |
|       - |  6973 | `	}` |
|       - |  6974 | `	/* Mark as an interface */` |
|    8554 |  6975 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  6976 | `	/* Assume no base class is given */` |
|    8554 |  6977 | `	pBase = 0;` |
|    8554 |  6978 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  6979 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  6980 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  6981 | `			SyString *pBaseName;` |
|       - |  6982 | `			/* Extract base interface */` |
|       3 |  6983 | `			pGen->pIn++;` |
|       3 |  6984 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6985 | `				/* Syntax error */` |
|     ! 0 |  6986 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6987 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  6988 | `					pName);` |
|     ! 0 |  6989 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  6990 | `				if( rc == SXERR_ABORT ){` |
|       - |  6991 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  6992 | `					return SXERR_ABORT;` |
|       - |  6993 | `				}` |
|     ! 0 |  6994 | `				return SXRET_OK;` |
|       - |  6995 | `			}` |
|       3 |  6996 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  6997 | `			{` |
|       - |  6998 | `				SyBlob sResolved;` |
|       3 |  6999 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 |  7000 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 |  7001 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 |  7002 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 |  7003 | `				SyBlobRelease(&sResolved);` |
|       - |  7004 | `			}` |
|       - |  7005 | `			/* Only interfaces is allowed */` |
|       3 |  7006 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7007 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7008 | `			}` |
|       3 |  7009 | `			if( pBase == 0 ){` |
|       - |  7010 | `				/* Inexistant interface */` |
|     ! 0 |  7011 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 |  7012 | `				if( rc == SXERR_ABORT ){` |
|       - |  7013 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7014 | `					return SXERR_ABORT;` |
|       - |  7015 | `				}` |
|     ! 0 |  7016 | `			}` |
|       - |  7017 | `			/* Advance the stream cursor */` |
|       3 |  7018 | `			pGen->pIn++;` |
|       1 |  7019 | `		}` |
|       1 |  7020 | `	}` |
|    8554 |  7021 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7022 | `		/* Syntax error */` |
|     ! 0 |  7023 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7024 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7025 | `		if( rc == SXERR_ABORT ){` |
|       - |  7026 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7027 | `			return SXERR_ABORT;` |
|       - |  7028 | `		}` |
|     ! 0 |  7029 | `		return SXRET_OK;` |
|       - |  7030 | `	}` |
|    8554 |  7031 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8554 |  7032 | `	pEnd = 0; /* cc warning */` |
|       - |  7033 | `	/* Delimit the interface body */` |
|    8554 |  7034 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8554 |  7035 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7036 | `		/* Syntax error */` |
|     ! 0 |  7037 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7038 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7039 | `		if( rc == SXERR_ABORT ){` |
|       - |  7040 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7041 | `			return SXERR_ABORT;` |
|       - |  7042 | `		}` |
|     ! 0 |  7043 | `		return SXRET_OK;` |
|       - |  7044 | `	}` |
|       - |  7045 | `	/* Swap token stream */` |
|    8554 |  7046 | `	pTmp = pGen->pEnd;` |
|    8554 |  7047 | `	pGen->pEnd = pEnd;` |
|       - |  7048 | `	/* Start the parse process` |
|       - |  7049 | `	 * Note (According to the PHP reference manual):` |
|       - |  7050 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7051 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7052 | `	 */` |
|   15647 |  7053 | `	for(;;){` |
|       - |  7054 | `		/* Jump leading/trailing semi-colons */` |
|   54038 |  7055 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   22744 |  7056 | `			pGen->pIn++;` |
|       2 |  7057 | `		}` |
|   31296 |  7058 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7059 | `			/* End of interface body */` |
|    8552 |  7060 | `			break;` |
|       - |  7061 | `		}` |
|   22746 |  7062 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7063 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7064 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7065 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7066 | `			if( rc == SXERR_ABORT ){` |
|       - |  7067 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7068 | `				return SXERR_ABORT;` |
|       - |  7069 | `			}` |
|     ! 0 |  7070 | `			goto done;` |
|       - |  7071 | `		}` |
|       - |  7072 | `		/* Extract the current keyword */` |
|   22746 |  7073 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22746 |  7074 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7075 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7076 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7077 | `			const char *zKind = "member";` |
|       3 |  7078 | `			SyString *pMemberName = 0;` |
|       3 |  7079 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7080 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7081 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7082 | `					zKind = "constant";` |
|       3 |  7083 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7084 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7085 | `					}` |
|       1 |  7086 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7087 | `					zKind = "method";` |
|     ! 0 |  7088 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7089 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7090 | `					}` |
|     ! 0 |  7091 | `				}` |
|       1 |  7092 | `			}` |
|       3 |  7093 | `			if( pMemberName ){` |
|       4 |  7094 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7095 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7096 | `			}else{` |
|     ! 0 |  7097 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7098 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7099 | `			}` |
|       3 |  7100 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7101 | `				return SXERR_ABORT;` |
|       - |  7102 | `			}` |
|       3 |  7103 | `			goto done;` |
|       - |  7104 | `		}` |
|   22744 |  7105 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7106 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7107 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7108 | `			if( rc == SXERR_ABORT ){` |
|       - |  7109 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7110 | `				return SXERR_ABORT;` |
|       - |  7111 | `			}` |
|     ! 0 |  7112 | `			goto done;` |
|       - |  7113 | `		}` |
|   22744 |  7114 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7115 | `			/* Advance the stream cursor */` |
|   22740 |  7116 | `			pGen->pIn++;` |
|   22740 |  7117 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7118 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7119 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7120 | `				if( rc == SXERR_ABORT ){` |
|       - |  7121 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7122 | `					return SXERR_ABORT;` |
|       - |  7123 | `				}` |
|     ! 0 |  7124 | `				goto done;` |
|       - |  7125 | `			}` |
|   22740 |  7126 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22740 |  7127 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7128 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7129 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7130 | `				if( rc == SXERR_ABORT ){` |
|       - |  7131 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7132 | `					return SXERR_ABORT;` |
|       - |  7133 | `				}` |
|     ! 0 |  7134 | `				goto done;` |
|       - |  7135 | `			}` |
|   11369 |  7136 | `		}` |
|   22744 |  7137 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7138 | `			/* Parse constant */` |
|       3 |  7139 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7140 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7141 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7142 | `					return SXERR_ABORT;` |
|       - |  7143 | `				}` |
|     ! 0 |  7144 | `				goto done;` |
|       - |  7145 | `			}` |
|       2 |  7146 | `		}else{` |
|   22742 |  7147 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   22742 |  7148 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7149 | `				/* Static method,record that */` |
|     ! 0 |  7150 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7151 | `				/* Advance the stream cursor */` |
|     ! 0 |  7152 | `				pGen->pIn++;` |
|     ! 0 |  7153 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  7154 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7155 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7156 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7157 | `						if( rc == SXERR_ABORT ){` |
|       - |  7158 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7159 | `							return SXERR_ABORT;` |
|       - |  7160 | `						}` |
|     ! 0 |  7161 | `						goto done;` |
|       - |  7162 | `				}` |
|     ! 0 |  7163 | `			}` |
|       - |  7164 | `			/* Process method signature (no body for interface methods) */` |
|   22742 |  7165 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   22742 |  7166 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7167 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7168 | `					return SXERR_ABORT;` |
|       - |  7169 | `				}` |
|     ! 0 |  7170 | `				goto done;` |
|       - |  7171 | `			}` |
|       - |  7172 | `		}` |
|       2 |  7173 | `	}` |
|       - |  7174 | `	/* Install the interface */` |
|    8552 |  7175 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8552 |  7176 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7177 | `		/* Inherit from the base interface */` |
|       3 |  7178 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 |  7179 | `	}` |
|    8552 |  7180 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7181 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7182 | `		return SXERR_ABORT;` |
|       - |  7183 | `	}` |
|    4275 |  7184 | `done:` |
|       - |  7185 | `	/* Point beyond the interface body */` |
|    8554 |  7186 | `	pGen->pIn  = &pEnd[1];` |
|    8554 |  7187 | `	pGen->pEnd = pTmp;` |
|    8554 |  7188 | `	return PH7_OK;` |
|    4278 |  7189 |  |
|       - |  7190 | `/*` |
|       - |  7191 | ` * Compile a user-defined class.` |
|       - |  7192 | ` * According to the PHP language reference manual` |
|       - |  7193 | ` *  class` |
|       - |  7194 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7195 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7196 | ` *  of the properties and methods belonging to the class.` |
|       - |  7197 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7198 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7199 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7200 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7201 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7202 | ` *  (called "methods").` |
|       - |  7203 | ` */` |
|       - |  7204 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7205 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7206 | `struct TraitUseEntry {` |
|       - |  7207 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7208 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7209 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7210 | `};` |
|       - |  7211 | `/*` |
|       - |  7212 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7213 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7214 | ` */` |
|   40350 |  7215 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7216 |  |
|       - |  7217 | `	ph7_class **apIface;` |
|       - |  7218 | `	sxu32 nIface,i;` |
|       - |  7219 | `	sxi32 rc;` |
|   40352 |  7220 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7221 | `		return SXRET_OK;` |
|       - |  7222 | `	}` |
|   40352 |  7223 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   40352 |  7224 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   43228 |  7225 | `	for(i = 0; i < nIface; i++){` |
|    2878 |  7226 | `		ph7_class *pIface = apIface[i];` |
|       - |  7227 | `		SyHashEntry *pEntry;` |
|    2878 |  7228 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   17146 |  7229 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   14270 |  7230 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7231 | `			ph7_class_method *pImplMeth;` |
|   14270 |  7232 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7233 | `			/* Find the implementing method in the class */` |
|   14270 |  7234 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   14270 |  7235 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7236 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7237 | `			}` |
|       - |  7238 | `			/* Check visibility: interface methods must be implemented as public */` |
|   14256 |  7239 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7240 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7241 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7242 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7243 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7244 | `					return SXERR_ABORT;` |
|       - |  7245 | `				}` |
|       1 |  7246 | `			}` |
|       - |  7247 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7248 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7249 | `			 */` |
|       - |  7250 | `			{` |
|   14256 |  7251 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   14256 |  7252 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   14256 |  7253 | `				int sigError = 0;` |
|   14256 |  7254 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7255 | `					sigError = 1;` |
|   14255 |  7256 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7257 | `					/* Extra parameters must all have default values */` |
|       5 |  7258 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7259 | `					sxu32 k;` |
|       7 |  7260 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7261 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7262 | `							sigError = 1;` |
|       3 |  7263 | `							break;` |
|       - |  7264 | `						}` |
|       2 |  7265 | `					}` |
|       2 |  7266 | `				}` |
|   14256 |  7267 | `				if( sigError ){` |
|       - |  7268 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7269 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7270 | `					sxu32 j;` |
|       5 |  7271 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7272 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7273 | `					/* Build implementing method signature */` |
|       5 |  7274 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7275 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7276 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7277 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7278 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7279 | `					}` |
|       - |  7280 | `					/* Build interface method signature */` |
|       5 |  7281 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7282 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7283 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7284 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7285 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7286 | `					}` |
|       7 |  7287 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7288 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7289 | `						&pClass->sName,pMName,` |
|       4 |  7290 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7291 | `						&pIface->sName,pMName,` |
|       4 |  7292 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7293 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7294 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7295 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7296 | `						return SXERR_ABORT;` |
|       - |  7297 | `					}` |
|       2 |  7298 | `				}` |
|       - |  7299 | `			}` |
|       2 |  7300 | `		}` |
|    1440 |  7301 | `	}` |
|   40352 |  7302 | `	return SXRET_OK;` |
|   20177 |  7303 |  |
|       - |  7304 | `/*` |
|       - |  7305 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7306 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7307 | ` */` |
|   40350 |  7308 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7309 |  |
|       - |  7310 | `	ph7_class_method *pMeth;` |
|       - |  7311 | `	SyHashEntry *pEntry;` |
|       - |  7312 | `	sxu32 nAbstract;` |
|       - |  7313 | `	SyBlob sMsg;` |
|       - |  7314 | `	sxi32 rc;` |
|       - |  7315 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   40352 |  7316 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 |  7317 | `		return SXRET_OK;` |
|       - |  7318 | `	}` |
|       - |  7319 | `	/* Count abstract methods */` |
|   40334 |  7320 | `	nAbstract = 0;` |
|   40334 |  7321 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  381780 |  7322 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  341448 |  7323 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  341448 |  7324 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7325 | `			nAbstract++;` |
|       8 |  7326 | `		}` |
|       2 |  7327 | `	}` |
|   40334 |  7328 | `	if( nAbstract == 0 ){` |
|   40320 |  7329 | `		return SXRET_OK;` |
|       - |  7330 | `	}` |
|       - |  7331 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7332 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7333 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7334 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7335 | `		&pClass->sName,nAbstract,` |
|       7 |  7336 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7337 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7338 | `	/* Second pass: list methods with origins */` |
|       - |  7339 | `	{` |
|      15 |  7340 | `		sxu32 nListed = 0;` |
|      15 |  7341 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7342 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7343 | `			ph7_class *pOrigin = 0;` |
|       - |  7344 | `			SyString *pMName;` |
|      19 |  7345 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7346 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7347 | `				continue;` |
|       - |  7348 | `			}` |
|      17 |  7349 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7350 | `			if( nListed > 0 ){` |
|       3 |  7351 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7352 | `			}` |
|       - |  7353 | `			/* Find the origin of this abstract method.` |
|       - |  7354 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7355 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7356 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7357 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7358 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7359 | `			 * class's namespace.` |
|       - |  7360 | `			 */` |
|       - |  7361 | `			{` |
|       - |  7362 | `				ph7_class **apIface;` |
|       - |  7363 | `				ph7_class **apTrait;` |
|       - |  7364 | `				ph7_class *pWalk;` |
|       - |  7365 | `				sxu32 i;` |
|       - |  7366 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7367 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7368 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7369 | `				 */` |
|      17 |  7370 | `				if( pClass->pBase ){` |
|       9 |  7371 | `					pWalk = pClass->pBase;` |
|      17 |  7372 | `					while( pWalk ){` |
|       - |  7373 | `						ph7_class_method *pParentMeth;` |
|      11 |  7374 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7375 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7376 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7377 | `							 * in this class's ancestor chain.` |
|       - |  7378 | `							 */` |
|      11 |  7379 | `							int fromIface = 0;` |
|      11 |  7380 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7381 | `							while( pAnc ){` |
|       - |  7382 | `								ph7_class **apPI;` |
|       - |  7383 | `								sxu32 j;` |
|      13 |  7384 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7385 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7386 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7387 | `										fromIface = 1;` |
|       9 |  7388 | `										break;` |
|       - |  7389 | `									}` |
|     ! 0 |  7390 | `								}` |
|      13 |  7391 | `								if( fromIface ) break;` |
|       5 |  7392 | `								pAnc = pAnc->pBase;` |
|       1 |  7393 | `							}` |
|      11 |  7394 | `							if( !fromIface ){` |
|       3 |  7395 | `								pOrigin = pWalk;` |
|       3 |  7396 | `								break;` |
|       - |  7397 | `							}` |
|       4 |  7398 | `						}` |
|       9 |  7399 | `						pWalk = pWalk->pBase;` |
|       1 |  7400 | `					}` |
|       4 |  7401 | `				}` |
|       - |  7402 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7403 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7404 | `				 */` |
|      17 |  7405 | `				if( !pOrigin ){` |
|      15 |  7406 | `					pWalk = pClass;` |
|      37 |  7407 | `					while( pWalk && !pOrigin ){` |
|      23 |  7408 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7409 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7410 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7411 | `							ph7_class *pDeepest = 0;` |
|      25 |  7412 | `							while( pIface ){` |
|      13 |  7413 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7414 | `									pDeepest = pIface;` |
|       6 |  7415 | `								}` |
|      13 |  7416 | `								pIface = pIface->pBase;` |
|       1 |  7417 | `							}` |
|      13 |  7418 | `							if( pDeepest ){` |
|      13 |  7419 | `								pOrigin = pDeepest;` |
|      13 |  7420 | `								break;` |
|       - |  7421 | `							}` |
|     ! 0 |  7422 | `						}` |
|      23 |  7423 | `						pWalk = pWalk->pBase;` |
|       1 |  7424 | `					}` |
|       7 |  7425 | `				}` |
|       - |  7426 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7427 | `				if( !pOrigin ){` |
|       3 |  7428 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7429 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7430 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7431 | `							pOrigin = pClass;` |
|       3 |  7432 | `							break;` |
|       - |  7433 | `						}` |
|     ! 0 |  7434 | `					}` |
|       1 |  7435 | `				}` |
|       - |  7436 | `			}` |
|      17 |  7437 | `			if( pOrigin ){` |
|      17 |  7438 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7439 | `			}else{` |
|       - |  7440 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7441 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7442 | `			}` |
|      17 |  7443 | `			nListed++;` |
|       1 |  7444 | `		}` |
|       - |  7445 | `	}` |
|      15 |  7446 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7447 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7448 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7449 | `	SyBlobRelease(&sMsg);` |
|      15 |  7450 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7451 | `		return SXERR_ABORT;` |
|       - |  7452 | `	}` |
|      15 |  7453 | `	return SXRET_OK;` |
|   20177 |  7454 |  |
|   40354 |  7455 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  7456 |  |
|   40356 |  7457 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7458 | `	ph7_class *pClass,*pBase;` |
|       - |  7459 | `	SyToken *pEnd,*pTmp;` |
|       - |  7460 | `	sxi32 iProtection;` |
|       - |  7461 | `	SySet aInterfaces;` |
|       - |  7462 | `	SySet aUseEntries;` |
|       - |  7463 | `	sxi32 iAttrflags;` |
|       - |  7464 | `	SyString *pName;` |
|       - |  7465 | `	sxi32 nKwrd;` |
|       - |  7466 | `	sxi32 rc;` |
|       - |  7467 | `	/* Jump the 'class' keyword */` |
|   40356 |  7468 | `	pGen->pIn++;` |
|   40356 |  7469 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7470 | `		/* Syntax error */` |
|     ! 0 |  7471 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  7472 | `		if( rc == SXERR_ABORT ){` |
|       - |  7473 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7474 | `			return SXERR_ABORT;` |
|       - |  7475 | `		}` |
|       - |  7476 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  7477 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  7478 | `			pGen->pIn++;` |
|     ! 0 |  7479 | `		}` |
|     ! 0 |  7480 | `		return SXRET_OK;` |
|       - |  7481 | `	}` |
|       - |  7482 | `	/* Extract class name */` |
|   40356 |  7483 | `	pName = &pGen->pIn->sData;` |
|       - |  7484 | `	/* Advance the stream cursor */` |
|   40356 |  7485 | `	pGen->pIn++;` |
|       - |  7486 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7487 | `		SyBlob sFQN;` |
|       - |  7488 | `		SyString sFQNStr;` |
|   40356 |  7489 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   40356 |  7490 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   40356 |  7491 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   40356 |  7492 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   40356 |  7493 | `		SyBlobRelease(&sFQN);` |
|       - |  7494 | `	}` |
|   40356 |  7495 | `	if( pClass == 0 ){` |
|     ! 0 |  7496 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7497 | `		return SXERR_ABORT;` |
|       - |  7498 | `	}` |
|       - |  7499 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   40356 |  7500 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   40356 |  7501 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  7502 | `	/* Assume a standalone class */` |
|   40356 |  7503 | `	pBase = 0;` |
|   40356 |  7504 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7505 | `		SyString *pBaseName;` |
|   28534 |  7506 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   28534 |  7507 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   25660 |  7508 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   25660 |  7509 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7510 | `				/* Syntax error */` |
|     ! 0 |  7511 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7512 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 |  7513 | `					pName);` |
|     ! 0 |  7514 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7515 | `				if( rc == SXERR_ABORT ){` |
|       - |  7516 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7517 | `					return SXERR_ABORT;` |
|       - |  7518 | `				}` |
|     ! 0 |  7519 | `				return SXRET_OK;` |
|       - |  7520 | `			}` |
|       - |  7521 | `			/* Extract base class name and resolve through namespace/imports */` |
|   25660 |  7522 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  7523 | `			{` |
|       - |  7524 | `				SyBlob sResolved;` |
|   25660 |  7525 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   25660 |  7526 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   38489 |  7527 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   25658 |  7528 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   25660 |  7529 | `				SyBlobRelease(&sResolved);` |
|       - |  7530 | `			}` |
|       - |  7531 | `			/* Interfaces are not allowed */` |
|   25660 |  7532 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  7533 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7534 | `			}` |
|   25660 |  7535 | `			if( pBase == 0 ){` |
|       - |  7536 | `				/* Inexistant base class */` |
|     ! 0 |  7537 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 |  7538 | `				if( rc == SXERR_ABORT ){` |
|       - |  7539 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7540 | `					return SXERR_ABORT;` |
|       - |  7541 | `				}` |
|     ! 0 |  7542 | `			}else{` |
|   25660 |  7543 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  7544 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7545 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  7546 | `					if( rc == SXERR_ABORT ){` |
|       - |  7547 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7548 | `						return SXERR_ABORT;` |
|       - |  7549 | `					}` |
|     ! 0 |  7550 | `				}` |
|       - |  7551 | `			}` |
|       - |  7552 | `			/* Advance the stream cursor */` |
|   25660 |  7553 | `			pGen->pIn++;` |
|   12829 |  7554 | `		}` |
|   28534 |  7555 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  7556 | `			ph7_class *pInterface;` |
|       - |  7557 | `			SyString *pIntName;` |
|       - |  7558 | `			/* Interface implementation */` |
|    2878 |  7559 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1438 |  7560 | `			for(;;){` |
|    2878 |  7561 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7562 | `					/* Syntax error */` |
|     ! 0 |  7563 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7564 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  7565 | `						pName);` |
|     ! 0 |  7566 | `					if( rc == SXERR_ABORT ){` |
|       - |  7567 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7568 | `						return SXERR_ABORT;` |
|       - |  7569 | `					}` |
|     ! 0 |  7570 | `					break;` |
|       - |  7571 | `				}` |
|       - |  7572 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2878 |  7573 | `				pIntName = &pGen->pIn->sData;` |
|       - |  7574 | `				{` |
|       - |  7575 | `					SyBlob sResolved;` |
|    2878 |  7576 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2878 |  7577 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5754 |  7578 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2876 |  7579 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2878 |  7580 | `					SyBlobRelease(&sResolved);` |
|       - |  7581 | `				}` |
|       - |  7582 | `				/* Only interfaces are allowed */` |
|    2878 |  7583 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7584 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  7585 | `				}` |
|    2878 |  7586 | `				if( pInterface == 0 ){` |
|       - |  7587 | `					/* Inexistant interface */` |
|     ! 0 |  7588 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 |  7589 | `					if( rc == SXERR_ABORT ){` |
|       - |  7590 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7591 | `						return SXERR_ABORT;` |
|       - |  7592 | `					}` |
|     ! 0 |  7593 | `				}else{` |
|       - |  7594 | `					/* Register interface */` |
|    2878 |  7595 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  7596 | `				}` |
|       - |  7597 | `				/* Advance the stream cursor */` |
|    2878 |  7598 | `				pGen->pIn++;` |
|    2878 |  7599 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1440 |  7600 | `					break;` |
|       - |  7601 | `				}` |
|     ! 0 |  7602 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  7603 | `			}` |
|    1438 |  7604 | `		}` |
|   14266 |  7605 | `	}` |
|   40356 |  7606 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7607 | `		/* Syntax error */` |
|     ! 0 |  7608 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  7609 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7610 | `		if( rc == SXERR_ABORT ){` |
|       - |  7611 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7612 | `			return SXERR_ABORT;` |
|       - |  7613 | `		}` |
|     ! 0 |  7614 | `		return SXRET_OK;` |
|       - |  7615 | `	}` |
|   40356 |  7616 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   40356 |  7617 | `	pEnd = 0; /* cc warning */` |
|       - |  7618 | `	/* Delimit the class body */` |
|   40356 |  7619 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   40356 |  7620 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7621 | `		/* Syntax error */` |
|     ! 0 |  7622 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  7623 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7624 | `		if( rc == SXERR_ABORT ){` |
|       - |  7625 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7626 | `			return SXERR_ABORT;` |
|       - |  7627 | `		}` |
|     ! 0 |  7628 | `		return SXRET_OK;` |
|       - |  7629 | `	}` |
|       - |  7630 | `	/* Swap token stream */` |
|   40356 |  7631 | `	pTmp = pGen->pEnd;` |
|   40356 |  7632 | `	pGen->pEnd = pEnd;` |
|       - |  7633 | `	/* Set the inherited flags */` |
|   40356 |  7634 | `	pClass->iFlags = iFlags;` |
|       - |  7635 | `	/* Start the parse process */` |
|   77162 |  7636 | `	for(;;){` |
|       - |  7637 | `		/* Jump leading/trailing semi-colons */` |
|  228900 |  7638 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   37306 |  7639 | `			pGen->pIn++;` |
|       2 |  7640 | `		}` |
|  191596 |  7641 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7642 | `			/* End of class body */` |
|   40352 |  7643 | `			break;` |
|       - |  7644 | `		}` |
|  151246 |  7645 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  7646 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7647 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7648 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7649 | `			if( rc == SXERR_ABORT ){` |
|       - |  7650 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7651 | `				return SXERR_ABORT;` |
|       - |  7652 | `			}` |
|     ! 0 |  7653 | `			goto done;` |
|       - |  7654 | `		}` |
|       - |  7655 | `		/* Assume public visibility */` |
|  151246 |  7656 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  151246 |  7657 | `		iAttrflags = 0;` |
|  151246 |  7658 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  7659 | `			/* Extract the current keyword */` |
|  151246 |  7660 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  151246 |  7661 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  7662 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  7663 | `				TraitUseEntry sUse;` |
|      44 |  7664 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  7665 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  7666 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  7667 | `				for(;;){` |
|       - |  7668 | `					ph7_class *pTrait;` |
|       - |  7669 | `					SyString *pTraitName;` |
|      52 |  7670 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7671 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7672 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  7673 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7674 | `							return SXERR_ABORT;` |
|       - |  7675 | `						}` |
|     ! 0 |  7676 | `						break;` |
|       - |  7677 | `					}` |
|      52 |  7678 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  7679 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  7680 | `						SyBlob sResolved;` |
|      52 |  7681 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  7682 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  7683 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  7684 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  7685 | `						SyBlobRelease(&sResolved);` |
|       - |  7686 | `					}` |
|       - |  7687 | `					/* Only traits are allowed */` |
|      52 |  7688 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  7689 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  7690 | `					}` |
|      52 |  7691 | `					if( pTrait == 0 ){` |
|     ! 0 |  7692 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7693 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  7694 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7695 | `							return SXERR_ABORT;` |
|       - |  7696 | `						}` |
|     ! 0 |  7697 | `					}else{` |
|      52 |  7698 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  7699 | `					}` |
|      52 |  7700 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  7701 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  7702 | `						break;` |
|       - |  7703 | `					}` |
|       9 |  7704 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  7705 | `				}` |
|       - |  7706 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  7707 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  7708 | `					SyToken *pBlock;` |
|       9 |  7709 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  7710 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  7711 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  7712 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  7713 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  7714 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  7715 | `					}else{` |
|     ! 0 |  7716 | `						pGen->pIn = pGen->pEnd;` |
|       - |  7717 | `					}` |
|       4 |  7718 | `				}` |
|      44 |  7719 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  7720 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  7721 | `				continue;` |
|       - |  7722 | `			}` |
|  151204 |  7723 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  148256 |  7724 | `				iProtection = nKwrd;` |
|  148256 |  7725 | `				pGen->pIn++; /* Jump the visibility token */` |
|  148254 |  7726 | `				if( pGen->pIn >= pGen->pEnd` |
|  148256 |  7727 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  7728 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7729 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7730 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  7731 | `					if( rc == SXERR_ABORT ){` |
|       - |  7732 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7733 | `						return SXERR_ABORT;` |
|       - |  7734 | `					}` |
|     ! 0 |  7735 | `					goto done;` |
|       - |  7736 | `				}` |
|  148256 |  7737 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  7738 | `					/* Attribute declaration (untyped) */` |
|   37128 |  7739 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   37128 |  7740 | `					if( rc != SXRET_OK ){` |
|       3 |  7741 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7742 | `							return SXERR_ABORT;` |
|       - |  7743 | `						}` |
|       3 |  7744 | `						goto done;` |
|       - |  7745 | `					}` |
|   37126 |  7746 | `					continue;` |
|       - |  7747 | `				}` |
|  111130 |  7748 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  7749 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      96 |  7750 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      96 |  7751 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  7752 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7753 | `							return SXERR_ABORT;` |
|       - |  7754 | `						}` |
|     ! 0 |  7755 | `						goto done;` |
|       - |  7756 | `					}` |
|      96 |  7757 | `					continue;` |
|       - |  7758 | `				}` |
|       - |  7759 | `				/* Extract the keyword */` |
|  111036 |  7760 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   55517 |  7761 | `			}` |
|  113984 |  7762 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7763 | `				/* Process constant declaration */` |
|      30 |  7764 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  7765 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  7766 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7767 | `						return SXERR_ABORT;` |
|       - |  7768 | `					}` |
|     ! 0 |  7769 | `					goto done;` |
|       - |  7770 | `				}` |
|      16 |  7771 | `			}else{` |
|  113956 |  7772 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7773 | `					/* Static method or attribute,record that */` |
|    2876 |  7774 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2876 |  7775 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2876 |  7776 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7777 | `						/* Extract the keyword */` |
|    2872 |  7778 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2872 |  7779 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  7780 | `							iProtection = nKwrd;` |
|     ! 0 |  7781 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  7782 | `						}` |
|    1435 |  7783 | `					}` |
|    2874 |  7784 | `					if( pGen->pIn >= pGen->pEnd` |
|    2876 |  7785 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  7786 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7787 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  7788 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  7789 | `						if( rc == SXERR_ABORT ){` |
|       - |  7790 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7791 | `							return SXERR_ABORT;` |
|       - |  7792 | `						}` |
|     ! 0 |  7793 | `						goto done;` |
|       - |  7794 | `					}` |
|    2876 |  7795 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  7796 | `						/* Attribute declaration */` |
|       5 |  7797 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  7798 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  7799 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7800 | `								return SXERR_ABORT;` |
|       - |  7801 | `							}` |
|     ! 0 |  7802 | `							goto done;` |
|       - |  7803 | `						}` |
|       5 |  7804 | `						continue;` |
|       - |  7805 | `					}` |
|    2872 |  7806 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  7807 | `						/* Typed static attribute declaration */` |
|       8 |  7808 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  7809 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  7810 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7811 | `								return SXERR_ABORT;` |
|       - |  7812 | `							}` |
|     ! 0 |  7813 | `							goto done;` |
|       - |  7814 | `						}` |
|       8 |  7815 | `						continue;` |
|       - |  7816 | `					}` |
|       - |  7817 | `					/* Extract the keyword */` |
|    2866 |  7818 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  112514 |  7819 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  7820 | `					/* Abstract method,record that */` |
|      10 |  7821 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  7822 | `					/* Mark the whole class as abstract */` |
|      10 |  7823 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  7824 | `					/* Advance the stream cursor */` |
|      10 |  7825 | `					pGen->pIn++;` |
|      10 |  7826 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 |  7827 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 |  7828 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 |  7829 | `							iProtection = nKwrd;` |
|       8 |  7830 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  7831 | `						}` |
|       4 |  7832 | `					}` |
|      10 |  7833 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  7834 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  7835 | `							/* Static method */` |
|     ! 0 |  7836 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  7837 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  7838 | `					}` |
|      10 |  7839 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 |  7840 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7841 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7842 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  7843 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  7844 | `							if( rc == SXERR_ABORT ){` |
|       - |  7845 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  7846 | `								return SXERR_ABORT;` |
|       - |  7847 | `							}` |
|     ! 0 |  7848 | `							goto done;` |
|       - |  7849 | `					}` |
|      10 |  7850 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  111078 |  7851 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  7852 | `					/* final method ,record that */` |
|       5 |  7853 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  7854 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  7855 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7856 | `						/* Extract the keyword */` |
|       5 |  7857 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  7858 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  7859 | `							iProtection = nKwrd;` |
|       5 |  7860 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  7861 | `						}` |
|       2 |  7862 | `					}` |
|       5 |  7863 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  7864 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  7865 | `							/* Static method */` |
|     ! 0 |  7866 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  7867 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  7868 | `					}` |
|       5 |  7869 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  7870 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7871 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7872 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  7873 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  7874 | `							if( rc == SXERR_ABORT ){` |
|       - |  7875 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  7876 | `								return SXERR_ABORT;` |
|       - |  7877 | `							}` |
|     ! 0 |  7878 | `							goto done;` |
|       - |  7879 | `					}` |
|       5 |  7880 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  7881 | `				}` |
|  113946 |  7882 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  7883 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7884 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  7885 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  7886 | `						if( rc == SXERR_ABORT ){` |
|       - |  7887 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7888 | `							return SXERR_ABORT;` |
|       - |  7889 | `						}` |
|     ! 0 |  7890 | `						goto done;` |
|       - |  7891 | `				}` |
|  113946 |  7892 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  7893 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  7894 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  7895 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7896 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  7897 | `						if( rc == SXERR_ABORT ){` |
|       - |  7898 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7899 | `							return SXERR_ABORT;` |
|       - |  7900 | `						}` |
|     ! 0 |  7901 | `						goto done;` |
|       - |  7902 | `					}` |
|       - |  7903 | `					/* Attribute declaration */` |
|       7 |  7904 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  7905 | `				}else{` |
|       - |  7906 | `					/* Process method declaration */` |
|  113940 |  7907 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  7908 | `				}` |
|  113946 |  7909 | `				if( rc != SXRET_OK ){` |
|       3 |  7910 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7911 | `						return SXERR_ABORT;` |
|       - |  7912 | `					}` |
|       3 |  7913 | `					goto done;` |
|       - |  7914 | `				}` |
|       - |  7915 | `			}` |
|   56987 |  7916 | `		}else{` |
|       - |  7917 | `			/* Attribute declaration */` |
|     ! 0 |  7918 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  7919 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7920 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7921 | `					return SXERR_ABORT;` |
|       - |  7922 | `				}` |
|     ! 0 |  7923 | `				goto done;` |
|       - |  7924 | `			}` |
|       - |  7925 | `		}` |
|       2 |  7926 | `	}` |
|       - |  7927 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  7928 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  7929 | `	 */` |
|       - |  7930 | `	{` |
|       - |  7931 | `		TraitUseEntry *apUse;` |
|       - |  7932 | `		sxu32 nU;` |
|   40352 |  7933 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   40394 |  7934 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  7935 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  7936 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  7937 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  7938 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  7939 | `			sxu32 nT;` |
|      44 |  7940 | `			if( !hasResolution ){` |
|       - |  7941 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  7942 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  7943 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  7944 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  7945 | `						break;` |
|       - |  7946 | `					}` |
|      22 |  7947 | `				}` |
|      19 |  7948 | `			}else{` |
|       - |  7949 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  7950 | `				 * then use the block to resolve method conflicts.` |
|       - |  7951 | `				 */` |
|       - |  7952 | `				SyToken *pR;` |
|      19 |  7953 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  7954 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  7955 | `					ph7_class_attr *pAR;` |
|       - |  7956 | `					SyHashEntry *pER;` |
|       - |  7957 | `					SyString *pNR;` |
|      11 |  7958 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  7959 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  7960 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  7961 | `						pNR = &pAR->sName;` |
|     ! 0 |  7962 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  7963 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  7964 | `						}` |
|     ! 0 |  7965 | `					}` |
|      11 |  7966 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  7967 | `				}` |
|       - |  7968 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  7969 | `				pR = pUse->pResolvStart;` |
|      21 |  7970 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  7971 | `					SyString sTrait,sMethod;` |
|       - |  7972 | `					ph7_class *pSrcTrait;` |
|       - |  7973 | `					ph7_class_method *pMeth;` |
|       - |  7974 | `					sxi32 nRKwrd;` |
|      33 |  7975 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  7976 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  7977 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  7978 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  7979 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  7980 | `					sMethod = pR->sData;` |
|      13 |  7981 | `					pR++;` |
|      13 |  7982 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  7983 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  7984 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  7985 | `							sTrait = sMethod;` |
|       7 |  7986 | `							pR++;` |
|       7 |  7987 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  7988 | `							sMethod = pR->sData;` |
|       7 |  7989 | `							pR++;` |
|       3 |  7990 | `						}` |
|       3 |  7991 | `					}` |
|      13 |  7992 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7993 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  7994 | `						continue;` |
|       - |  7995 | `					}` |
|      13 |  7996 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  7997 | `					pR++;` |
|      13 |  7998 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  7999 | `						pSrcTrait = 0;` |
|       7 |  8000 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8001 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8002 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8003 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8004 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8005 | `								break;` |
|       - |  8006 | `							}` |
|       2 |  8007 | `						}` |
|       5 |  8008 | `						if( pSrcTrait ){` |
|       5 |  8009 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8010 | `							if( pMeth ){` |
|       5 |  8011 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8012 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8013 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8014 | `								}` |
|       2 |  8015 | `							}` |
|       2 |  8016 | `						}` |
|       2 |  8017 | `					}` |
|      29 |  8018 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8019 | `				}` |
|       - |  8020 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8021 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8022 | `					ph7_class_method *pMR;` |
|       - |  8023 | `					SyHashEntry *pER;` |
|       - |  8024 | `					SyString *pNR;` |
|      11 |  8025 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8026 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8027 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8028 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8029 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8030 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8031 | `						}` |
|       1 |  8032 | `					}` |
|       6 |  8033 | `				}` |
|       - |  8034 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8035 | `				pR = pUse->pResolvStart;` |
|      21 |  8036 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8037 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8038 | `					ph7_class *pSrcTrait;` |
|       - |  8039 | `					ph7_class_method *pMeth;` |
|      21 |  8040 | `					int hasQual = 0;` |
|       - |  8041 | `					sxi32 nRKwrd;` |
|      33 |  8042 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8043 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8044 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8045 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8046 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8047 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8048 | `					sMethod = pR->sData;` |
|      13 |  8049 | `					pR++;` |
|      13 |  8050 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8051 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8052 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8053 | `							sTrait = sMethod;` |
|       7 |  8054 | `							hasQual = 1;` |
|       7 |  8055 | `							pR++;` |
|       7 |  8056 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8057 | `							sMethod = pR->sData;` |
|       7 |  8058 | `							pR++;` |
|       3 |  8059 | `						}` |
|       3 |  8060 | `					}` |
|      13 |  8061 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8062 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8063 | `						continue;` |
|       - |  8064 | `					}` |
|      13 |  8065 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8066 | `					pR++;` |
|      13 |  8067 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8068 | `						sxi32 iNewVis = -1;` |
|       9 |  8069 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8070 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8071 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8072 | `								iNewVis = nAK;` |
|       7 |  8073 | `								pR++;` |
|       3 |  8074 | `							}` |
|       3 |  8075 | `						}` |
|       9 |  8076 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8077 | `							sAlias = pR->sData;` |
|       7 |  8078 | `							pR++;` |
|       3 |  8079 | `						}` |
|       9 |  8080 | `						pMeth = 0;` |
|       9 |  8081 | `						if( hasQual ){` |
|       3 |  8082 | `							pSrcTrait = 0;` |
|       5 |  8083 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8084 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8085 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8086 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8087 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8088 | `									break;` |
|       - |  8089 | `								}` |
|       2 |  8090 | `							}` |
|       3 |  8091 | `							if( pSrcTrait ){` |
|       3 |  8092 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8093 | `							}` |
|       2 |  8094 | `						}else{` |
|       7 |  8095 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8096 | `						}` |
|       9 |  8097 | `						if( pMeth ){` |
|       9 |  8098 | `							if( sAlias.nByte > 0 ){` |
|       - |  8099 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8100 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8101 | `								 */` |
|       - |  8102 | `								ph7_class_method *pAlias;` |
|       - |  8103 | `								char *zAliasDup;` |
|       7 |  8104 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8105 | `								if( pAlias ){` |
|       7 |  8106 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8107 | `									if( iNewVis >= 0 ){` |
|       5 |  8108 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8109 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8110 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8111 | `									}` |
|       7 |  8112 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8113 | `									if( zAliasDup ){` |
|       7 |  8114 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8115 | `									}` |
|       4 |  8116 | `								}` |
|       6 |  8117 | `							}else if( iNewVis >= 0 ){` |
|       - |  8118 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8119 | `								ph7_class_method *pCopy;` |
|       3 |  8120 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8121 | `								if( pCopy ){` |
|       3 |  8122 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8123 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8124 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8125 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8126 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8127 | `									/* Replace the method in the class hash */` |
|       3 |  8128 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8129 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8130 | `								}` |
|       1 |  8131 | `							}` |
|       4 |  8132 | `						}` |
|       4 |  8133 | `						SXUNUSED(hasQual);` |
|       4 |  8134 | `					}` |
|      17 |  8135 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8136 | `				}` |
|       - |  8137 | `			}` |
|      44 |  8138 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  8139 | `		}` |
|       - |  8140 | `	}` |
|       - |  8141 | `	/* Install the class */` |
|   40352 |  8142 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   40352 |  8143 | `	if( rc == SXRET_OK ){` |
|       - |  8144 | `		ph7_class **apInterface;` |
|       - |  8145 | `		sxu32 n;` |
|   40352 |  8146 | `		if( pBase ){` |
|       - |  8147 | `			/* Inherit from base class and mark as a subclass */` |
|   25660 |  8148 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   12829 |  8149 | `		}` |
|   40352 |  8150 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   43228 |  8151 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8152 | `			/* Implements one or more interface */` |
|    2878 |  8153 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2878 |  8154 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8155 | `				break;` |
|       - |  8156 | `			}` |
|    1440 |  8157 | `		}` |
|       - |  8158 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   40352 |  8159 | `		if( rc == SXRET_OK ){` |
|   40352 |  8160 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   40352 |  8161 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8162 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8163 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8164 | `				return SXERR_ABORT;` |
|       - |  8165 | `			}` |
|   20175 |  8166 | `		}` |
|       - |  8167 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   40352 |  8168 | `		if( rc == SXRET_OK ){` |
|   40352 |  8169 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   40352 |  8170 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8171 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8172 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8173 | `				return SXERR_ABORT;` |
|       - |  8174 | `			}` |
|   20175 |  8175 | `		}` |
|   20175 |  8176 | `	}` |
|   40352 |  8177 | `	SySetRelease(&aUseEntries);` |
|   40352 |  8178 | `	SySetRelease(&aInterfaces);` |
|   40352 |  8179 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8180 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8181 | `		return SXERR_ABORT;` |
|       - |  8182 | `	}` |
|   20175 |  8183 | `done:` |
|       - |  8184 | `	/* Point beyond the class body */` |
|   40356 |  8185 | `	pGen->pIn = &pEnd[1];` |
|   40356 |  8186 | `	pGen->pEnd = pTmp;` |
|   40356 |  8187 | `	return PH7_OK;` |
|   20179 |  8188 |  |
|       - |  8189 | `/*` |
|       - |  8190 | ` * Compile a user-defined abstract class.` |
|       - |  8191 | ` *  According to the PHP language reference manual` |
|       - |  8192 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8193 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8194 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8195 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8196 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8197 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8198 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8199 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8200 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8201 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8202 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8203 | ` *   could differ.` |
|       - |  8204 | ` */` |
|      16 |  8205 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8206 |  |
|       - |  8207 | `	sxi32 rc;` |
|      18 |  8208 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 |  8209 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 |  8210 | `	return rc;` |
|       2 |  8211 |  |
|       - |  8212 | `/*` |
|       - |  8213 | ` * Compile a user-defined final class.` |
|       - |  8214 | ` *  According to the PHP language reference manual` |
|       - |  8215 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8216 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8217 | ` *    final then it cannot be extended.` |
|       - |  8218 | ` */` |
|       2 |  8219 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8220 |  |
|       - |  8221 | `	sxi32 rc;` |
|       3 |  8222 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8223 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8224 | `	return rc;` |
|       1 |  8225 |  |
|       - |  8226 | `/*` |
|       - |  8227 | ` * Compile a user-defined trait.` |
|       - |  8228 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8229 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8230 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8231 | ` */` |
|      54 |  8232 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8233 |  |
|      56 |  8234 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8235 | `	ph7_class *pClass;` |
|       - |  8236 | `	SyToken *pEnd,*pTmp;` |
|       - |  8237 | `	sxi32 iProtection;` |
|       - |  8238 | `	sxi32 iAttrflags;` |
|       - |  8239 | `	SyString *pName;` |
|       - |  8240 | `	sxi32 nKwrd;` |
|       - |  8241 | `	sxi32 rc;` |
|       - |  8242 | `	/* Jump the 'trait' keyword */` |
|      56 |  8243 | `	pGen->pIn++;` |
|      56 |  8244 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8245 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8246 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8247 | `			return SXERR_ABORT;` |
|       - |  8248 | `		}` |
|     ! 0 |  8249 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8250 | `			pGen->pIn++;` |
|     ! 0 |  8251 | `		}` |
|     ! 0 |  8252 | `		return SXRET_OK;` |
|       - |  8253 | `	}` |
|       - |  8254 | `	/* Extract trait name */` |
|      56 |  8255 | `	pName = &pGen->pIn->sData;` |
|      56 |  8256 | `	pGen->pIn++;` |
|       - |  8257 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8258 | `		SyBlob sFQN;` |
|       - |  8259 | `		SyString sFQNStr;` |
|      56 |  8260 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8261 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8262 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8263 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8264 | `		SyBlobRelease(&sFQN);` |
|       - |  8265 | `	}` |
|      56 |  8266 | `	if( pClass == 0 ){` |
|     ! 0 |  8267 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8268 | `		return SXERR_ABORT;` |
|       - |  8269 | `	}` |
|       - |  8270 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8271 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8273 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8274 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8275 | `			return SXERR_ABORT;` |
|       - |  8276 | `		}` |
|     ! 0 |  8277 | `		return SXRET_OK;` |
|       - |  8278 | `	}` |
|      56 |  8279 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8280 | `	pEnd = 0;` |
|      56 |  8281 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8282 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8283 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8284 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8285 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8286 | `			return SXERR_ABORT;` |
|       - |  8287 | `		}` |
|     ! 0 |  8288 | `		return SXRET_OK;` |
|       - |  8289 | `	}` |
|       - |  8290 | `	/* Swap token stream */` |
|      56 |  8291 | `	pTmp = pGen->pEnd;` |
|      56 |  8292 | `	pGen->pEnd = pEnd;` |
|       - |  8293 | `	/* Mark as trait */` |
|      56 |  8294 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8295 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8296 | `	for(;;){` |
|     154 |  8297 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8298 | `			pGen->pIn++;` |
|       2 |  8299 | `		}` |
|     130 |  8300 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8301 | `			break;` |
|       - |  8302 | `		}` |
|      76 |  8303 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8304 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8305 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8306 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8307 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8308 | `				return SXERR_ABORT;` |
|       - |  8309 | `			}` |
|     ! 0 |  8310 | `			goto done;` |
|       - |  8311 | `		}` |
|      76 |  8312 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8313 | `		iAttrflags = 0;` |
|      76 |  8314 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8315 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8316 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8317 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8318 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8319 | `				for(;;){` |
|       - |  8320 | `					ph7_class *pUsedTrait;` |
|       - |  8321 | `					SyString *pUsedName;` |
|       5 |  8322 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8323 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8324 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8325 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8326 | `							return SXERR_ABORT;` |
|       - |  8327 | `						}` |
|     ! 0 |  8328 | `						break;` |
|       - |  8329 | `					}` |
|       5 |  8330 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8331 | `					{` |
|       - |  8332 | `						SyBlob sResolved;` |
|       5 |  8333 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8334 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8335 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8336 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8337 | `						SyBlobRelease(&sResolved);` |
|       - |  8338 | `					}` |
|       5 |  8339 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8340 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8341 | `					}` |
|       5 |  8342 | `					if( pUsedTrait == 0 ){` |
|       4 |  8343 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8344 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8345 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8346 | `							return SXERR_ABORT;` |
|       - |  8347 | `						}` |
|       2 |  8348 | `					}else{` |
|       3 |  8349 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8350 | `					}` |
|       5 |  8351 | `					pGen->pIn++;` |
|       5 |  8352 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8353 | `						break;` |
|       - |  8354 | `					}` |
|     ! 0 |  8355 | `					pGen->pIn++;` |
|     ! 0 |  8356 | `				}` |
|       5 |  8357 | `				continue;` |
|       - |  8358 | `			}` |
|      72 |  8359 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8360 | `				iProtection = nKwrd;` |
|      68 |  8361 | `				pGen->pIn++;` |
|      66 |  8362 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8363 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8364 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8365 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8366 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8367 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8368 | `						return SXERR_ABORT;` |
|       - |  8369 | `					}` |
|     ! 0 |  8370 | `					goto done;` |
|       - |  8371 | `				}` |
|      68 |  8372 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8373 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8374 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8375 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8376 | `							return SXERR_ABORT;` |
|       - |  8377 | `						}` |
|     ! 0 |  8378 | `						goto done;` |
|       - |  8379 | `					}` |
|      11 |  8380 | `					continue;` |
|       - |  8381 | `				}` |
|      58 |  8382 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8383 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8384 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8385 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8386 | `							return SXERR_ABORT;` |
|       - |  8387 | `						}` |
|     ! 0 |  8388 | `						goto done;` |
|       - |  8389 | `					}` |
|       5 |  8390 | `					continue;` |
|       - |  8391 | `				}` |
|      53 |  8392 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8393 | `			}` |
|      57 |  8394 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8395 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8396 | `					"Traits cannot have constants");` |
|     ! 0 |  8397 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8398 | `					return SXERR_ABORT;` |
|       - |  8399 | `				}` |
|     ! 0 |  8400 | `				goto done;` |
|     ! 0 |  8401 | `			}else{` |
|      57 |  8402 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8403 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8404 | `					pGen->pIn++;` |
|       5 |  8405 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8406 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8407 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8408 | `							iProtection = nKwrd;` |
|     ! 0 |  8409 | `							pGen->pIn++;` |
|     ! 0 |  8410 | `						}` |
|       1 |  8411 | `					}` |
|       4 |  8412 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8413 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8414 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8415 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8416 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8417 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8418 | `							return SXERR_ABORT;` |
|       - |  8419 | `						}` |
|     ! 0 |  8420 | `						goto done;` |
|       - |  8421 | `					}` |
|       5 |  8422 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8423 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8424 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8425 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8426 | `								return SXERR_ABORT;` |
|       - |  8427 | `							}` |
|     ! 0 |  8428 | `							goto done;` |
|       - |  8429 | `						}` |
|       3 |  8430 | `						continue;` |
|       - |  8431 | `					}` |
|       3 |  8432 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8433 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8434 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8435 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8436 | `								return SXERR_ABORT;` |
|       - |  8437 | `							}` |
|     ! 0 |  8438 | `							goto done;` |
|       - |  8439 | `						}` |
|     ! 0 |  8440 | `						continue;` |
|       - |  8441 | `					}` |
|       3 |  8442 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  8443 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  8444 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  8445 | `					pGen->pIn++;` |
|       5 |  8446 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  8447 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8448 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8449 | `							iProtection = nKwrd;` |
|       5 |  8450 | `							pGen->pIn++;` |
|       2 |  8451 | `						}` |
|       2 |  8452 | `					}` |
|       5 |  8453 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8454 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8455 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8456 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  8457 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8458 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8459 | `							return SXERR_ABORT;` |
|       - |  8460 | `						}` |
|     ! 0 |  8461 | `						goto done;` |
|       - |  8462 | `					}` |
|       5 |  8463 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8464 | `				}` |
|      55 |  8465 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8466 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8467 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  8468 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8469 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8470 | `						return SXERR_ABORT;` |
|       - |  8471 | `					}` |
|     ! 0 |  8472 | `					goto done;` |
|       - |  8473 | `				}` |
|      55 |  8474 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  8475 | `					pGen->pIn++;` |
|     ! 0 |  8476 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8477 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8478 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8479 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8480 | `							return SXERR_ABORT;` |
|       - |  8481 | `						}` |
|     ! 0 |  8482 | `						goto done;` |
|       - |  8483 | `					}` |
|     ! 0 |  8484 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8485 | `				}else{` |
|      55 |  8486 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8487 | `				}` |
|      55 |  8488 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8489 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8490 | `						return SXERR_ABORT;` |
|       - |  8491 | `					}` |
|     ! 0 |  8492 | `					goto done;` |
|       - |  8493 | `				}` |
|       - |  8494 | `			}` |
|      28 |  8495 | `		}else{` |
|     ! 0 |  8496 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8497 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8498 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8499 | `					return SXERR_ABORT;` |
|       - |  8500 | `				}` |
|     ! 0 |  8501 | `				goto done;` |
|       - |  8502 | `			}` |
|       - |  8503 | `		}` |
|       1 |  8504 | `	}` |
|       - |  8505 | `	/* Install the trait */` |
|      56 |  8506 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  8507 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8508 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8509 | `		return SXERR_ABORT;` |
|       - |  8510 | `	}` |
|      27 |  8511 | `done:` |
|       - |  8512 | `	/* Point beyond the trait body */` |
|      56 |  8513 | `	pGen->pIn = &pEnd[1];` |
|      56 |  8514 | `	pGen->pEnd = pTmp;` |
|      56 |  8515 | `	return PH7_OK;` |
|      29 |  8516 |  |
|       - |  8517 | `/*` |
|       - |  8518 | ` * Compile a user-defined class.` |
|       - |  8519 | ` *  According to the PHP language reference manual` |
|       - |  8520 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  8521 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  8522 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  8523 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  8524 | ` *   and functions (called "methods").` |
|       - |  8525 | ` */` |
|   40336 |  8526 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  8527 |  |
|       - |  8528 | `	sxi32 rc;` |
|   40338 |  8529 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   40338 |  8530 | `	return rc;` |
|       2 |  8531 |  |
|       - |  8532 | `/*` |
|       - |  8533 | ` * Exception handling.` |
|       - |  8534 | ` *  According to the PHP language reference manual` |
|       - |  8535 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  8536 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  8537 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  8538 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  8539 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  8540 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  8541 | ` *    (or re-thrown) within a catch block.` |
|       - |  8542 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  8543 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  8544 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  8545 | ` *    been defined with set_exception_handler().` |
|       - |  8546 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  8547 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  8548 | ` */` |
|       - |  8549 | `/*` |
|       - |  8550 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  8551 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  8552 | ` * indicates failure.` |
|       - |  8553 | ` */` |
|    8574 |  8554 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  8555 |  |
|    8576 |  8556 | `	sxi32 rc = SXRET_OK;` |
|    8576 |  8557 | `	if( pRoot->pOp ){` |
|    8570 |  8558 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4287 |  8559 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  8560 | `			/* Unexpected expression */` |
|     ! 0 |  8561 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8562 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  8563 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  8564 | `				rc = SXERR_INVALID;` |
|     ! 0 |  8565 | `			}` |
|       2 |  8566 | `		}` |
|    4290 |  8567 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  8568 | `		/* Unexpected expression */` |
|     ! 0 |  8569 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8570 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  8571 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  8572 | `			rc = SXERR_INVALID;` |
|     ! 0 |  8573 | `		}` |
|     ! 0 |  8574 | `	}` |
|    8576 |  8575 | `	return rc;` |
|       2 |  8576 |  |
|       - |  8577 | `/*` |
|       - |  8578 | ` * Compile a 'throw' statement.` |
|       - |  8579 | ` * throw: This is how you trigger an exception.` |
|       - |  8580 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  8581 | ` */` |
|    8574 |  8582 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  8583 |  |
|    8576 |  8584 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8585 | `	GenBlock *pBlock;` |
|       - |  8586 | `	sxu32 nIdx;` |
|       - |  8587 | `	sxi32 rc;` |
|    8576 |  8588 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  8589 | `	/* Compile the expression */` |
|    8576 |  8590 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8576 |  8591 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8592 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  8593 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8594 | `			return SXERR_ABORT;` |
|       - |  8595 | `		}` |
|     ! 0 |  8596 | `		return SXRET_OK;` |
|       - |  8597 | `	}` |
|    8576 |  8598 | `	pBlock = pGen->pCurrent;` |
|       - |  8599 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   39854 |  8600 | `	while(pBlock->pParent){` |
|   39850 |  8601 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8572 |  8602 | `			break;` |
|       - |  8603 | `		}` |
|       - |  8604 | `		/* Point to the parent block */` |
|   31280 |  8605 | `		pBlock = pBlock->pParent;` |
|       2 |  8606 | `	}` |
|       - |  8607 | `	/* Emit the throw instruction */` |
|    8576 |  8608 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  8609 | `	/* Emit the jump */` |
|    8576 |  8610 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8576 |  8611 | `	return SXRET_OK;` |
|    4289 |  8612 |  |
|       - |  8613 | `/*` |
|       - |  8614 | ` * Compile a 'catch' block.` |
|       - |  8615 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  8616 | ` * an object containing the exception information.` |
|       - |  8617 | ` */` |
|     158 |  8618 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  8619 |  |
|     160 |  8620 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8621 | `	ph7_exception_block sCatch;` |
|       - |  8622 | `	SySet *pInstrContainer;` |
|       - |  8623 | `	SyString sClassName;` |
|       - |  8624 | `	GenBlock *pCatch;` |
|       - |  8625 | `	SyToken *pToken;` |
|       - |  8626 | `	SyString *pName;` |
|       - |  8627 | `	char *zDup;` |
|       - |  8628 | `	sxi32 rc;` |
|     160 |  8629 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  8630 | `	/* Zero the structure */` |
|     160 |  8631 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  8632 | `	/* Initialize fields */` |
|     160 |  8633 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     160 |  8634 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     160 |  8635 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  8636 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  8637 | `			pToken = pGen->pIn;` |
|     ! 0 |  8638 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8639 | `				pToken--;` |
|     ! 0 |  8640 | `			}` |
|     ! 0 |  8641 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8642 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8643 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8644 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8645 | `				return SXERR_ABORT;` |
|       - |  8646 | `			}` |
|     ! 0 |  8647 | `			return SXERR_INVALID;` |
|       - |  8648 | `	}` |
|       - |  8649 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     160 |  8650 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      91 |  8651 | `	for(;;){` |
|     184 |  8652 | `		int isAbsolute = 0;` |
|       - |  8653 | `		SyBlob sName;` |
|     184 |  8654 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - |  8655 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     184 |  8656 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 |  8657 | `			isAbsolute = 1;` |
|       7 |  8658 | `			pGen->pIn++;` |
|       3 |  8659 | `		}` |
|     184 |  8660 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     182 |  8661 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 |  8662 | `			SyBlobRelease(&sName);` |
|       5 |  8663 | `			pToken = pGen->pIn;` |
|       5 |  8664 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8665 | `				pToken--;` |
|     ! 0 |  8666 | `			}` |
|       7 |  8667 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8668 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  8669 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  8670 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8671 | `				return SXERR_ABORT;` |
|       - |  8672 | `			}` |
|       5 |  8673 | `			return SXERR_INVALID;` |
|       - |  8674 | `		}` |
|       - |  8675 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     180 |  8676 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     180 |  8677 | `		pGen->pIn++;` |
|     273 |  8678 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      97 |  8679 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 |  8680 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 |  8681 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 |  8682 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 |  8683 | `			pGen->pIn++;` |
|       1 |  8684 | `		}` |
|       - |  8685 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     180 |  8686 | `		if( !isAbsolute ){` |
|       - |  8687 | `			SyString sRaw;` |
|       - |  8688 | `			SyBlob sResolved;` |
|     174 |  8689 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     174 |  8690 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     174 |  8691 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     260 |  8692 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     172 |  8693 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     174 |  8694 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     174 |  8695 | `			SyBlobRelease(&sResolved);` |
|      88 |  8696 | `		}else{` |
|       - |  8697 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 |  8698 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 |  8699 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 |  8700 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - |  8701 | `		}` |
|     180 |  8702 | `		SyBlobRelease(&sName);` |
|     180 |  8703 | `		if( zDup == 0 ){` |
|     ! 0 |  8704 | `			goto Mem;` |
|       - |  8705 | `		}` |
|     180 |  8706 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     180 |  8707 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  8708 | `			goto Mem;` |
|       - |  8709 | `		}` |
|       - |  8710 | `		/* Check for '\|' (multi-catch separator) */` |
|     190 |  8711 | `		if( pGen->pIn < pGen->pEnd &&` |
|     178 |  8712 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 |  8713 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 |  8714 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 |  8715 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 |  8716 | `			continue;` |
|       - |  8717 | `		}` |
|     156 |  8718 | `		break;` |
|     ! 0 |  8719 | `	}` |
|     231 |  8720 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     156 |  8721 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8722 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  8723 | `			pToken = pGen->pIn;` |
|     ! 0 |  8724 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8725 | `				pToken--;` |
|     ! 0 |  8726 | `			}` |
|     ! 0 |  8727 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8728 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8729 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8730 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8731 | `				return SXERR_ABORT;` |
|       - |  8732 | `			}` |
|     ! 0 |  8733 | `			return SXERR_INVALID;` |
|       - |  8734 | `	}` |
|     156 |  8735 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  8736 | `	/* Duplicate instance name */` |
|     156 |  8737 | `	pName = &pGen->pIn->sData;` |
|     156 |  8738 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     156 |  8739 | `	if( zDup == 0 ){` |
|     ! 0 |  8740 | `		goto Mem;` |
|       - |  8741 | `	}` |
|     156 |  8742 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     156 |  8743 | `	pGen->pIn++;` |
|     156 |  8744 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  8745 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  8746 | `		pToken = pGen->pIn;` |
|     ! 0 |  8747 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8748 | `			pToken--;` |
|     ! 0 |  8749 | `		}` |
|     ! 0 |  8750 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8751 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8752 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8753 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8754 | `			return SXERR_ABORT;` |
|       - |  8755 | `		}` |
|     ! 0 |  8756 | `		return SXERR_INVALID;` |
|       - |  8757 | `	}` |
|       - |  8758 | `	/* Compile the block */` |
|     156 |  8759 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  8760 | `	/* Create the catch block */` |
|     156 |  8761 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     156 |  8762 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8763 | `		return SXERR_ABORT;` |
|       - |  8764 | `	}` |
|       - |  8765 | `	/* Swap bytecode container */` |
|     156 |  8766 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     156 |  8767 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  8768 | `	/* Compile the block */` |
|     156 |  8769 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  8770 | `	/* Fix forward jumps now the destination is resolved  */` |
|     156 |  8771 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  8772 | `	/* Emit the DONE instruction */` |
|     156 |  8773 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  8774 | `	/* Leave the block */` |
|     156 |  8775 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  8776 | `	/* Restore the default container */` |
|     156 |  8777 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  8778 | `	/* Install the catch block */` |
|     156 |  8779 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     156 |  8780 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8781 | `		goto Mem;` |
|       - |  8782 | `	}` |
|     156 |  8783 | `	return SXRET_OK;` |
|     ! 0 |  8784 | `Mem:` |
|     ! 0 |  8785 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8786 | `	return SXERR_ABORT;` |
|      81 |  8787 |  |
|       - |  8788 | `/*` |
|       - |  8789 | ` * Compile a 'try' block.` |
|       - |  8790 | ` * A function using an exception should be in a "try" block.` |
|       - |  8791 | ` * If the exception does not trigger, the code will continue` |
|       - |  8792 | ` * as normal. However if the exception triggers, an exception` |
|       - |  8793 | ` * is "thrown".` |
|       - |  8794 | ` */` |
|     166 |  8795 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  8796 |  |
|       - |  8797 | `	ph7_exception *pException;` |
|     168 |  8798 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8799 | `	GenBlock *pTry;` |
|       - |  8800 | `	sxu32 nJmpIdx;` |
|       - |  8801 | `	sxi32 rc;` |
|       - |  8802 | `	/* Create the exception container */` |
|     168 |  8803 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     168 |  8804 | `	if( pException == 0 ){` |
|     ! 0 |  8805 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  8806 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8807 | `		return SXERR_ABORT;` |
|       - |  8808 | `	}` |
|       - |  8809 | `	/* Zero the structure */` |
|     168 |  8810 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  8811 | `	/* Initialize fields */` |
|     168 |  8812 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     168 |  8813 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     168 |  8814 | `	pException->iHasFinally = 0;` |
|     168 |  8815 | `	pException->iFinallyDone = 0;` |
|     168 |  8816 | `	pException->pVm = pGen->pVm;` |
|       - |  8817 | `	/* Create the try block */` |
|     168 |  8818 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     168 |  8819 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8820 | `		return SXERR_ABORT;` |
|       - |  8821 | `	}` |
|       - |  8822 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     168 |  8823 | `	pTry->pUserData = pException;` |
|       - |  8824 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     168 |  8825 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  8826 | `	/* Fix the jump later when the destination is resolved */` |
|     168 |  8827 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     168 |  8828 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  8829 | `	/* Compile the block */` |
|     168 |  8830 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     168 |  8831 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8832 | `		return SXERR_ABORT;` |
|       - |  8833 | `	}` |
|       - |  8834 | `	/* Fix forward jumps now the destination is resolved */` |
|     168 |  8835 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  8836 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     168 |  8837 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  8838 | `	/* Leave the block */` |
|     168 |  8839 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  8840 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     168 |  8841 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     164 |  8842 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  8843 | `		/* Compile one or more catch blocks */` |
|     156 |  8844 | `		for(;;){` |
|     312 |  8845 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     244 |  8846 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      79 |  8847 | `					break;` |
|       - |  8848 | `			}` |
|     160 |  8849 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     160 |  8850 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8851 | `				return SXERR_ABORT;` |
|       - |  8852 | `			}` |
|       2 |  8853 | `		}` |
|      77 |  8854 | `	}` |
|       - |  8855 | `	/* Compile optional finally block */` |
|     168 |  8856 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      82 |  8857 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  8858 | `		SySet *pInstrContainer;` |
|       - |  8859 | `		GenBlock *pFinBlock;` |
|      32 |  8860 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  8861 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  8862 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  8863 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  8864 | `			return SXERR_ABORT;` |
|       - |  8865 | `		}` |
|       - |  8866 | `		/* Swap bytecode container */` |
|      32 |  8867 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  8868 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  8869 | `		/* Compile the finally body */` |
|      32 |  8870 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  8871 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8872 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  8873 | `			return SXERR_ABORT;` |
|       - |  8874 | `		}` |
|       - |  8875 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  8876 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  8877 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  8878 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  8879 | `		/* Leave the block */` |
|      32 |  8880 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  8881 | `		/* Restore the default container */` |
|      32 |  8882 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  8883 | `		pException->iHasFinally = 1;` |
|      15 |  8884 | `	}` |
|       - |  8885 | `	/* Must have at least one catch or finally */` |
|     168 |  8886 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  8887 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  8888 | `			"Cannot use try without catch or finally");` |
|       7 |  8889 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8890 | `			return SXERR_ABORT;` |
|       - |  8891 | `		}` |
|       3 |  8892 | `	}` |
|     168 |  8893 | `	return SXRET_OK;` |
|      85 |  8894 |  |
|       - |  8895 | `/*` |
|       - |  8896 | ` * Compile a switch block.` |
|       - |  8897 | ` *  (See block-comment below for more information)` |
|       - |  8898 | ` */` |
|     108 |  8899 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  8900 |  |
|     110 |  8901 | `	sxi32 rc = SXRET_OK;` |
|     110 |  8902 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  8903 | `		/* Unexpected token */` |
|     ! 0 |  8904 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  8905 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8906 | `			return SXERR_ABORT;` |
|       - |  8907 | `		}` |
|     ! 0 |  8908 | `		pGen->pIn++;` |
|     ! 0 |  8909 | `	}` |
|     110 |  8910 | `	pGen->pIn++;` |
|       - |  8911 | `	/* First instruction to execute in this block. */` |
|     110 |  8912 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  8913 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  8914 | `	 * or the '}' token */` |
|     182 |  8915 | `	for(;;){` |
|     366 |  8916 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8917 | `			/* No more input to process */` |
|     ! 0 |  8918 | `			break;` |
|       - |  8919 | `		}` |
|     366 |  8920 | `		rc = SXRET_OK;` |
|     366 |  8921 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  8922 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  8923 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  8924 | `					/* Unexpected token */` |
|     ! 0 |  8925 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  8926 | `						&pGen->pIn->sData);` |
|     ! 0 |  8927 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8928 | `						return SXERR_ABORT;` |
|       - |  8929 | `					}` |
|       - |  8930 | `					/* FALL THROUGH */` |
|     ! 0 |  8931 | `				}` |
|      28 |  8932 | `				rc = SXERR_EOF;` |
|      28 |  8933 | `				break;` |
|       - |  8934 | `			}` |
|      23 |  8935 | `		}else{` |
|       - |  8936 | `			sxi32 nKwrd;` |
|       - |  8937 | `			/* Extract the keyword */` |
|     298 |  8938 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  8939 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  8940 | `				break;` |
|       - |  8941 | `			}` |
|     218 |  8942 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  8943 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  8944 | `					/* Unexpected token */` |
|     ! 0 |  8945 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  8946 | `						&pGen->pIn->sData);` |
|     ! 0 |  8947 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8948 | `						return SXERR_ABORT;` |
|       - |  8949 | `					}` |
|       - |  8950 | `					/* FALL THROUGH */` |
|     ! 0 |  8951 | `				}` |
|       - |  8952 | `				/* Block compiled */` |
|       3 |  8953 | `				break;` |
|       - |  8954 | `			}` |
|       - |  8955 | `		}` |
|       - |  8956 | `		/* Compile block */` |
|     258 |  8957 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  8958 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8959 | `			return SXERR_ABORT;` |
|       - |  8960 | `		}` |
|       2 |  8961 | `	}` |
|     110 |  8962 | `	return rc;` |
|      56 |  8963 |  |
|       - |  8964 | `/*` |
|       - |  8965 | ` * Compile a case eXpression.` |
|       - |  8966 | ` *  (See block-comment below for more information)` |
|       - |  8967 | ` */` |
|      88 |  8968 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  8969 |  |
|       - |  8970 | `	SySet *pInstrContainer;` |
|       - |  8971 | `	SyToken *pEnd,*pTmp;` |
|      90 |  8972 | `	sxi32 iNest = 0;` |
|       - |  8973 | `	sxi32 rc;` |
|       - |  8974 | `	/* Delimit the expression */` |
|      90 |  8975 | `	pEnd = pGen->pIn;` |
|     186 |  8976 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  8977 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  8978 | `			/* Increment nesting level */` |
|       3 |  8979 | `			iNest++;` |
|     185 |  8980 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  8981 | `			/* Decrement nesting level */` |
|       3 |  8982 | `			iNest--;` |
|     183 |  8983 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  8984 | `			break;` |
|       - |  8985 | `		}` |
|      98 |  8986 | `		pEnd++;` |
|       2 |  8987 | `	}` |
|      90 |  8988 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  8989 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  8990 | `		if( rc == SXERR_ABORT ){` |
|       - |  8991 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8992 | `			return SXERR_ABORT;` |
|       - |  8993 | `		}` |
|     ! 0 |  8994 | `	}` |
|       - |  8995 | `	/* Swap token stream */` |
|      90 |  8996 | `	pTmp = pGen->pEnd;` |
|      90 |  8997 | `	pGen->pEnd = pEnd;` |
|      90 |  8998 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  8999 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9000 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9001 | `	/* Emit the done instruction */` |
|      90 |  9002 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9003 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9004 | `	/* Update token stream */` |
|      90 |  9005 | `	pGen->pIn  = pEnd;` |
|      90 |  9006 | `	pGen->pEnd = pTmp;` |
|      90 |  9007 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9008 | `		return SXERR_ABORT;` |
|       - |  9009 | `	}` |
|      90 |  9010 | `	return SXRET_OK;` |
|      46 |  9011 |  |
|       - |  9012 | `/*` |
|       - |  9013 | ` * Compile the smart switch statement.` |
|       - |  9014 | ` * According to the PHP language reference manual` |
|       - |  9015 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9016 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9017 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9018 | ` *  This is exactly what the switch statement is for.` |
|       - |  9019 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9020 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9021 | ` *  of the outer loop, use continue 2.` |
|       - |  9022 | ` *  Note that switch/case does loose comparision.` |
|       - |  9023 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9024 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9025 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9026 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9027 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9028 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9029 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9030 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9031 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9032 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9033 | ` *  list for the next case.` |
|       - |  9034 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9035 | ` *  or floating-point numbers and strings.` |
|       - |  9036 | ` */` |
|      28 |  9037 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9038 |  |
|       - |  9039 | `	GenBlock *pSwitchBlock;` |
|       - |  9040 | `	SyToken *pTmp,*pEnd;` |
|       - |  9041 | `	ph7_switch *pSwitch;` |
|       - |  9042 | `	sxu32 nToken;` |
|       - |  9043 | `	sxu32 nLine;` |
|       - |  9044 | `	sxi32 rc;` |
|      30 |  9045 | `	nLine = pGen->pIn->nLine;` |
|       - |  9046 | `	/* Jump the 'switch' keyword */` |
|      30 |  9047 | `	pGen->pIn++;` |
|      30 |  9048 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9049 | `		/* Syntax error */` |
|     ! 0 |  9050 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9051 | `		if( rc == SXERR_ABORT ){` |
|       - |  9052 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9053 | `			return SXERR_ABORT;` |
|       - |  9054 | `		}` |
|     ! 0 |  9055 | `		goto Synchronize;` |
|       - |  9056 | `	}` |
|       - |  9057 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9058 | `	pGen->pIn++;` |
|      30 |  9059 | `	pEnd = 0; /* cc warning */` |
|       - |  9060 | `	/* Create the loop block */` |
|      44 |  9061 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9062 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9063 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9064 | `		return SXERR_ABORT;` |
|       - |  9065 | `	}` |
|       - |  9066 | `	/* Delimit the condition */` |
|      30 |  9067 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9068 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9069 | `		/* Empty expression */` |
|     ! 0 |  9070 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9071 | `		if( rc == SXERR_ABORT ){` |
|       - |  9072 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9073 | `			return SXERR_ABORT;` |
|       - |  9074 | `		}` |
|     ! 0 |  9075 | `	}` |
|       - |  9076 | `	/* Swap token streams */` |
|      30 |  9077 | `	pTmp = pGen->pEnd;` |
|      30 |  9078 | `	pGen->pEnd = pEnd;` |
|       - |  9079 | `	/* Compile the expression */` |
|      30 |  9080 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9081 | `	if( rc == SXERR_ABORT ){` |
|       - |  9082 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9083 | `		return SXERR_ABORT;` |
|       - |  9084 | `	}` |
|       - |  9085 | `	/* Update token stream */` |
|      30 |  9086 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9087 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9088 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9089 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9090 | `			return SXERR_ABORT;` |
|       - |  9091 | `		}` |
|     ! 0 |  9092 | `		pGen->pIn++;` |
|     ! 0 |  9093 | `	}` |
|      30 |  9094 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9095 | `	pGen->pEnd = pTmp;` |
|      30 |  9096 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9097 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9098 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9099 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9100 | `				pTmp--;` |
|     ! 0 |  9101 | `			}` |
|       - |  9102 | `			/* Unexpected token */` |
|     ! 0 |  9103 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9104 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9105 | `				return SXERR_ABORT;` |
|       - |  9106 | `			}` |
|     ! 0 |  9107 | `			goto Synchronize;` |
|       - |  9108 | `	}` |
|       - |  9109 | `	/* Set the delimiter token */` |
|      30 |  9110 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9111 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9112 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9113 | `	}else{` |
|      28 |  9114 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9115 | `	}` |
|      30 |  9116 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9117 | `	/* Create the switch blocks container */` |
|      30 |  9118 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9119 | `	if( pSwitch == 0 ){` |
|       - |  9120 | `		/* Abort compilation */` |
|     ! 0 |  9121 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9122 | `		return SXERR_ABORT;` |
|       - |  9123 | `	}` |
|       - |  9124 | `	/* Zero the structure */` |
|      30 |  9125 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9126 | `	/* Initialize fields */` |
|      30 |  9127 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9128 | `	/* Emit the switch instruction */` |
|      30 |  9129 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9130 | `	/* Compile case blocks */` |
|      96 |  9131 | `	for(;;){` |
|       - |  9132 | `		sxu32 nKwrd;` |
|     112 |  9133 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9134 | `			/* No more input to process */` |
|     ! 0 |  9135 | `			break;` |
|       - |  9136 | `		}` |
|     112 |  9137 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9138 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9139 | `				/* Unexpected token */` |
|     ! 0 |  9140 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9141 | `					&pGen->pIn->sData);` |
|     ! 0 |  9142 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9143 | `					return SXERR_ABORT;` |
|       - |  9144 | `				}` |
|       - |  9145 | `				/* FALL THROUGH */` |
|     ! 0 |  9146 | `			}` |
|       - |  9147 | `			/* Block compiled */` |
|     ! 0 |  9148 | `			break;` |
|       - |  9149 | `		}` |
|       - |  9150 | `		/* Extract the keyword */` |
|     112 |  9151 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9152 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9153 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9154 | `				/* Unexpected token */` |
|     ! 0 |  9155 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9156 | `					&pGen->pIn->sData);` |
|     ! 0 |  9157 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9158 | `					return SXERR_ABORT;` |
|       - |  9159 | `				}` |
|       - |  9160 | `				/* FALL THROUGH */` |
|     ! 0 |  9161 | `			}` |
|       - |  9162 | `			/* Block compiled */` |
|       3 |  9163 | `			break;` |
|       - |  9164 | `		}` |
|     110 |  9165 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9166 | `			/*` |
|       - |  9167 | `			 * Accroding to the PHP language reference manual` |
|       - |  9168 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9169 | `			 *  that wasn't matched by the other cases.` |
|       - |  9170 | `			 */` |
|      22 |  9171 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9172 | `				/* Default case already compiled */` |
|     ! 0 |  9173 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9174 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9175 | `					return SXERR_ABORT;` |
|       - |  9176 | `				}` |
|     ! 0 |  9177 | `			}` |
|      22 |  9178 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9179 | `			/* Compile the default block */` |
|      22 |  9180 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9181 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9182 | `				return SXERR_ABORT;` |
|      22 |  9183 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9184 | `				break;` |
|       1 |  9185 | `			}` |
|      91 |  9186 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9187 | `			ph7_case_expr sCase;` |
|       - |  9188 | `			/* Standard case block */` |
|      90 |  9189 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9190 | `			/* initialize the structure */` |
|      90 |  9191 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9192 | `			/* Compile the case expression */` |
|      90 |  9193 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9194 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9195 | `				return SXERR_ABORT;` |
|       - |  9196 | `			}` |
|       - |  9197 | `			/* Compile the case block */` |
|      90 |  9198 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9199 | `			/* Insert in the switch container */` |
|      90 |  9200 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9201 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9202 | `				return SXERR_ABORT;` |
|      90 |  9203 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9204 | `				break;` |
|       - |  9205 | `			}` |
|      42 |  9206 | `		}else{` |
|       - |  9207 | `			/* Unexpected token */` |
|     ! 0 |  9208 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9209 | `				&pGen->pIn->sData);` |
|     ! 0 |  9210 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9211 | `				return SXERR_ABORT;` |
|       - |  9212 | `			}` |
|     ! 0 |  9213 | `			break;` |
|       - |  9214 | `		}` |
|       2 |  9215 | `	}` |
|       - |  9216 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9217 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9218 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9219 | `	/* Release the loop block */` |
|      30 |  9220 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9221 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9222 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9223 | `		pGen->pIn++;` |
|      14 |  9224 | `	}` |
|       - |  9225 | `	/* Statement successfully compiled */` |
|      30 |  9226 | `	return SXRET_OK;` |
|     ! 0 |  9227 | `Synchronize:` |
|       - |  9228 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9229 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9230 | `		pGen->pIn++;` |
|     ! 0 |  9231 | `	}` |
|     ! 0 |  9232 | `	return SXRET_OK;` |
|      16 |  9233 |  |
|       - |  9234 | `/*` |
|       - |  9235 | ` * Generate bytecode for a given expression tree.` |
|       - |  9236 | ` * If something goes wrong while generating bytecode` |
|       - |  9237 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9238 | ` * this function takes care of generating the appropriate` |
|       - |  9239 | ` * error message.` |
|       - |  9240 | ` */` |
| 2553480 |  9241 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9242 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9243 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9244 | `	sxi32 iFlags /* Control flags */` |
|       - |  9245 | `	)` |
|       2 |  9246 |  |
|       - |  9247 | `	VmInstr *pInstr;` |
|       - |  9248 | `	sxu32 nJmpIdx;` |
| 2553482 |  9249 | `	sxi32 iP1 = 0;` |
| 2553482 |  9250 | `	sxu32 iP2 = 0;` |
| 2553482 |  9251 | `	void *p3  = 0;` |
|       - |  9252 | `	sxi32 iVmOp;` |
|       - |  9253 | `	sxi32 rc;` |
| 2553482 |  9254 | `	if( pNode->xCode ){` |
|       - |  9255 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9256 | `		/* Compile node */` |
| 1582784 |  9257 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1582784 |  9258 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1582784 |  9259 | `		RE_SWAP_DELIMITER(pGen);` |
| 1582784 |  9260 | `		return rc;` |
|       - |  9261 | `	}` |
|  970700 |  9262 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9263 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9264 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9265 | `		return SXERR_ABORT;` |
|       - |  9266 | `	}` |
|  970700 |  9267 | `	iVmOp = pNode->pOp->iVmOp;` |
|  970700 |  9268 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9269 | `		sxu32 nJmp = 0;` |
|       - |  9270 | `		VmInstr *pInstrFix;` |
|       - |  9271 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9272 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9273 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9274 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9275 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9276 | `		if( pNode->pRight ){` |
|      47 |  9277 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9278 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9279 | `				return rc;` |
|       - |  9280 | `			}` |
|       - |  9281 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9282 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9283 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9284 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9285 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9286 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9287 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9288 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9289 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9290 | `				pInstrFix->iP2 = 3;` |
|       9 |  9291 | `			}` |
|      23 |  9292 | `		}` |
|       - |  9293 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9294 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9295 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9296 | `		if( pNode->pLeft ){` |
|      47 |  9297 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9298 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9299 | `				return rc;` |
|       - |  9300 | `			}` |
|      23 |  9301 | `		}` |
|       - |  9302 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9303 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9304 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9305 | `		if( nJmp > 0 ){` |
|      47 |  9306 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9307 | `			if( pInstrFix ){` |
|      47 |  9308 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9309 | `			}` |
|      23 |  9310 | `		}` |
|      47 |  9311 | `		return SXRET_OK;` |
|       - |  9312 | `	}` |
|  970654 |  9313 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9314 | `		sxu32 nJz,nJmp;` |
|       - |  9315 | `		/* Ternary operator require special handling */` |
|       - |  9316 | `		/* Phase#1: Compile the condition */` |
|    1978 |  9317 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1978 |  9318 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9319 | `			return rc;` |
|       - |  9320 | `		}` |
|    1978 |  9321 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1978 |  9322 | `		if( pNode->pLeft ){` |
|       - |  9323 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9324 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1910 |  9325 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9326 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1910 |  9327 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1910 |  9328 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9329 | `				return rc;` |
|       - |  9330 | `			}` |
|     956 |  9331 | `		}else{` |
|       - |  9332 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9333 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9334 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9335 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9337 | `		}` |
|       - |  9338 | `		/* Phase#4: Emit the unconditional jump */` |
|    1978 |  9339 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9340 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1978 |  9341 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1978 |  9342 | `		if( pInstr ){` |
|    1978 |  9343 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     988 |  9344 | `		}` |
|    1978 |  9345 | `		if( !pNode->pLeft ){` |
|       - |  9346 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9347 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9348 | `		}` |
|       - |  9349 | `		/* Phase#6: Compile the 'else' expression */` |
|    1978 |  9350 | `		if( pNode->pRight ){` |
|    1978 |  9351 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1978 |  9352 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9353 | `				return rc;` |
|       - |  9354 | `			}` |
|     988 |  9355 | `		}` |
|    1978 |  9356 | `		if( nJmp > 0 ){` |
|       - |  9357 | `			/* Phase#7: Fix the unconditional jump */` |
|    1978 |  9358 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1978 |  9359 | `			if( pInstr ){` |
|    1978 |  9360 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     988 |  9361 | `			}` |
|     988 |  9362 | `		}` |
|       - |  9363 | `		/* All done */` |
|    1978 |  9364 | `		return SXRET_OK;` |
|       - |  9365 | `	}` |
|       - |  9366 | `	/* Generate code for the left tree */` |
|  968678 |  9367 | `	if( pNode->pLeft ){` |
|  968642 |  9368 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - |  9369 | `			ph7_expr_node **apNode;` |
|  325108 |  9370 | `			int hasSpread = 0;` |
|  325108 |  9371 | `			int hasNamed = 0;` |
|       - |  9372 | `			sxi32 nArgs;` |
|       - |  9373 | `			sxi32 n;` |
|       - |  9374 | `			/* Recurse and generate bytecodes for function arguments */` |
|  325108 |  9375 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  325108 |  9376 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - |  9377 | `			/* Validate: no positional arguments after named arguments */` |
|       - |  9378 | `			{` |
|  325108 |  9379 | `				int seenNamed = 0;` |
|  649384 |  9380 | `				for( n = 0; n < nArgs; ++n ){` |
|  324280 |  9381 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     176 |  9382 | `						seenNamed = 1;` |
|     176 |  9383 | `						hasNamed = 1;` |
|  324193 |  9384 | `					}else if( seenNamed && !(apNode[n]->iFlags & EXPR_NODE_SPREAD) ){` |
|       3 |  9385 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - |  9386 | `							"Cannot use positional argument after named argument");` |
|       3 |  9387 | `						return SXERR_SYNTAX;` |
|       - |  9388 | `					}` |
|  162140 |  9389 | `				}` |
|       - |  9390 | `			}` |
|       - |  9391 | `			/* Read-only load */` |
|  325106 |  9392 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  649380 |  9393 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  324276 |  9394 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  324276 |  9395 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9396 | `					return rc;` |
|       - |  9397 | `				}` |
|  324276 |  9398 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - |  9399 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 |  9400 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 |  9401 | `					hasSpread = 1;` |
|       9 |  9402 | `				}` |
|  162139 |  9403 | `			}` |
|       - |  9404 | `			/* Total number of given arguments */` |
|  325106 |  9405 | `			iP1 = nArgs;` |
|  325106 |  9406 | `			iP2 = hasSpread;` |
|       - |  9407 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - |  9408 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  325106 |  9409 | `			if( hasNamed ){` |
|      94 |  9410 | `				sxu32 nStrBytes = 0;` |
|       - |  9411 | `				char *zBuf;` |
|     278 |  9412 | `				for( n = 0; n < nArgs; ++n ){` |
|     186 |  9413 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9414 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      86 |  9415 | `					}` |
|      94 |  9416 | `				}` |
|       - |  9417 | `				{` |
|      94 |  9418 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      94 |  9419 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      92 |  9420 | `					&pGen->pVm->sAllocator, mapSize);` |
|      94 |  9421 | `				if( pMap ){` |
|      94 |  9422 | `					SyZero(pMap, mapSize);` |
|      94 |  9423 | `					pMap->bHasNamed = 1;` |
|      94 |  9424 | `					pMap->nTotal = (sxu32)nArgs;` |
|      94 |  9425 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      94 |  9426 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     278 |  9427 | `					for( n = 0; n < nArgs; ++n ){` |
|     186 |  9428 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9429 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     174 |  9430 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     174 |  9431 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     174 |  9432 | `							zBuf += nb;` |
|      86 |  9433 | `						}` |
|       - |  9434 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      94 |  9435 | `					}` |
|      94 |  9436 | `					p3 = (void *)pMap;` |
|      46 |  9437 | `				}` |
|       - |  9438 | `				}` |
|      46 |  9439 | `			}` |
|       - |  9440 | `			/* Remove stale flags now */` |
|  325106 |  9441 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  162552 |  9442 | `		}` |
|  968640 |  9443 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  968640 |  9444 | `		if( rc != SXRET_OK ){` |
|      31 |  9445 | `			return rc;` |
|       - |  9446 | `		}` |
|  968610 |  9447 | `		if( iVmOp == PH7_OP_CALL ){` |
|  325106 |  9448 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  325106 |  9449 | `			if( pInstr ){` |
|  325106 |  9450 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  324318 |  9451 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - |  9452 | `					sxu32 nQual;` |
|       - |  9453 | `					/* Prevent constant expansion */` |
|  324318 |  9454 | `					pInstr->iP1 = 0;` |
|       - |  9455 | `					/* Namespace-qualify the function name for CALL.` |
|       - |  9456 | `					 * Only check function imports — class imports must NOT` |
|       - |  9457 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - |  9458 | `					 * handler fires before NEW; we store the original literal` |
|       - |  9459 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - |  9460 | `					 * can recover the unqualified name and re-qualify with` |
|       - |  9461 | `					 * class imports. */ {` |
|  324318 |  9462 | `						int fromImport = 0;` |
|  324318 |  9463 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  324318 |  9464 | `						pInstr->iP2 = (sxi32)nQual;` |
|  324318 |  9465 | `						if( nQual != nOrig ){` |
|       - |  9466 | `							/* Store original literal index in CALL's iP2 so the` |
|       - |  9467 | `							 * NEW handler can recover the unqualified name. */` |
|      72 |  9468 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      72 |  9469 | `							if( !fromImport ){` |
|       - |  9470 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      62 |  9471 | `								if( p3 == 0 ){` |
|      62 |  9472 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      60 |  9473 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      62 |  9474 | `									if( pMap ){` |
|      62 |  9475 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      62 |  9476 | `										p3 = (void *)pMap;` |
|      30 |  9477 | `									}` |
|      30 |  9478 | `								}` |
|      62 |  9479 | `								if( p3 ){` |
|      62 |  9480 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      30 |  9481 | `								}` |
|      30 |  9482 | `							}` |
|      37 |  9483 | `						}` |
|       - |  9484 | `					}` |
|  162948 |  9485 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - |  9486 | `					/* Method call,flag that */` |
|     638 |  9487 | `					pInstr->iP2 = 1;` |
|     318 |  9488 | `				}` |
|  162554 |  9489 | `			}` |
|  806058 |  9490 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - |  9491 | `			ph7_expr_node **apNode;` |
|       - |  9492 | `			sxi32 n;` |
|       - |  9493 | `			/* Recurse and generate bytecodes for array index */` |
|   72834 |  9494 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  131406 |  9495 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   58574 |  9496 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   58574 |  9497 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9498 | `					return rc;` |
|       - |  9499 | `				}` |
|   29288 |  9500 | `			}` |
|   72834 |  9501 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   58574 |  9502 | `				iP1 = 1; /* Node have an index associated with it */` |
|   29286 |  9503 | `			}` |
|   72834 |  9504 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - |  9505 | `				/* Create an empty entry when the desired index is not found */` |
|   28780 |  9506 | `				iP2 = 1;` |
|   14391 |  9507 | `			}` |
|  607090 |  9508 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - |  9509 | `			/* POP the left node */` |
|      32 |  9510 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 |  9511 | `		}` |
|  484304 |  9512 | `	}` |
|  968646 |  9513 | `	rc = SXRET_OK;` |
|  968646 |  9514 | `	nJmpIdx = 0;` |
|       - |  9515 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - |  9516 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - |  9517 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  968646 |  9518 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     262 |  9519 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     262 |  9520 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     262 |  9521 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     262 |  9522 | `			int isSpecial = 0;` |
|     262 |  9523 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     178 |  9524 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     178 |  9525 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     190 |  9526 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     155 |  9527 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      80 |  9528 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      88 |  9529 | `					isSpecial = 1;` |
|      43 |  9530 | `				}` |
|     109 |  9531 | `			}` |
|     304 |  9532 | `			pInstr->iP1 = 0;` |
|     304 |  9533 | `			if( !isSpecial ){` |
|     134 |  9534 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      66 |  9535 | `			}` |
|       - |  9536 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - |  9537 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     220 |  9538 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     134 |  9539 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     134 |  9540 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 |  9541 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 |  9542 | `					return SXRET_OK;` |
|       - |  9543 | `				}` |
|      45 |  9544 | `			}` |
|      88 |  9545 | `		}` |
|     160 |  9546 | `	}` |
|       - |  9547 | `	/* Generate code for the right tree */` |
|  968570 |  9548 | `	if( pNode->pRight ){` |
|  506056 |  9549 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - |  9550 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8936 |  9551 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  501589 |  9552 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - |  9553 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2984 |  9554 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  495631 |  9555 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - |  9556 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 |  9557 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 |  9558 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  494125 |  9559 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  220810 |  9560 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  110404 |  9561 | `		}` |
|  506056 |  9562 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  506056 |  9563 | `		if( iVmOp == PH7_OP_STORE ){` |
|  217790 |  9564 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  217764 |  9565 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - |  9566 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - |  9567 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - |  9568 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - |  9569 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - |  9570 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - |  9571 | `				 */` |
|      54 |  9572 | `				iVmOp = 0;` |
|  217764 |  9573 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  217738 |  9574 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - |  9575 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   48482 |  9576 | `					iP2 = 1;` |
|   24242 |  9577 | `				}else{` |
|  169258 |  9578 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - |  9579 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   28718 |  9580 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   28718 |  9581 | `						iP1 = pInstr->iP1;` |
|   14360 |  9582 | `					}else{` |
|  140542 |  9583 | `						p3 = pInstr->p3;` |
|       - |  9584 | `					}` |
|       - |  9585 | `					/* POP the last dynamic load instruction */` |
|  169258 |  9586 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  9587 | `				}` |
|  108870 |  9588 | `			}` |
|  397162 |  9589 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 |  9590 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 |  9591 | `			if( pInstr ){` |
|      48 |  9592 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - |  9593 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - |  9594 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - |  9595 | `					 */` |
|      15 |  9596 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 |  9597 | `					iP1 = pInstr->iP1;` |
|      15 |  9598 | `					iP2 = pInstr->iP2;` |
|      15 |  9599 | `					p3  = pInstr->p3;` |
|       8 |  9600 | `				}else{` |
|      34 |  9601 | `					p3 = pInstr->p3;` |
|       - |  9602 | `				}` |
|      23 |  9603 | `			}` |
|      23 |  9604 | `		}` |
|  253027 |  9605 | `	}` |
|  968570 |  9606 | `	if( iVmOp > 0 ){` |
|  968458 |  9607 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11614 |  9608 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - |  9609 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8524 |  9610 | `				iP1 = 1;` |
|    4263 |  9611 | `			}` |
|  962652 |  9612 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - |  9613 | `			/* Namespace-qualify the class name for NEW */ {` |
|   14776 |  9614 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   14776 |  9615 | `				VmInstr *pCallInstr = 0;` |
|   14776 |  9616 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   14760 |  9617 | `					pCallInstr = pPeek;` |
|   14760 |  9618 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7379 |  9619 | `				}` |
|   14776 |  9620 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - |  9621 | `					sxu32 nLitForClass;` |
|       - |  9622 | `					/* If the CALL handler already qualified the name using` |
|       - |  9623 | `					 * function imports, recover the original unqualified` |
|       - |  9624 | `					 * literal so we can re-qualify with class imports. */` |
|   14774 |  9625 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 |  9626 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 |  9627 | `					}else{` |
|   14742 |  9628 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - |  9629 | `					}` |
|   14774 |  9630 | `					pPeek->iP1 = 0;` |
|   14774 |  9631 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7386 |  9632 | `				}` |
|       - |  9633 | `			}` |
|   14776 |  9634 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   14776 |  9635 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - |  9636 | `				VmInstr *pPrev;` |
|   14760 |  9637 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   14760 |  9638 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - |  9639 | `					/* Pop the call instruction, preserve named-arg map */` |
|   14760 |  9640 | `					iP1 = pInstr->iP1;` |
|   14760 |  9641 | `					if( pInstr->p3 ){` |
|      38 |  9642 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      18 |  9643 | `					}` |
|   14760 |  9644 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7379 |  9645 | `				}` |
|    7381 |  9646 | `			}` |
|  949459 |  9647 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - |  9648 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - |  9649 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 |  9650 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 |  9651 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 |  9652 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 |  9653 | `				int isSpecialIs = 0;` |
|      50 |  9654 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 |  9655 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 |  9656 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 |  9657 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 |  9658 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 |  9659 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 |  9660 | `						isSpecialIs = 1;` |
|       5 |  9661 | `					}` |
|      23 |  9662 | `				}` |
|      52 |  9663 | `				pInstr->iP1 = 0;` |
|      52 |  9664 | `				if( !isSpecialIs ){` |
|      38 |  9665 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 |  9666 | `				}` |
|      25 |  9667 | `			}` |
|  942051 |  9668 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - |  9669 | `			/* Prevent constant expansion for member/property names.` |
|       - |  9670 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - |  9671 | `			 * should not trigger constant lookup. */` |
|  109294 |  9672 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  109294 |  9673 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  109260 |  9674 | `				pInstr->iP1 = 0;` |
|   54629 |  9675 | `			}` |
|  109294 |  9676 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - |  9677 | `				/* Static member access,remember that */` |
|     186 |  9678 | `				iP1 = 1;` |
|     186 |  9679 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     186 |  9680 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      28 |  9681 | `					p3 = pInstr->p3;` |
|      28 |  9682 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      13 |  9683 | `				}` |
|      92 |  9684 | `			}` |
|   54646 |  9685 | `		}` |
|       - |  9686 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  968456 |  9687 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  484227 |  9688 | `	}` |
|  968568 |  9689 | `	if( nJmpIdx > 0 ){` |
|       - |  9690 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11948 |  9691 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11948 |  9692 | `		if( pInstr ){` |
|   11948 |  9693 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5973 |  9694 | `		}` |
|    5973 |  9695 | `	}` |
|  968568 |  9696 | `	return rc;` |
| 1276724 |  9697 |  |
|       - |  9698 | `/*` |
|       - |  9699 | ` * Compile a PHP expression.` |
|       - |  9700 | ` * According to the PHP language reference manual:` |
|       - |  9701 | ` *  Expressions are the most important building stones of PHP.` |
|       - |  9702 | ` *  In PHP, almost anything you write is an expression.` |
|       - |  9703 | ` *  The simplest yet most accurate way to define an expression` |
|       - |  9704 | ` *  is "anything that has a value".` |
|       - |  9705 | ` * If something goes wrong while compiling the expression,this` |
|       - |  9706 | ` * function takes care of generating the appropriate error` |
|       - |  9707 | ` * message.` |
|       - |  9708 | ` */` |
|  690136 |  9709 | `static sxi32 PH7_CompileExpr(` |
|       - |  9710 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  9711 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  9712 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - |  9713 | `	)` |
|       2 |  9714 |  |
|       - |  9715 | `	ph7_expr_node *pRoot;` |
|       - |  9716 | `	SySet sExprNode;` |
|       - |  9717 | `	SyToken *pEnd;` |
|       - |  9718 | `	sxi32 nExpr;` |
|       - |  9719 | `	sxi32 iNest;` |
|       - |  9720 | `	sxi32 rc;` |
|       - |  9721 | `	/* Initialize worker variables */` |
|  690138 |  9722 | `	nExpr = 0;` |
|  690138 |  9723 | `	pRoot = 0;` |
|  690138 |  9724 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  690138 |  9725 | `	SySetAlloc(&sExprNode,0x10);` |
|  690138 |  9726 | `	rc = SXRET_OK;` |
|       - |  9727 | `	/* Delimit the expression */` |
|  690138 |  9728 | `	pEnd = pGen->pIn;` |
|  690138 |  9729 | `	iNest = 0;` |
| 4651658 |  9730 | `	while( pEnd < pGen->pEnd ){` |
| 4410824 |  9731 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - |  9732 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     326 |  9733 | `			iNest++;` |
| 4410662 |  9734 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     334 |  9735 | `			iNest--;` |
| 4410334 |  9736 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  449518 |  9737 | `			if( iNest <= 0 ){` |
|  449304 |  9738 | `				break;` |
|       - |  9739 | `			}` |
|     107 |  9740 | `		}` |
| 3961522 |  9741 | `		pEnd++;` |
|       2 |  9742 | `	}` |
|  690138 |  9743 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   11654 |  9744 | `		SyToken *pEnd2 = pGen->pIn;` |
|   11654 |  9745 | `		iNest = 0;` |
|       - |  9746 | `		/* Stop at the first comma */` |
|   23336 |  9747 | `		while( pEnd2 < pEnd ){` |
|   11688 |  9748 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      12 |  9749 | `				iNest++;` |
|   11683 |  9750 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      12 |  9751 | `				iNest--;` |
|   11673 |  9752 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       9 |  9753 | `				if( iNest <= 0 ){` |
|       5 |  9754 | `					break;` |
|       - |  9755 | `				}` |
|       2 |  9756 | `			}` |
|   11684 |  9757 | `			pEnd2++;` |
|       2 |  9758 | `		}` |
|   11654 |  9759 | `		if( pEnd2 <pEnd ){` |
|       5 |  9760 | `			pEnd = pEnd2;` |
|       2 |  9761 | `		}` |
|    5826 |  9762 | `	}` |
|  690138 |  9763 | `	if( pEnd > pGen->pIn ){` |
|  690128 |  9764 | `		SyToken *pTmp = pGen->pEnd;` |
|       - |  9765 | `		/* Swap delimiter */` |
|  690128 |  9766 | `		pGen->pEnd = pEnd;` |
|       - |  9767 | `		/* Try to get an expression tree */` |
|  690128 |  9768 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  690128 |  9769 | `		if( rc == SXRET_OK && pRoot ){` |
|  689956 |  9770 | `			rc = SXRET_OK;` |
|  689956 |  9771 | `			if( xTreeValidator ){` |
|       - |  9772 | `				/* Call the upper layer validator callback */` |
|   14826 |  9773 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    7412 |  9774 | `			}` |
|  689956 |  9775 | `			if( rc != SXERR_ABORT ){` |
|       - |  9776 | `				/* Generate code for the given tree */` |
|  689956 |  9777 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  344977 |  9778 | `			}` |
|  689956 |  9779 | `			nExpr = 1;` |
|  344977 |  9780 | `		}` |
|       - |  9781 | `		/* Release the whole tree */` |
|  690128 |  9782 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - |  9783 | `		/* Synchronize token stream */` |
|  690128 |  9784 | `		pGen->pEnd = pTmp;` |
|  690128 |  9785 | `		pGen->pIn  = pEnd;` |
|  690128 |  9786 | `		if( rc == SXERR_ABORT ){` |
|      11 |  9787 | `			SySetRelease(&sExprNode);` |
|      11 |  9788 | `			return SXERR_ABORT;` |
|       - |  9789 | `		}` |
|  345058 |  9790 | `	}` |
|  690128 |  9791 | `	SySetRelease(&sExprNode);` |
|  690128 |  9792 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  345070 |  9793 |  |
|       - |  9794 | `/*` |
|       - |  9795 | ` * Return a pointer to the node construct handler associated` |
|       - |  9796 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - |  9797 | ` */` |
|  171890 |  9798 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 |  9799 |  |
|  171892 |  9800 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - |  9801 | `		/* Numeric literal: Either real or integer */` |
|   94292 |  9802 | `		return PH7_CompileNumLiteral;` |
|   77602 |  9803 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - |  9804 | `		/* Double quoted string */` |
|   16586 |  9805 | `		return PH7_CompileString;` |
|   61018 |  9806 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - |  9807 | `		/* Single quoted string */` |
|   60906 |  9808 | `		return PH7_CompileSimpleString;` |
|     114 |  9809 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - |  9810 | `		/* Heredoc */` |
|      66 |  9811 | `		return PH7_CompileHereDoc;` |
|      50 |  9812 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - |  9813 | `		/* Nowdoc */` |
|      44 |  9814 | `		return PH7_CompileNowDoc;` |
|       7 |  9815 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - |  9816 | `		/* Backtick quoted string */` |
|       5 |  9817 | `		return PH7_CompileBacktic;` |
|       - |  9818 | `	}` |
|       3 |  9819 | `	return 0;` |
|   85947 |  9820 |  |
|       - |  9821 | `/*` |
|       - |  9822 | ` * Compile an unset() statement.` |
|       - |  9823 | ` * unset($var, $arr[$key], ...);` |
|       - |  9824 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - |  9825 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - |  9826 | ` * parent array before extracting the element to unset.` |
|       - |  9827 | ` */` |
|    2732 |  9828 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 |  9829 |  |
|    2734 |  9830 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2734 |  9831 | `	sxu32 nIdx = 0;` |
|       - |  9832 | `	SyString sName;` |
|       - |  9833 | `	sxi32 rc;` |
|       - |  9834 | `	/* Jump the 'unset' keyword */` |
|    2734 |  9835 | `	pGen->pIn++;` |
|       - |  9836 | `	/* Save delimiter */` |
|    2734 |  9837 | `	pTmp = pGen->pEnd;` |
|       - |  9838 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2734 |  9839 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2734 |  9840 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - |  9841 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - |  9842 | `		SyToken *pClose;` |
|    2734 |  9843 | `		pGen->pIn++;   /* Skip '(' */` |
|    2734 |  9844 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2734 |  9845 | `		pEnd = pClose; /* Stop at ')' */` |
|    1366 |  9846 | `	}` |
|    2734 |  9847 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - |  9848 | `	/* Resolve the 'unset' builtin name once */` |
|    2734 |  9849 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     332 |  9850 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     332 |  9851 | `		if( pObj == 0 ){` |
|     ! 0 |  9852 | `			return SXERR_ABORT;` |
|       - |  9853 | `		}` |
|     332 |  9854 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     332 |  9855 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     165 |  9856 | `	}` |
|       - |  9857 | `	/* Compile each comma-separated argument */` |
|    9054 |  9858 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6322 |  9859 | `		if( pGen->pIn < pNext ){` |
|    6322 |  9860 | `			pGen->pEnd = pNext;` |
|    6322 |  9861 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - |  9862 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6322 |  9863 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9864 | `				return SXERR_ABORT;` |
|       - |  9865 | `			}` |
|    6322 |  9866 | `			if( rc != SXERR_EMPTY ){` |
|       - |  9867 | `				/* Emit call for this single argument */` |
|    6320 |  9868 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6320 |  9869 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6320 |  9870 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3159 |  9871 | `			}` |
|    3160 |  9872 | `		}` |
|       - |  9873 | `		/* Jump trailing commas */` |
|    9912 |  9874 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3592 |  9875 | `			pNext++;` |
|       2 |  9876 | `		}` |
|    6322 |  9877 | `		pGen->pIn = pNext;` |
|       2 |  9878 | `	}` |
|       - |  9879 | `	/* Skip past the closing ')' if present */` |
|    2734 |  9880 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2734 |  9881 | `		pGen->pIn++;` |
|    1366 |  9882 | `	}` |
|       - |  9883 | `	/* Restore token stream */` |
|    2734 |  9884 | `	pGen->pEnd = pTmp;` |
|    2734 |  9885 | `	return SXRET_OK;` |
|    1368 |  9886 |  |
|       - |  9887 | `/*` |
|       - |  9888 | ` * PHP Language construct table.` |
|       - |  9889 | ` */` |
|       - |  9890 | `static const LangConstruct aLangConstruct[] = {` |
|       - |  9891 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - |  9892 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - |  9893 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - |  9894 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - |  9895 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - |  9896 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - |  9897 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - |  9898 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - |  9899 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - |  9900 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - |  9901 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - |  9902 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - |  9903 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - |  9904 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - |  9905 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - |  9906 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - |  9907 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - |  9908 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - |  9909 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - |  9910 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - |  9911 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - |  9912 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - |  9913 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - |  9914 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - |  9915 | `};` |
|       - |  9916 | `/*` |
|       - |  9917 | ` * Return a pointer to the statement handler routine associated` |
|       - |  9918 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - |  9919 | ` */` |
|  418276 |  9920 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - |  9921 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - |  9922 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - |  9923 | `	)` |
|       2 |  9924 |  |
|  418278 |  9925 | `	sxu32 n = 0;` |
| 1759808 |  9926 | `	for(;;){` |
| 3519618 |  9927 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   49098 |  9928 | `			break;` |
|       - |  9929 | `		}` |
| 3470522 |  9930 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  369182 |  9931 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 |  9932 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 |  9933 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - |  9934 | `					/* 'static' (class context),return null */` |
|     ! 0 |  9935 | `					return 0;` |
|       - |  9936 | `				}` |
|     ! 0 |  9937 | `			}` |
|  369180 |  9938 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 |  9939 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 |  9940 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - |  9941 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 |  9942 | `				return 0;` |
|       - |  9943 | `			}` |
|       - |  9944 | `			/* Return a pointer to the handler.` |
|       - |  9945 | `			*/` |
|  369182 |  9946 | `			return aLangConstruct[n].xConstruct;` |
|       - |  9947 | `		}` |
| 3101342 |  9948 | `		n++;` |
|       2 |  9949 | `	}` |
|   49098 |  9950 | `	if( pLookahed ){` |
|   49098 |  9951 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8554 |  9952 | `			return PH7_CompileClassInterface;` |
|   40546 |  9953 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   40338 |  9954 | `			return PH7_CompileClass;` |
|     210 |  9955 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 |  9956 | `			return PH7_CompileTrait;` |
|     154 |  9957 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 |  9958 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 |  9959 | `				return PH7_CompileAbstractClass;` |
|     138 |  9960 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 |  9961 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 |  9962 | `				return PH7_CompileFinalClass;` |
|       - |  9963 | `		}` |
|      68 |  9964 | `	}` |
|       - |  9965 | `	/* Not a language construct */` |
|     138 |  9966 | `	return 0;` |
|  209140 |  9967 |  |
|       - |  9968 | `/*` |
|       - |  9969 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - |  9970 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - |  9971 | ` */` |
|     136 |  9972 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 |  9973 |  |
|       - |  9974 | `	int rc;` |
|     138 |  9975 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     138 |  9976 | `	if( rc == FALSE ){` |
|      42 |  9977 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 |  9978 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - |  9979 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - |  9980 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - |  9981 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - |  9982 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - |  9983 | `			*/` |
|       - |  9984 | `			){` |
|      36 |  9985 | `				rc = TRUE;` |
|      17 |  9986 | `		}` |
|      21 |  9987 | `	}` |
|     138 |  9988 | `	return rc;` |
|       2 |  9989 |  |
|       - |  9990 | `/*` |
|       - |  9991 | ` * Compile a PHP chunk.` |
|       - |  9992 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - |  9993 | ` * takes care of generating the appropriate error message.` |
|       - |  9994 | ` */` |
|  561574 |  9995 | `static sxi32 GenStateCompileChunk(` |
|       - |  9996 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  9997 | `	sxi32 iFlags         /* Compile flags */` |
|       - |  9998 | `	)` |
|       2 |  9999 |  |
|       - | 10000 | `	ProcLangConstruct xCons;` |
|       - | 10001 | `	sxi32 rc;` |
|  561576 | 10002 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  335574 | 10003 | `	for(;;){` |
|  671150 | 10004 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10005 | `			/* No more input to process */` |
|   12118 | 10006 | `			break;` |
|       - | 10007 | `		}` |
|  659034 | 10008 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10009 | `			/* Compile block */` |
|      16 | 10010 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      16 | 10011 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10012 | `				break;` |
|       - | 10013 | `			}` |
|       9 | 10014 | `		}else{` |
|  659020 | 10015 | `			xCons = 0;` |
|  659020 | 10016 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  418278 | 10017 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10018 | `				/* Try to extract a language construct handler */` |
|  418278 | 10019 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  418278 | 10020 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10021 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10022 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10023 | `						&pGen->pIn->sData);` |
|       9 | 10024 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10025 | `						break;` |
|       - | 10026 | `					}` |
|       - | 10027 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10028 | `					 * this erroneous statement.` |
|       - | 10029 | `					 */` |
|       9 | 10030 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10031 | `				}` |
|  449882 | 10032 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   42172 | 10033 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10034 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10035 | `				xCons = PH7_CompileLabel;` |
|      56 | 10036 | `			}` |
|  659020 | 10037 | `			if( xCons == 0 ){` |
|       - | 10038 | `				/* Assume an expression an try to compile it */` |
|  240760 | 10039 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  240760 | 10040 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10041 | `					/* Pop l-value */` |
|  240620 | 10042 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  120309 | 10043 | `				}` |
|  120381 | 10044 | `			}else{` |
|       - | 10045 | `				/* Go compile the sucker */` |
|  418262 | 10046 | `				rc = xCons(&(*pGen));` |
|       - | 10047 | `			}` |
|  659020 | 10048 | `			if( rc == SXERR_ABORT ){` |
|       - | 10049 | `				/* Request to abort compilation */` |
|      11 | 10050 | `				break;` |
|       - | 10051 | `			}` |
|       - | 10052 | `		}` |
|       - | 10053 | `		/* Ignore trailing semi-colons ';' */` |
| 1091434 | 10054 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  432412 | 10055 | `			pGen->pIn++;` |
|       2 | 10056 | `		}` |
|  659024 | 10057 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10058 | `			/* Compile a single statement and return */` |
|  549450 | 10059 | `			break;` |
|       - | 10060 | `		}` |
|       - | 10061 | `		/* LOOP ONE */` |
|       - | 10062 | `		/* LOOP TWO */` |
|       - | 10063 | `		/* LOOP THREE */` |
|       - | 10064 | `		/* LOOP FOUR */` |
|       2 | 10065 | `	}` |
|       - | 10066 | `	/* Return compilation status */` |
|  561576 | 10067 | `	return rc;` |
|       2 | 10068 |  |
|       - | 10069 | `/*` |
|       - | 10070 | ` * Compile a Raw PHP chunk.` |
|       - | 10071 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10072 | ` * takes care of generating the appropriate error message.` |
|       - | 10073 | ` */` |
|   12128 | 10074 | `static sxi32 PH7_CompilePHP(` |
|       - | 10075 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10076 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10077 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10078 | `	)` |
|       2 | 10079 |  |
|   12130 | 10080 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10081 | `	sxi32 rc;` |
|       - | 10082 | `	/* Reset the token set */` |
|   12130 | 10083 | `	SySetReset(&(*pTokenSet));` |
|       - | 10084 | `	/* Mark as the default token set */` |
|   12130 | 10085 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10086 | `	/* Advance the stream cursor */` |
|   12130 | 10087 | `	pGen->pRawIn++;` |
|       - | 10088 | `	/* Tokenize the PHP chunk first */` |
|   12130 | 10089 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10090 | `	/* Point to the head and tail of the token stream. */` |
|   12130 | 10091 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   12130 | 10092 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   12130 | 10093 | `	if( is_expr ){` |
|     ! 0 | 10094 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10095 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10096 | `			/* A simple expression,compile it */` |
|     ! 0 | 10097 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10098 | `		}` |
|       - | 10099 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10100 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10101 | `		return SXRET_OK;` |
|       - | 10102 | `	}` |
|   12130 | 10103 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10104 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10105 | `		/*` |
|       - | 10106 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10107 | `		 * According to the PHP reference manual:` |
|       - | 10108 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10109 | `		 *  immediately follow` |
|       - | 10110 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10111 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10112 | `		 * Symisc extension:` |
|       - | 10113 | `		 *   This short syntax works with all PHP opening` |
|       - | 10114 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10115 | `		 *   only short tag.` |
|       - | 10116 | `		 */` |
|       - | 10117 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10118 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10119 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 10120 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 10121 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 10122 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 10123 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 10124 | `		}` |
|       3 | 10125 | `		return SXRET_OK;` |
|       - | 10126 | `	}` |
|       - | 10127 | `	/* Compile the PHP chunk */` |
|   12128 | 10128 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 10129 | `	/* Fix exceptions jumps */` |
|   12128 | 10130 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10131 | `	/* Fix gotos now, the jump destination is resolved */` |
|   12128 | 10132 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 10133 | `		rc = SXERR_ABORT;` |
|       1 | 10134 | `	}` |
|       - | 10135 | `	/* Reset container */` |
|   12128 | 10136 | `	SySetReset(&pGen->aGoto);` |
|   12128 | 10137 | `	SySetReset(&pGen->aLabel);` |
|       - | 10138 | `	/* Compilation result */` |
|   12128 | 10139 | `	return rc;` |
|    6066 | 10140 |  |
|       - | 10141 | `/*` |
|       - | 10142 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 10143 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 10144 | ` * This is the only compile interface exported from this file.` |
|       - | 10145 | ` */` |
|   14380 | 10146 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 10147 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 10148 | `	SyString *pScript,  /* Script to compile */` |
|       - | 10149 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 10150 | `	)` |
|       2 | 10151 |  |
|       - | 10152 | `	SySet aPhpToken,aRawToken;` |
|       - | 10153 | `	ph7_gen_state *pCodeGen;` |
|       - | 10154 | `	ph7_value *pRawObj;` |
|       - | 10155 | `	sxu32 nObjIdx;` |
|       - | 10156 | `	sxi32 nRawObj;` |
|       - | 10157 | `	int is_expr;` |
|       - | 10158 | `	sxi32 rc;` |
|   14382 | 10159 | `	if( pScript->nByte < 1 ){` |
|       - | 10160 | `		/* Nothing to compile */` |
|     ! 0 | 10161 | `		return PH7_OK;` |
|       - | 10162 | `	}` |
|       - | 10163 | `	/* Initialize the tokens containers */` |
|   14382 | 10164 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14382 | 10165 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14382 | 10166 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14382 | 10167 | `	is_expr = 0;` |
|   14382 | 10168 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 10169 | `		SyToken sTmp;` |
|       - | 10170 | `		/* PHP only: -*/` |
|    2868 | 10171 | `		sTmp.nLine = 1;` |
|    2868 | 10172 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2868 | 10173 | `		sTmp.pUserData = 0;` |
|    2868 | 10174 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2868 | 10175 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2868 | 10176 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 10177 | `			/* A simple PHP expression */` |
|     ! 0 | 10178 | `			is_expr = 1;` |
|     ! 0 | 10179 | `		}` |
|    1435 | 10180 | `	}else{` |
|       - | 10181 | `		/* Tokenize raw text */` |
|   11516 | 10182 | `		SySetAlloc(&aRawToken,32);` |
|   11516 | 10183 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 10184 | `	}` |
|   14382 | 10185 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 10186 | `	/* Process high-level tokens */` |
|   14382 | 10187 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14382 | 10188 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14382 | 10189 | `	rc = PH7_OK;` |
|   14382 | 10190 | `	if( is_expr ){` |
|       - | 10191 | `		/* Compile the expression */` |
|     ! 0 | 10192 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 10193 | `		goto cleanup;` |
|       - | 10194 | `	}` |
|   14382 | 10195 | `	nObjIdx = 0;` |
|       - | 10196 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 10197 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 10198 | `	 * preventing namespace bleeding across include()d files. */` |
|   14382 | 10199 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 10200 | `	/* Start the compilation process */` |
|   12951 | 10201 | `	for(;;){` |
|   38020 | 10202 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14370 | 10203 | `			break; /* No more tokens to process */` |
|       - | 10204 | `		}` |
|   23652 | 10205 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 10206 | `			/* Compile the PHP chunk */` |
|   12130 | 10207 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   12130 | 10208 | `			if( rc == SXERR_ABORT ){` |
|      13 | 10209 | `				break;` |
|       - | 10210 | `			}` |
|   12118 | 10211 | `			continue;` |
|       - | 10212 | `		}` |
|       - | 10213 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11524 | 10214 | `		nRawObj = 0;` |
|   23046 | 10215 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 10216 | `			/* Consume the raw chunk without any processing */` |
|   11524 | 10217 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11524 | 10218 | `			if( pRawObj == 0 ){` |
|     ! 0 | 10219 | `				rc = SXERR_MEM;` |
|     ! 0 | 10220 | `				break;` |
|       - | 10221 | `			}` |
|       - | 10222 | `			/* Mark as constant and emit the load constant instruction */` |
|   11524 | 10223 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11524 | 10224 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11524 | 10225 | `			++nRawObj;` |
|   11524 | 10226 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 10227 | `		}` |
|   11524 | 10228 | `		if( nRawObj > 0 ){` |
|       - | 10229 | `			/* Emit the consume instruction */` |
|   11524 | 10230 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5761 | 10231 | `		}` |
|    7192 | 10232 | `	}` |
|    7190 | 10233 | `cleanup:` |
|   14382 | 10234 | `	SySetRelease(&aRawToken);` |
|   14382 | 10235 | `	SySetRelease(&aPhpToken);` |
|   14382 | 10236 | `	return rc;` |
|    7192 | 10237 |  |
|       - | 10238 | `/*` |
|       - | 10239 | ` * Utility routines.Initialize the code generator.` |
|       - | 10240 | ` */` |
|    2838 | 10241 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 10242 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10243 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10244 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10245 | `	)` |
|       2 | 10246 |  |
|    2840 | 10247 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10248 | `	/* Zero the structure */` |
|    2840 | 10249 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 10250 | `	/* Initial state */` |
|    2840 | 10251 | `	pGen->pVm  = &(*pVm);` |
|    2840 | 10252 | `	pGen->xErr = xErr;` |
|    2840 | 10253 | `	pGen->pErrData = pErrData;` |
|    2840 | 10254 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2840 | 10255 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2840 | 10256 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2840 | 10257 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 10258 | `	/* Error log buffer */` |
|    2840 | 10259 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 10260 | `	/* General purpose working buffer */` |
|    2840 | 10261 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 10262 | `	/* Namespace state */` |
|    2840 | 10263 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2840 | 10264 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2840 | 10265 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2840 | 10266 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10267 | `	/* Create the global scope */` |
|    2840 | 10268 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 10269 | `	/* Point to the global scope */` |
|    2840 | 10270 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2840 | 10271 | `	return SXRET_OK;` |
|       2 | 10272 |  |
|       - | 10273 | `/*` |
|       - | 10274 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 10275 | ` */` |
|   16934 | 10276 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 10277 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10278 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10279 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10280 | `	)` |
|       2 | 10281 |  |
|   16936 | 10282 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10283 | `	GenBlock *pBlock,*pParent;` |
|       - | 10284 | `	/* Reset state */` |
|   16936 | 10285 | `	SySetReset(&pGen->aLabel);` |
|   16936 | 10286 | `	SySetReset(&pGen->aGoto);` |
|   16936 | 10287 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   16936 | 10288 | `	SyBlobRelease(&pGen->sWorker);` |
|   16936 | 10289 | `	SyBlobRelease(&pGen->sNamespace);` |
|   16936 | 10290 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   16936 | 10291 | `	SyHashRelease(&pGen->hUseImports);` |
|   16936 | 10292 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   16936 | 10293 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   16936 | 10294 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   16936 | 10295 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   16936 | 10296 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10297 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 10298 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 10299 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 10300 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 10301 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 10302 | `	 * number of unique names, which is acceptable. */` |
|       - | 10303 | `	/* Point to the global scope */` |
|   16936 | 10304 | `	pBlock = pGen->pCurrent;` |
|   16936 | 10305 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 10306 | `		pParent = pBlock->pParent;` |
|     ! 0 | 10307 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 10308 | `		pBlock = pParent;` |
|     ! 0 | 10309 | `	}` |
|   16936 | 10310 | `	pGen->xErr = xErr;` |
|   16936 | 10311 | `	pGen->pErrData = pErrData;` |
|   16936 | 10312 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   16936 | 10313 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   16936 | 10314 | `	pGen->pIn = pGen->pEnd = 0;` |
|   16936 | 10315 | `	pGen->nErr = 0;` |
|   16936 | 10316 | `	return SXRET_OK;` |
|       2 | 10317 |  |
|       - | 10318 | `/*` |
|       - | 10319 | ` * Generate a compile-time error message.` |
|       - | 10320 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 10321 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 10322 | ` * abort compilation immediately.` |
|       - | 10323 | ` */` |
|     528 | 10324 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 10325 |  |
|     530 | 10326 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     530 | 10327 | `	const char *zErr = "Error";` |
|       - | 10328 | `	SyString *pFile;` |
|       - | 10329 | `	va_list ap;` |
|       - | 10330 | `	sxi32 rc;` |
|       - | 10331 | `	/* Reset the working buffer */` |
|     530 | 10332 | `	SyBlobReset(pWorker);` |
|       - | 10333 | `	/* Peek the processed file path if available */` |
|     530 | 10334 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     530 | 10335 | `	if( nErrType == E_ERROR ){` |
|       - | 10336 | `		/* Increment the error counter */` |
|     440 | 10337 | `		pGen->nErr++;` |
|     440 | 10338 | `		if( pGen->nErr > 15 ){` |
|       - | 10339 | `			/* Error count limit reached */` |
|       5 | 10340 | `			if( pGen->xErr ){` |
|       5 | 10341 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 10342 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 10343 | `				if( pFile ){` |
|       5 | 10344 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 10345 | `				}` |
|       5 | 10346 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 10347 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 10348 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 10349 | `				}` |
|       2 | 10350 | `			}` |
|       - | 10351 | `			/* Abort immediately */` |
|       5 | 10352 | `			return SXERR_ABORT;` |
|       - | 10353 | `		}` |
|     217 | 10354 | `	}` |
|     526 | 10355 | `	if( pGen->xErr == 0 ){` |
|       - | 10356 | `		/* No available error consumer,return immediately */` |
|       3 | 10357 | `		return SXRET_OK;` |
|       - | 10358 | `	}` |
|     523 | 10359 | `	switch(nErrType){` |
|     433 | 10360 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 10361 | `	case E_WARNING: zErr = "Warning";     break;` |
|      57 | 10362 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 10363 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 10364 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 10365 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 10366 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 10367 | `	default:` |
|     ! 0 | 10368 | `		break;` |
|       - | 10369 | `	}` |
|     523 | 10370 | `	rc = SXRET_OK;` |
|       - | 10371 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     523 | 10372 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     523 | 10373 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     523 | 10374 | `	va_start(ap,zFormat);` |
|     523 | 10375 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     523 | 10376 | `	va_end(ap);` |
|     523 | 10377 | `	if( pFile ){` |
|     523 | 10378 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     261 | 10379 | `	}` |
|       - | 10380 | `	/* Append a new line */` |
|     523 | 10381 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     523 | 10382 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 10383 | `		/* Consume the generated error message */` |
|     523 | 10384 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     261 | 10385 | `	}` |
|     523 | 10386 | `	return rc;` |
|     266 | 10387 |  |
|       - | 10388 |  |
