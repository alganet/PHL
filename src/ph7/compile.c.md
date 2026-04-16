# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5117/6469 lines (79.10%)

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
|    3192 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3194 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    9011 |   131 | `	for(;;){` |
|   18024 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    3086 |   133 | `			iCount--; /* Decrement nesting level */` |
|    3086 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    3060 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   14966 |   140 | `		pBlock = pBlock->pParent;` |
|   14966 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1598 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  689558 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  689560 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  689560 |   162 | `	pBlock->pUserData   = pUserData;` |
|  689560 |   163 | `	pBlock->pGen        = pGen;` |
|  689560 |   164 | `	pBlock->iFlags      = iType;` |
|  689560 |   165 | `	pBlock->pParent     = 0;` |
|  689560 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  689560 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  689560 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  686632 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  686634 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  686634 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  686634 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  686634 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  686634 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  686634 |   200 | `	pGen->pCurrent = pBlock;` |
|  686634 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  333498 |   203 | `		*ppBlock = pBlock;` |
|  166748 |   204 | `	}` |
|  686634 |   205 | `	return SXRET_OK;` |
|  343318 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  686624 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  686626 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  686626 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  686626 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  686624 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  686626 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  686626 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  686626 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  686626 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  686624 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  686626 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  686626 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  686626 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  686626 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  686626 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  686626 |   244 | `	return SXRET_OK;` |
|  343314 |   245 |  |
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
|  195188 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  195190 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  195190 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  195190 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  195190 |   265 | `	return rc;` |
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
|  480678 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  480680 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  865556 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  384878 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  153786 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  231094 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   35908 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  195188 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  195188 |   298 | `		if( pInstr ){` |
|  195188 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  195188 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  195188 |   302 | `			aFix[n].nJumpType = -1;` |
|   97593 |   303 | `		}` |
|   97595 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  480680 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  195202 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  195204 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  195350 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  195202 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  195334 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  195202 |   358 | `	return SXRET_OK;` |
|   97603 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  617094 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  617096 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  617096 |   367 | `	if( pEntry == 0 ){` |
|  268348 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  348750 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  348750 |   371 | `	return SXRET_OK;` |
|  308549 |   372 |  |
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
|  268346 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  268348 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  268348 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  134173 |   387 | `	}` |
|  268348 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|  102344 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|  102346 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|  102346 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  102346 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|  102346 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|  102346 |   408 | `	return pObj;` |
|   51174 |   409 |  |
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
|  102868 |   471 | `static int GenStateFindBadNumericSeparator(` |
|       - |   472 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   473 |  |
|  102870 |   474 | `	const char *z = pRaw->zString;` |
|  102870 |   475 | `	sxu32 n = pRaw->nByte;` |
|  102870 |   476 | `	int base = 10;` |
|       - |   477 | `	sxu32 i, start;` |
|  102870 |   478 | `	if( n < 2 ) return 0;` |
|    8664 |   479 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   480 | `		base = 16;` |
|    8629 |   481 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   482 | `		base = 2;` |
|     139 |   483 | `	}` |
|   31946 |   484 | `	for( i = 0; i < n; ++i ){` |
|   23298 |   485 | `		if( z[i] != '_' ) continue;` |
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
|    8650 |   502 | `	return 0;` |
|   51436 |   503 |  |
|       - |   504 | `/*` |
|       - |   505 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   506 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   507 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   508 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   509 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   510 | ` * so callers can bail from the current construct).` |
|       - |   511 | ` */` |
|  102868 |   512 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   513 |  |
|  102870 |   514 | `	const char *zBad = 0;` |
|  102870 |   515 | `	sxu32 nBad = 0;` |
|       - |   516 | `	SyString sBad;` |
|       - |   517 | `	sxi32 rc;` |
|  102870 |   518 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  102856 |   519 | `		return SXRET_OK;` |
|       - |   520 | `	}` |
|      15 |   521 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   522 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   523 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   524 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   525 | `		return SXERR_ABORT;` |
|       - |   526 | `	}` |
|      15 |   527 | `	return SXERR_SYNTAX;` |
|   51436 |   528 |  |
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
|  102854 |   545 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   546 | `	SyMemBackend *pAlloc,` |
|       - |   547 | `	const SyString *pToken,` |
|       - |   548 | `	char *zScratch, sxu32 nScratch,` |
|       - |   549 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   550 |  |
|       - |   551 | `	sxu32 i, j;` |
|  102856 |   552 | `	int hasUnderscore = 0;` |
|       - |   553 | `	char *zBuf;` |
|  102856 |   554 | `	*pzAlloc = 0;` |
|  218278 |   555 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  115676 |   556 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   57713 |   557 | `	}` |
|  102856 |   558 | `	if( !hasUnderscore ){` |
|  102604 |   559 | `		SyStringDupPtr(pOut, pToken);` |
|  102604 |   560 | `		return SXRET_OK;` |
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
|   51429 |   577 |  |
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
|  102840 |   594 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   595 |  |
|  102842 |   596 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  102842 |   597 | `	sxu32 nIdx = 0;` |
|       - |   598 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  102842 |   599 | `	char *zAlloc = 0;` |
|       - |   600 | `	SyString sNum;` |
|       - |   601 | `	sxi32 rc;` |
|   51420 |   602 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  102842 |   603 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  102842 |   604 | `	if( rc != SXRET_OK ){` |
|      11 |   605 | `		return rc;` |
|       - |   606 | `	}` |
|  154247 |   607 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   51415 |   608 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  102832 |   609 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   610 | `		return SXERR_ABORT;` |
|       - |   611 | `	}` |
|  102832 |   612 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   613 | `		ph7_value *pObj;` |
|       - |   614 | `		sxi64 iValue;` |
|  102346 |   615 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  102346 |   616 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  102346 |   617 | `		if( pObj == 0 ){` |
|     ! 0 |   618 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   619 | `			return SXERR_ABORT;` |
|       - |   620 | `		}` |
|  102346 |   621 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   51174 |   622 | `	}else{` |
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
|  102832 |   635 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   636 | `	/* Emit the load constant instruction */` |
|  102832 |   637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   638 | `	/* Node successfully compiled */` |
|  102832 |   639 | `	return SXRET_OK;` |
|   51422 |   640 |  |
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
|   74242 |   652 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   653 |  |
|   74244 |   654 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   655 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   656 | `	ph7_value *pObj;` |
|       - |   657 | `	sxu32 nIdx;` |
|   74244 |   658 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   659 | `	/* Delimit the string */` |
|   74244 |   660 | `	zIn  = pStr->zString;` |
|   74244 |   661 | `	zEnd = &zIn[pStr->nByte];` |
|   74244 |   662 | `	if( zIn >= zEnd ){` |
|       - |   663 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   664 | `		 * rather than reserving a new object each time. */` |
|    5996 |   665 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    5996 |   666 | `		return SXRET_OK;` |
|       - |   667 | `	}` |
|   68250 |   668 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   669 | `		/* Already processed,emit the load constant instruction` |
|       - |   670 | `		 * and return.` |
|       - |   671 | `		 */` |
|   27038 |   672 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   27038 |   673 | `		return SXRET_OK;` |
|       - |   674 | `	}` |
|       - |   675 | `	/* Reserve a new constant */` |
|   41214 |   676 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   41214 |   677 | `	if( pObj == 0 ){` |
|     ! 0 |   678 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   679 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   680 | `		return SXERR_ABORT;` |
|       - |   681 | `	}` |
|   41214 |   682 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   683 | `	/* Compile the node */` |
|   41254 |   684 | `	for(;;){` |
|   82510 |   685 | `		if( zIn >= zEnd ){` |
|       - |   686 | `			/* End of input */` |
|   41214 |   687 | `			break;` |
|       - |   688 | `		}` |
|   41298 |   689 | `		zCur = zIn;` |
|  649854 |   690 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  608558 |   691 | `			zIn++;` |
|       2 |   692 | `		}` |
|   41298 |   693 | `		if( zIn > zCur ){` |
|       - |   694 | `			/* Append raw contents*/` |
|   41278 |   695 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   20638 |   696 | `		}` |
|   41298 |   697 | `		zIn++;` |
|   41298 |   698 | `		if( zIn < zEnd ){` |
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
|   41298 |   713 | `		zIn++;` |
|       2 |   714 | `	}` |
|       - |   715 | `	/* Emit the load constant instruction */` |
|   41214 |   716 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   41214 |   717 | `	if( pStr->nByte < 1024 ){` |
|       - |   718 | `		/* Install in the literal table */` |
|   41214 |   719 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   20606 |   720 | `	}` |
|       - |   721 | `	/* Node successfully compiled */` |
|   41214 |   722 | `	return SXRET_OK;` |
|   37123 |   723 |  |
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
|   18572 |   923 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   924 |  |
|       - |   925 | `	ph7_value *pConstObj;` |
|   18574 |   926 | `	sxu32 nIdx = 0;` |
|       - |   927 | `	/* Reserve a new constant */` |
|   18574 |   928 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   18574 |   929 | `	if( pConstObj == 0 ){` |
|     ! 0 |   930 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   931 | `		return 0;` |
|       - |   932 | `	}` |
|   18574 |   933 | `	(*pCount)++;` |
|   18574 |   934 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   935 | `	/* Emit the load constant instruction */` |
|   18574 |   936 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18574 |   937 | `	return pConstObj;` |
|    9288 |   938 |  |
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
|   17182 |   977 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |   978 |  |
|   17184 |   979 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |   980 | `	const char *zIn,*zCur,*zEnd;` |
|   17184 |   981 | `	ph7_value *pObj = 0;` |
|       - |   982 | `	sxi32 iCons;` |
|       - |   983 | `	sxi32 rc;` |
|       - |   984 | `	/* Delimit the string */` |
|   17184 |   985 | `	zIn  = pStr->zString;` |
|   17184 |   986 | `	zEnd = &zIn[pStr->nByte];` |
|   17184 |   987 | `	if( zIn >= zEnd ){` |
|       - |   988 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |   989 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |   990 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |   991 | `		 */` |
|     234 |   992 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     234 |   993 | `		return SXRET_OK;` |
|       - |   994 | `	}` |
|   16952 |   995 | `	zCur = 0;` |
|       - |   996 | `	/* Compile the node */` |
|   16952 |   997 | `	iCons = 0;` |
|    9445 |   998 | `	for(;;){` |
|   28590 |   999 | `		zCur = zIn;` |
|  145418 |  1000 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  118770 |  1001 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      59 |  1002 | `				break;` |
|  118656 |  1003 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1828 |  1004 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     914 |  1005 | `					break;` |
|       - |  1006 | `			}` |
|  116830 |  1007 | `			zIn++;` |
|       2 |  1008 | `		}` |
|   28590 |  1009 | `		if( zIn > zCur ){` |
|   12954 |  1010 | `			if( pObj == 0 ){` |
|   12678 |  1011 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12678 |  1012 | `				if( pObj == 0 ){` |
|     ! 0 |  1013 | `					return SXERR_ABORT;` |
|       - |  1014 | `				}` |
|    6338 |  1015 | `			}` |
|   12954 |  1016 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6476 |  1017 | `		}` |
|   28590 |  1018 | `		if( zIn >= zEnd ){` |
|   16952 |  1019 | `			break;` |
|       - |  1020 | `		}` |
|   11640 |  1021 | `		if( zIn[0] == '\\' ){` |
|    9700 |  1022 | `			const char *zPtr = 0;` |
|       - |  1023 | `			sxu32 n;` |
|    9700 |  1024 | `			zIn++;` |
|    9700 |  1025 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1026 | `				break;` |
|       - |  1027 | `			}` |
|    9700 |  1028 | `			if( pObj == 0 ){` |
|    5898 |  1029 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5898 |  1030 | `				if( pObj == 0 ){` |
|     ! 0 |  1031 | `					return SXERR_ABORT;` |
|       - |  1032 | `				}` |
|    2948 |  1033 | `			}` |
|    9700 |  1034 | `			n = sizeof(char); /* size of conversion */` |
|    9700 |  1035 | `			switch( zIn[0] ){` |
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
|    4484 |  1056 | `			case 'n':` |
|       - |  1057 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8970 |  1058 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8970 |  1059 | `				break;` |
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
|    9700 |  1127 | `			zIn += n;` |
|    9700 |  1128 | `			continue;` |
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
|   16952 |  1246 | `	if( iCons > 1 ){` |
|       - |  1247 | `		/* Concatenate all compiled constants */` |
|    1438 |  1248 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     718 |  1249 | `	}` |
|       - |  1250 | `	/* Node successfully compiled */` |
|   16952 |  1251 | `	return SXRET_OK;` |
|    8593 |  1252 |  |
|       - |  1253 | `/*` |
|       - |  1254 | ` * Compile a double quoted string.` |
|       - |  1255 | ` *  See the block-comment above for more information.` |
|       - |  1256 | ` */` |
|   17122 |  1257 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1258 |  |
|       - |  1259 | `	sxi32 rc;` |
|   17124 |  1260 | `	rc = GenStateCompileString(&(*pGen));` |
|    8561 |  1261 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1262 | `	/* Compilation result */` |
|   17124 |  1263 | `	return rc;` |
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
|   17114 |  1307 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   17116 |  1318 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1319 | `	/* Compile the expression*/` |
|   17116 |  1320 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1321 | `	/* Restore token stream */` |
|   17116 |  1322 | `	RE_SWAP_DELIMITER(pGen);` |
|   17116 |  1323 | `	return rc;` |
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
|   25470 |  1362 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1363 |  |
|       - |  1364 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1365 | `	SyToken *pKey,*pCur;` |
|   25472 |  1366 | `	sxi32 iEmitRef = 0;` |
|   25472 |  1367 | `	sxi32 nPair = 0;` |
|       - |  1368 | `	sxi32 iNest;` |
|       - |  1369 | `	sxi32 rc;` |
|   25472 |  1370 | `	xValidator = 0;` |
|   20625 |  1371 | `	for(;;){` |
|       - |  1372 | `		/* Jump leading commas */` |
|   46546 |  1373 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5296 |  1374 | `			pGen->pIn++;` |
|       2 |  1375 | `		}` |
|   41252 |  1376 | `		pCur = pGen->pIn;` |
|   41252 |  1377 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1378 | `			/* No more entry to process */` |
|   25460 |  1379 | `			break;` |
|       - |  1380 | `		}` |
|   15794 |  1381 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1382 | `			continue;` |
|       - |  1383 | `		}` |
|       - |  1384 | `		/* Compile the key if available */` |
|   15794 |  1385 | `		pKey = pCur;` |
|   15794 |  1386 | `		iNest = 0;` |
|   44020 |  1387 | `		while( pCur < pGen->pIn ){` |
|   29448 |  1388 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 |  1389 | `				break;` |
|       - |  1390 | `			}` |
|       - |  1391 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1392 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1393 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1394 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1395 | `			 */` |
|   28232 |  1396 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   28226 |  1460 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      86 |  1461 | `				iNest++;` |
|   28184 |  1462 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - |  1463 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - |  1464 | `				 * parser will shortly detect any syntax error.` |
|       - |  1465 | `				 */` |
|      86 |  1466 | `				iNest--;` |
|      42 |  1467 | `			}` |
|   28226 |  1468 | `			pCur++;` |
|       2 |  1469 | `		}` |
|   15794 |  1470 | `		rc = SXERR_EMPTY;` |
|   15794 |  1471 | `		if( pCur < pGen->pIn ){` |
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
|   15181 |  1487 | `		}else if( pKey == pCur ){` |
|       - |  1488 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1489 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1490 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1491 | `		}else{` |
|       - |  1492 | `			/* Reset back the cursor and point to the entry value */` |
|   14578 |  1493 | `			pCur = pKey;` |
|       - |  1494 | `		}` |
|   15784 |  1495 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1496 | `			/* No available key,load NULL */` |
|   14580 |  1497 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7289 |  1498 | `		}` |
|   15784 |  1499 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   15782 |  1514 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15782 |  1515 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1516 | `			return SXERR_ABORT;` |
|       - |  1517 | `		}` |
|   15782 |  1518 | `		if( iEmitRef ){` |
|       - |  1519 | `			/* Emit the load reference instruction */` |
|      32 |  1520 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1521 | `		}` |
|   15782 |  1522 | `		xValidator = 0;` |
|   15782 |  1523 | `		iEmitRef = 0;` |
|   15782 |  1524 | `		nPair++;` |
|       2 |  1525 | `	}` |
|       - |  1526 | `	/* Emit the load map instruction */` |
|   25460 |  1527 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1528 | `	/* Node successfully compiled */` |
|   25460 |  1529 | `	return SXRET_OK;` |
|   12737 |  1530 |  |
|       - |  1531 | `/*` |
|       - |  1532 | ` * Compile the 'array' language construct.` |
|       - |  1533 | ` *	 According to the PHP language reference manual` |
|       - |  1534 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1535 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1536 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1537 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1538 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1539 | ` */` |
|   25172 |  1540 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1541 |  |
|       - |  1542 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   25174 |  1543 | `	pGen->pIn += 2;` |
|   25174 |  1544 | `	pGen->pEnd--;` |
|   12586 |  1545 | `	SXUNUSED(iCompileFlag);` |
|   25174 |  1546 | `	return GenStateCompileArrayBody(pGen);` |
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
|  916512 |  2687 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2688 |  |
|  916514 |  2689 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2690 | `	sxi32 iVv;` |
|       - |  2691 | `	sxi32 iP1;` |
|       - |  2692 | `	void *p3;` |
|       - |  2693 | `	sxi32 rc;` |
|  916514 |  2694 | `	iVv = -1; /* Variable variable counter */` |
| 1833038 |  2695 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  916526 |  2696 | `		pGen->pIn++;` |
|  916526 |  2697 | `		iVv++;` |
|       2 |  2698 | `	}` |
|  916514 |  2699 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2700 | `		/* Invalid variable name */` |
|     ! 0 |  2701 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2702 | `		if( rc == SXERR_ABORT ){` |
|       - |  2703 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2704 | `			return SXERR_ABORT;` |
|       - |  2705 | `		}` |
|     ! 0 |  2706 | `		return SXRET_OK;` |
|       - |  2707 | `	}` |
|  916514 |  2708 | `	p3  = 0;` |
|  916514 |  2709 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  916498 |  2729 | `		char *zName = 0;` |
|       - |  2730 | `		/* Extract variable name */` |
|  916498 |  2731 | `		pName = &pGen->pIn->sData;` |
|       - |  2732 | `		/* Advance the stream cursor */` |
|  916498 |  2733 | `		pGen->pIn++;` |
|  916498 |  2734 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  916498 |  2735 | `		if( pEntry == 0 ){` |
|       - |  2736 | `			/* Duplicate name */` |
|  123006 |  2737 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  123006 |  2738 | `			if( zName == 0 ){` |
|     ! 0 |  2739 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2740 | `				return SXERR_ABORT;` |
|       - |  2741 | `			}` |
|       - |  2742 | `			/* Install in the hashtable */` |
|  123006 |  2743 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   61504 |  2744 | `		}else{` |
|       - |  2745 | `			/* Name already available */` |
|  793494 |  2746 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2747 | `		}` |
|  916498 |  2748 | `		p3 = (void *)zName;` |
|       - |  2749 | `	}` |
|  916510 |  2750 | `	iP1 = 0;` |
|  916510 |  2751 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  334132 |  2752 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2753 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  327674 |  2754 | `			iP1 = 1;` |
|  163836 |  2755 | `		}` |
|  167065 |  2756 | `	}` |
|       - |  2757 | `	/* Emit the load instruction */` |
|  916510 |  2758 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  916522 |  2759 | `	while( iVv > 0 ){` |
|      13 |  2760 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2761 | `		iVv--;` |
|       1 |  2762 | `	}` |
|       - |  2763 | `	/* Node successfully compiled */` |
|  916510 |  2764 | `	return SXRET_OK;` |
|  458258 |  2765 |  |
|       - |  2766 | `/*` |
|       - |  2767 | ` * Load a literal.` |
|       - |  2768 | ` */` |
|  644046 |  2769 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2770 |  |
|  644048 |  2771 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2772 | `	ph7_value *pObj;` |
|       - |  2773 | `	SyString *pStr;` |
|       - |  2774 | `	sxu32 nIdx;` |
|       - |  2775 | `	/* Extract token value */` |
|  644048 |  2776 | `	pStr = &pToken->sData;` |
|       - |  2777 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  644048 |  2778 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  136536 |  2779 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2780 | `			/* NULL constant are always indexed at 0 */` |
|   50288 |  2781 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   50288 |  2782 | `			return SXRET_OK;` |
|   86250 |  2783 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2784 | `			/* TRUE constant are always indexed at 1 */` |
|     520 |  2785 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     520 |  2786 | `			return SXRET_OK;` |
|       2 |  2787 | `		}` |
|  601470 |  2788 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  102182 |  2789 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2790 | `			/* FALSE constant are always indexed at 2 */` |
|   38624 |  2791 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   38624 |  2792 | `			return SXRET_OK;` |
|  514722 |  2793 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   91660 |  2794 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2795 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    8782 |  2796 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    8782 |  2797 | `			if( pObj == 0 ){` |
|     ! 0 |  2798 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2799 | `				return SXERR_ABORT;` |
|       - |  2800 | `			}` |
|    8782 |  2801 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2802 | `			/* Emit the load constant instruction */` |
|    8782 |  2803 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    8782 |  2804 | `			return SXRET_OK;` |
|  474871 |  2805 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   29518 |  2806 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  473999 |  2822 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   12308 |  2823 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  467839 |  2824 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   15484 |  2825 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  545826 |  2855 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2856 | `		ph7_value *pLitObj;` |
|       - |  2857 | `		/* Unknown literal,install it in the literal table */` |
|  226694 |  2858 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  226694 |  2859 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2860 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2861 | `			return SXERR_ABORT;` |
|       - |  2862 | `		}` |
|  226694 |  2863 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  226694 |  2864 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  113346 |  2865 | `	}` |
|       - |  2866 | `	/* Emit the load constant instruction */` |
|  545826 |  2867 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  545826 |  2868 | `	return SXRET_OK;` |
|  322025 |  2869 |  |
|       - |  2870 | `/*` |
|       - |  2871 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2872 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2873 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2874 | ` * Otherwise, load the simple literal directly.` |
|       - |  2875 | ` */` |
|  644080 |  2876 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2877 |  |
|       - |  2878 | `	sxi32 rc;` |
|  644082 |  2879 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2880 | `		return SXRET_OK;` |
|       - |  2881 | `	}` |
|       - |  2882 | `	/* Check if this is a multi-token namespace path */` |
|  644082 |  2883 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
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
|  644048 |  2936 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  644048 |  2937 | `	return rc;` |
|  322042 |  2938 |  |
|       - |  2939 | `/*` |
|       - |  2940 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2941 | ` */` |
|  644080 |  2942 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2943 |  |
|       - |  2944 | `	sxi32 rc;` |
|  644082 |  2945 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  644082 |  2946 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2947 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2948 | `		return rc;` |
|       - |  2949 | `	}` |
|       - |  2950 | `	/* Node successfully compiled */` |
|  644082 |  2951 | `	return SXRET_OK;` |
|  322042 |  2952 |  |
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
|    3054 |  3112 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3113 |  |
|    3056 |  3114 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17870 |  3115 | `	while( pBlock && pBlock != pTarget ){` |
|   14816 |  3116 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   14816 |  3128 | `		pBlock = pBlock->pParent;` |
|       2 |  3129 | `	}` |
|    3056 |  3130 |  |
|    2970 |  3131 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3132 |  |
|       - |  3133 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3134 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3135 | `	sxu32 nLineLocal;` |
|       - |  3136 | `	sxi32 rc;` |
|    2972 |  3137 | `	nLineLocal = pGen->pIn->nLine;` |
|    2972 |  3138 | `	iLevel = 0;` |
|       - |  3139 | `	/* Jump the 'continue' keyword */` |
|    2972 |  3140 | `	pGen->pIn++;` |
|    2972 |  3141 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2972 |  3167 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2972 |  3168 | `	if( pLoop == 0 ){` |
|       - |  3169 | `		/* Illegal continue */` |
|      11 |  3170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3171 | `		if( rc == SXERR_ABORT ){` |
|       - |  3172 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3173 | `			return SXERR_ABORT;` |
|       - |  3174 | `		}` |
|       6 |  3175 | `	}else{` |
|    2962 |  3176 | `		sxu32 nInstrIdx = 0;` |
|       - |  3177 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2962 |  3178 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2962 |  3179 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2958 |  3191 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2958 |  3192 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3193 | `				JumpFixup sJumpFix;` |
|       - |  3194 | `				/* Post-continue */` |
|      14 |  3195 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3196 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3197 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3198 | `			}` |
|       - |  3199 | `		}` |
|       - |  3200 | `	}` |
|    2972 |  3201 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3202 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3203 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3204 | `	}` |
|       - |  3205 | `	/* Statement successfully compiled */` |
|    2972 |  3206 | `	return SXRET_OK;` |
|    1487 |  3207 |  |
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
|  354544 |  3484 | `static sxi32 PH7_CompileBlock(` |
|       - |  3485 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3486 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3487 | `	)` |
|       2 |  3488 |  |
|       - |  3489 | `	sxi32 rc;` |
|       - |  3490 | `	sxu32 nLine;` |
|  354546 |  3491 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  353138 |  3492 | `		nLine = pGen->pIn->nLine;` |
|  353138 |  3493 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  353138 |  3494 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3495 | `			return SXERR_ABORT;` |
|       - |  3496 | `		}` |
|  353138 |  3497 | `		pGen->pIn++;` |
|       - |  3498 | `		/* Compile until we hit the closing braces '}' */` |
|  482495 |  3499 | `		for(;;){` |
|  964992 |  3500 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  964972 |  3511 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3512 | `				/* Closing braces found,break immediately*/` |
|  353118 |  3513 | `				pGen->pIn++;` |
|  353118 |  3514 | `				break;` |
|       - |  3515 | `			}` |
|       - |  3516 | `			/* Compile a single statement */` |
|  611856 |  3517 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  611856 |  3518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3519 | `				return SXERR_ABORT;` |
|       - |  3520 | `			}` |
|       2 |  3521 | `		}` |
|  353138 |  3522 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  177978 |  3523 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|  354546 |  3573 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3574 | `		pGen->pIn++;` |
|     ! 0 |  3575 | `	}` |
|  354546 |  3576 | `	return SXRET_OK;` |
|  177274 |  3577 |  |
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
|   11806 |  3597 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3598 |  |
|   11808 |  3599 | `	GenBlock *pWhileBlock = 0;` |
|   11808 |  3600 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3601 | `	sxu32 nFalseJump;` |
|       - |  3602 | `	sxu32 nLine;` |
|       - |  3603 | `	sxi32 rc;` |
|   11808 |  3604 | `	nLine = pGen->pIn->nLine;` |
|       - |  3605 | `	/* Jump the 'while' keyword */` |
|   11808 |  3606 | `	pGen->pIn++;` |
|   11808 |  3607 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3608 | `		/* Syntax error */` |
|     ! 0 |  3609 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3610 | `		if( rc == SXERR_ABORT ){` |
|       - |  3611 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3612 | `			return SXERR_ABORT;` |
|       - |  3613 | `		}` |
|     ! 0 |  3614 | `		goto Synchronize;` |
|       - |  3615 | `	}` |
|       - |  3616 | `	/* Jump the left parenthesis '(' */` |
|   11808 |  3617 | `	pGen->pIn++;` |
|       - |  3618 | `	/* Create the loop block */` |
|   11808 |  3619 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11808 |  3620 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3621 | `		return SXERR_ABORT;` |
|       - |  3622 | `	}` |
|       - |  3623 | `	/* Delimit the condition */` |
|   11808 |  3624 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11808 |  3625 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3626 | `		/* Empty expression */` |
|       3 |  3627 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3628 | `		if( rc == SXERR_ABORT ){` |
|       - |  3629 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3630 | `			return SXERR_ABORT;` |
|       - |  3631 | `		}` |
|       1 |  3632 | `	}` |
|       - |  3633 | `	/* Swap token streams */` |
|   11808 |  3634 | `	pTmp = pGen->pEnd;` |
|   11808 |  3635 | `	pGen->pEnd = pEnd;` |
|       - |  3636 | `	/* Compile the expression */` |
|   11808 |  3637 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11808 |  3638 | `	if( rc == SXERR_ABORT ){` |
|       - |  3639 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3640 | `		return SXERR_ABORT;` |
|       - |  3641 | `	}` |
|       - |  3642 | `	/* Update token stream */` |
|   11808 |  3643 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3644 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3645 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3646 | `			return SXERR_ABORT;` |
|       - |  3647 | `		}` |
|     ! 0 |  3648 | `		pGen->pIn++;` |
|     ! 0 |  3649 | `	}` |
|       - |  3650 | `	/* Synchronize pointers */` |
|   11808 |  3651 | `	pGen->pIn  = &pEnd[1];` |
|   11808 |  3652 | `	pGen->pEnd = pTmp;` |
|       - |  3653 | `	/* Emit the false jump */` |
|   11808 |  3654 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3655 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11808 |  3656 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3657 | `	/* Compile the loop body */` |
|   11808 |  3658 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11808 |  3659 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3660 | `		return SXERR_ABORT;` |
|       - |  3661 | `	}` |
|       - |  3662 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11808 |  3663 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3664 | `	/* Fix all jumps now the destination is resolved */` |
|   11808 |  3665 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3666 | `	/* Release the loop block */` |
|   11808 |  3667 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3668 | `	/* Statement successfully compiled */` |
|   11808 |  3669 | `	return SXRET_OK;` |
|     ! 0 |  3670 | `Synchronize:` |
|       - |  3671 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3672 | `	 * compiling this erroneous block.` |
|       - |  3673 | `	 */` |
|     ! 0 |  3674 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3675 | `		pGen->pIn++;` |
|     ! 0 |  3676 | `	}` |
|     ! 0 |  3677 | `	return SXRET_OK;` |
|    5905 |  3678 |  |
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
|   11810 |  3826 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3827 |  |
|   11812 |  3828 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11812 |  3829 | `	GenBlock *pForBlock = 0;` |
|       - |  3830 | `	sxu32 nFalseJump;` |
|       - |  3831 | `	sxu32 nLine;` |
|       - |  3832 | `	sxi32 rc;` |
|   11812 |  3833 | `	nLine = pGen->pIn->nLine;` |
|       - |  3834 | `	/* Jump the 'for' keyword */` |
|   11812 |  3835 | `	pGen->pIn++;` |
|   11812 |  3836 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3837 | `		/* Syntax error */` |
|     ! 0 |  3838 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3839 | `		if( rc == SXERR_ABORT ){` |
|       - |  3840 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3841 | `			return SXERR_ABORT;` |
|       - |  3842 | `		}` |
|     ! 0 |  3843 | `		return SXRET_OK;` |
|       - |  3844 | `	}` |
|       - |  3845 | `	/* Jump the left parenthesis '(' */` |
|   11812 |  3846 | `	pGen->pIn++;` |
|       - |  3847 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11812 |  3848 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11812 |  3849 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   11812 |  3864 | `	pTmp = pGen->pEnd;` |
|   11812 |  3865 | `	pGen->pEnd = pEnd;` |
|       - |  3866 | `	/* Compile initialization expressions if available */` |
|   11812 |  3867 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3868 | `	/* Pop operand lvalues */` |
|   11812 |  3869 | `	if( rc == SXERR_ABORT ){` |
|       - |  3870 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3871 | `		return SXERR_ABORT;` |
|   11812 |  3872 | `	}else if( rc != SXERR_EMPTY ){` |
|   11810 |  3873 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5904 |  3874 | `	}` |
|   11812 |  3875 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11812 |  3886 | `	pGen->pIn++;` |
|       - |  3887 | `	/* Create the loop block */` |
|   11812 |  3888 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11812 |  3889 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3890 | `		return SXERR_ABORT;` |
|       - |  3891 | `	}` |
|       - |  3892 | `	/* Deffer continue jumps */` |
|   11812 |  3893 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3894 | `	/* Compile the condition */` |
|   11812 |  3895 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11812 |  3896 | `	if( rc == SXERR_ABORT ){` |
|       - |  3897 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3898 | `		return SXERR_ABORT;` |
|   11812 |  3899 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3900 | `		/* Emit the false jump */` |
|   11810 |  3901 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3902 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11810 |  3903 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5904 |  3904 | `	}` |
|   11812 |  3905 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11808 |  3916 | `	pGen->pIn++;` |
|       - |  3917 | `	/* Save the post condition stream */` |
|   11808 |  3918 | `	pPostStart = pGen->pIn;` |
|       - |  3919 | `	/* Compile the loop body */` |
|   11808 |  3920 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11808 |  3921 | `	pGen->pEnd = pTmp;` |
|   11808 |  3922 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11808 |  3923 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3924 | `		return SXERR_ABORT;` |
|       - |  3925 | `	}` |
|       - |  3926 | `	/* Fix post-continue jumps */` |
|   11808 |  3927 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   11808 |  3943 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3944 | `		pPostStart++;` |
|     ! 0 |  3945 | `	}` |
|   11808 |  3946 | `	if( pPostStart < pEnd ){` |
|       - |  3947 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11808 |  3948 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11808 |  3949 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11808 |  3950 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3951 | `			/* Syntax error */` |
|     ! 0 |  3952 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  3953 | `			if( rc == SXERR_ABORT ){` |
|       - |  3954 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3955 | `				return SXERR_ABORT;` |
|       - |  3956 | `			}` |
|     ! 0 |  3957 | `			return SXRET_OK;` |
|       - |  3958 | `		}` |
|   11808 |  3959 | `		RE_SWAP_DELIMITER(pGen);` |
|   11808 |  3960 | `		if( rc == SXERR_ABORT ){` |
|       - |  3961 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3962 | `			return SXERR_ABORT;` |
|   11808 |  3963 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  3964 | `			/* Pop operand lvalue */` |
|   11808 |  3965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5903 |  3966 | `		}` |
|    5903 |  3967 | `	}` |
|       - |  3968 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11808 |  3969 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  3970 | `	/* Fix all jumps now the destination is resolved */` |
|   11808 |  3971 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3972 | `	/* Release the loop block */` |
|   11808 |  3973 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3974 | `	/* Statement successfully compiled */` |
|   11808 |  3975 | `	return SXRET_OK;` |
|    5907 |  3976 |  |
|       - |  3977 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  3978 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  3979 | ` * are allowed.` |
|       - |  3980 | ` */` |
|    6270 |  3981 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  3982 |  |
|    6272 |  3983 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6272 |  3984 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  3985 | `		/* Unexpected expression */` |
|     ! 0 |  3986 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  3987 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  3988 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  3989 | `			rc = SXERR_INVALID;` |
|     ! 0 |  3990 | `		}` |
|     ! 0 |  3991 | `	}` |
|    6272 |  3992 | `	return rc;` |
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
|    3192 |  4020 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  4021 |  |
|    3194 |  4022 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3194 |  4023 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3194 |  4024 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4025 | `	ph7_foreach_info *pInfo;` |
|       - |  4026 | `	sxu32 nFalseJump;` |
|       - |  4027 | `	VmInstr *pInstr;` |
|       - |  4028 | `	sxu32 nLine;` |
|       - |  4029 | `	sxi32 rc;` |
|    3194 |  4030 | `	nLine = pGen->pIn->nLine;` |
|       - |  4031 | `	/* Jump the 'foreach' keyword */` |
|    3194 |  4032 | `	pGen->pIn++;` |
|    3194 |  4033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4034 | `		/* Syntax error */` |
|     ! 0 |  4035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4036 | `		if( rc == SXERR_ABORT ){` |
|       - |  4037 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4038 | `			return SXERR_ABORT;` |
|       - |  4039 | `		}` |
|     ! 0 |  4040 | `		goto Synchronize;` |
|       - |  4041 | `	}` |
|       - |  4042 | `	/* Jump the left parenthesis '(' */` |
|    3194 |  4043 | `	pGen->pIn++;` |
|       - |  4044 | `	/* Create the loop block */` |
|    3194 |  4045 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3194 |  4046 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4047 | `		return SXERR_ABORT;` |
|       - |  4048 | `	}` |
|       - |  4049 | `	/* Delimit the expression */` |
|    3194 |  4050 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3194 |  4051 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    3194 |  4066 | `	pCur = pGen->pIn;` |
|   21450 |  4067 | `	while( pCur < pEnd ){` |
|   21450 |  4068 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3204 |  4069 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3204 |  4070 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4071 | `				/* Break with the first 'as' found */` |
|    3194 |  4072 | `				break;` |
|       - |  4073 | `			}` |
|       5 |  4074 | `		}` |
|       - |  4075 | `		/* Advance the stream cursor */` |
|   18258 |  4076 | `		pCur++;` |
|       2 |  4077 | `	}` |
|    3194 |  4078 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4079 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4080 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4081 | `		if( rc == SXERR_ABORT ){` |
|       - |  4082 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4083 | `			return SXERR_ABORT;` |
|       - |  4084 | `		}` |
|     ! 0 |  4085 | `		goto Synchronize;` |
|       - |  4086 | `	}` |
|       - |  4087 | `	/* Swap token streams */` |
|    3194 |  4088 | `	pTmp = pGen->pEnd;` |
|    3194 |  4089 | `	pGen->pEnd = pCur;` |
|    3194 |  4090 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3194 |  4091 | `	if( rc == SXERR_ABORT ){` |
|       - |  4092 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4093 | `		return SXERR_ABORT;` |
|       - |  4094 | `	}` |
|       - |  4095 | `	/* Update token stream */` |
|    3194 |  4096 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4097 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4098 | `		if( rc == SXERR_ABORT ){` |
|       - |  4099 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4100 | `			return SXERR_ABORT;` |
|       - |  4101 | `		}` |
|     ! 0 |  4102 | `		pGen->pIn++;` |
|     ! 0 |  4103 | `	}` |
|    3194 |  4104 | `	pCur++; /* Jump the 'as' keyword */` |
|    3194 |  4105 | `	pGen->pIn = pCur;` |
|    3194 |  4106 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4107 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4108 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4109 | `			return SXERR_ABORT;` |
|       - |  4110 | `		}` |
|     ! 0 |  4111 | `	}` |
|       - |  4112 | `	/* Create the foreach context */` |
|    3194 |  4113 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3194 |  4114 | `	if( pInfo == 0 ){` |
|     ! 0 |  4115 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4116 | `		return SXERR_ABORT;` |
|       - |  4117 | `	}` |
|       - |  4118 | `	/* Zero the structure */` |
|    3194 |  4119 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4120 | `	/* Initialize structure fields */` |
|    3194 |  4121 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4122 | `	/* Check if we have a key field */` |
|    9628 |  4123 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6436 |  4124 | `		pCur++;` |
|       2 |  4125 | `	}` |
|    3194 |  4126 | `	if( pCur < pEnd ){` |
|       - |  4127 | `		/* Compile the expression holding the key name */` |
|    3090 |  4128 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4129 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4130 | `			if( rc == SXERR_ABORT ){` |
|       - |  4131 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4132 | `				return SXERR_ABORT;` |
|       - |  4133 | `			}` |
|     ! 0 |  4134 | `		}else{` |
|    3090 |  4135 | `			pGen->pEnd = pCur;` |
|    3090 |  4136 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3090 |  4137 | `			if( rc == SXERR_ABORT ){` |
|       - |  4138 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4139 | `				return SXERR_ABORT;` |
|       - |  4140 | `			}` |
|    3090 |  4141 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3090 |  4142 | `			if( pInstr->p3 ){` |
|       - |  4143 | `				/* Record key name */` |
|    3090 |  4144 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1544 |  4145 | `			}` |
|    3090 |  4146 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4147 | `		}` |
|    3090 |  4148 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1544 |  4149 | `	}` |
|    3194 |  4150 | `	pGen->pEnd = pEnd;` |
|    3194 |  4151 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4152 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4153 | `		if( rc == SXERR_ABORT ){` |
|       - |  4154 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4155 | `			return SXERR_ABORT;` |
|       - |  4156 | `		}` |
|     ! 0 |  4157 | `		goto Synchronize;` |
|       - |  4158 | `	}` |
|    3194 |  4159 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4160 | `		pGen->pIn++;` |
|       - |  4161 | `		/* Pass by reference  */` |
|      11 |  4162 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4163 | `	}` |
|       - |  4164 | `	/* Check if the value target is list() */` |
|    3194 |  4165 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    3189 |  4206 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|    3184 |  4239 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3184 |  4240 | `		if( rc == SXERR_ABORT ){` |
|       - |  4241 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4242 | `			return SXERR_ABORT;` |
|       - |  4243 | `		}` |
|    3184 |  4244 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3184 |  4245 | `		if( pInstr->p3 ){` |
|       - |  4246 | `			/* Record value name */` |
|    3184 |  4247 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1591 |  4248 | `		}` |
|       - |  4249 | `	}` |
|       - |  4250 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3192 |  4251 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4252 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3192 |  4253 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4254 | `	/* Record the first instruction to execute */` |
|    3192 |  4255 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4256 | `	/* Emit the FOREACH_STEP instruction */` |
|    3192 |  4257 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4258 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3192 |  4259 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4260 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3192 |  4261 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    3192 |  4289 | `	pGen->pIn = &pEnd[1];` |
|    3192 |  4290 | `	pGen->pEnd = pTmp;` |
|    3192 |  4291 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3192 |  4292 | `	if( rc == SXERR_ABORT ){` |
|       - |  4293 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4294 | `		return SXERR_ABORT;` |
|       - |  4295 | `	}` |
|       - |  4296 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3192 |  4297 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4298 | `	/* Fix all jumps now the destination is resolved */` |
|    3192 |  4299 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4300 | `	/* Release the loop block */` |
|    3192 |  4301 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4302 | `	/* Statement successfully compiled */` |
|    3192 |  4303 | `	return SXRET_OK;` |
|       1 |  4304 | `Synchronize:` |
|       - |  4305 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4306 | `	 * compiling this erroneous block.` |
|       - |  4307 | `	 */` |
|       3 |  4308 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4309 | `		pGen->pIn++;` |
|     ! 0 |  4310 | `	}` |
|       3 |  4311 | `	return SXRET_OK;` |
|    1598 |  4312 |  |
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
|  123016 |  4345 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4346 |  |
|  123018 |  4347 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  123018 |  4348 | `	GenBlock *pCondBlock = 0;` |
|       - |  4349 | `	sxu32 nJumpIdx;` |
|       - |  4350 | `	sxu32 nKeyID;` |
|       - |  4351 | `	sxi32 rc;` |
|       - |  4352 | `	/* Jump the 'if' keyword */` |
|  123018 |  4353 | `	pGen->pIn++;` |
|  123018 |  4354 | `	pToken = pGen->pIn;` |
|       - |  4355 | `	/* Create the conditional block */` |
|  123018 |  4356 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  123018 |  4357 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4358 | `		return SXERR_ABORT;` |
|       - |  4359 | `	}` |
|       - |  4360 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   67373 |  4361 | `	for(;;){` |
|  134748 |  4362 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  134748 |  4375 | `		pToken++;` |
|       - |  4376 | `		/* Delimit the condition */` |
|  134748 |  4377 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  134748 |  4378 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  134748 |  4391 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4392 | `		/* Compile the condition */` |
|  134748 |  4393 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4394 | `		/* Update token stream */` |
|  134748 |  4395 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4396 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4397 | `			pGen->pIn++;` |
|     ! 0 |  4398 | `		}` |
|  134748 |  4399 | `		pGen->pIn  = &pEnd[1];` |
|  134748 |  4400 | `		pGen->pEnd = pTmp;` |
|  134748 |  4401 | `		if( rc == SXERR_ABORT ){` |
|       - |  4402 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4403 | `			return SXERR_ABORT;` |
|       - |  4404 | `		}` |
|       - |  4405 | `		/* Emit the false jump */` |
|  134748 |  4406 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4407 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  134748 |  4408 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4409 | `		/* Compile the body */` |
|  134748 |  4410 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  134748 |  4411 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4412 | `			return SXERR_ABORT;` |
|       - |  4413 | `		}` |
|  134748 |  4414 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   37596 |  4415 | `			break;` |
|       - |  4416 | `		}` |
|       - |  4417 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   59560 |  4418 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   59560 |  4419 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   38324 |  4420 | `			break;` |
|       - |  4421 | `		}` |
|       - |  4422 | `		/* Emit the unconditional jump */` |
|   21238 |  4423 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4424 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21238 |  4425 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   21238 |  4426 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   15360 |  4427 | `			pToken = &pGen->pIn[1];` |
|   15360 |  4428 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5882 |  4429 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4755 |  4430 | `					break;` |
|       - |  4431 | `			}` |
|    5854 |  4432 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2926 |  4433 | `		}` |
|   11732 |  4434 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4435 | `		/* Synchronize cursors */` |
|   11732 |  4436 | `		pToken = pGen->pIn;` |
|       - |  4437 | `		/* Fix the false jump */` |
|   11732 |  4438 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4439 | `	} /* For(;;) */` |
|       - |  4440 | `	/* Fix the false jump */` |
|  123018 |  4441 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  123018 |  4442 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   47828 |  4443 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4444 | `			/* Compile the else block */` |
|    9508 |  4445 | `			pGen->pIn++;` |
|    9508 |  4446 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9508 |  4447 | `			if( rc == SXERR_ABORT ){` |
|       - |  4448 |  |
|     ! 0 |  4449 | `				return SXERR_ABORT;` |
|       - |  4450 | `			}` |
|    4753 |  4451 | `	}` |
|  123018 |  4452 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4453 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  123018 |  4454 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4455 | `	/* Release the conditional block */` |
|  123018 |  4456 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4457 | `	/* Statement successfully compiled */` |
|  123018 |  4458 | `	return SXRET_OK;` |
|     ! 0 |  4459 | `Synchronize:` |
|       - |  4460 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4461 | `	 */` |
|     ! 0 |  4462 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4463 | `		pGen->pIn++;` |
|     ! 0 |  4464 | `	}` |
|     ! 0 |  4465 | `	return SXRET_OK;` |
|   61510 |  4466 |  |
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
|  193870 |  4560 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4561 |  |
|  193872 |  4562 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4563 | `	sxi32 rc;` |
|       - |  4564 | `	/* Jump the 'return' keyword */` |
|  193872 |  4565 | `	pGen->pIn++;` |
|  193872 |  4566 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4567 | `		/* Compile the expression */` |
|  193850 |  4568 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  193850 |  4569 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4570 | `			return SXERR_ABORT;` |
|  193850 |  4571 | `		}else if(rc != SXERR_EMPTY ){` |
|  193850 |  4572 | `			nRet = 1;` |
|   96924 |  4573 | `		}` |
|   96924 |  4574 | `	}` |
|       - |  4575 | `	/* Emit the done instruction */` |
|  193872 |  4576 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  193872 |  4577 | `	return SXRET_OK;` |
|   96937 |  4578 |  |
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
|   11872 |  4669 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4670 |  |
|   11874 |  4671 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4672 | `	sxi32 rc;` |
|       - |  4673 | `	/* Jump the 'echo' keyword */` |
|   11874 |  4674 | `	pGen->pIn++;` |
|       - |  4675 | `	/* Compile arguments one after one */` |
|   11874 |  4676 | `	pTmp = pGen->pEnd;` |
|   24896 |  4677 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   13024 |  4678 | `		if( pGen->pIn < pNext ){` |
|   13024 |  4679 | `			pGen->pEnd = pNext;` |
|   13024 |  4680 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   13024 |  4681 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4682 | `				return SXERR_ABORT;` |
|   13024 |  4683 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4684 | `				/* Emit the consume instruction */` |
|   13000 |  4685 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6499 |  4686 | `			}` |
|    6511 |  4687 | `		}` |
|       - |  4688 | `		/* Jump trailing commas */` |
|   14174 |  4689 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    1152 |  4690 | `			pNext++;` |
|       2 |  4691 | `		}` |
|   13024 |  4692 | `		pGen->pIn = pNext;` |
|       2 |  4693 | `	}` |
|       - |  4694 | `	/* Restore token stream */` |
|   11874 |  4695 | `	pGen->pEnd = pTmp;` |
|   11874 |  4696 | `	return SXRET_OK;` |
|    5938 |  4697 |  |
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
|  361610 |  4864 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
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
|  361612 |  4875 | `	if( pFromImport ){` |
|  345998 |  4876 | `		*pFromImport = 0;` |
|  172998 |  4877 | `	}` |
|  361612 |  4878 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  361612 |  4879 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4880 | `		return nOrigIdx;` |
|       - |  4881 | `	}` |
|  361612 |  4882 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  361612 |  4883 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4884 | `	/* Skip if already qualified (contains backslash) */` |
|  361612 |  4885 | `	hasNsSep = 0;` |
| 3911108 |  4886 | `	for( k = 0; k < nLit; k++ ){` |
| 3549506 |  4887 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1774750 |  4888 | `	}` |
|  361612 |  4889 | `	if( hasNsSep ){` |
|       9 |  4890 | `		return nOrigIdx;` |
|       - |  4891 | `	}` |
|       - |  4892 | `	/* Check use imports first (works even outside namespaces) */` |
|  361604 |  4893 | `	SyBlobReset(&pGen->sWorker);` |
|  361604 |  4894 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  361604 |  4895 | `	if( pImport ){` |
|      38 |  4896 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4897 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4898 | `		if( pFromImport ){` |
|      18 |  4899 | `			*pFromImport = 1;` |
|       8 |  4900 | `		}` |
|      20 |  4901 | `	}else{` |
|  361568 |  4902 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  361478 |  4903 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
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
|  180807 |  4922 |  |
|       - |  4923 | `/*` |
|       - |  4924 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4925 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4926 | ` */` |
|   32642 |  4927 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4928 |  |
|       - |  4929 | `	SyHashEntry *pImport;` |
|       - |  4930 | `	/* Check use imports first */` |
|   32644 |  4931 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   32644 |  4932 | `	if( pImport ){` |
|      14 |  4933 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  4934 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  4935 | `		return;` |
|       - |  4936 | `	}` |
|       - |  4937 | `	/* Prepend current namespace if active */` |
|   32632 |  4938 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4939 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4940 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4941 | `	}` |
|   32632 |  4942 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   16323 |  4943 |  |
|       - |  4944 | `/*` |
|       - |  4945 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4946 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4947 | ` * The caller must release pOut when done.` |
|       - |  4948 | ` */` |
|   53536 |  4949 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4950 |  |
|   53538 |  4951 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      60 |  4952 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      60 |  4953 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  4954 | `	}` |
|   53538 |  4955 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   53538 |  4956 |  |
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
|       - |  5240 | `/*` |
|       - |  5241 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5242 | ` */` |
|      36 |  5243 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       2 |  5244 |  |
|      52 |  5245 | `	return SyStringLength(pName) == nWant` |
|      36 |  5246 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       2 |  5247 |  |
|       - |  5248 |  |
|      24 |  5249 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       2 |  5250 |  |
|      26 |  5251 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      26 |  5252 | `	SyToken *pBodyEnd = 0;` |
|       - |  5253 | `	SyToken *pBodyStart;` |
|       - |  5254 | `	SyToken *pCursor;` |
|       - |  5255 | `	int bHasStrictTypes;` |
|       - |  5256 | `	int bBlockForm;` |
|       - |  5257 | `	int bPlacementOk;` |
|       - |  5258 | `	sxi32 rc;` |
|      26 |  5259 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      26 |  5260 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5261 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5262 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5263 | `			return SXERR_ABORT;` |
|       - |  5264 | `		}` |
|       5 |  5265 | `		goto Synchro;` |
|       - |  5266 | `	}` |
|      22 |  5267 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      22 |  5268 | `	pBodyStart = pGen->pIn;` |
|       - |  5269 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      22 |  5270 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      22 |  5271 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5272 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5273 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5274 | `			return SXERR_ABORT;` |
|       - |  5275 | `		}` |
|     ! 0 |  5276 | `		return SXRET_OK;` |
|       - |  5277 | `	}` |
|       - |  5278 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5279 | `	 * now delimits the comma-separated directive list. */` |
|      22 |  5280 | `	pGen->pIn = &pBodyEnd[1];` |
|      22 |  5281 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5282 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5283 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5284 | `			return SXERR_ABORT;` |
|       - |  5285 | `		}` |
|     ! 0 |  5286 | `	}` |
|      22 |  5287 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      22 |  5288 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      22 |  5289 | `	bHasStrictTypes = 0;` |
|       - |  5290 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5291 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5292 | `	 * directive appears anywhere in the list, before validating values. */` |
|      22 |  5293 | `	pCursor = pBodyStart;` |
|      34 |  5294 | `	while( pCursor < pBodyEnd ){` |
|      30 |  5295 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      22 |  5296 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      18 |  5297 | `				bHasStrictTypes = 1;` |
|      18 |  5298 | `				break;` |
|       - |  5299 | `			}` |
|       2 |  5300 | `		}` |
|      13 |  5301 | `		pCursor++;` |
|       1 |  5302 | `	}` |
|      22 |  5303 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5304 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5305 | `			"strict_types declaration must not use block mode");` |
|       3 |  5306 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5307 | `		return SXRET_OK;` |
|       - |  5308 | `	}` |
|      20 |  5309 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       5 |  5310 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5311 | `			"strict_types declaration must be the very first statement in the script");` |
|       5 |  5312 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       5 |  5313 | `		return SXRET_OK;` |
|       - |  5314 | `	}` |
|       - |  5315 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      16 |  5316 | `	pCursor = pBodyStart;` |
|      30 |  5317 | `	while( pCursor < pBodyEnd ){` |
|       - |  5318 | `		SyToken *pNameTok;` |
|       - |  5319 | `		SyToken *pEqTok;` |
|       - |  5320 | `		SyToken *pValTok;` |
|       - |  5321 | `		SyString *pDirName;` |
|       - |  5322 | `		int bIsStrict;` |
|       - |  5323 | `		int iStrictValue;` |
|      18 |  5324 | `		pNameTok = pCursor;` |
|      18 |  5325 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5326 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5327 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5328 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5329 | `			return SXRET_OK;` |
|       - |  5330 | `		}` |
|      18 |  5331 | `		pEqTok = pNameTok + 1;` |
|      18 |  5332 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5333 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5334 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5335 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5336 | `			return SXRET_OK;` |
|       - |  5337 | `		}` |
|      18 |  5338 | `		pValTok = pEqTok + 1;` |
|      18 |  5339 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5340 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5341 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5342 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5343 | `			return SXRET_OK;` |
|       - |  5344 | `		}` |
|      18 |  5345 | `		pDirName = &pNameTok->sData;` |
|      18 |  5346 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      18 |  5347 | `		if( bIsStrict ){` |
|       - |  5348 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5349 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      14 |  5350 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5351 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5352 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5353 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5354 | `				return SXRET_OK;` |
|       - |  5355 | `			}` |
|      14 |  5356 | `			iStrictValue = -1;` |
|      14 |  5357 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      14 |  5358 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      14 |  5359 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      14 |  5360 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      12 |  5361 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       6 |  5362 | `			}` |
|      14 |  5363 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5364 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5365 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5366 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5367 | `				return SXRET_OK;` |
|       - |  5368 | `			}` |
|      12 |  5369 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       7 |  5370 | `		}else{` |
|       - |  5371 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5372 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5373 | `			 * behavior don't regress. */` |
|       7 |  5374 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5375 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5376 | `				ph7_lib_version()` |
|       - |  5377 | `				);` |
|       - |  5378 | `		}` |
|      16 |  5379 | `		pCursor = pValTok + 1;` |
|       - |  5380 | `		/* Consume separating comma (or end). */` |
|      16 |  5381 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5382 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5383 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5384 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5385 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5386 | `				return SXRET_OK;` |
|       - |  5387 | `			}` |
|       3 |  5388 | `			pCursor++;` |
|       1 |  5389 | `		}` |
|       2 |  5390 | `	}` |
|       - |  5391 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5392 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5393 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      14 |  5394 | `	return SXRET_OK;` |
|       2 |  5395 | `Synchro:` |
|       - |  5396 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5397 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5398 | `		pGen->pIn++;` |
|       1 |  5399 | `	}` |
|       5 |  5400 | `	return SXRET_OK;` |
|      14 |  5401 |  |
|       - |  5402 | `/*` |
|       - |  5403 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5404 | ` * as follows:` |
|       - |  5405 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5406 | ` * {` |
|       - |  5407 | ` *   return "Making a cup of $type.\n";` |
|       - |  5408 | ` * }` |
|       - |  5409 | ` * Symisc eXtension.` |
|       - |  5410 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5411 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5412 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5413 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5414 | ` *      {` |
|       - |  5415 | ` *       var_dump($a);` |
|       - |  5416 | ` *      }` |
|       - |  5417 | ` *     //call test without args` |
|       - |  5418 | ` *      test();` |
|       - |  5419 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5420 | ` *      Example:` |
|       - |  5421 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5422 | ` * 3 -) Function overloading!!` |
|       - |  5423 | ` *      Example:` |
|       - |  5424 | ` *      function foo($a) {` |
|       - |  5425 | ` *   	  return $a.PHP_EOL;` |
|       - |  5426 | ` *	    }` |
|       - |  5427 | ` *	    function foo($a, $b) {` |
|       - |  5428 | ` *   	  return $a + $b;` |
|       - |  5429 | ` *	    }` |
|       - |  5430 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5431 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5432 | ` *      // Same arg` |
|       - |  5433 | ` *	   function foo(string $a)` |
|       - |  5434 | ` *	   {` |
|       - |  5435 | ` *	     echo "a is a string\n";` |
|       - |  5436 | ` *	     var_dump($a);` |
|       - |  5437 | ` *	   }` |
|       - |  5438 | ` *	  function foo(int $a)` |
|       - |  5439 | ` *	  {` |
|       - |  5440 | ` *	    echo "a is integer\n";` |
|       - |  5441 | ` *	    var_dump($a);` |
|       - |  5442 | ` *	  }` |
|       - |  5443 | ` *	  function foo(array $a)` |
|       - |  5444 | ` *	  {` |
|       - |  5445 | ` * 	    echo "a is an array\n";` |
|       - |  5446 | ` * 	    var_dump($a);` |
|       - |  5447 | ` *	  }` |
|       - |  5448 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5449 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5450 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5451 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5452 | ` * introduced by the PH7 engine.` |
|       - |  5453 | ` */` |
|   55624 |  5454 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5455 |  |
|       - |  5456 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5457 | `	SySet *pInstrContainer;` |
|       - |  5458 | `	sxi32 rc;` |
|       - |  5459 | `	/* Swap token stream */` |
|   55626 |  5460 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   55626 |  5461 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   55626 |  5462 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5463 | `	/* Compile the expression holding the argument value */` |
|   55626 |  5464 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5465 | `	/* Emit the done instruction */` |
|   55626 |  5466 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   55626 |  5467 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   55626 |  5468 | `	RE_SWAP_DELIMITER(pGen);` |
|   55626 |  5469 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5470 | `		return SXERR_ABORT;` |
|       - |  5471 | `	}` |
|   55626 |  5472 | `	return SXRET_OK;` |
|   27814 |  5473 |  |
|       - |  5474 | `/*` |
|       - |  5475 | ` * Collect function arguments one after one.` |
|       - |  5476 | ` * According to the PHP language reference manual.` |
|       - |  5477 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5478 | ` * list of expressions.` |
|       - |  5479 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5480 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5481 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5482 | ` * for more information.` |
|       - |  5483 | ` * Example #1 Passing arrays to functions` |
|       - |  5484 | ` * <?php` |
|       - |  5485 | ` * function takes_array($input)` |
|       - |  5486 | ` * {` |
|       - |  5487 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5488 | ` * }` |
|       - |  5489 | ` * ?>` |
|       - |  5490 | ` * Making arguments be passed by reference` |
|       - |  5491 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5492 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5493 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5494 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5495 | ` * to the argument name in the function definition:` |
|       - |  5496 | ` * Example #2 Passing function parameters by reference` |
|       - |  5497 | ` * <?php` |
|       - |  5498 | ` * function add_some_extra(&$string)` |
|       - |  5499 | ` * {` |
|       - |  5500 | ` *   $string .= 'and something extra.';` |
|       - |  5501 | ` * }` |
|       - |  5502 | ` * $str = 'This is a string, ';` |
|       - |  5503 | ` * add_some_extra($str);` |
|       - |  5504 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5505 | ` * ?>` |
|       - |  5506 | ` *` |
|       - |  5507 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5508 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5509 | ` * on these extension.` |
|       - |  5510 | ` */` |
|   59360 |  5511 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       2 |  5512 |  |
|       - |  5513 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5514 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5515 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5516 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5517 | `	sxi32 rc;` |
|       - |  5518 |  |
|   59362 |  5519 | `	pIn = pGen->pIn;` |
|   59362 |  5520 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5521 | `	/* Process arguments one after one */` |
|   77211 |  5522 | `	for(;;){` |
|  154424 |  5523 | `		if( pIn >= pEnd ){` |
|       - |  5524 | `			/* No more arguments to process */` |
|   59350 |  5525 | `			break;` |
|       - |  5526 | `		}` |
|   95076 |  5527 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   95076 |  5528 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   95076 |  5529 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   95076 |  5530 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5531 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|   95076 |  5532 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   52798 |  5533 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   52798 |  5534 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      42 |  5535 | `				if( !bCtorCtx ){` |
|       5 |  5536 | `					if( bAbstractCtx ){` |
|       3 |  5537 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5538 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5539 | `					}else{` |
|       3 |  5540 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5541 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5542 | `					}` |
|       5 |  5543 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5544 | `						return SXERR_ABORT;` |
|       - |  5545 | `					}` |
|       5 |  5546 | `					return SXERR_SYNTAX;` |
|       - |  5547 | `				}` |
|      38 |  5548 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      38 |  5549 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5550 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      37 |  5551 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5552 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5553 | `				}else{` |
|      34 |  5554 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5555 | `				}` |
|      38 |  5556 | `				pIn++;` |
|      18 |  5557 | `			}` |
|   26396 |  5558 | `		}` |
|       - |  5559 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  125883 |  5560 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   79823 |  5561 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   63103 |  5562 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   61610 |  5563 | `			sxu32 nLineLocal = pIn->nLine;` |
|   61610 |  5564 | `			sxi32 iTFlags = 0;` |
|   61610 |  5565 | `			pGen->pIn = pIn;` |
|   61610 |  5566 | `			rc = GenStateParseUnionTypeDecl(` |
|   30804 |  5567 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   30804 |  5568 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5569 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5570 | `				/* bAllowVoid */ 0,` |
|   30804 |  5571 | `						nLineLocal);` |
|   61610 |  5572 | `			pIn = pGen->pIn;` |
|   61610 |  5573 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5574 | `				return SXERR_ABORT;` |
|   61610 |  5575 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5576 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5577 | `				return SXERR_SYNTAX;` |
|   61608 |  5578 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5579 | `				if( pIn < pEnd ){` |
|       7 |  5580 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5581 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5582 | `						&pIn->sData);` |
|       3 |  5583 | `				}else{` |
|     ! 0 |  5584 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5585 | `						"syntax error, unexpected end of file");` |
|       - |  5586 | `				}` |
|       5 |  5587 | `				return SXERR_SYNTAX;` |
|       - |  5588 | `			}` |
|   61604 |  5589 | `			sArg.iFlags \|= iTFlags;` |
|   30801 |  5590 | `		}` |
|   95066 |  5591 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5592 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5593 | `			return rc;` |
|       - |  5594 | `		}` |
|   95066 |  5595 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5596 | `			/* Pass by reference,record that */` |
|    2954 |  5597 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2954 |  5598 | `			pIn++;` |
|    1476 |  5599 | `		}` |
|   95066 |  5600 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5601 | `			/* Variadic parameter: ...$args */` |
|      40 |  5602 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      40 |  5603 | `			pIn++;` |
|      19 |  5604 | `		}` |
|   95066 |  5605 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5606 | `			/* Invalid argument */` |
|     ! 0 |  5607 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5608 | `			return rc;` |
|       - |  5609 | `		}` |
|   95066 |  5610 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5611 | `		/* Copy argument name */` |
|   95066 |  5612 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   95066 |  5613 | `		if( zDup == 0 ){` |
|     ! 0 |  5614 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5615 | `			return SXERR_ABORT;` |
|       - |  5616 | `		}` |
|   95066 |  5617 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   95066 |  5618 | `		pIn++;` |
|   95066 |  5619 | `		if( pIn < pEnd ){` |
|   62078 |  5620 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5621 | `				SyToken *pDefend;` |
|   55628 |  5622 | `				sxi32 iNest = 0;` |
|   55628 |  5623 | `				pIn++; /* Jump the equal sign */` |
|   55628 |  5624 | `				pDefend = pIn;` |
|       - |  5625 | `				/* Process the default value associated with this argument */` |
|  117104 |  5626 | `				while( pDefend < pEnd ){` |
|   90746 |  5627 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   29270 |  5628 | `						break;` |
|       - |  5629 | `					}` |
|   61478 |  5630 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5631 | `						/* Increment nesting level */` |
|    2928 |  5632 | `						iNest++;` |
|   60015 |  5633 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5634 | `						/* Decrement nesting level */` |
|    2928 |  5635 | `						iNest--;` |
|    1463 |  5636 | `					}` |
|   61478 |  5637 | `					pDefend++;` |
|       2 |  5638 | `				}` |
|   55628 |  5639 | `				if( pIn >= pDefend ){` |
|       3 |  5640 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5641 | `					return rc;` |
|       - |  5642 | `				}` |
|       - |  5643 | `				/* Process default value */` |
|   55626 |  5644 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   55626 |  5645 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5646 | `					return rc;` |
|       - |  5647 | `				}` |
|       - |  5648 | `				/* Point beyond the default value */` |
|   55626 |  5649 | `				pIn = pDefend;` |
|   27812 |  5650 | `			}` |
|   62076 |  5651 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5652 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5653 | `				return rc;` |
|       - |  5654 | `			}` |
|   62076 |  5655 | `			pIn++; /* Jump the trailing comma */` |
|   31037 |  5656 | `		}` |
|       - |  5657 | `		/* Append argument signature */` |
|   95064 |  5658 | `		if( sArg.nType > 0 ){` |
|   61564 |  5659 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5660 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    8796 |  5661 | `				int marker = 'o';` |
|    8796 |  5662 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    8796 |  5663 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4399 |  5664 | `			}else{` |
|       - |  5665 | `				int c;` |
|   52770 |  5666 | `				c = 'n'; /* cc warning */` |
|       - |  5667 | `				/* Type leading character */` |
|   52770 |  5668 | `				switch(sArg.nType){` |
|     ! 0 |  5669 | `				case MEMOBJ_HASHMAP:` |
|       - |  5670 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5671 | `					c = 'h';` |
|     ! 0 |  5672 | `					break;` |
|    7343 |  5673 | `				case MEMOBJ_INT:` |
|       - |  5674 | `					/* Integer */` |
|   14688 |  5675 | `					c = 'i';` |
|   14688 |  5676 | `					break;` |
|     ! 0 |  5677 | `				case MEMOBJ_BOOL:` |
|       - |  5678 | `					/* Bool */` |
|     ! 0 |  5679 | `					c = 'b';` |
|     ! 0 |  5680 | `					break;` |
|     ! 0 |  5681 | `				case MEMOBJ_REAL:` |
|       - |  5682 | `					/* Float */` |
|     ! 0 |  5683 | `					c = 'f';` |
|     ! 0 |  5684 | `					break;` |
|   19034 |  5685 | `				case MEMOBJ_STRING:` |
|       - |  5686 | `					/* String */` |
|   38070 |  5687 | `					c = 's';` |
|   38070 |  5688 | `					break;` |
|       7 |  5689 | `				case MEMOBJ_OBJ:` |
|       - |  5690 | `					/* Object */` |
|      16 |  5691 | `					c = 'o';` |
|      14 |  5692 | `					break;` |
|     ! 0 |  5693 | `				default:` |
|     ! 0 |  5694 | `					break;` |
|       - |  5695 | `				}` |
|   52770 |  5696 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5697 | `			}` |
|   30783 |  5698 | `		}else{` |
|       - |  5699 | `			/* No type is associated with this parameter which mean` |
|       - |  5700 | `			 * that this function is not condidate for overloading.` |
|       - |  5701 | `			 */` |
|   33502 |  5702 | `			SyBlobRelease(&sSig);` |
|       - |  5703 | `		}` |
|       - |  5704 | `		/* Save in the argument set */` |
|   95064 |  5705 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5706 | `	}` |
|   59350 |  5707 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5708 | `		/* Save function signature */` |
|   38136 |  5709 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   19067 |  5710 | `	}` |
|   59350 |  5711 | `	return SXRET_OK;` |
|   29682 |  5712 |  |
|       - |  5713 | `/*` |
|       - |  5714 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5715 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5716 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5717 | ` */` |
|  182758 |  5718 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5719 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5720 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5721 | `	)` |
|       2 |  5722 |  |
|       - |  5723 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5724 | `	GenBlock *pBlock;` |
|       - |  5725 | `	sxu32 nGotoOfft;` |
|       - |  5726 | `	sxi32 rc;` |
|       - |  5727 | `	/* Attach the new function */` |
|  182760 |  5728 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  182760 |  5729 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5730 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5731 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5732 | `		return SXERR_ABORT;` |
|       - |  5733 | `	}` |
|  182760 |  5734 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5735 | `	/* Swap bytecode containers */` |
|  182760 |  5736 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  182760 |  5737 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5738 | `	/* Emit constructor property promotion prologue:` |
|       - |  5739 | `	 *   $this->NAME = $NAME;` |
|       - |  5740 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5741 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5742 | `	{` |
|  182760 |  5743 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5744 | `		sxu32 i;` |
|  274818 |  5745 | `		for( i = 0; i < nArg; i++ ){` |
|   92060 |  5746 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5747 | `			char *zSrc;` |
|       - |  5748 | `			sxu32 nSrc,nName;` |
|       - |  5749 | `			SySet sToken;` |
|       - |  5750 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5751 | `			sxi32 rcPromote;` |
|   92060 |  5752 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   92032 |  5753 | `				continue;` |
|       - |  5754 | `			}` |
|       - |  5755 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5756 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5757 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5758 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5759 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      30 |  5760 | `			nName = SyStringLength(&pArg->sName);` |
|      30 |  5761 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      30 |  5762 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      30 |  5763 | `			if( zSrc == 0 ){` |
|     ! 0 |  5764 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5765 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5766 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5767 | `				return SXERR_ABORT;` |
|       - |  5768 | `			}` |
|       - |  5769 | `			{` |
|      30 |  5770 | `				char *z = zSrc;` |
|      30 |  5771 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      30 |  5772 | `				z += sizeof("$this->")-1;` |
|      30 |  5773 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5774 | `				z += nName;` |
|      30 |  5775 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      30 |  5776 | `				z += sizeof(" = $")-1;` |
|      30 |  5777 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5778 | `				z += nName;` |
|      30 |  5779 | `				*z = 0;` |
|       - |  5780 | `			}` |
|      30 |  5781 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      30 |  5782 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      30 |  5783 | `			pTmpIn = pGen->pIn;` |
|      30 |  5784 | `			pTmpEnd = pGen->pEnd;` |
|      30 |  5785 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      30 |  5786 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      30 |  5787 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  5788 | `			pGen->pIn = pTmpIn;` |
|      30 |  5789 | `			pGen->pEnd = pTmpEnd;` |
|      30 |  5790 | `			SySetRelease(&sToken);` |
|      30 |  5791 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5792 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5793 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5794 | `				return SXERR_ABORT;` |
|       - |  5795 | `			}` |
|       - |  5796 | `			/* Discard the assignment result — this is a statement expression. */` |
|      30 |  5797 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      16 |  5798 | `		}` |
|       - |  5799 | `	}` |
|       - |  5800 | `	/* Compile the body */` |
|  182760 |  5801 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5802 | `	/* Fix exception jumps now the destination is resolved */` |
|  182760 |  5803 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5804 | `	/* Emit the final return if not yet done */` |
|  182760 |  5805 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5806 | `	/* Fix gotos jumps now the destination is resolved */` |
|  182760 |  5807 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5808 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5809 | `	}` |
|  182760 |  5810 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5811 | `	/* Restore the default container */` |
|  182760 |  5812 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5813 | `	/* Leave function block */` |
|  182760 |  5814 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  182760 |  5815 | `	if( rc == SXERR_ABORT ){` |
|       - |  5816 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5817 | `		return SXERR_ABORT;` |
|       - |  5818 | `	}` |
|       - |  5819 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5820 | `	{` |
|  182760 |  5821 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5822 | `		sxu32 i;` |
| 3574056 |  5823 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3391316 |  5824 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5825 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5826 | `				break;` |
|       - |  5827 | `			}` |
| 1695650 |  5828 | `		}` |
|       - |  5829 | `	}` |
|       - |  5830 | `	/* All done, function body compiled */` |
|  182760 |  5831 | `	return SXRET_OK;` |
|   91381 |  5832 |  |
|       - |  5833 | `/*` |
|       - |  5834 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5835 | ` * According to the PHP language reference manual.` |
|       - |  5836 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5837 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5838 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5839 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5840 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5841 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5842 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5843 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5844 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5845 | ` *` |
|       - |  5846 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5847 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5848 | ` * on these extension.` |
|       - |  5849 | ` */` |
|       - |  5850 | `/*` |
|       - |  5851 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5852 | ` */` |
|      76 |  5853 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5854 |  |
|       - |  5855 | `	sxu32 i;` |
|     238 |  5856 | `	for( i = 0; i < n; i++ ){` |
|     212 |  5857 | `		int a = zA[i], b = zB[i];` |
|     212 |  5858 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     212 |  5859 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     212 |  5860 | `		if( a != b ) return a - b;` |
|      82 |  5861 | `	}` |
|      28 |  5862 | `	return 0;` |
|      40 |  5863 |  |
|       - |  5864 | `/*` |
|       - |  5865 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5866 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5867 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5868 | ` */` |
|       - |  5869 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5870 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5871 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5872 |  |
|       - |  5873 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5874 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5875 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5876 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5877 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5878 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5879 |  |
|       - |  5880 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5881 | `struct PhlTypeAtom {` |
|       - |  5882 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5883 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5884 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5885 | `	sxu32 nCanon;` |
|       - |  5886 | `};` |
|       - |  5887 |  |
|       - |  5888 | `/*` |
|       - |  5889 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5890 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5891 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5892 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5893 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5894 | ` * already be consumed by the caller.` |
|       - |  5895 | ` */` |
|   61902 |  5896 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5897 |  |
|   61904 |  5898 | `	SyToken *pIn = pGen->pIn;` |
|   61904 |  5899 | `	SyZero(pOut, sizeof(*pOut));` |
|   61904 |  5900 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   61904 |  5901 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5902 | `		return SXERR_SYNTAX;` |
|       - |  5903 | `	}` |
|       - |  5904 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   61904 |  5905 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5906 | `		pIn++;` |
|       8 |  5907 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5908 | `			return SXERR_SYNTAX;` |
|       - |  5909 | `		}` |
|       3 |  5910 | `	}` |
|   61904 |  5911 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5912 | `		return SXERR_SYNTAX;` |
|       - |  5913 | `	}` |
|   61904 |  5914 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   53046 |  5915 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   53046 |  5916 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5917 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   53039 |  5918 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       8 |  5919 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   53029 |  5920 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   14822 |  5921 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   45616 |  5922 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   38156 |  5923 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   19129 |  5924 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      22 |  5925 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      42 |  5926 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5927 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5928 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5929 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5930 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5931 | `			pOut->sClass = pIn->sData;` |
|       4 |  5932 | `		}else{` |
|       3 |  5933 | `			return SXERR_SYNTAX;` |
|       - |  5934 | `		}` |
|   53044 |  5935 | `		pIn++;` |
|   26523 |  5936 | `	}else{` |
|       - |  5937 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5938 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    8860 |  5939 | `		SyString *pT = &pIn->sData;` |
|    8860 |  5940 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5941 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5942 | `			pIn++;` |
|    8855 |  5943 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      12 |  5944 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      12 |  5945 | `			pIn++;` |
|    8845 |  5946 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5947 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5948 | `			pIn++;` |
|       2 |  5949 | `		}else{` |
|       - |  5950 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    8838 |  5951 | `			SyToken *pFirst = pIn;` |
|    8838 |  5952 | `			SyToken *pLast = pIn;` |
|    8838 |  5953 | `			pOut->nType = SXU32_HIGH;` |
|    8838 |  5954 | `			pOut->sClass = pIn->sData;` |
|    8838 |  5955 | `			pIn++;` |
|   13257 |  5956 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    8841 |  5957 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  5958 | `				pLast = &pIn[1];` |
|       3 |  5959 | `				pIn += 2;` |
|       1 |  5960 | `			}` |
|    8838 |  5961 | `			if( pLast != pFirst ){` |
|       3 |  5962 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  5963 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  5964 | `				pOut->sClass.zString = zFirst;` |
|       3 |  5965 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  5966 | `			}` |
|       - |  5967 | `		}` |
|       - |  5968 | `	}` |
|   61902 |  5969 | `	pGen->pIn = pIn;` |
|   61902 |  5970 | `	return SXRET_OK;` |
|   30953 |  5971 |  |
|       - |  5972 |  |
|       - |  5973 | `/*` |
|       - |  5974 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  5975 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  5976 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  5977 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  5978 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  5979 | ` */` |
|   61806 |  5980 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  5981 |  |
|       - |  5982 | `	int i;` |
|   61808 |  5983 | `	int nNonNull = 0;` |
|  123694 |  5984 | `	for( i = 0; i < nAtoms; i++ ){` |
|   61888 |  5985 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   61878 |  5986 | `			nNonNull++;` |
|   30938 |  5987 | `		}` |
|   30945 |  5988 | `	}` |
|   61808 |  5989 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  5990 | `		/* Shorthand: ?T */` |
|      54 |  5991 | `		for( i = 0; i < nAtoms; i++ ){` |
|      54 |  5992 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      54 |  5993 | `			SyBlobAppend(pBlob, "?", 1);` |
|      54 |  5994 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5995 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  5996 | `			}else{` |
|      44 |  5997 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  5998 | `			}` |
|      54 |  5999 | `			return;` |
|     ! 0 |  6000 | `		}` |
|     ! 0 |  6001 | `	}` |
|       - |  6002 | `	{` |
|   61756 |  6003 | `		int bFirst = 1;` |
|       - |  6004 | `		/* 1) Classes in declaration order */` |
|  123584 |  6005 | `		for( i = 0; i < nAtoms; i++ ){` |
|   61830 |  6006 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    8832 |  6007 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    8832 |  6008 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    8832 |  6009 | `				bFirst = 0;` |
|    4415 |  6010 | `			}` |
|   30916 |  6011 | `		}` |
|       - |  6012 | `		/* 2) Built-ins in canonical order */` |
|       - |  6013 | `		{` |
|       - |  6014 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6015 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6016 | `			int k;` |
|  432280 |  6017 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  688418 |  6018 | `				for( i = 0; i < nAtoms; i++ ){` |
|  370880 |  6019 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   52988 |  6020 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   52988 |  6021 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   52988 |  6022 | `						bFirst = 0;` |
|   52988 |  6023 | `						break;` |
|       - |  6024 | `					}` |
|  158948 |  6025 | `				}` |
|  185264 |  6026 | `			}` |
|       - |  6027 | `		}` |
|       - |  6028 | `		/* 3) null suffix */` |
|   61756 |  6029 | `		if( bNullable ){` |
|       6 |  6030 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  6031 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  6032 | `		}` |
|       - |  6033 | `	}` |
|   30905 |  6034 |  |
|       - |  6035 |  |
|       - |  6036 | `/*` |
|       - |  6037 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6038 | ` *` |
|       - |  6039 | ` * Outputs:` |
|       - |  6040 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6041 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6042 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6043 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6044 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6045 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6046 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6047 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6048 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6049 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6050 | ` *` |
|       - |  6051 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6052 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6053 | ` */` |
|   61816 |  6054 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6055 | `	ph7_gen_state *pGen,` |
|       - |  6056 | `	sxu32 *pnType,` |
|       - |  6057 | `	SyString *pClass,` |
|       - |  6058 | `	SySet *pAlts,` |
|       - |  6059 | `	sxi32 *piTypeFlags,` |
|       - |  6060 | `	SyString *pTypeText,` |
|       - |  6061 | `	int iNullableFlag,` |
|       - |  6062 | `	int iUnionFlag,` |
|       - |  6063 | `	int bAllowVoid,` |
|       - |  6064 | `	sxu32 nLine` |
|       2 |  6065 | `){` |
|       - |  6066 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   61818 |  6067 | `	int nAtoms = 0;` |
|   61818 |  6068 | `	int bShortNullable = 0;` |
|   61818 |  6069 | `	int bExplicitNull = 0;` |
|       - |  6070 | `	sxi32 rc;` |
|   61818 |  6071 | `	*pnType = 0;` |
|   61818 |  6072 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   61818 |  6073 | `	*piTypeFlags = 0;` |
|   61818 |  6074 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6075 |  |
|   61818 |  6076 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6077 | `		return SXRET_OK;` |
|       - |  6078 | `	}` |
|       - |  6079 | ``	/* Optional `?` shorthand prefix */`` |
|   61816 |  6080 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      50 |  6081 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      50 |  6082 | `		bShortNullable = 1;` |
|      50 |  6083 | `		pGen->pIn++;` |
|      50 |  6084 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6085 | `			return SXERR_SYNTAX;` |
|       - |  6086 | `		}` |
|      24 |  6087 | `	}` |
|       - |  6088 | `	/* First atom is mandatory */` |
|   61818 |  6089 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   61818 |  6090 | `	if( rc != SXRET_OK ){` |
|       3 |  6091 | `		return rc;` |
|       - |  6092 | `	}` |
|   61816 |  6093 | `	nAtoms = 1;` |
|       - |  6094 | ``	/* Subsequent atoms separated by `\|` */`` |
|   92852 |  6095 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   61947 |  6096 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      90 |  6097 | `		if( bShortNullable ){` |
|       - |  6098 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6099 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6100 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6101 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6102 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6103 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6104 | `		}` |
|      88 |  6105 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6106 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6107 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6108 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6109 | `		}` |
|      88 |  6110 | ``		pGen->pIn++; /* skip `\|` */`` |
|      88 |  6111 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      88 |  6112 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6113 | `			return rc;` |
|       - |  6114 | `		}` |
|      88 |  6115 | `		nAtoms++;` |
|       2 |  6116 | `	}` |
|       - |  6117 | `	/* Validation pass.` |
|       - |  6118 | `	 *` |
|       - |  6119 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6120 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6121 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6122 | `	 */` |
|       - |  6123 | `	{` |
|       - |  6124 | `		int i, j;` |
|   61814 |  6125 | `		int bHasNonNull = 0;` |
|  123706 |  6126 | `		for( i = 0; i < nAtoms; i++ ){` |
|   61900 |  6127 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      12 |  6128 | `				if( nAtoms > 1 ){` |
|       3 |  6129 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6130 | `						"Void can only be used as a standalone type");` |
|       3 |  6131 | `					return SXERR_SYNTAX;` |
|       - |  6132 | `				}` |
|      10 |  6133 | `				if( !bAllowVoid ){` |
|     ! 0 |  6134 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6135 | `						"void cannot be used here");` |
|     ! 0 |  6136 | `					return SXERR_SYNTAX;` |
|       - |  6137 | `				}` |
|      10 |  6138 | `				if( bShortNullable ){` |
|     ! 0 |  6139 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6140 | `						"Void type cannot be nullable");` |
|     ! 0 |  6141 | `					return SXERR_SYNTAX;` |
|       - |  6142 | `				}` |
|       4 |  6143 | `			}` |
|   61898 |  6144 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6145 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6146 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6147 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6148 | `				 * does not return), and folding them would mislead any` |
|       - |  6149 | `				 * future return-enforcement work. */` |
|       3 |  6150 | `				if( nAtoms > 1 ){` |
|       3 |  6151 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6152 | `						"never can only be used as a standalone type");` |
|       3 |  6153 | `					return SXERR_SYNTAX;` |
|       - |  6154 | `				}` |
|     ! 0 |  6155 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6156 | `					"never type is not yet implemented");` |
|     ! 0 |  6157 | `				return SXERR_SYNTAX;` |
|       - |  6158 | `			}` |
|   61896 |  6159 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  6160 | `				bExplicitNull = 1;` |
|       7 |  6161 | `			}else{` |
|   61886 |  6162 | `				bHasNonNull = 1;` |
|       - |  6163 | `			}` |
|       - |  6164 | `			/* Duplicate detection */` |
|   62012 |  6165 | `			for( j = 0; j < i; j++ ){` |
|     120 |  6166 | `				int bDup = 0;` |
|     120 |  6167 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  6168 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6169 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6170 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6171 | `								aAtoms[j].sClass.zString,` |
|      12 |  6172 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6173 | `							bDup = 1;` |
|     ! 0 |  6174 | `						}` |
|       8 |  6175 | `					}else{` |
|       3 |  6176 | `						bDup = 1;` |
|       - |  6177 | `					}` |
|       7 |  6178 | `				}` |
|     120 |  6179 | `				if( bDup ){` |
|       - |  6180 | `					const char *zName;` |
|       - |  6181 | `					sxu32 nName;` |
|       3 |  6182 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6183 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6184 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6185 | `					}else{` |
|       3 |  6186 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6187 | `						nName = aAtoms[i].nCanon;` |
|       - |  6188 | `					}` |
|       4 |  6189 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6190 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6191 | `					return SXERR_SYNTAX;` |
|       - |  6192 | `				}` |
|      60 |  6193 | `			}` |
|   30948 |  6194 | `		}` |
|   61808 |  6195 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  6196 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6197 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  6198 | `			return SXERR_SYNTAX;` |
|       - |  6199 | `		}` |
|       - |  6200 | `	}` |
|       - |  6201 | `	/* Compute nullability flag */` |
|   61808 |  6202 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      58 |  6203 | `		*piTypeFlags \|= iNullableFlag;` |
|      28 |  6204 | `	}` |
|       - |  6205 | `	/* Build canonical type text */` |
|   61808 |  6206 | `	if( pTypeText ){` |
|       - |  6207 | `		SyBlob sBlob;` |
|   61808 |  6208 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   92688 |  6209 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   30903 |  6210 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   61808 |  6211 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   92699 |  6212 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   61798 |  6213 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   61800 |  6214 | `			if( zDup ){` |
|   61800 |  6215 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   30899 |  6216 | `			}` |
|   30899 |  6217 | `		}` |
|   61808 |  6218 | `		SyBlobRelease(&sBlob);` |
|   30903 |  6219 | `	}` |
|       - |  6220 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6221 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6222 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6223 | `	{` |
|   61808 |  6224 | `		int nNonNull = 0;` |
|   61808 |  6225 | `		int iNonNullIdx = -1;` |
|       - |  6226 | `		int i;` |
|  123694 |  6227 | `		for( i = 0; i < nAtoms; i++ ){` |
|   61888 |  6228 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   61878 |  6229 | `				nNonNull++;` |
|   61878 |  6230 | `				iNonNullIdx = i;` |
|   30938 |  6231 | `			}` |
|   30945 |  6232 | `		}` |
|   61808 |  6233 | `		if( nNonNull <= 1 ){` |
|       - |  6234 | `			/* Fast path: store as single type. */` |
|   61754 |  6235 | `			if( iNonNullIdx >= 0 ){` |
|   61754 |  6236 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   61754 |  6237 | `				if( pA->nType == SXU32_HIGH ){` |
|   13223 |  6238 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4407 |  6239 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    8816 |  6240 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    8816 |  6241 | `					*pnType = SXU32_HIGH;` |
|    8816 |  6242 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   57347 |  6243 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      10 |  6244 | `					*pnType = MEMOBJ_VOID;` |
|       6 |  6245 | `				}else{` |
|       - |  6246 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6247 | `					 * pass above rejects it as not-yet-implemented. */` |
|   52932 |  6248 | `					*pnType = pA->nType;` |
|       - |  6249 | `				}` |
|   30876 |  6250 | `			}` |
|   30878 |  6251 | `		}else{` |
|       - |  6252 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      56 |  6253 | `			*piTypeFlags \|= iUnionFlag;` |
|     184 |  6254 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6255 | `				ph7_type_alt sAlt;` |
|     130 |  6256 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     126 |  6257 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     126 |  6258 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      41 |  6259 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      13 |  6260 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      28 |  6261 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      28 |  6262 | `					sAlt.nType = SXU32_HIGH;` |
|      28 |  6263 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      15 |  6264 | `				}else{` |
|     100 |  6265 | `					sAlt.nType = aAtoms[i].nType;` |
|     100 |  6266 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6267 | `				}` |
|     126 |  6268 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      64 |  6269 | `			}` |
|       - |  6270 | `		}` |
|       - |  6271 | `	}` |
|   61808 |  6272 | `	return SXRET_OK;` |
|   30910 |  6273 |  |
|       - |  6274 |  |
|       - |  6275 | `/*` |
|       - |  6276 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6277 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6278 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6279 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6280 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6281 | `` *          and union types `: T\|U`.`` |
|       - |  6282 | ` */` |
|  229718 |  6283 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6284 |  |
|  229720 |  6285 | `	sxi32 iFlags = 0;` |
|       - |  6286 | `	sxi32 rc;` |
|       - |  6287 | `	sxu32 nLine;` |
|  229720 |  6288 | `	pFunc->nReturnType = 0;` |
|  229720 |  6289 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  229720 |  6290 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  229720 |  6291 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  229628 |  6292 | `		return SXRET_OK;` |
|       - |  6293 | `	}` |
|      94 |  6294 | `	pGen->pIn++; /* Skip ':' */` |
|      94 |  6295 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6296 | `		return SXRET_OK;` |
|       - |  6297 | `	}` |
|      94 |  6298 | `	nLine = pGen->pIn->nLine;` |
|      94 |  6299 | `	rc = GenStateParseUnionTypeDecl(` |
|      46 |  6300 | `		pGen,` |
|      46 |  6301 | `		&pFunc->nReturnType,` |
|      46 |  6302 | `		&pFunc->sReturnClass,` |
|      46 |  6303 | `		&pFunc->aReturnUnion,` |
|       - |  6304 | `		&iFlags,` |
|      46 |  6305 | `		&pFunc->sReturnTypeName,` |
|       - |  6306 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6307 | `		/* iUnionFlag */ 0,` |
|       - |  6308 | `		/* bAllowVoid */ 1,` |
|      46 |  6309 | `		nLine);` |
|      46 |  6310 | `	(void)iFlags;` |
|      94 |  6311 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6312 | `		return SXERR_ABORT;` |
|       - |  6313 | `	}` |
|      94 |  6314 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6315 | `		/* Error already reported */` |
|     ! 0 |  6316 | `		return SXERR_SYNTAX;` |
|       - |  6317 | `	}` |
|      94 |  6318 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6319 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6320 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6321 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6322 | `				&pGen->pIn->sData);` |
|       3 |  6323 | `		}else{` |
|     ! 0 |  6324 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6325 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6326 | `		}` |
|       5 |  6327 | `		return SXERR_SYNTAX;` |
|       - |  6328 | `	}` |
|      90 |  6329 | `	return SXRET_OK;` |
|  114861 |  6330 |  |
|       - |  6331 |  |
|   38884 |  6332 | `static sxi32 GenStateCompileFunc(` |
|       - |  6333 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6334 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6335 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6336 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6337 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6338 | `	)` |
|       2 |  6339 |  |
|       - |  6340 | `	ph7_vm_func *pFunc;` |
|       - |  6341 | `	SyToken *pEnd;` |
|       - |  6342 | `	sxu32 nLine;` |
|       - |  6343 | `	char *zName;` |
|       - |  6344 | `	sxi32 rc;` |
|       - |  6345 | `	/* Extract line number */` |
|   38886 |  6346 | `	nLine = pGen->pIn->nLine;` |
|       - |  6347 | `	/* Jump the left parenthesis '(' */` |
|   38886 |  6348 | `	pGen->pIn++;` |
|       - |  6349 | `	/* Delimit the function signature */` |
|   38886 |  6350 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   38886 |  6351 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6352 | `		/* Syntax error */` |
|       7 |  6353 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6354 | `		if( rc == SXERR_ABORT ){` |
|       - |  6355 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6356 | `			return SXERR_ABORT;` |
|       - |  6357 | `		}` |
|       7 |  6358 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6359 | `		return SXRET_OK;` |
|       - |  6360 | `	}` |
|       - |  6361 | `	/* Create the function state */` |
|   38880 |  6362 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   38880 |  6363 | `	if( pFunc == 0 ){` |
|     ! 0 |  6364 | `		goto OutOfMem;` |
|       - |  6365 | `	}` |
|       - |  6366 | `	/* Build the function name, prepending namespace if active */` |
|   38887 |  6367 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6368 | `		SyBlob sFQN;` |
|       - |  6369 | `		sxu32 nLen;` |
|      16 |  6370 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6371 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6372 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6373 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6374 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6375 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6376 | `		SyBlobRelease(&sFQN);` |
|      16 |  6377 | `		if( zName == 0 ){` |
|     ! 0 |  6378 | `			goto OutOfMem;` |
|       - |  6379 | `		}` |
|      16 |  6380 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6381 | `	}else{` |
|   38866 |  6382 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   38866 |  6383 | `		if( zName == 0 ){` |
|     ! 0 |  6384 | `			goto OutOfMem;` |
|       - |  6385 | `		}` |
|   38866 |  6386 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6387 | `	}` |
|   38880 |  6388 | `	if( pGen->pIn < pEnd ){` |
|       - |  6389 | `		/* Collect function arguments */` |
|   26980 |  6390 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   26980 |  6391 | `		if( rc == SXERR_ABORT ){` |
|       - |  6392 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6393 | `			return SXERR_ABORT;` |
|       - |  6394 | `		}` |
|   13489 |  6395 | `	}` |
|       - |  6396 | `	/* Point past ')' and parse optional return type ': type' */` |
|   38880 |  6397 | `	pGen->pIn = &pEnd[1];` |
|       - |  6398 | `	{` |
|   38880 |  6399 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   38880 |  6400 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6401 | `			return SXERR_ABORT;` |
|   38880 |  6402 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6403 | `			return SXERR_SYNTAX;` |
|       - |  6404 | `		}` |
|       - |  6405 | `	}` |
|   38876 |  6406 | `	if( bHandleClosure ){` |
|       - |  6407 | `		ph7_vm_func_closure_env sEnv;` |
|     178 |  6408 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     176 |  6409 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      97 |  6410 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 |  6411 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6412 | `				/* Closure,record environment variable */` |
|      16 |  6413 | `				pGen->pIn++;` |
|      16 |  6414 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6415 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6416 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6417 | `						return SXERR_ABORT;` |
|       - |  6418 | `					}` |
|     ! 0 |  6419 | `				}` |
|      16 |  6420 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6421 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 |  6422 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 |  6423 | `					int iFlagsLocal = 0;` |
|      34 |  6424 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 |  6425 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 |  6426 | `						break;` |
|       - |  6427 | `					}` |
|      20 |  6428 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 |  6429 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6430 | `						/* Pass by reference,record that */` |
|     ! 0 |  6431 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6432 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6433 | `							);` |
|     ! 0 |  6434 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6435 | `						pGen->pIn++;` |
|     ! 0 |  6436 | `					}` |
|      18 |  6437 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 |  6438 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6439 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6440 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6441 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6442 | `								return SXERR_ABORT;` |
|       - |  6443 | `							}` |
|       - |  6444 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6445 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6446 | `								pGen->pIn++;` |
|     ! 0 |  6447 | `							}` |
|     ! 0 |  6448 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6449 | `								pGen->pIn++;` |
|     ! 0 |  6450 | `							}` |
|     ! 0 |  6451 | `							break;` |
|       - |  6452 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6453 | `					}else{` |
|       - |  6454 | `						SyString *pNameLocal;` |
|       - |  6455 | `						char *zDup;` |
|       - |  6456 | `						/* Duplicate variable name */` |
|      20 |  6457 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 |  6458 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 |  6459 | `						if( zDup ){` |
|       - |  6460 | `							/* Zero the structure */` |
|      20 |  6461 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 |  6462 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 |  6463 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 |  6464 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 |  6465 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6466 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6467 | `									got_this = 1;` |
|     ! 0 |  6468 | `							}` |
|       - |  6469 | `							/* Save imported variable */` |
|      20 |  6470 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 |  6471 | `						}else{` |
|     ! 0 |  6472 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6473 | `							 return SXERR_ABORT;` |
|       - |  6474 | `						}` |
|       - |  6475 | `					}` |
|      20 |  6476 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 |  6477 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6478 | `						/* Ignore trailing commas */` |
|       7 |  6479 | `						pGen->pIn++;` |
|       1 |  6480 | `					}` |
|       2 |  6481 | `				}` |
|      16 |  6482 | `				if( !got_this ){` |
|       - |  6483 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6484 | `					 * available to the closure environment.` |
|       - |  6485 | `					 */` |
|      16 |  6486 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 |  6487 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 |  6488 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 |  6489 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 |  6490 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 |  6491 | `				}` |
|      16 |  6492 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6493 | `					/* Mark as closure */` |
|      16 |  6494 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 |  6495 | `				}` |
|       7 |  6496 | `		}` |
|      88 |  6497 | `	}` |
|       - |  6498 | `	/* Compile the body */` |
|   38876 |  6499 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   38876 |  6500 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6501 | `		return SXERR_ABORT;` |
|       - |  6502 | `	}` |
|   38876 |  6503 | `	if( ppFunc ){` |
|     178 |  6504 | `		*ppFunc = pFunc;` |
|      88 |  6505 | `	}` |
|   38876 |  6506 | `	rc = SXRET_OK;` |
|   38876 |  6507 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6508 | `		/* Finally register the function */` |
|   38862 |  6509 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   19430 |  6510 | `	}` |
|   38876 |  6511 | `	if( rc == SXRET_OK ){` |
|   38876 |  6512 | `		return SXRET_OK;` |
|       - |  6513 | `	}` |
|       - |  6514 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6515 | `OutOfMem:` |
|       - |  6516 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6517 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6518 | `	 */` |
|     ! 0 |  6519 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6520 | `	return SXERR_ABORT;` |
|   19444 |  6521 |  |
|       - |  6522 | `/*` |
|       - |  6523 | ` * Compile a standard PHP function.` |
|       - |  6524 | ` *  Refer to the block-comment above for more information.` |
|       - |  6525 | ` */` |
|   38714 |  6526 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6527 |  |
|       - |  6528 | `	SyString *pName;` |
|       - |  6529 | `	sxi32 iFlags;` |
|       - |  6530 | `	sxu32 nLine;` |
|       - |  6531 | `	sxi32 rc;` |
|       - |  6532 |  |
|   38716 |  6533 | `	nLine = pGen->pIn->nLine;` |
|   38716 |  6534 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   38716 |  6535 | `	iFlags = 0;` |
|   38716 |  6536 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6537 | `		/* Return by reference,remember that */` |
|       7 |  6538 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6539 | `		/* Jump the '&' token */` |
|       7 |  6540 | `		pGen->pIn++;` |
|       3 |  6541 | `	}` |
|   38716 |  6542 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6543 | `		/* Invalid function name */` |
|       5 |  6544 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6545 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6546 | `			return SXERR_ABORT;` |
|       - |  6547 | `		}` |
|       - |  6548 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6549 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6550 | `			pGen->pIn++;` |
|       1 |  6551 | `		}` |
|       5 |  6552 | `		return SXRET_OK;` |
|       - |  6553 | `	}` |
|   38712 |  6554 | `	pName = &pGen->pIn->sData;` |
|   38712 |  6555 | `	nLine = pGen->pIn->nLine;` |
|       - |  6556 | `	/* Jump the function name */` |
|   38712 |  6557 | `	pGen->pIn++;` |
|   38712 |  6558 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6559 | `		/* Syntax error */` |
|       3 |  6560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6561 | `		if( rc == SXERR_ABORT ){` |
|       - |  6562 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6563 | `			return SXERR_ABORT;` |
|       - |  6564 | `		}` |
|       - |  6565 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6566 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6567 | `			pGen->pIn++;` |
|     ! 0 |  6568 | `		}` |
|       3 |  6569 | `		return SXRET_OK;` |
|       - |  6570 | `	}` |
|       - |  6571 | `	/* Compile function body */` |
|   38710 |  6572 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   38710 |  6573 | `	return rc;` |
|   19359 |  6574 |  |
|       - |  6575 | `/*` |
|       - |  6576 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6577 | ` * According to the PHP language reference manual` |
|       - |  6578 | ` *  Visibility:` |
|       - |  6579 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6580 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6581 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6582 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6583 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6584 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6585 | ` */` |
|  246814 |  6586 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6587 |  |
|  246816 |  6588 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8848 |  6589 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  237970 |  6590 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   38082 |  6591 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6592 | `	}` |
|       - |  6593 | `	/* Assume public by default */` |
|  199890 |  6594 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  123409 |  6595 |  |
|       - |  6596 | `/*` |
|       - |  6597 | ` * Compile a class constant.` |
|       - |  6598 | ` * According to the PHP language reference manual` |
|       - |  6599 | ` *  Class Constants` |
|       - |  6600 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6601 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6602 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6603 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6604 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6605 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6606 | ` * Symisc eXtension.` |
|       - |  6607 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6608 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6609 | ` *  Example:` |
|       - |  6610 | ` *   class Test{` |
|       - |  6611 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6612 | ` *   };` |
|       - |  6613 | ` *   var_dump(TEST::MyConst);` |
|       - |  6614 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6615 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6616 | ` */` |
|      30 |  6617 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6618 |  |
|      32 |  6619 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6620 | `	SySet *pInstrContainer;` |
|       - |  6621 | `	ph7_class_attr *pCons;` |
|       - |  6622 | `	SyString *pName;` |
|       - |  6623 | `	sxi32 rc;` |
|       - |  6624 | `	/* Extract visibility level */` |
|      32 |  6625 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6626 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6627 | `loop:` |
|       - |  6628 | `	/* Mark as constant */` |
|      32 |  6629 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6630 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6631 | `		/* Invalid constant name */` |
|     ! 0 |  6632 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6633 | `		if( rc == SXERR_ABORT ){` |
|       - |  6634 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6635 | `			return SXERR_ABORT;` |
|       - |  6636 | `		}` |
|     ! 0 |  6637 | `		goto Synchronize;` |
|       - |  6638 | `	}` |
|       - |  6639 | `	/* Peek constant name */` |
|      32 |  6640 | `	pName = &pGen->pIn->sData;` |
|       - |  6641 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6642 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6643 | `		/* Reserved constant name */` |
|     ! 0 |  6644 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6645 | `		if( rc == SXERR_ABORT ){` |
|       - |  6646 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6647 | `			return SXERR_ABORT;` |
|       - |  6648 | `		}` |
|     ! 0 |  6649 | `		goto Synchronize;` |
|       - |  6650 | `	}` |
|       - |  6651 | `	/* Advance the stream cursor */` |
|      32 |  6652 | `	pGen->pIn++;` |
|      32 |  6653 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6654 | `		/* Invalid declaration */` |
|     ! 0 |  6655 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6656 | `		if( rc == SXERR_ABORT ){` |
|       - |  6657 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6658 | `			return SXERR_ABORT;` |
|       - |  6659 | `		}` |
|     ! 0 |  6660 | `		goto Synchronize;` |
|       - |  6661 | `	}` |
|      32 |  6662 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6663 | `	/* Allocate a new class attribute */` |
|      32 |  6664 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6665 | `	if( pCons == 0 ){` |
|     ! 0 |  6666 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6667 | `		return SXERR_ABORT;` |
|       - |  6668 | `	}` |
|       - |  6669 | `	/* Swap bytecode container */` |
|      32 |  6670 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6671 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6672 | `	/* Compile constant value.` |
|       - |  6673 | `	 */` |
|      32 |  6674 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6675 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6676 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6677 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6678 | `			return SXERR_ABORT;` |
|       - |  6679 | `		}` |
|       1 |  6680 | `	}` |
|       - |  6681 | `	/* Emit the done instruction */` |
|      32 |  6682 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6683 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6684 | `	if( rc == SXERR_ABORT ){` |
|       - |  6685 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6686 | `		return SXERR_ABORT;` |
|       - |  6687 | `	}` |
|       - |  6688 | `	/* All done,install the constant */` |
|      32 |  6689 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6690 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6691 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6692 | `		return SXERR_ABORT;` |
|       - |  6693 | `	}` |
|      32 |  6694 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6695 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6696 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6697 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6698 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6699 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6700 | `				pTok--;` |
|     ! 0 |  6701 | `			}` |
|     ! 0 |  6702 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6703 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6704 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6705 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6706 | `				return SXERR_ABORT;` |
|       - |  6707 | `			}` |
|     ! 0 |  6708 | `		}else{` |
|     ! 0 |  6709 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6710 | `				goto loop;` |
|       - |  6711 | `			}` |
|       - |  6712 | `		}` |
|     ! 0 |  6713 | `	}` |
|      32 |  6714 | `	return SXRET_OK;` |
|     ! 0 |  6715 | `Synchronize:` |
|       - |  6716 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6717 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6718 | `		pGen->pIn++;` |
|     ! 0 |  6719 | `	}` |
|     ! 0 |  6720 | `	return SXERR_CORRUPT;` |
|      17 |  6721 |  |
|       - |  6722 | `/*` |
|       - |  6723 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6724 | ` * According to the PHP language reference manual` |
|       - |  6725 | ` *  Properties` |
|       - |  6726 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6727 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6728 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6729 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6730 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6731 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6732 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6733 | ` * Symisc eXtension.` |
|       - |  6734 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6735 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6736 | ` *  Example:` |
|       - |  6737 | ` *   class Test{` |
|       - |  6738 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6739 | ` *   };` |
|       - |  6740 | ` *   var_dump(TEST::myVar);` |
|       - |  6741 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6742 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6743 | ` */` |
|       - |  6744 | `/*` |
|       - |  6745 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6746 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6747 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6748 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6749 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6750 | ` */` |
|  143976 |  6751 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6752 |  |
|  143978 |  6753 | `	SyToken *p = pStart;` |
|  143978 |  6754 | `	if( p >= pEnd ) return 0;` |
|  143978 |  6755 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6756 | `		p++;` |
|      16 |  6757 | `		if( p >= pEnd ) return 0;` |
|       7 |  6758 | `	}` |
|  143978 |  6759 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6760 | `		p++;` |
|       3 |  6761 | `		if( p >= pEnd ) return 0;` |
|       1 |  6762 | `	}` |
|  143978 |  6763 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6764 | `		return 0;` |
|       - |  6765 | `	}` |
|       - |  6766 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6767 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6768 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6769 | `	 * dispatch site. */` |
|  143978 |  6770 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  143960 |  6771 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  144007 |  6772 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3077 |  6773 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  143862 |  6774 | `			return 0;` |
|       - |  6775 | `		}` |
|      49 |  6776 | `	}` |
|     118 |  6777 | `	p++;` |
|       - |  6778 | `	/* Consume optional namespace path */` |
|     120 |  6779 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6780 | `		p += 2;` |
|       1 |  6781 | `	}` |
|       - |  6782 | ``	/* Consume any `\| Type` union alternatives */`` |
|     192 |  6783 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      78 |  6784 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6785 | `		p++;` |
|      14 |  6786 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6787 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6788 | `		p++;` |
|      14 |  6789 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6790 | `			p += 2;` |
|     ! 0 |  6791 | `		}` |
|       2 |  6792 | `	}` |
|     118 |  6793 | `	if( p >= pEnd ) return 0;` |
|     118 |  6794 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   71990 |  6795 |  |
|       - |  6796 |  |
|       - |  6797 | `/*` |
|       - |  6798 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6799 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6800 | ` * if not). Recognized forms:` |
|       - |  6801 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6802 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6803 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6804 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6805 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6806 | ` * on unrecoverable error.` |
|       - |  6807 | ` *` |
|       - |  6808 | ` * When a type is parsed:` |
|       - |  6809 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6810 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6811 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6812 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6813 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6814 | ` */` |
|     116 |  6815 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6816 | `	ph7_gen_state *pGen,` |
|       - |  6817 | `	sxu32 *pnType,` |
|       - |  6818 | `	SyString *pClass,` |
|       - |  6819 | `	sxi32 *piTypeFlags,` |
|       - |  6820 | `	SyString *pTypeText,` |
|       - |  6821 | `	SySet *pAlts` |
|       2 |  6822 | `){` |
|     118 |  6823 | `	sxi32 iFlags = 0;` |
|       - |  6824 | `	sxi32 rc;` |
|     118 |  6825 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6826 | `		return SXRET_OK;` |
|       - |  6827 | `	}` |
|       - |  6828 | `	/* If the first token is '$', there's no type */` |
|     118 |  6829 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6830 | `		return SXRET_OK;` |
|       - |  6831 | `	}` |
|     118 |  6832 | `	rc = GenStateParseUnionTypeDecl(` |
|      58 |  6833 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6834 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6835 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6836 | `		/* bAllowVoid */ 0,` |
|     116 |  6837 | `		pGen->pIn->nLine);` |
|     118 |  6838 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6839 | `		return rc;` |
|       - |  6840 | `	}` |
|       - |  6841 | `	/* Verify next token is '$' (start of property name) */` |
|     118 |  6842 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6843 | `		return SXERR_SYNTAX;` |
|       - |  6844 | `	}` |
|     118 |  6845 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     118 |  6846 | `	return SXRET_OK;` |
|      60 |  6847 |  |
|       - |  6848 |  |
|       - |  6849 | `/*` |
|       - |  6850 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6851 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6852 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6853 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6854 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6855 | ` * by the type parser itself before reaching here.` |
|       - |  6856 | ` *` |
|       - |  6857 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6858 | ` * use in the error message.` |
|       - |  6859 | ` */` |
|     182 |  6860 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6861 | `	sxu32 nType,` |
|       - |  6862 | `	const SyString *pClass,` |
|       - |  6863 | `	const char **pzName,` |
|       - |  6864 | `	sxu32 *pnName)` |
|       2 |  6865 |  |
|       - |  6866 | `	const char *z;` |
|       - |  6867 | `	sxu32 n;` |
|     184 |  6868 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     154 |  6869 | `		return 0;` |
|       - |  6870 | `	}` |
|      32 |  6871 | `	z = pClass->zString;` |
|      32 |  6872 | `	n = pClass->nByte;` |
|      32 |  6873 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       5 |  6874 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6875 | `	}` |
|      28 |  6876 | `	if( n == 5 && SyMemcmpNoCase(z,"mixed",5) == 0 ){` |
|     ! 0 |  6877 | `		*pzName = "mixed"; *pnName = 5; return 1;` |
|       - |  6878 | `	}` |
|      28 |  6879 | `	if( n == 8 && SyMemcmpNoCase(z,"iterable",8) == 0 ){` |
|     ! 0 |  6880 | `		*pzName = "iterable"; *pnName = 8; return 1;` |
|       - |  6881 | `	}` |
|      28 |  6882 | `	return 0;` |
|      93 |  6883 |  |
|       - |  6884 |  |
|       - |  6885 | `/*` |
|       - |  6886 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6887 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6888 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6889 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6890 | `` * unions like `callable\|int`).`` |
|       - |  6891 | ` *` |
|       - |  6892 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6893 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6894 | ` */` |
|     154 |  6895 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6896 | `	ph7_gen_state *pGen,` |
|       - |  6897 | `	ph7_class *pClass,` |
|       - |  6898 | `	const SyString *pPropName,` |
|       - |  6899 | `	sxu32 nType,` |
|       - |  6900 | `	const SyString *pTypeClass,` |
|       - |  6901 | `	const SyString *pTypeText,` |
|       - |  6902 | `	SySet *pUnionAlts,` |
|       - |  6903 | `	sxu32 nLine)` |
|       2 |  6904 |  |
|     156 |  6905 | `	const char *zBad = 0;` |
|     156 |  6906 | `	sxu32 nBad = 0;` |
|       - |  6907 | `	SyString sFallback;` |
|       - |  6908 | `	const SyString *pBad;` |
|       - |  6909 | `	sxi32 rc;` |
|     156 |  6910 | `	int bDisallowed = 0;` |
|     156 |  6911 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6912 | `		bDisallowed = 1;` |
|     155 |  6913 | `	}else if( pUnionAlts ){` |
|       - |  6914 | `		sxu32 i;` |
|      42 |  6915 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      30 |  6916 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      30 |  6917 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6918 | `				bDisallowed = 1;` |
|       3 |  6919 | `				break;` |
|       - |  6920 | `			}` |
|      15 |  6921 | `		}` |
|       7 |  6922 | `	}` |
|     156 |  6923 | `	if( !bDisallowed ){` |
|     152 |  6924 | `		return SXRET_OK;` |
|       - |  6925 | `	}` |
|       - |  6926 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6927 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6928 | `	 * canonical spelling if the type text is unavailable. */` |
|       5 |  6929 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       5 |  6930 | `		pBad = pTypeText;` |
|       3 |  6931 | `	}else{` |
|     ! 0 |  6932 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6933 | `		pBad = &sFallback;` |
|       - |  6934 | `	}` |
|       7 |  6935 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6936 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6937 | `		&pClass->sName,pPropName,pBad);` |
|       5 |  6938 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6939 | `		return SXERR_ABORT;` |
|       - |  6940 | `	}` |
|       5 |  6941 | `	return SXERR_SYNTAX;` |
|      79 |  6942 |  |
|       - |  6943 |  |
|   56024 |  6944 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6945 |  |
|   56026 |  6946 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6947 | `	ph7_class_attr *pAttr;` |
|       - |  6948 | `	SyString *pName;` |
|       - |  6949 | `	sxi32 rc;` |
|   56026 |  6950 | `	sxu32 nType = 0;` |
|       - |  6951 | `	SyString sTypeClass;` |
|       - |  6952 | `	SyString sTypeText;` |
|       - |  6953 | `	SySet aUnionAlts;` |
|   56026 |  6954 | `	sxi32 iTypeFlags = 0;` |
|   56026 |  6955 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   56026 |  6956 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   56026 |  6957 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6958 | `	/* Extract visibility level */` |
|   56026 |  6959 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6960 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   56084 |  6961 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     118 |  6962 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     118 |  6963 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6964 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6965 | `			goto Synchronize;` |
|     118 |  6966 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  6967 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6968 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  6969 | `				&pGen->pIn->sData);` |
|     ! 0 |  6970 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6971 | `				return SXERR_ABORT;` |
|       - |  6972 | `			}` |
|     ! 0 |  6973 | `			goto Synchronize;` |
|     118 |  6974 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6975 | `			return SXERR_ABORT;` |
|       - |  6976 | `		}` |
|      58 |  6977 | `	}` |
|     ! 0 |  6978 | `loop:` |
|   56030 |  6979 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6980 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  6981 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6982 | `			return SXERR_ABORT;` |
|       - |  6983 | `		}` |
|     ! 0 |  6984 | `		goto Synchronize;` |
|       - |  6985 | `	}` |
|   56030 |  6986 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   56030 |  6987 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  6988 | `		/* Invalid attribute name */` |
|     ! 0 |  6989 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  6990 | `		if( rc == SXERR_ABORT ){` |
|       - |  6991 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6992 | `			return SXERR_ABORT;` |
|       - |  6993 | `		}` |
|     ! 0 |  6994 | `		goto Synchronize;` |
|       - |  6995 | `	}` |
|       - |  6996 | `	/* Peek attribute name */` |
|   56030 |  6997 | `	pName = &pGen->pIn->sData;` |
|       - |  6998 | `	/* Advance the stream cursor */` |
|   56030 |  6999 | `	pGen->pIn++;` |
|   56030 |  7000 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7001 | `		/* Invalid declaration */` |
|       3 |  7002 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7003 | `		if( rc == SXERR_ABORT ){` |
|       - |  7004 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7005 | `			return SXERR_ABORT;` |
|       - |  7006 | `		}` |
|       3 |  7007 | `		goto Synchronize;` |
|       - |  7008 | `	}` |
|       - |  7009 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7010 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7011 | `	 * by the type parser. */` |
|   56028 |  7012 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     182 |  7013 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7014 | `			&sTypeText,` |
|     120 |  7015 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     122 |  7016 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7017 | `			return SXERR_ABORT;` |
|     122 |  7018 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7019 | `			goto Synchronize;` |
|       - |  7020 | `		}` |
|      60 |  7021 | `	}` |
|       - |  7022 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   56028 |  7023 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7024 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7025 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7026 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7027 | `			return SXERR_ABORT;` |
|       - |  7028 | `		}` |
|       3 |  7029 | `		goto Synchronize;` |
|       - |  7030 | `	}` |
|       - |  7031 | `	/* Allocate a new class attribute */` |
|   56026 |  7032 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   56026 |  7033 | `	if( pAttr == 0 ){` |
|     ! 0 |  7034 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7035 | `		return SXERR_ABORT;` |
|       - |  7036 | `	}` |
|   56026 |  7037 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     120 |  7038 | `		pAttr->nType = nType;` |
|     120 |  7039 | `		pAttr->sClass = sTypeClass;` |
|     120 |  7040 | `		pAttr->sTypeName = sTypeText;` |
|     120 |  7041 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7042 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  7043 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  7044 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  7045 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  7046 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  7047 | `			sxu32 i;` |
|      32 |  7048 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  7049 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  7050 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  7051 | `			}` |
|       5 |  7052 | `		}` |
|      59 |  7053 | `	}` |
|   56026 |  7054 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7055 | `		SySet *pInstrContainer;` |
|   17886 |  7056 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7057 | `		/* Swap bytecode container */` |
|   17886 |  7058 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   17886 |  7059 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7060 | `		/* Compile attribute value.` |
|       - |  7061 | `		 */` |
|   17886 |  7062 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   17886 |  7063 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7064 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7065 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7066 | `				return SXERR_ABORT;` |
|       - |  7067 | `			}` |
|     ! 0 |  7068 | `		}` |
|       - |  7069 | `		/* Emit the done instruction */` |
|   17886 |  7070 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   17886 |  7071 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    8942 |  7072 | `	}` |
|       - |  7073 | `	/* All done,install the attribute */` |
|   56026 |  7074 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   56026 |  7075 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7076 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7077 | `		return SXERR_ABORT;` |
|       - |  7078 | `	}` |
|   56026 |  7079 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7080 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7081 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7082 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7083 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7084 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7085 | `				pTok--;` |
|     ! 0 |  7086 | `			}` |
|     ! 0 |  7087 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7088 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7089 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7090 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7091 | `				return SXERR_ABORT;` |
|       - |  7092 | `			}` |
|     ! 0 |  7093 | `		}else{` |
|       5 |  7094 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7095 | `				goto loop;` |
|       - |  7096 | `			}` |
|       - |  7097 | `		}` |
|     ! 0 |  7098 | `	}` |
|   56022 |  7099 | `	SySetRelease(&aUnionAlts);` |
|   56022 |  7100 | `	return SXRET_OK;` |
|       2 |  7101 | `Synchronize:` |
|       - |  7102 | `	/* Synchronize with the first semi-colon */` |
|      11 |  7103 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7104 | `		pGen->pIn++;` |
|       1 |  7105 | `	}` |
|       5 |  7106 | `	SySetRelease(&aUnionAlts);` |
|       5 |  7107 | `	return SXERR_CORRUPT;` |
|   28014 |  7108 |  |
|       - |  7109 | `/*` |
|       - |  7110 | ` * Compile a class method.` |
|       - |  7111 | ` *` |
|       - |  7112 | ` * Refer to the official documentation for more information` |
|       - |  7113 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7114 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7115 | ` * overloading and many more.` |
|       - |  7116 | ` */` |
|  190760 |  7117 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7118 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7119 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7120 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7121 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7122 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7123 | `	)` |
|       2 |  7124 |  |
|  190762 |  7125 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7126 | `	ph7_class_method *pMeth;` |
|       - |  7127 | `	sxi32 iFuncFlags;` |
|       - |  7128 | `	SyString *pName;` |
|       - |  7129 | `	SyToken *pEnd;` |
|       - |  7130 | `	sxi32 rc;` |
|       - |  7131 | `	/* Extract visibility level */` |
|  190762 |  7132 | `	iProtection = GetProtectionLevel(iProtection);` |
|  190762 |  7133 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  190762 |  7134 | `	iFuncFlags = 0;` |
|  190762 |  7135 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7136 | `		/* Invalid method name */` |
|     ! 0 |  7137 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7138 | `		if( rc == SXERR_ABORT ){` |
|       - |  7139 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7140 | `			return SXERR_ABORT;` |
|       - |  7141 | `		}` |
|     ! 0 |  7142 | `		goto Synchronize;` |
|       - |  7143 | `	}` |
|  190762 |  7144 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7145 | `		/* Return by reference,remember that */` |
|     ! 0 |  7146 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7147 | `		/* Jump the '&' token */` |
|     ! 0 |  7148 | `		pGen->pIn++;` |
|     ! 0 |  7149 | `	}` |
|  190762 |  7150 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7151 | `		/* Invalid method name */` |
|     ! 0 |  7152 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7153 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7154 | `			return SXERR_ABORT;` |
|       - |  7155 | `		}` |
|     ! 0 |  7156 | `		goto Synchronize;` |
|       - |  7157 | `	}` |
|       - |  7158 | `	/* Peek method name */` |
|  190762 |  7159 | `	pName = &pGen->pIn->sData;` |
|  190762 |  7160 | `	nLine = pGen->pIn->nLine;` |
|       - |  7161 | `	/* Jump the method name */` |
|  190762 |  7162 | `	pGen->pIn++;` |
|  190762 |  7163 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7164 | `		/* Abstract method */` |
|   46868 |  7165 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7166 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7167 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7168 | `				&pClass->sName,pName);` |
|     ! 0 |  7169 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7170 | `				return SXERR_ABORT;` |
|       - |  7171 | `			}` |
|     ! 0 |  7172 | `		}` |
|       - |  7173 | `		/* Assemble method signature only */` |
|   46868 |  7174 | `		doBody = FALSE;` |
|   23433 |  7175 | `	}` |
|  190762 |  7176 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7177 | `		/* Syntax error */` |
|     ! 0 |  7178 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7179 | `		if( rc == SXERR_ABORT ){` |
|       - |  7180 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7181 | `			return SXERR_ABORT;` |
|       - |  7182 | `		}` |
|     ! 0 |  7183 | `		goto Synchronize;` |
|       - |  7184 | `	}` |
|       - |  7185 | `	/* Allocate a new class_method instance */` |
|  190762 |  7186 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  190762 |  7187 | `	if( pMeth == 0 ){` |
|     ! 0 |  7188 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7189 | `		return SXERR_ABORT;` |
|       - |  7190 | `	}` |
|       - |  7191 | `	/* Jump the left parenthesis '(' */` |
|  190762 |  7192 | `	pGen->pIn++;` |
|  190762 |  7193 | `	pEnd = 0; /* cc warning */` |
|       - |  7194 | `	/* Delimit the method signature */` |
|  190762 |  7195 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  190762 |  7196 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7197 | `		/* Syntax error */` |
|       3 |  7198 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7199 | `		if( rc == SXERR_ABORT ){` |
|       - |  7200 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7201 | `			return SXERR_ABORT;` |
|       - |  7202 | `		}` |
|       3 |  7203 | `		goto Synchronize;` |
|       - |  7204 | `	}` |
|       - |  7205 | `	{` |
|  190760 |  7206 | `		int bIsCtor = 0;` |
|  190760 |  7207 | `		int bAbstractCtor = 0;` |
|  277314 |  7208 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  114456 |  7209 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  181937 |  7210 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   17648 |  7211 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7212 | `				bAbstractCtor = 1;` |
|       2 |  7213 | `			}else{` |
|   17646 |  7214 | `				bIsCtor = 1;` |
|       - |  7215 | `			}` |
|    8823 |  7216 | `		}` |
|  190760 |  7217 | `		if( pGen->pIn < pEnd ){` |
|       - |  7218 | `			/* Collect method arguments */` |
|   32332 |  7219 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   32332 |  7220 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7221 | `				return SXERR_ABORT;` |
|       - |  7222 | `			}` |
|   16165 |  7223 | `		}` |
|       - |  7224 | `	}` |
|       - |  7225 | `	/* Point past ')' and parse optional return type ': type' */` |
|  190760 |  7226 | `	pGen->pIn = &pEnd[1];` |
|       - |  7227 | `	{` |
|  190760 |  7228 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  190760 |  7229 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7230 | `			return SXERR_ABORT;` |
|  190760 |  7231 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7232 | `			goto Synchronize;` |
|       - |  7233 | `		}` |
|       - |  7234 | `	}` |
|       - |  7235 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7236 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7237 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7238 | `	{` |
|  190760 |  7239 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7240 | `		sxu32 i;` |
|  249458 |  7241 | `		for( i = 0; i < nArg; i++ ){` |
|   58708 |  7242 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7243 | `			ph7_class_attr *pAttr;` |
|   58708 |  7244 | `			sxi32 iAttrFlags = 0;` |
|   58708 |  7245 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   58672 |  7246 | `				continue;` |
|       - |  7247 | `			}` |
|      38 |  7248 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7249 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7250 | `					"Cannot declare variadic promoted property");` |
|       3 |  7251 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7252 | `					return SXERR_ABORT;` |
|       - |  7253 | `				}` |
|       3 |  7254 | `				goto Synchronize;` |
|       - |  7255 | `			}` |
|       - |  7256 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7257 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7258 | `			 * appear as an alternative of a union type. */` |
|      34 |  7259 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       6 |  7260 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      53 |  7261 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7262 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7263 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7264 | `					nLine);` |
|      36 |  7265 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7266 | `					return SXERR_ABORT;` |
|      36 |  7267 | `				}else if( rc != SXRET_OK ){` |
|       5 |  7268 | `					goto Synchronize;` |
|       - |  7269 | `				}` |
|      15 |  7270 | `			}` |
|       - |  7271 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      32 |  7272 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7273 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7274 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7275 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7276 | `					return SXERR_ABORT;` |
|       - |  7277 | `				}` |
|       3 |  7278 | `				goto Synchronize;` |
|       - |  7279 | `			}` |
|      30 |  7280 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      28 |  7281 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7282 | `			}` |
|      30 |  7283 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7284 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7285 | `			}` |
|      30 |  7286 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7287 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7288 | `			}` |
|      30 |  7289 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      30 |  7290 | `			if( pAttr == 0 ){` |
|     ! 0 |  7291 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7292 | `				return SXERR_ABORT;` |
|       - |  7293 | `			}` |
|      30 |  7294 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      28 |  7295 | `				pAttr->nType = pArg->nType;` |
|      28 |  7296 | `				pAttr->sClass = pArg->sClass;` |
|      28 |  7297 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      28 |  7298 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7299 | `					sxu32 k;` |
|     ! 0 |  7300 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7301 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7302 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7303 | `					}` |
|     ! 0 |  7304 | `				}` |
|      13 |  7305 | `			}` |
|      30 |  7306 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      30 |  7307 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7308 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7309 | `				return SXERR_ABORT;` |
|       - |  7310 | `			}` |
|      16 |  7311 | `		}` |
|       - |  7312 | `	}` |
|  190752 |  7313 | `	if( doBody ){` |
|       - |  7314 | `		/* Compile method body */` |
|  143886 |  7315 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  143886 |  7316 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7317 | `			return SXERR_ABORT;` |
|       - |  7318 | `		}` |
|   71944 |  7319 | `	}else{` |
|       - |  7320 | `		/* Only method signature is allowed */` |
|   46868 |  7321 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7322 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7323 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7324 | `				if( rc == SXERR_ABORT ){` |
|       - |  7325 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7326 | `					return SXERR_ABORT;` |
|       - |  7327 | `				}` |
|     ! 0 |  7328 | `				return SXERR_CORRUPT;` |
|       - |  7329 | `			}` |
|       - |  7330 | `	}` |
|       - |  7331 | `	/* All done,install the method */` |
|  190752 |  7332 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  190752 |  7333 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7334 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7335 | `		return SXERR_ABORT;` |
|       - |  7336 | `	}` |
|  190752 |  7337 | `	return SXRET_OK;` |
|       5 |  7338 | `Synchronize:` |
|       - |  7339 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7340 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      21 |  7341 | `		pGen->pIn++;` |
|       1 |  7342 | `	}` |
|      11 |  7343 | `	return SXERR_CORRUPT;` |
|   95382 |  7344 |  |
|       - |  7345 | `/*` |
|       - |  7346 | ` * Compile an object interface.` |
|       - |  7347 | ` *  According to the PHP language reference manual` |
|       - |  7348 | ` *   Object Interfaces:` |
|       - |  7349 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7350 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7351 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7352 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7353 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7354 | ` */` |
|   11746 |  7355 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  7356 |  |
|   11748 |  7357 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7358 | `	ph7_class *pClass,*pBase;` |
|       - |  7359 | `	SyToken *pEnd,*pTmp;` |
|       - |  7360 | `	SyString *pName;` |
|       - |  7361 | `	sxi32 nKwrd;` |
|       - |  7362 | `	sxi32 rc;` |
|       - |  7363 | `	/* Jump the 'interface' keyword */` |
|   11748 |  7364 | `	pGen->pIn++;` |
|       - |  7365 | `	/* Extract interface name */` |
|   11748 |  7366 | `	pName = &pGen->pIn->sData;` |
|       - |  7367 | `	/* Advance the stream cursor */` |
|   11748 |  7368 | `	pGen->pIn++;` |
|       - |  7369 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7370 | `		SyBlob sFQN;` |
|       - |  7371 | `		SyString sFQNStr;` |
|   11748 |  7372 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   11748 |  7373 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   11748 |  7374 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   11748 |  7375 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   11748 |  7376 | `		SyBlobRelease(&sFQN);` |
|       - |  7377 | `	}` |
|   11748 |  7378 | `	if( pClass == 0 ){` |
|     ! 0 |  7379 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7380 | `		return SXERR_ABORT;` |
|       - |  7381 | `	}` |
|       - |  7382 | `	/* Mark as an interface */` |
|   11748 |  7383 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7384 | `	/* Assume no base class is given */` |
|   11748 |  7385 | `	pBase = 0;` |
|   11748 |  7386 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 |  7387 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 |  7388 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7389 | `			SyBlob sResolved;` |
|       - |  7390 | `			SyString sBaseName;` |
|       - |  7391 | `			sxu32 nRefLine;` |
|       - |  7392 | `			/* Extract base interface */` |
|       8 |  7393 | `			pGen->pIn++;` |
|       8 |  7394 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       8 |  7395 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       8 |  7396 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7397 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7398 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7399 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7400 | `					pName);` |
|     ! 0 |  7401 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7402 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7403 | `					return SXERR_ABORT;` |
|       - |  7404 | `				}` |
|     ! 0 |  7405 | `				return SXRET_OK;` |
|       - |  7406 | `			}` |
|      11 |  7407 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|       6 |  7408 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       8 |  7409 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7410 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7411 | `			/* Only interfaces is allowed */` |
|       8 |  7412 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7413 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7414 | `			}` |
|       8 |  7415 | `			if( pBase == 0 ){` |
|     ! 0 |  7416 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7417 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7418 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7419 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7420 | `					return SXERR_ABORT;` |
|       - |  7421 | `				}` |
|     ! 0 |  7422 | `			}` |
|       8 |  7423 | `			SyBlobRelease(&sResolved);` |
|       3 |  7424 | `		}` |
|       3 |  7425 | `	}` |
|   11748 |  7426 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7427 | `		/* Syntax error */` |
|     ! 0 |  7428 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7429 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7430 | `		if( rc == SXERR_ABORT ){` |
|       - |  7431 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7432 | `			return SXERR_ABORT;` |
|       - |  7433 | `		}` |
|     ! 0 |  7434 | `		return SXRET_OK;` |
|       - |  7435 | `	}` |
|   11748 |  7436 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   11748 |  7437 | `	pEnd = 0; /* cc warning */` |
|       - |  7438 | `	/* Delimit the interface body */` |
|   11748 |  7439 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   11748 |  7440 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7441 | `		/* Syntax error */` |
|     ! 0 |  7442 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7443 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7444 | `		if( rc == SXERR_ABORT ){` |
|       - |  7445 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7446 | `			return SXERR_ABORT;` |
|       - |  7447 | `		}` |
|     ! 0 |  7448 | `		return SXRET_OK;` |
|       - |  7449 | `	}` |
|       - |  7450 | `	/* Swap token stream */` |
|   11748 |  7451 | `	pTmp = pGen->pEnd;` |
|   11748 |  7452 | `	pGen->pEnd = pEnd;` |
|       - |  7453 | `	/* Start the parse process` |
|       - |  7454 | `	 * Note (According to the PHP reference manual):` |
|       - |  7455 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7456 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7457 | `	 */` |
|   29300 |  7458 | `	for(;;){` |
|       - |  7459 | `		/* Jump leading/trailing semi-colons */` |
|  105456 |  7460 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   46856 |  7461 | `			pGen->pIn++;` |
|       2 |  7462 | `		}` |
|   58602 |  7463 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7464 | `			/* End of interface body */` |
|   11746 |  7465 | `			break;` |
|       - |  7466 | `		}` |
|   46858 |  7467 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7468 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7469 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7470 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7471 | `			if( rc == SXERR_ABORT ){` |
|       - |  7472 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7473 | `				return SXERR_ABORT;` |
|       - |  7474 | `			}` |
|     ! 0 |  7475 | `			goto done;` |
|       - |  7476 | `		}` |
|       - |  7477 | `		/* Extract the current keyword */` |
|   46858 |  7478 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   46858 |  7479 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7480 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7481 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7482 | `			const char *zKind = "member";` |
|       3 |  7483 | `			SyString *pMemberName = 0;` |
|       3 |  7484 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7485 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7486 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7487 | `					zKind = "constant";` |
|       3 |  7488 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7489 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7490 | `					}` |
|       1 |  7491 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7492 | `					zKind = "method";` |
|     ! 0 |  7493 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7494 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7495 | `					}` |
|     ! 0 |  7496 | `				}` |
|       1 |  7497 | `			}` |
|       3 |  7498 | `			if( pMemberName ){` |
|       4 |  7499 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7500 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7501 | `			}else{` |
|     ! 0 |  7502 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7503 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7504 | `			}` |
|       3 |  7505 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7506 | `				return SXERR_ABORT;` |
|       - |  7507 | `			}` |
|       3 |  7508 | `			goto done;` |
|       - |  7509 | `		}` |
|   46856 |  7510 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7511 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7512 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7513 | `			if( rc == SXERR_ABORT ){` |
|       - |  7514 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7515 | `				return SXERR_ABORT;` |
|       - |  7516 | `			}` |
|     ! 0 |  7517 | `			goto done;` |
|       - |  7518 | `		}` |
|   46856 |  7519 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7520 | `			/* Advance the stream cursor */` |
|   46852 |  7521 | `			pGen->pIn++;` |
|   46852 |  7522 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7523 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7524 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7525 | `				if( rc == SXERR_ABORT ){` |
|       - |  7526 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7527 | `					return SXERR_ABORT;` |
|       - |  7528 | `				}` |
|     ! 0 |  7529 | `				goto done;` |
|       - |  7530 | `			}` |
|   46852 |  7531 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   46852 |  7532 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7533 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7534 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7535 | `				if( rc == SXERR_ABORT ){` |
|       - |  7536 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7537 | `					return SXERR_ABORT;` |
|       - |  7538 | `				}` |
|     ! 0 |  7539 | `				goto done;` |
|       - |  7540 | `			}` |
|   23425 |  7541 | `		}` |
|   46856 |  7542 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7543 | `			/* Parse constant */` |
|       3 |  7544 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7545 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7546 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7547 | `					return SXERR_ABORT;` |
|       - |  7548 | `				}` |
|     ! 0 |  7549 | `				goto done;` |
|       - |  7550 | `			}` |
|       2 |  7551 | `		}else{` |
|   46854 |  7552 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   46854 |  7553 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7554 | `				/* Static method,record that */` |
|     ! 0 |  7555 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7556 | `				/* Advance the stream cursor */` |
|     ! 0 |  7557 | `				pGen->pIn++;` |
|     ! 0 |  7558 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  7559 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7560 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7561 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7562 | `						if( rc == SXERR_ABORT ){` |
|       - |  7563 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7564 | `							return SXERR_ABORT;` |
|       - |  7565 | `						}` |
|     ! 0 |  7566 | `						goto done;` |
|       - |  7567 | `				}` |
|     ! 0 |  7568 | `			}` |
|       - |  7569 | `			/* Process method signature (no body for interface methods) */` |
|   46854 |  7570 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   46854 |  7571 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7572 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7573 | `					return SXERR_ABORT;` |
|       - |  7574 | `				}` |
|     ! 0 |  7575 | `				goto done;` |
|       - |  7576 | `			}` |
|       - |  7577 | `		}` |
|       2 |  7578 | `	}` |
|       - |  7579 | `	/* Install the interface */` |
|   11746 |  7580 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   11746 |  7581 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7582 | `		/* Inherit from the base interface */` |
|       8 |  7583 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       3 |  7584 | `	}` |
|   11746 |  7585 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7586 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7587 | `		return SXERR_ABORT;` |
|       - |  7588 | `	}` |
|    5872 |  7589 | `done:` |
|       - |  7590 | `	/* Point beyond the interface body */` |
|   11748 |  7591 | `	pGen->pIn  = &pEnd[1];` |
|   11748 |  7592 | `	pGen->pEnd = pTmp;` |
|   11748 |  7593 | `	return PH7_OK;` |
|    5875 |  7594 |  |
|       - |  7595 | `/*` |
|       - |  7596 | ` * Compile a user-defined class.` |
|       - |  7597 | ` * According to the PHP language reference manual` |
|       - |  7598 | ` *  class` |
|       - |  7599 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7600 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7601 | ` *  of the properties and methods belonging to the class.` |
|       - |  7602 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7603 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7604 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7605 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7606 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7607 | ` *  (called "methods").` |
|       - |  7608 | ` */` |
|       - |  7609 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7610 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7611 | `struct TraitUseEntry {` |
|       - |  7612 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7613 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7614 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7615 | `};` |
|       - |  7616 | `/*` |
|       - |  7617 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7618 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7619 | ` */` |
|   41706 |  7620 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7621 |  |
|       - |  7622 | `	ph7_class **apIface;` |
|       - |  7623 | `	sxu32 nIface,i;` |
|       - |  7624 | `	sxi32 rc;` |
|   41708 |  7625 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7626 | `		return SXRET_OK;` |
|       - |  7627 | `	}` |
|   41708 |  7628 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   41708 |  7629 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   50528 |  7630 | `	for(i = 0; i < nIface; i++){` |
|    8822 |  7631 | `		ph7_class *pIface = apIface[i];` |
|       - |  7632 | `		SyHashEntry *pEntry;` |
|    8822 |  7633 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   70378 |  7634 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   61558 |  7635 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7636 | `			ph7_class_method *pImplMeth;` |
|   61558 |  7637 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7638 | `			/* Find the implementing method in the class */` |
|   61558 |  7639 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   61558 |  7640 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7641 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7642 | `			}` |
|       - |  7643 | `			/* Check visibility: interface methods must be implemented as public */` |
|   61544 |  7644 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7645 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7646 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7647 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7648 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7649 | `					return SXERR_ABORT;` |
|       - |  7650 | `				}` |
|       1 |  7651 | `			}` |
|       - |  7652 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7653 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7654 | `			 */` |
|       - |  7655 | `			{` |
|   61544 |  7656 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   61544 |  7657 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   61544 |  7658 | `				int sigError = 0;` |
|   61544 |  7659 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7660 | `					sigError = 1;` |
|   61543 |  7661 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7662 | `					/* Extra parameters must all have default values */` |
|       5 |  7663 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7664 | `					sxu32 k;` |
|       7 |  7665 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7666 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7667 | `							sigError = 1;` |
|       3 |  7668 | `							break;` |
|       - |  7669 | `						}` |
|       2 |  7670 | `					}` |
|       2 |  7671 | `				}` |
|   61544 |  7672 | `				if( sigError ){` |
|       - |  7673 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7674 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7675 | `					sxu32 j;` |
|       5 |  7676 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7677 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7678 | `					/* Build implementing method signature */` |
|       5 |  7679 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7680 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7681 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7682 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7683 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7684 | `					}` |
|       - |  7685 | `					/* Build interface method signature */` |
|       5 |  7686 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7687 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7688 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7689 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7690 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7691 | `					}` |
|       7 |  7692 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7693 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7694 | `						&pClass->sName,pMName,` |
|       4 |  7695 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7696 | `						&pIface->sName,pMName,` |
|       4 |  7697 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7698 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7699 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7700 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7701 | `						return SXERR_ABORT;` |
|       - |  7702 | `					}` |
|       2 |  7703 | `				}` |
|       - |  7704 | `			}` |
|       2 |  7705 | `		}` |
|    4412 |  7706 | `	}` |
|   41708 |  7707 | `	return SXRET_OK;` |
|   20855 |  7708 |  |
|       - |  7709 | `/*` |
|       - |  7710 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7711 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7712 | ` */` |
|   41706 |  7713 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7714 |  |
|       - |  7715 | `	ph7_class_method *pMeth;` |
|       - |  7716 | `	SyHashEntry *pEntry;` |
|       - |  7717 | `	sxu32 nAbstract;` |
|       - |  7718 | `	SyBlob sMsg;` |
|       - |  7719 | `	sxi32 rc;` |
|       - |  7720 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   41708 |  7721 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      22 |  7722 | `		return SXRET_OK;` |
|       - |  7723 | `	}` |
|       - |  7724 | `	/* Count abstract methods */` |
|   41688 |  7725 | `	nAbstract = 0;` |
|   41688 |  7726 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  393890 |  7727 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  352204 |  7728 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  352204 |  7729 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7730 | `			nAbstract++;` |
|       8 |  7731 | `		}` |
|       2 |  7732 | `	}` |
|   41688 |  7733 | `	if( nAbstract == 0 ){` |
|   41674 |  7734 | `		return SXRET_OK;` |
|       - |  7735 | `	}` |
|       - |  7736 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7737 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7738 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7739 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7740 | `		&pClass->sName,nAbstract,` |
|       7 |  7741 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7742 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7743 | `	/* Second pass: list methods with origins */` |
|       - |  7744 | `	{` |
|      15 |  7745 | `		sxu32 nListed = 0;` |
|      15 |  7746 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7747 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7748 | `			ph7_class *pOrigin = 0;` |
|       - |  7749 | `			SyString *pMName;` |
|      19 |  7750 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7751 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7752 | `				continue;` |
|       - |  7753 | `			}` |
|      17 |  7754 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7755 | `			if( nListed > 0 ){` |
|       3 |  7756 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7757 | `			}` |
|       - |  7758 | `			/* Find the origin of this abstract method.` |
|       - |  7759 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7760 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7761 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7762 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7763 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7764 | `			 * class's namespace.` |
|       - |  7765 | `			 */` |
|       - |  7766 | `			{` |
|       - |  7767 | `				ph7_class **apIface;` |
|       - |  7768 | `				ph7_class **apTrait;` |
|       - |  7769 | `				ph7_class *pWalk;` |
|       - |  7770 | `				sxu32 i;` |
|       - |  7771 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7772 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7773 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7774 | `				 */` |
|      17 |  7775 | `				if( pClass->pBase ){` |
|       9 |  7776 | `					pWalk = pClass->pBase;` |
|      17 |  7777 | `					while( pWalk ){` |
|       - |  7778 | `						ph7_class_method *pParentMeth;` |
|      11 |  7779 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7780 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7781 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7782 | `							 * in this class's ancestor chain.` |
|       - |  7783 | `							 */` |
|      11 |  7784 | `							int fromIface = 0;` |
|      11 |  7785 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7786 | `							while( pAnc ){` |
|       - |  7787 | `								ph7_class **apPI;` |
|       - |  7788 | `								sxu32 j;` |
|      13 |  7789 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7790 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7791 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7792 | `										fromIface = 1;` |
|       9 |  7793 | `										break;` |
|       - |  7794 | `									}` |
|     ! 0 |  7795 | `								}` |
|      13 |  7796 | `								if( fromIface ) break;` |
|       5 |  7797 | `								pAnc = pAnc->pBase;` |
|       1 |  7798 | `							}` |
|      11 |  7799 | `							if( !fromIface ){` |
|       3 |  7800 | `								pOrigin = pWalk;` |
|       3 |  7801 | `								break;` |
|       - |  7802 | `							}` |
|       4 |  7803 | `						}` |
|       9 |  7804 | `						pWalk = pWalk->pBase;` |
|       1 |  7805 | `					}` |
|       4 |  7806 | `				}` |
|       - |  7807 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7808 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7809 | `				 */` |
|      17 |  7810 | `				if( !pOrigin ){` |
|      15 |  7811 | `					pWalk = pClass;` |
|      37 |  7812 | `					while( pWalk && !pOrigin ){` |
|      23 |  7813 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7814 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7815 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7816 | `							ph7_class *pDeepest = 0;` |
|      25 |  7817 | `							while( pIface ){` |
|      13 |  7818 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7819 | `									pDeepest = pIface;` |
|       6 |  7820 | `								}` |
|      13 |  7821 | `								pIface = pIface->pBase;` |
|       1 |  7822 | `							}` |
|      13 |  7823 | `							if( pDeepest ){` |
|      13 |  7824 | `								pOrigin = pDeepest;` |
|      13 |  7825 | `								break;` |
|       - |  7826 | `							}` |
|     ! 0 |  7827 | `						}` |
|      23 |  7828 | `						pWalk = pWalk->pBase;` |
|       1 |  7829 | `					}` |
|       7 |  7830 | `				}` |
|       - |  7831 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7832 | `				if( !pOrigin ){` |
|       3 |  7833 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7834 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7835 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7836 | `							pOrigin = pClass;` |
|       3 |  7837 | `							break;` |
|       - |  7838 | `						}` |
|     ! 0 |  7839 | `					}` |
|       1 |  7840 | `				}` |
|       - |  7841 | `			}` |
|      17 |  7842 | `			if( pOrigin ){` |
|      17 |  7843 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7844 | `			}else{` |
|       - |  7845 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7846 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7847 | `			}` |
|      17 |  7848 | `			nListed++;` |
|       1 |  7849 | `		}` |
|       - |  7850 | `	}` |
|      15 |  7851 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7852 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7853 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7854 | `	SyBlobRelease(&sMsg);` |
|      15 |  7855 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7856 | `		return SXERR_ABORT;` |
|       - |  7857 | `	}` |
|      15 |  7858 | `	return SXRET_OK;` |
|   20855 |  7859 |  |
|       - |  7860 | `/*` |
|       - |  7861 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  7862 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  7863 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  7864 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  7865 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  7866 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  7867 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  7868 | ` */` |
|   32618 |  7869 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       2 |  7870 |  |
|   32620 |  7871 | `	int isAbsolute = 0;` |
|   32620 |  7872 | `	SyToken *pStart = pGen->pIn;` |
|       - |  7873 | `	SyBlob sName;` |
|   32620 |  7874 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      28 |  7875 | `		isAbsolute = 1;` |
|      28 |  7876 | `		pGen->pIn++;` |
|      13 |  7877 | `	}` |
|   32620 |  7878 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       7 |  7879 | `		pGen->pIn = pStart;` |
|       7 |  7880 | `		return SXERR_INVALID;` |
|       - |  7881 | `	}` |
|   32614 |  7882 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   32614 |  7883 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   32614 |  7884 | `	pGen->pIn++;` |
|   48936 |  7885 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   16326 |  7886 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  7887 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  7888 | `		pGen->pIn++;` |
|      13 |  7889 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  7890 | `		pGen->pIn++;` |
|       1 |  7891 | `	}` |
|   32614 |  7892 | `	if( isAbsolute ){` |
|      25 |  7893 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      13 |  7894 | `	}else{` |
|       - |  7895 | `		SyString sRaw;` |
|   32590 |  7896 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   32590 |  7897 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  7898 | `	}` |
|   32614 |  7899 | `	SyBlobRelease(&sName);` |
|   32614 |  7900 | `	return SXRET_OK;` |
|   16311 |  7901 |  |
|       - |  7902 | `/*` |
|       - |  7903 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  7904 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  7905 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  7906 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  7907 | ` * either direction cannot run unbounded.` |
|       - |  7908 | ` */` |
|       - |  7909 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    8826 |  7910 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       2 |  7911 |  |
|       - |  7912 | `	ph7_class **apParent;` |
|       - |  7913 | `	sxu32 n;` |
|   11798 |  7914 | `	while( pInterface ){` |
|    8834 |  7915 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  7916 | `			return FALSE;` |
|       - |  7917 | `		}` |
|   11768 |  7918 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    5868 |  7919 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    5864 |  7920 | `			return TRUE;` |
|       - |  7921 | `		}` |
|    2972 |  7922 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    2972 |  7923 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  7924 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  7925 | `				return TRUE;` |
|       - |  7926 | `			}` |
|     ! 0 |  7927 | `		}` |
|    2972 |  7928 | `		pInterface = pInterface->pBase;` |
|    2972 |  7929 | `		iDepth++;` |
|       2 |  7930 | `	}` |
|    2966 |  7931 | `	return FALSE;` |
|    4415 |  7932 |  |
|    8826 |  7933 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       2 |  7934 |  |
|    8828 |  7935 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       2 |  7936 |  |
|       - |  7937 | `/*` |
|       - |  7938 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  7939 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  7940 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  7941 | ` */` |
|    5862 |  7942 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       2 |  7943 |  |
|    5868 |  7944 | `	while( pBase ){` |
|      10 |  7945 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  7946 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  7947 | `			return TRUE;` |
|       - |  7948 | `		}` |
|      10 |  7949 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  7950 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  7951 | `			return TRUE;` |
|       - |  7952 | `		}` |
|       5 |  7953 | `		pBase = pBase->pBase;` |
|       1 |  7954 | `	}` |
|    5860 |  7955 | `	return FALSE;` |
|    2933 |  7956 |  |
|   41722 |  7957 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  7958 |  |
|   41724 |  7959 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7960 | `	ph7_class *pClass,*pBase;` |
|       - |  7961 | `	SyToken *pEnd,*pTmp;` |
|       - |  7962 | `	sxi32 iProtection;` |
|       - |  7963 | `	SySet aInterfaces;` |
|       - |  7964 | `	SySet aUseEntries;` |
|       - |  7965 | `	sxi32 iAttrflags;` |
|       - |  7966 | `	SyString *pName;` |
|       - |  7967 | `	sxi32 nKwrd;` |
|       - |  7968 | `	sxi32 rc;` |
|       - |  7969 | `	/* Jump the 'class' keyword */` |
|   41724 |  7970 | `	pGen->pIn++;` |
|   41724 |  7971 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7972 | `		/* Syntax error */` |
|     ! 0 |  7973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  7974 | `		if( rc == SXERR_ABORT ){` |
|       - |  7975 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7976 | `			return SXERR_ABORT;` |
|       - |  7977 | `		}` |
|       - |  7978 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  7979 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  7980 | `			pGen->pIn++;` |
|     ! 0 |  7981 | `		}` |
|     ! 0 |  7982 | `		return SXRET_OK;` |
|       - |  7983 | `	}` |
|       - |  7984 | `	/* Extract class name */` |
|   41724 |  7985 | `	pName = &pGen->pIn->sData;` |
|       - |  7986 | `	/* Advance the stream cursor */` |
|   41724 |  7987 | `	pGen->pIn++;` |
|       - |  7988 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7989 | `		SyBlob sFQN;` |
|       - |  7990 | `		SyString sFQNStr;` |
|   41724 |  7991 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   41724 |  7992 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   41724 |  7993 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   41724 |  7994 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   41724 |  7995 | `		SyBlobRelease(&sFQN);` |
|       - |  7996 | `	}` |
|   41724 |  7997 | `	if( pClass == 0 ){` |
|     ! 0 |  7998 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7999 | `		return SXERR_ABORT;` |
|       - |  8000 | `	}` |
|       - |  8001 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   41724 |  8002 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   41724 |  8003 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8004 | `	/* Assume a standalone class */` |
|   41724 |  8005 | `	pBase = 0;` |
|   41724 |  8006 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   32368 |  8007 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   32368 |  8008 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8009 | `			SyBlob sResolved;` |
|       - |  8010 | `			SyString sBaseName;` |
|       - |  8011 | `			sxu32 nRefLine;` |
|   23548 |  8012 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   23548 |  8013 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   23548 |  8014 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   23548 |  8015 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8016 | `				SyBlobRelease(&sResolved);` |
|       4 |  8017 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8018 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8019 | `					pName);` |
|       3 |  8020 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8021 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8022 | `					return SXERR_ABORT;` |
|       - |  8023 | `				}` |
|       3 |  8024 | `				return SXRET_OK;` |
|       - |  8025 | `			}` |
|   35318 |  8026 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   23544 |  8027 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   23546 |  8028 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8029 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8030 | `			/* Interfaces are not allowed */` |
|   23546 |  8031 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8032 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8033 | `			}` |
|   23546 |  8034 | `			if( pBase == 0 ){` |
|     ! 0 |  8035 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8036 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8037 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8038 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8039 | `					return SXERR_ABORT;` |
|       - |  8040 | `				}` |
|     ! 0 |  8041 | `			}else{` |
|   23546 |  8042 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8043 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8044 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8045 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8046 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8047 | `						return SXERR_ABORT;` |
|       - |  8048 | `					}` |
|     ! 0 |  8049 | `				}` |
|       - |  8050 | `			}` |
|   23546 |  8051 | `			SyBlobRelease(&sResolved);` |
|   11772 |  8052 | `		}` |
|   32366 |  8053 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8054 | `			ph7_class *pInterface;` |
|       - |  8055 | `			/* Interface implementation */` |
|    8828 |  8056 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4413 |  8057 | `			for(;;){` |
|       - |  8058 | `				SyBlob sResolved;` |
|       - |  8059 | `				SyString sIntName;` |
|       - |  8060 | `				sxu32 nRefLine;` |
|    8828 |  8061 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    8828 |  8062 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    8828 |  8063 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8064 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8065 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8066 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8067 | `						pName);` |
|     ! 0 |  8068 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8069 | `						return SXERR_ABORT;` |
|       - |  8070 | `					}` |
|     ! 0 |  8071 | `					break;` |
|       - |  8072 | `				}` |
|   17654 |  8073 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    8826 |  8074 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    8828 |  8075 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8076 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8077 | `				/* Only interfaces are allowed */` |
|    8828 |  8078 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8079 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8080 | `				}` |
|    8828 |  8081 | `				if( pInterface == 0 ){` |
|     ! 0 |  8082 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8083 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8084 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8085 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8086 | `						return SXERR_ABORT;` |
|       - |  8087 | `					}` |
|     ! 0 |  8088 | `				}else{` |
|       - |  8089 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8090 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8091 | `					 * unless they already extend Exception or Error.` |
|       - |  8092 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8093 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8094 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    8828 |  8095 | `					SyString *pFqn = &pClass->sName;` |
|    8828 |  8096 | `					int bIsExceptionOrError =` |
|    7339 |  8097 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   14702 |  8098 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    7366 |  8099 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    2932 |  8100 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14686 |  8101 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    8793 |  8102 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    2929 |  8103 | `						!bIsExceptionOrError ){` |
|      10 |  8104 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8105 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8106 | `							&pClass->sName);` |
|       7 |  8107 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8108 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8109 | `							return SXERR_ABORT;` |
|       - |  8110 | `						}` |
|       - |  8111 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8112 | `						 * check does not produce a duplicate fatal. */` |
|       4 |  8113 | `					}else{` |
|    8822 |  8114 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8115 | `					}` |
|       - |  8116 | `				}` |
|    8828 |  8117 | `				SyBlobRelease(&sResolved);` |
|    8828 |  8118 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4415 |  8119 | `					break;` |
|       - |  8120 | `				}` |
|     ! 0 |  8121 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8122 | `			}` |
|    4413 |  8123 | `		}` |
|   16182 |  8124 | `	}` |
|   41722 |  8125 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8126 | `		/* Syntax error */` |
|     ! 0 |  8127 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8128 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8129 | `		if( rc == SXERR_ABORT ){` |
|       - |  8130 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8131 | `			return SXERR_ABORT;` |
|       - |  8132 | `		}` |
|     ! 0 |  8133 | `		return SXRET_OK;` |
|       - |  8134 | `	}` |
|   41722 |  8135 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   41722 |  8136 | `	pEnd = 0; /* cc warning */` |
|       - |  8137 | `	/* Delimit the class body */` |
|   41722 |  8138 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   41722 |  8139 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8140 | `		/* Syntax error */` |
|     ! 0 |  8141 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8142 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8143 | `		if( rc == SXERR_ABORT ){` |
|       - |  8144 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8145 | `			return SXERR_ABORT;` |
|       - |  8146 | `		}` |
|     ! 0 |  8147 | `		return SXRET_OK;` |
|       - |  8148 | `	}` |
|       - |  8149 | `	/* Swap token stream */` |
|   41722 |  8150 | `	pTmp = pGen->pEnd;` |
|   41722 |  8151 | `	pGen->pEnd = pEnd;` |
|       - |  8152 | `	/* Set the inherited flags */` |
|   41722 |  8153 | `	pClass->iFlags = iFlags;` |
|       - |  8154 | `	/* Start the parse process */` |
|   92799 |  8155 | `	for(;;){` |
|       - |  8156 | `		/* Jump leading/trailing semi-colons */` |
|  297716 |  8157 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   56078 |  8158 | `			pGen->pIn++;` |
|       2 |  8159 | `		}` |
|  241640 |  8160 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8161 | `			/* End of class body */` |
|   41708 |  8162 | `			break;` |
|       - |  8163 | `		}` |
|  199934 |  8164 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8165 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8166 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8167 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8168 | `			if( rc == SXERR_ABORT ){` |
|       - |  8169 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8170 | `				return SXERR_ABORT;` |
|       - |  8171 | `			}` |
|     ! 0 |  8172 | `			goto done;` |
|       - |  8173 | `		}` |
|       - |  8174 | `		/* Assume public visibility */` |
|  199934 |  8175 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  199934 |  8176 | `		iAttrflags = 0;` |
|  199934 |  8177 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8178 | `			/* Extract the current keyword */` |
|  199934 |  8179 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  199934 |  8180 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8181 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8182 | `				TraitUseEntry sUse;` |
|      44 |  8183 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  8184 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  8185 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  8186 | `				for(;;){` |
|       - |  8187 | `					ph7_class *pTrait;` |
|       - |  8188 | `					SyString *pTraitName;` |
|      52 |  8189 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8190 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8191 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8192 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8193 | `							return SXERR_ABORT;` |
|       - |  8194 | `						}` |
|     ! 0 |  8195 | `						break;` |
|       - |  8196 | `					}` |
|      52 |  8197 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8198 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8199 | `						SyBlob sResolved;` |
|      52 |  8200 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  8201 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  8202 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  8203 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  8204 | `						SyBlobRelease(&sResolved);` |
|       - |  8205 | `					}` |
|       - |  8206 | `					/* Only traits are allowed */` |
|      52 |  8207 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8208 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8209 | `					}` |
|      52 |  8210 | `					if( pTrait == 0 ){` |
|     ! 0 |  8211 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8212 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8213 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8214 | `							return SXERR_ABORT;` |
|       - |  8215 | `						}` |
|     ! 0 |  8216 | `					}else{` |
|      52 |  8217 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8218 | `					}` |
|      52 |  8219 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  8220 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  8221 | `						break;` |
|       - |  8222 | `					}` |
|       9 |  8223 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  8224 | `				}` |
|       - |  8225 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  8226 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8227 | `					SyToken *pBlock;` |
|       9 |  8228 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  8229 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  8230 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  8231 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  8232 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  8233 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  8234 | `					}else{` |
|     ! 0 |  8235 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8236 | `					}` |
|       4 |  8237 | `				}` |
|      44 |  8238 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8239 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  8240 | `				continue;` |
|       - |  8241 | `			}` |
|  199892 |  8242 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  196846 |  8243 | `				iProtection = nKwrd;` |
|  196846 |  8244 | `				pGen->pIn++; /* Jump the visibility token */` |
|  196844 |  8245 | `				if( pGen->pIn >= pGen->pEnd` |
|  196846 |  8246 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8247 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8248 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8249 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8250 | `					if( rc == SXERR_ABORT ){` |
|       - |  8251 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8252 | `						return SXERR_ABORT;` |
|       - |  8253 | `					}` |
|     ! 0 |  8254 | `					goto done;` |
|       - |  8255 | `				}` |
|  196846 |  8256 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8257 | `					/* Attribute declaration (untyped) */` |
|   55888 |  8258 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   55888 |  8259 | `					if( rc != SXRET_OK ){` |
|       3 |  8260 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8261 | `							return SXERR_ABORT;` |
|       - |  8262 | `						}` |
|       3 |  8263 | `						goto done;` |
|       - |  8264 | `					}` |
|   55886 |  8265 | `					continue;` |
|       - |  8266 | `				}` |
|  140960 |  8267 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8268 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     106 |  8269 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     106 |  8270 | `					if( rc != SXRET_OK ){` |
|       3 |  8271 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8272 | `							return SXERR_ABORT;` |
|       - |  8273 | `						}` |
|       3 |  8274 | `						goto done;` |
|       - |  8275 | `					}` |
|     104 |  8276 | `					continue;` |
|       - |  8277 | `				}` |
|       - |  8278 | `				/* Extract the keyword */` |
|  140856 |  8279 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   70427 |  8280 | `			}` |
|  143902 |  8281 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8282 | `				/* Process constant declaration */` |
|      30 |  8283 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  8284 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8285 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8286 | `						return SXERR_ABORT;` |
|       - |  8287 | `					}` |
|     ! 0 |  8288 | `					goto done;` |
|       - |  8289 | `				}` |
|      16 |  8290 | `			}else{` |
|  143874 |  8291 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8292 | `					/* Static method or attribute,record that */` |
|    2966 |  8293 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2966 |  8294 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2966 |  8295 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8296 | `						/* Extract the keyword */` |
|    2960 |  8297 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2960 |  8298 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8299 | `							iProtection = nKwrd;` |
|     ! 0 |  8300 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8301 | `						}` |
|    1479 |  8302 | `					}` |
|    2964 |  8303 | `					if( pGen->pIn >= pGen->pEnd` |
|    2966 |  8304 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8305 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8306 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8307 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8308 | `						if( rc == SXERR_ABORT ){` |
|       - |  8309 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8310 | `							return SXERR_ABORT;` |
|       - |  8311 | `						}` |
|     ! 0 |  8312 | `						goto done;` |
|       - |  8313 | `					}` |
|    2966 |  8314 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8315 | `						/* Attribute declaration */` |
|       5 |  8316 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8317 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8318 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8319 | `								return SXERR_ABORT;` |
|       - |  8320 | `							}` |
|     ! 0 |  8321 | `							goto done;` |
|       - |  8322 | `						}` |
|       5 |  8323 | `						continue;` |
|       - |  8324 | `					}` |
|    2962 |  8325 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8326 | `						/* Typed static attribute declaration */` |
|      10 |  8327 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 |  8328 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8329 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8330 | `								return SXERR_ABORT;` |
|       - |  8331 | `							}` |
|     ! 0 |  8332 | `							goto done;` |
|       - |  8333 | `						}` |
|      10 |  8334 | `						continue;` |
|       - |  8335 | `					}` |
|       - |  8336 | `					/* Extract the keyword */` |
|    2954 |  8337 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  142386 |  8338 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8339 | `					/* Abstract method,record that */` |
|      12 |  8340 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8341 | `					/* Mark the whole class as abstract */` |
|      12 |  8342 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8343 | `					/* Advance the stream cursor */` |
|      12 |  8344 | `					pGen->pIn++;` |
|      12 |  8345 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8346 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8347 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8348 | `							iProtection = nKwrd;` |
|      10 |  8349 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8350 | `						}` |
|       5 |  8351 | `					}` |
|      12 |  8352 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8353 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8354 | `							/* Static method */` |
|     ! 0 |  8355 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8356 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8357 | `					}` |
|      12 |  8358 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8359 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8360 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8361 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8362 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8363 | `							if( rc == SXERR_ABORT ){` |
|       - |  8364 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8365 | `								return SXERR_ABORT;` |
|       - |  8366 | `							}` |
|     ! 0 |  8367 | `							goto done;` |
|       - |  8368 | `					}` |
|      12 |  8369 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  140905 |  8370 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8371 | `					/* final method ,record that */` |
|       5 |  8372 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  8373 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  8374 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8375 | `						/* Extract the keyword */` |
|       5 |  8376 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8377 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8378 | `							iProtection = nKwrd;` |
|       5 |  8379 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8380 | `						}` |
|       2 |  8381 | `					}` |
|       5 |  8382 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8383 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8384 | `							/* Static method */` |
|     ! 0 |  8385 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8386 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8387 | `					}` |
|       5 |  8388 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8389 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8390 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8391 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8392 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8393 | `							if( rc == SXERR_ABORT ){` |
|       - |  8394 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8395 | `								return SXERR_ABORT;` |
|       - |  8396 | `							}` |
|     ! 0 |  8397 | `							goto done;` |
|       - |  8398 | `					}` |
|       5 |  8399 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8400 | `				}` |
|  143862 |  8401 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8402 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8403 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8404 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8405 | `						if( rc == SXERR_ABORT ){` |
|       - |  8406 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8407 | `							return SXERR_ABORT;` |
|       - |  8408 | `						}` |
|     ! 0 |  8409 | `						goto done;` |
|       - |  8410 | `				}` |
|  143862 |  8411 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8412 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8413 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8414 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8415 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8416 | `						if( rc == SXERR_ABORT ){` |
|       - |  8417 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8418 | `							return SXERR_ABORT;` |
|       - |  8419 | `						}` |
|     ! 0 |  8420 | `						goto done;` |
|       - |  8421 | `					}` |
|       - |  8422 | `					/* Attribute declaration */` |
|       7 |  8423 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8424 | `				}else{` |
|       - |  8425 | `					/* Process method declaration */` |
|  143856 |  8426 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8427 | `				}` |
|  143862 |  8428 | `				if( rc != SXRET_OK ){` |
|      11 |  8429 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8430 | `						return SXERR_ABORT;` |
|       - |  8431 | `					}` |
|      11 |  8432 | `					goto done;` |
|       - |  8433 | `				}` |
|       - |  8434 | `			}` |
|   71941 |  8435 | `		}else{` |
|       - |  8436 | `			/* Attribute declaration */` |
|     ! 0 |  8437 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8438 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8439 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8440 | `					return SXERR_ABORT;` |
|       - |  8441 | `				}` |
|     ! 0 |  8442 | `				goto done;` |
|       - |  8443 | `			}` |
|       - |  8444 | `		}` |
|       2 |  8445 | `	}` |
|       - |  8446 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8447 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8448 | `	 */` |
|       - |  8449 | `	{` |
|       - |  8450 | `		TraitUseEntry *apUse;` |
|       - |  8451 | `		sxu32 nU;` |
|   41708 |  8452 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   41750 |  8453 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  8454 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  8455 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  8456 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  8457 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8458 | `			sxu32 nT;` |
|      44 |  8459 | `			if( !hasResolution ){` |
|       - |  8460 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  8461 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  8462 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  8463 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8464 | `						break;` |
|       - |  8465 | `					}` |
|      22 |  8466 | `				}` |
|      19 |  8467 | `			}else{` |
|       - |  8468 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8469 | `				 * then use the block to resolve method conflicts.` |
|       - |  8470 | `				 */` |
|       - |  8471 | `				SyToken *pR;` |
|      19 |  8472 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  8473 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8474 | `					ph7_class_attr *pAR;` |
|       - |  8475 | `					SyHashEntry *pER;` |
|       - |  8476 | `					SyString *pNR;` |
|      11 |  8477 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  8478 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8479 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8480 | `						pNR = &pAR->sName;` |
|     ! 0 |  8481 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8482 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8483 | `						}` |
|     ! 0 |  8484 | `					}` |
|      11 |  8485 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  8486 | `				}` |
|       - |  8487 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  8488 | `				pR = pUse->pResolvStart;` |
|      21 |  8489 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8490 | `					SyString sTrait,sMethod;` |
|       - |  8491 | `					ph7_class *pSrcTrait;` |
|       - |  8492 | `					ph7_class_method *pMeth;` |
|       - |  8493 | `					sxi32 nRKwrd;` |
|      33 |  8494 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8495 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8496 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8497 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8498 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8499 | `					sMethod = pR->sData;` |
|      13 |  8500 | `					pR++;` |
|      13 |  8501 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8502 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8503 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8504 | `							sTrait = sMethod;` |
|       7 |  8505 | `							pR++;` |
|       7 |  8506 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8507 | `							sMethod = pR->sData;` |
|       7 |  8508 | `							pR++;` |
|       3 |  8509 | `						}` |
|       3 |  8510 | `					}` |
|      13 |  8511 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8512 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8513 | `						continue;` |
|       - |  8514 | `					}` |
|      13 |  8515 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8516 | `					pR++;` |
|      13 |  8517 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8518 | `						pSrcTrait = 0;` |
|       7 |  8519 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8520 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8521 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8522 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8523 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8524 | `								break;` |
|       - |  8525 | `							}` |
|       2 |  8526 | `						}` |
|       5 |  8527 | `						if( pSrcTrait ){` |
|       5 |  8528 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8529 | `							if( pMeth ){` |
|       5 |  8530 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8531 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8532 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8533 | `								}` |
|       2 |  8534 | `							}` |
|       2 |  8535 | `						}` |
|       2 |  8536 | `					}` |
|      29 |  8537 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8538 | `				}` |
|       - |  8539 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8540 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8541 | `					ph7_class_method *pMR;` |
|       - |  8542 | `					SyHashEntry *pER;` |
|       - |  8543 | `					SyString *pNR;` |
|      11 |  8544 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8545 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8546 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8547 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8548 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8549 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8550 | `						}` |
|       1 |  8551 | `					}` |
|       6 |  8552 | `				}` |
|       - |  8553 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8554 | `				pR = pUse->pResolvStart;` |
|      21 |  8555 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8556 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8557 | `					ph7_class *pSrcTrait;` |
|       - |  8558 | `					ph7_class_method *pMeth;` |
|      21 |  8559 | `					int hasQual = 0;` |
|       - |  8560 | `					sxi32 nRKwrd;` |
|      33 |  8561 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8562 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8563 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8564 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8565 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8566 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8567 | `					sMethod = pR->sData;` |
|      13 |  8568 | `					pR++;` |
|      13 |  8569 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8570 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8571 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8572 | `							sTrait = sMethod;` |
|       7 |  8573 | `							hasQual = 1;` |
|       7 |  8574 | `							pR++;` |
|       7 |  8575 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8576 | `							sMethod = pR->sData;` |
|       7 |  8577 | `							pR++;` |
|       3 |  8578 | `						}` |
|       3 |  8579 | `					}` |
|      13 |  8580 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8581 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8582 | `						continue;` |
|       - |  8583 | `					}` |
|      13 |  8584 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8585 | `					pR++;` |
|      13 |  8586 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8587 | `						sxi32 iNewVis = -1;` |
|       9 |  8588 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8589 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8590 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8591 | `								iNewVis = nAK;` |
|       7 |  8592 | `								pR++;` |
|       3 |  8593 | `							}` |
|       3 |  8594 | `						}` |
|       9 |  8595 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8596 | `							sAlias = pR->sData;` |
|       7 |  8597 | `							pR++;` |
|       3 |  8598 | `						}` |
|       9 |  8599 | `						pMeth = 0;` |
|       9 |  8600 | `						if( hasQual ){` |
|       3 |  8601 | `							pSrcTrait = 0;` |
|       5 |  8602 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8603 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8604 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8605 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8606 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8607 | `									break;` |
|       - |  8608 | `								}` |
|       2 |  8609 | `							}` |
|       3 |  8610 | `							if( pSrcTrait ){` |
|       3 |  8611 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8612 | `							}` |
|       2 |  8613 | `						}else{` |
|       7 |  8614 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8615 | `						}` |
|       9 |  8616 | `						if( pMeth ){` |
|       9 |  8617 | `							if( sAlias.nByte > 0 ){` |
|       - |  8618 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8619 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8620 | `								 */` |
|       - |  8621 | `								ph7_class_method *pAlias;` |
|       - |  8622 | `								char *zAliasDup;` |
|       7 |  8623 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8624 | `								if( pAlias ){` |
|       7 |  8625 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8626 | `									if( iNewVis >= 0 ){` |
|       5 |  8627 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8628 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8629 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8630 | `									}` |
|       7 |  8631 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8632 | `									if( zAliasDup ){` |
|       7 |  8633 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8634 | `									}` |
|       4 |  8635 | `								}` |
|       6 |  8636 | `							}else if( iNewVis >= 0 ){` |
|       - |  8637 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8638 | `								ph7_class_method *pCopy;` |
|       3 |  8639 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8640 | `								if( pCopy ){` |
|       3 |  8641 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8642 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8643 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8644 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8645 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8646 | `									/* Replace the method in the class hash */` |
|       3 |  8647 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8648 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8649 | `								}` |
|       1 |  8650 | `							}` |
|       4 |  8651 | `						}` |
|       4 |  8652 | `						SXUNUSED(hasQual);` |
|       4 |  8653 | `					}` |
|      17 |  8654 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8655 | `				}` |
|       - |  8656 | `			}` |
|      44 |  8657 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  8658 | `		}` |
|       - |  8659 | `	}` |
|       - |  8660 | `	/* Install the class */` |
|   41708 |  8661 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   41708 |  8662 | `	if( rc == SXRET_OK ){` |
|       - |  8663 | `		ph7_class **apInterface;` |
|       - |  8664 | `		sxu32 n;` |
|   41708 |  8665 | `		if( pBase ){` |
|       - |  8666 | `			/* Inherit from base class and mark as a subclass */` |
|   23546 |  8667 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   11772 |  8668 | `		}` |
|   41708 |  8669 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   50528 |  8670 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8671 | `			/* Implements one or more interface */` |
|    8822 |  8672 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    8822 |  8673 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8674 | `				break;` |
|       - |  8675 | `			}` |
|    4412 |  8676 | `		}` |
|       - |  8677 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   41708 |  8678 | `		if( rc == SXRET_OK ){` |
|   41708 |  8679 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   41708 |  8680 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8681 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8682 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8683 | `				return SXERR_ABORT;` |
|       - |  8684 | `			}` |
|   20853 |  8685 | `		}` |
|       - |  8686 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   41708 |  8687 | `		if( rc == SXRET_OK ){` |
|   41708 |  8688 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   41708 |  8689 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8690 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8691 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8692 | `				return SXERR_ABORT;` |
|       - |  8693 | `			}` |
|   20853 |  8694 | `		}` |
|   20853 |  8695 | `	}` |
|   41708 |  8696 | `	SySetRelease(&aUseEntries);` |
|   41708 |  8697 | `	SySetRelease(&aInterfaces);` |
|   41708 |  8698 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8699 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8700 | `		return SXERR_ABORT;` |
|       - |  8701 | `	}` |
|   20853 |  8702 | `done:` |
|       - |  8703 | `	/* Point beyond the class body */` |
|   41722 |  8704 | `	pGen->pIn = &pEnd[1];` |
|   41722 |  8705 | `	pGen->pEnd = pTmp;` |
|   41722 |  8706 | `	return PH7_OK;` |
|   20863 |  8707 |  |
|       - |  8708 | `/*` |
|       - |  8709 | ` * Compile a user-defined abstract class.` |
|       - |  8710 | ` *  According to the PHP language reference manual` |
|       - |  8711 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8712 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8713 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8714 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8715 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8716 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8717 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8718 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8719 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8720 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8721 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8722 | ` *   could differ.` |
|       - |  8723 | ` */` |
|      18 |  8724 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8725 |  |
|       - |  8726 | `	sxi32 rc;` |
|      20 |  8727 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      20 |  8728 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      20 |  8729 | `	return rc;` |
|       2 |  8730 |  |
|       - |  8731 | `/*` |
|       - |  8732 | ` * Compile a user-defined final class.` |
|       - |  8733 | ` *  According to the PHP language reference manual` |
|       - |  8734 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8735 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8736 | ` *    final then it cannot be extended.` |
|       - |  8737 | ` */` |
|       2 |  8738 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8739 |  |
|       - |  8740 | `	sxi32 rc;` |
|       3 |  8741 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8742 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8743 | `	return rc;` |
|       1 |  8744 |  |
|       - |  8745 | `/*` |
|       - |  8746 | ` * Compile a user-defined trait.` |
|       - |  8747 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8748 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8749 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8750 | ` */` |
|      54 |  8751 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8752 |  |
|      56 |  8753 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8754 | `	ph7_class *pClass;` |
|       - |  8755 | `	SyToken *pEnd,*pTmp;` |
|       - |  8756 | `	sxi32 iProtection;` |
|       - |  8757 | `	sxi32 iAttrflags;` |
|       - |  8758 | `	SyString *pName;` |
|       - |  8759 | `	sxi32 nKwrd;` |
|       - |  8760 | `	sxi32 rc;` |
|       - |  8761 | `	/* Jump the 'trait' keyword */` |
|      56 |  8762 | `	pGen->pIn++;` |
|      56 |  8763 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8764 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8765 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8766 | `			return SXERR_ABORT;` |
|       - |  8767 | `		}` |
|     ! 0 |  8768 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8769 | `			pGen->pIn++;` |
|     ! 0 |  8770 | `		}` |
|     ! 0 |  8771 | `		return SXRET_OK;` |
|       - |  8772 | `	}` |
|       - |  8773 | `	/* Extract trait name */` |
|      56 |  8774 | `	pName = &pGen->pIn->sData;` |
|      56 |  8775 | `	pGen->pIn++;` |
|       - |  8776 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8777 | `		SyBlob sFQN;` |
|       - |  8778 | `		SyString sFQNStr;` |
|      56 |  8779 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8780 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8781 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8782 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8783 | `		SyBlobRelease(&sFQN);` |
|       - |  8784 | `	}` |
|      56 |  8785 | `	if( pClass == 0 ){` |
|     ! 0 |  8786 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8787 | `		return SXERR_ABORT;` |
|       - |  8788 | `	}` |
|       - |  8789 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8790 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8791 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8792 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8793 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8794 | `			return SXERR_ABORT;` |
|       - |  8795 | `		}` |
|     ! 0 |  8796 | `		return SXRET_OK;` |
|       - |  8797 | `	}` |
|      56 |  8798 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8799 | `	pEnd = 0;` |
|      56 |  8800 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8801 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8802 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8803 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8804 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8805 | `			return SXERR_ABORT;` |
|       - |  8806 | `		}` |
|     ! 0 |  8807 | `		return SXRET_OK;` |
|       - |  8808 | `	}` |
|       - |  8809 | `	/* Swap token stream */` |
|      56 |  8810 | `	pTmp = pGen->pEnd;` |
|      56 |  8811 | `	pGen->pEnd = pEnd;` |
|       - |  8812 | `	/* Mark as trait */` |
|      56 |  8813 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8814 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8815 | `	for(;;){` |
|     154 |  8816 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8817 | `			pGen->pIn++;` |
|       2 |  8818 | `		}` |
|     130 |  8819 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8820 | `			break;` |
|       - |  8821 | `		}` |
|      76 |  8822 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8823 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8824 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8825 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8826 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8827 | `				return SXERR_ABORT;` |
|       - |  8828 | `			}` |
|     ! 0 |  8829 | `			goto done;` |
|       - |  8830 | `		}` |
|      76 |  8831 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8832 | `		iAttrflags = 0;` |
|      76 |  8833 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8834 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8835 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8836 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8837 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8838 | `				for(;;){` |
|       - |  8839 | `					ph7_class *pUsedTrait;` |
|       - |  8840 | `					SyString *pUsedName;` |
|       5 |  8841 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8842 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8843 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8844 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8845 | `							return SXERR_ABORT;` |
|       - |  8846 | `						}` |
|     ! 0 |  8847 | `						break;` |
|       - |  8848 | `					}` |
|       5 |  8849 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8850 | `					{` |
|       - |  8851 | `						SyBlob sResolved;` |
|       5 |  8852 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8853 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8854 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8855 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8856 | `						SyBlobRelease(&sResolved);` |
|       - |  8857 | `					}` |
|       5 |  8858 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8859 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8860 | `					}` |
|       5 |  8861 | `					if( pUsedTrait == 0 ){` |
|       4 |  8862 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8863 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8864 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8865 | `							return SXERR_ABORT;` |
|       - |  8866 | `						}` |
|       2 |  8867 | `					}else{` |
|       3 |  8868 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8869 | `					}` |
|       5 |  8870 | `					pGen->pIn++;` |
|       5 |  8871 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8872 | `						break;` |
|       - |  8873 | `					}` |
|     ! 0 |  8874 | `					pGen->pIn++;` |
|     ! 0 |  8875 | `				}` |
|       5 |  8876 | `				continue;` |
|       - |  8877 | `			}` |
|      72 |  8878 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8879 | `				iProtection = nKwrd;` |
|      68 |  8880 | `				pGen->pIn++;` |
|      66 |  8881 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8882 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8883 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8884 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8885 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8886 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8887 | `						return SXERR_ABORT;` |
|       - |  8888 | `					}` |
|     ! 0 |  8889 | `					goto done;` |
|       - |  8890 | `				}` |
|      68 |  8891 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8892 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8893 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8894 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8895 | `							return SXERR_ABORT;` |
|       - |  8896 | `						}` |
|     ! 0 |  8897 | `						goto done;` |
|       - |  8898 | `					}` |
|      11 |  8899 | `					continue;` |
|       - |  8900 | `				}` |
|      58 |  8901 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8902 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8903 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8904 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8905 | `							return SXERR_ABORT;` |
|       - |  8906 | `						}` |
|     ! 0 |  8907 | `						goto done;` |
|       - |  8908 | `					}` |
|       5 |  8909 | `					continue;` |
|       - |  8910 | `				}` |
|      53 |  8911 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8912 | `			}` |
|      57 |  8913 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8914 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8915 | `					"Traits cannot have constants");` |
|     ! 0 |  8916 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8917 | `					return SXERR_ABORT;` |
|       - |  8918 | `				}` |
|     ! 0 |  8919 | `				goto done;` |
|     ! 0 |  8920 | `			}else{` |
|      57 |  8921 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8922 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8923 | `					pGen->pIn++;` |
|       5 |  8924 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8925 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8926 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8927 | `							iProtection = nKwrd;` |
|     ! 0 |  8928 | `							pGen->pIn++;` |
|     ! 0 |  8929 | `						}` |
|       1 |  8930 | `					}` |
|       4 |  8931 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8932 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8933 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8934 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8935 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8936 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8937 | `							return SXERR_ABORT;` |
|       - |  8938 | `						}` |
|     ! 0 |  8939 | `						goto done;` |
|       - |  8940 | `					}` |
|       5 |  8941 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8942 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8943 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8944 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8945 | `								return SXERR_ABORT;` |
|       - |  8946 | `							}` |
|     ! 0 |  8947 | `							goto done;` |
|       - |  8948 | `						}` |
|       3 |  8949 | `						continue;` |
|       - |  8950 | `					}` |
|       3 |  8951 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8952 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8953 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8954 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8955 | `								return SXERR_ABORT;` |
|       - |  8956 | `							}` |
|     ! 0 |  8957 | `							goto done;` |
|       - |  8958 | `						}` |
|     ! 0 |  8959 | `						continue;` |
|       - |  8960 | `					}` |
|       3 |  8961 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  8962 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  8963 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  8964 | `					pGen->pIn++;` |
|       5 |  8965 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  8966 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8967 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8968 | `							iProtection = nKwrd;` |
|       5 |  8969 | `							pGen->pIn++;` |
|       2 |  8970 | `						}` |
|       2 |  8971 | `					}` |
|       5 |  8972 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8973 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8974 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8975 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  8976 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8977 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8978 | `							return SXERR_ABORT;` |
|       - |  8979 | `						}` |
|     ! 0 |  8980 | `						goto done;` |
|       - |  8981 | `					}` |
|       5 |  8982 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8983 | `				}` |
|      55 |  8984 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8985 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8986 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  8987 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8988 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8989 | `						return SXERR_ABORT;` |
|       - |  8990 | `					}` |
|     ! 0 |  8991 | `					goto done;` |
|       - |  8992 | `				}` |
|      55 |  8993 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  8994 | `					pGen->pIn++;` |
|     ! 0 |  8995 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8996 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8997 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8998 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8999 | `							return SXERR_ABORT;` |
|       - |  9000 | `						}` |
|     ! 0 |  9001 | `						goto done;` |
|       - |  9002 | `					}` |
|     ! 0 |  9003 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9004 | `				}else{` |
|      55 |  9005 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9006 | `				}` |
|      55 |  9007 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9008 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9009 | `						return SXERR_ABORT;` |
|       - |  9010 | `					}` |
|     ! 0 |  9011 | `					goto done;` |
|       - |  9012 | `				}` |
|       - |  9013 | `			}` |
|      28 |  9014 | `		}else{` |
|     ! 0 |  9015 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9016 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9017 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9018 | `					return SXERR_ABORT;` |
|       - |  9019 | `				}` |
|     ! 0 |  9020 | `				goto done;` |
|       - |  9021 | `			}` |
|       - |  9022 | `		}` |
|       1 |  9023 | `	}` |
|       - |  9024 | `	/* Install the trait */` |
|      56 |  9025 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  9026 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9027 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9028 | `		return SXERR_ABORT;` |
|       - |  9029 | `	}` |
|      27 |  9030 | `done:` |
|       - |  9031 | `	/* Point beyond the trait body */` |
|      56 |  9032 | `	pGen->pIn = &pEnd[1];` |
|      56 |  9033 | `	pGen->pEnd = pTmp;` |
|      56 |  9034 | `	return PH7_OK;` |
|      29 |  9035 |  |
|       - |  9036 | `/*` |
|       - |  9037 | ` * Compile a user-defined class.` |
|       - |  9038 | ` *  According to the PHP language reference manual` |
|       - |  9039 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9040 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9041 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9042 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9043 | ` *   and functions (called "methods").` |
|       - |  9044 | ` */` |
|   41702 |  9045 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  9046 |  |
|       - |  9047 | `	sxi32 rc;` |
|   41704 |  9048 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   41704 |  9049 | `	return rc;` |
|       2 |  9050 |  |
|       - |  9051 | `/*` |
|       - |  9052 | ` * Exception handling.` |
|       - |  9053 | ` *  According to the PHP language reference manual` |
|       - |  9054 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9055 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9056 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9057 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9058 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9059 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9060 | ` *    (or re-thrown) within a catch block.` |
|       - |  9061 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9062 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9063 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9064 | ` *    been defined with set_exception_handler().` |
|       - |  9065 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9066 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9067 | ` */` |
|       - |  9068 | `/*` |
|       - |  9069 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9070 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9071 | ` * indicates failure.` |
|       - |  9072 | ` */` |
|    8896 |  9073 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  9074 |  |
|    8898 |  9075 | `	sxi32 rc = SXRET_OK;` |
|    8898 |  9076 | `	if( pRoot->pOp ){` |
|    8890 |  9077 | `		switch( pRoot->pOp->iOp ){` |
|    4444 |  9078 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9079 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9080 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9081 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9082 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9083 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    8890 |  9084 | `			break;` |
|     ! 0 |  9085 | `		default:` |
|       - |  9086 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9087 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9088 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9089 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9090 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9091 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9092 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9093 | `			}` |
|     ! 0 |  9094 | `			break;` |
|       - |  9095 | `		}` |
|    4454 |  9096 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9097 | `		/* Unexpected expression */` |
|     ! 0 |  9098 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9099 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9100 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9101 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9102 | `		}` |
|     ! 0 |  9103 | `	}` |
|    8898 |  9104 | `	return rc;` |
|       2 |  9105 |  |
|       - |  9106 | `/*` |
|       - |  9107 | ` * Compile a 'throw' statement.` |
|       - |  9108 | ` * throw: This is how you trigger an exception.` |
|       - |  9109 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9110 | ` */` |
|    8860 |  9111 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  9112 |  |
|    8862 |  9113 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9114 | `	GenBlock *pBlock;` |
|       - |  9115 | `	sxu32 nIdx;` |
|       - |  9116 | `	sxi32 rc;` |
|    8862 |  9117 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9118 | `	/* Compile the expression */` |
|    8862 |  9119 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8862 |  9120 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9121 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9122 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9123 | `			return SXERR_ABORT;` |
|       - |  9124 | `		}` |
|     ! 0 |  9125 | `		return SXRET_OK;` |
|       - |  9126 | `	}` |
|    8862 |  9127 | `	pBlock = pGen->pCurrent;` |
|       - |  9128 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   41130 |  9129 | `	while(pBlock->pParent){` |
|   41126 |  9130 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8858 |  9131 | `			break;` |
|       - |  9132 | `		}` |
|       - |  9133 | `		/* Point to the parent block */` |
|   32270 |  9134 | `		pBlock = pBlock->pParent;` |
|       2 |  9135 | `	}` |
|       - |  9136 | `	/* Emit the throw instruction */` |
|    8862 |  9137 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9138 | `	/* Emit the jump */` |
|    8862 |  9139 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8862 |  9140 | `	return SXRET_OK;` |
|    4432 |  9141 |  |
|       - |  9142 | `/*` |
|       - |  9143 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9144 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9145 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9146 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9147 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9148 | ` */` |
|      36 |  9149 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9150 |  |
|      38 |  9151 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9152 | `	GenBlock *pBlock;` |
|       - |  9153 | `	sxu32 nIdx;` |
|       - |  9154 | `	sxi32 rc;` |
|      18 |  9155 | `	(void)iCompileFlag;` |
|      38 |  9156 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9157 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9158 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9159 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9160 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9161 | `			return SXERR_ABORT;` |
|       - |  9162 | `		}` |
|     ! 0 |  9163 | `		return SXRET_OK;` |
|       - |  9164 | `	}` |
|      38 |  9165 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9166 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9167 | `		return SXERR_ABORT;` |
|       - |  9168 | `	}` |
|      38 |  9169 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9170 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9171 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9172 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9173 | `			return SXERR_ABORT;` |
|       - |  9174 | `		}` |
|     ! 0 |  9175 | `		return SXRET_OK;` |
|       - |  9176 | `	}` |
|       - |  9177 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9178 | `	pBlock = pGen->pCurrent;` |
|      60 |  9179 | `	while( pBlock->pParent ){` |
|      49 |  9180 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9181 | `			break;` |
|       - |  9182 | `		}` |
|      23 |  9183 | `		pBlock = pBlock->pParent;` |
|       1 |  9184 | `	}` |
|      38 |  9185 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9186 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9187 | `	return SXRET_OK;` |
|      20 |  9188 |  |
|       - |  9189 | `/*` |
|       - |  9190 | ` * Compile a 'catch' block.` |
|       - |  9191 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9192 | ` * an object containing the exception information.` |
|       - |  9193 | ` */` |
|     214 |  9194 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  9195 |  |
|     216 |  9196 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9197 | `	ph7_exception_block sCatch;` |
|       - |  9198 | `	SySet *pInstrContainer;` |
|       - |  9199 | `	SyString sClassName;` |
|       - |  9200 | `	GenBlock *pCatch;` |
|       - |  9201 | `	SyToken *pToken;` |
|       - |  9202 | `	SyString *pName;` |
|       - |  9203 | `	char *zDup;` |
|       - |  9204 | `	sxi32 rc;` |
|     216 |  9205 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9206 | `	/* Zero the structure */` |
|     216 |  9207 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9208 | `	/* Initialize fields */` |
|     216 |  9209 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     216 |  9210 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     216 |  9211 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9212 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9213 | `			pToken = pGen->pIn;` |
|     ! 0 |  9214 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9215 | `				pToken--;` |
|     ! 0 |  9216 | `			}` |
|     ! 0 |  9217 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9218 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9219 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9220 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9221 | `				return SXERR_ABORT;` |
|       - |  9222 | `			}` |
|     ! 0 |  9223 | `			return SXERR_INVALID;` |
|       - |  9224 | `	}` |
|       - |  9225 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     216 |  9226 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     120 |  9227 | `	for(;;){` |
|       - |  9228 | `		SyBlob sResolved;` |
|     242 |  9229 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     242 |  9230 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       5 |  9231 | `			SyBlobRelease(&sResolved);` |
|       5 |  9232 | `			pToken = pGen->pIn;` |
|       5 |  9233 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9234 | `				pToken--;` |
|     ! 0 |  9235 | `			}` |
|       7 |  9236 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9237 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9238 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  9239 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9240 | `				return SXERR_ABORT;` |
|       - |  9241 | `			}` |
|       5 |  9242 | `			return SXERR_INVALID;` |
|       - |  9243 | `		}` |
|       - |  9244 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9245 | `		 * transient SyBlob allocation. */` |
|     356 |  9246 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     236 |  9247 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     238 |  9248 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     238 |  9249 | `		SyBlobRelease(&sResolved);` |
|     238 |  9250 | `		if( zDup == 0 ){` |
|     ! 0 |  9251 | `			goto Mem;` |
|       - |  9252 | `		}` |
|     238 |  9253 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     238 |  9254 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9255 | `			goto Mem;` |
|       - |  9256 | `		}` |
|       - |  9257 | `		/* Check for '\|' (multi-catch separator) */` |
|     249 |  9258 | `		if( pGen->pIn < pGen->pEnd &&` |
|     236 |  9259 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      28 |  9260 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9261 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9262 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9263 | `			continue;` |
|       - |  9264 | `		}` |
|     212 |  9265 | `		break;` |
|     ! 0 |  9266 | `	}` |
|     315 |  9267 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     212 |  9268 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9269 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9270 | `			pToken = pGen->pIn;` |
|     ! 0 |  9271 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9272 | `				pToken--;` |
|     ! 0 |  9273 | `			}` |
|     ! 0 |  9274 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9275 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9276 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9277 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9278 | `				return SXERR_ABORT;` |
|       - |  9279 | `			}` |
|     ! 0 |  9280 | `			return SXERR_INVALID;` |
|       - |  9281 | `	}` |
|     212 |  9282 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9283 | `	/* Duplicate instance name */` |
|     212 |  9284 | `	pName = &pGen->pIn->sData;` |
|     212 |  9285 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     212 |  9286 | `	if( zDup == 0 ){` |
|     ! 0 |  9287 | `		goto Mem;` |
|       - |  9288 | `	}` |
|     212 |  9289 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     212 |  9290 | `	pGen->pIn++;` |
|     212 |  9291 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9292 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9293 | `		pToken = pGen->pIn;` |
|     ! 0 |  9294 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9295 | `			pToken--;` |
|     ! 0 |  9296 | `		}` |
|     ! 0 |  9297 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9298 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9299 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9300 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9301 | `			return SXERR_ABORT;` |
|       - |  9302 | `		}` |
|     ! 0 |  9303 | `		return SXERR_INVALID;` |
|       - |  9304 | `	}` |
|       - |  9305 | `	/* Compile the block */` |
|     212 |  9306 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9307 | `	/* Create the catch block */` |
|     212 |  9308 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     212 |  9309 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9310 | `		return SXERR_ABORT;` |
|       - |  9311 | `	}` |
|       - |  9312 | `	/* Swap bytecode container */` |
|     212 |  9313 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     212 |  9314 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9315 | `	/* Compile the block */` |
|     212 |  9316 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9317 | `	/* Fix forward jumps now the destination is resolved  */` |
|     212 |  9318 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9319 | `	/* Emit the DONE instruction */` |
|     212 |  9320 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9321 | `	/* Leave the block */` |
|     212 |  9322 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9323 | `	/* Restore the default container */` |
|     212 |  9324 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9325 | `	/* Install the catch block */` |
|     212 |  9326 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     212 |  9327 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9328 | `		goto Mem;` |
|       - |  9329 | `	}` |
|     212 |  9330 | `	return SXRET_OK;` |
|     ! 0 |  9331 | `Mem:` |
|     ! 0 |  9332 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9333 | `	return SXERR_ABORT;` |
|     109 |  9334 |  |
|       - |  9335 | `/*` |
|       - |  9336 | ` * Compile a 'try' block.` |
|       - |  9337 | ` * A function using an exception should be in a "try" block.` |
|       - |  9338 | ` * If the exception does not trigger, the code will continue` |
|       - |  9339 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9340 | ` * is "thrown".` |
|       - |  9341 | ` */` |
|     218 |  9342 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  9343 |  |
|       - |  9344 | `	ph7_exception *pException;` |
|     220 |  9345 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9346 | `	GenBlock *pTry;` |
|       - |  9347 | `	sxu32 nJmpIdx;` |
|       - |  9348 | `	sxi32 rc;` |
|       - |  9349 | `	/* Create the exception container */` |
|     220 |  9350 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     220 |  9351 | `	if( pException == 0 ){` |
|     ! 0 |  9352 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9353 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9354 | `		return SXERR_ABORT;` |
|       - |  9355 | `	}` |
|       - |  9356 | `	/* Zero the structure */` |
|     220 |  9357 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9358 | `	/* Initialize fields */` |
|     220 |  9359 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     220 |  9360 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     220 |  9361 | `	pException->iHasFinally = 0;` |
|     220 |  9362 | `	pException->iFinallyDone = 0;` |
|     220 |  9363 | `	pException->pVm = pGen->pVm;` |
|       - |  9364 | `	/* Create the try block */` |
|     220 |  9365 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     220 |  9366 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9367 | `		return SXERR_ABORT;` |
|       - |  9368 | `	}` |
|       - |  9369 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     220 |  9370 | `	pTry->pUserData = pException;` |
|       - |  9371 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     220 |  9372 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9373 | `	/* Fix the jump later when the destination is resolved */` |
|     220 |  9374 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     220 |  9375 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9376 | `	/* Compile the block */` |
|     220 |  9377 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 |  9378 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9379 | `		return SXERR_ABORT;` |
|       - |  9380 | `	}` |
|       - |  9381 | `	/* Fix forward jumps now the destination is resolved */` |
|     220 |  9382 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9383 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     220 |  9384 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9385 | `	/* Leave the block */` |
|     220 |  9386 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9387 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     220 |  9388 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     216 |  9389 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9390 | `		/* Compile one or more catch blocks */` |
|     210 |  9391 | `		for(;;){` |
|     420 |  9392 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     323 |  9393 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     105 |  9394 | `					break;` |
|       - |  9395 | `			}` |
|     216 |  9396 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     216 |  9397 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9398 | `				return SXERR_ABORT;` |
|       - |  9399 | `			}` |
|       2 |  9400 | `		}` |
|     103 |  9401 | `	}` |
|       - |  9402 | `	/* Compile optional finally block */` |
|     220 |  9403 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      94 |  9404 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9405 | `		SySet *pInstrContainer;` |
|       - |  9406 | `		GenBlock *pFinBlock;` |
|      32 |  9407 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9408 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  9409 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  9410 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9411 | `			return SXERR_ABORT;` |
|       - |  9412 | `		}` |
|       - |  9413 | `		/* Swap bytecode container */` |
|      32 |  9414 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  9415 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9416 | `		/* Compile the finally body */` |
|      32 |  9417 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  9418 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9419 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9420 | `			return SXERR_ABORT;` |
|       - |  9421 | `		}` |
|       - |  9422 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  9423 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9424 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  9425 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9426 | `		/* Leave the block */` |
|      32 |  9427 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9428 | `		/* Restore the default container */` |
|      32 |  9429 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  9430 | `		pException->iHasFinally = 1;` |
|      15 |  9431 | `	}` |
|       - |  9432 | `	/* Must have at least one catch or finally */` |
|     220 |  9433 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  9434 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9435 | `			"Cannot use try without catch or finally");` |
|       7 |  9436 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9437 | `			return SXERR_ABORT;` |
|       - |  9438 | `		}` |
|       3 |  9439 | `	}` |
|     220 |  9440 | `	return SXRET_OK;` |
|     111 |  9441 |  |
|       - |  9442 | `/*` |
|       - |  9443 | ` * Compile a switch block.` |
|       - |  9444 | ` *  (See block-comment below for more information)` |
|       - |  9445 | ` */` |
|     108 |  9446 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  9447 |  |
|     110 |  9448 | `	sxi32 rc = SXRET_OK;` |
|     110 |  9449 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9450 | `		/* Unexpected token */` |
|     ! 0 |  9451 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9452 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9453 | `			return SXERR_ABORT;` |
|       - |  9454 | `		}` |
|     ! 0 |  9455 | `		pGen->pIn++;` |
|     ! 0 |  9456 | `	}` |
|     110 |  9457 | `	pGen->pIn++;` |
|       - |  9458 | `	/* First instruction to execute in this block. */` |
|     110 |  9459 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9460 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9461 | `	 * or the '}' token */` |
|     182 |  9462 | `	for(;;){` |
|     366 |  9463 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9464 | `			/* No more input to process */` |
|     ! 0 |  9465 | `			break;` |
|       - |  9466 | `		}` |
|     366 |  9467 | `		rc = SXRET_OK;` |
|     366 |  9468 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  9469 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  9470 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9471 | `					/* Unexpected token */` |
|     ! 0 |  9472 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9473 | `						&pGen->pIn->sData);` |
|     ! 0 |  9474 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9475 | `						return SXERR_ABORT;` |
|       - |  9476 | `					}` |
|       - |  9477 | `					/* FALL THROUGH */` |
|     ! 0 |  9478 | `				}` |
|      28 |  9479 | `				rc = SXERR_EOF;` |
|      28 |  9480 | `				break;` |
|       - |  9481 | `			}` |
|      23 |  9482 | `		}else{` |
|       - |  9483 | `			sxi32 nKwrd;` |
|       - |  9484 | `			/* Extract the keyword */` |
|     298 |  9485 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  9486 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  9487 | `				break;` |
|       - |  9488 | `			}` |
|     218 |  9489 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9490 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9491 | `					/* Unexpected token */` |
|     ! 0 |  9492 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9493 | `						&pGen->pIn->sData);` |
|     ! 0 |  9494 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9495 | `						return SXERR_ABORT;` |
|       - |  9496 | `					}` |
|       - |  9497 | `					/* FALL THROUGH */` |
|     ! 0 |  9498 | `				}` |
|       - |  9499 | `				/* Block compiled */` |
|       3 |  9500 | `				break;` |
|       - |  9501 | `			}` |
|       - |  9502 | `		}` |
|       - |  9503 | `		/* Compile block */` |
|     258 |  9504 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  9505 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9506 | `			return SXERR_ABORT;` |
|       - |  9507 | `		}` |
|       2 |  9508 | `	}` |
|     110 |  9509 | `	return rc;` |
|      56 |  9510 |  |
|       - |  9511 | `/*` |
|       - |  9512 | ` * Compile a case eXpression.` |
|       - |  9513 | ` *  (See block-comment below for more information)` |
|       - |  9514 | ` */` |
|      88 |  9515 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  9516 |  |
|       - |  9517 | `	SySet *pInstrContainer;` |
|       - |  9518 | `	SyToken *pEnd,*pTmp;` |
|      90 |  9519 | `	sxi32 iNest = 0;` |
|       - |  9520 | `	sxi32 rc;` |
|       - |  9521 | `	/* Delimit the expression */` |
|      90 |  9522 | `	pEnd = pGen->pIn;` |
|     186 |  9523 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  9524 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9525 | `			/* Increment nesting level */` |
|       3 |  9526 | `			iNest++;` |
|     185 |  9527 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9528 | `			/* Decrement nesting level */` |
|       3 |  9529 | `			iNest--;` |
|     183 |  9530 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  9531 | `			break;` |
|       - |  9532 | `		}` |
|      98 |  9533 | `		pEnd++;` |
|       2 |  9534 | `	}` |
|      90 |  9535 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9536 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9537 | `		if( rc == SXERR_ABORT ){` |
|       - |  9538 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9539 | `			return SXERR_ABORT;` |
|       - |  9540 | `		}` |
|     ! 0 |  9541 | `	}` |
|       - |  9542 | `	/* Swap token stream */` |
|      90 |  9543 | `	pTmp = pGen->pEnd;` |
|      90 |  9544 | `	pGen->pEnd = pEnd;` |
|      90 |  9545 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  9546 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9547 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9548 | `	/* Emit the done instruction */` |
|      90 |  9549 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9550 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9551 | `	/* Update token stream */` |
|      90 |  9552 | `	pGen->pIn  = pEnd;` |
|      90 |  9553 | `	pGen->pEnd = pTmp;` |
|      90 |  9554 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9555 | `		return SXERR_ABORT;` |
|       - |  9556 | `	}` |
|      90 |  9557 | `	return SXRET_OK;` |
|      46 |  9558 |  |
|       - |  9559 | `/*` |
|       - |  9560 | ` * Compile the smart switch statement.` |
|       - |  9561 | ` * According to the PHP language reference manual` |
|       - |  9562 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9563 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9564 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9565 | ` *  This is exactly what the switch statement is for.` |
|       - |  9566 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9567 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9568 | ` *  of the outer loop, use continue 2.` |
|       - |  9569 | ` *  Note that switch/case does loose comparision.` |
|       - |  9570 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9571 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9572 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9573 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9574 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9575 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9576 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9577 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9578 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9579 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9580 | ` *  list for the next case.` |
|       - |  9581 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9582 | ` *  or floating-point numbers and strings.` |
|       - |  9583 | ` */` |
|      28 |  9584 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9585 |  |
|       - |  9586 | `	GenBlock *pSwitchBlock;` |
|       - |  9587 | `	SyToken *pTmp,*pEnd;` |
|       - |  9588 | `	ph7_switch *pSwitch;` |
|       - |  9589 | `	sxu32 nToken;` |
|       - |  9590 | `	sxu32 nLine;` |
|       - |  9591 | `	sxi32 rc;` |
|      30 |  9592 | `	nLine = pGen->pIn->nLine;` |
|       - |  9593 | `	/* Jump the 'switch' keyword */` |
|      30 |  9594 | `	pGen->pIn++;` |
|      30 |  9595 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9596 | `		/* Syntax error */` |
|     ! 0 |  9597 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9598 | `		if( rc == SXERR_ABORT ){` |
|       - |  9599 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9600 | `			return SXERR_ABORT;` |
|       - |  9601 | `		}` |
|     ! 0 |  9602 | `		goto Synchronize;` |
|       - |  9603 | `	}` |
|       - |  9604 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9605 | `	pGen->pIn++;` |
|      30 |  9606 | `	pEnd = 0; /* cc warning */` |
|       - |  9607 | `	/* Create the loop block */` |
|      44 |  9608 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9609 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9610 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9611 | `		return SXERR_ABORT;` |
|       - |  9612 | `	}` |
|       - |  9613 | `	/* Delimit the condition */` |
|      30 |  9614 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9615 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9616 | `		/* Empty expression */` |
|     ! 0 |  9617 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9618 | `		if( rc == SXERR_ABORT ){` |
|       - |  9619 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9620 | `			return SXERR_ABORT;` |
|       - |  9621 | `		}` |
|     ! 0 |  9622 | `	}` |
|       - |  9623 | `	/* Swap token streams */` |
|      30 |  9624 | `	pTmp = pGen->pEnd;` |
|      30 |  9625 | `	pGen->pEnd = pEnd;` |
|       - |  9626 | `	/* Compile the expression */` |
|      30 |  9627 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9628 | `	if( rc == SXERR_ABORT ){` |
|       - |  9629 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9630 | `		return SXERR_ABORT;` |
|       - |  9631 | `	}` |
|       - |  9632 | `	/* Update token stream */` |
|      30 |  9633 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9634 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9635 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9636 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9637 | `			return SXERR_ABORT;` |
|       - |  9638 | `		}` |
|     ! 0 |  9639 | `		pGen->pIn++;` |
|     ! 0 |  9640 | `	}` |
|      30 |  9641 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9642 | `	pGen->pEnd = pTmp;` |
|      30 |  9643 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9644 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9645 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9646 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9647 | `				pTmp--;` |
|     ! 0 |  9648 | `			}` |
|       - |  9649 | `			/* Unexpected token */` |
|     ! 0 |  9650 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9651 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9652 | `				return SXERR_ABORT;` |
|       - |  9653 | `			}` |
|     ! 0 |  9654 | `			goto Synchronize;` |
|       - |  9655 | `	}` |
|       - |  9656 | `	/* Set the delimiter token */` |
|      30 |  9657 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9658 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9659 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9660 | `	}else{` |
|      28 |  9661 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9662 | `	}` |
|      30 |  9663 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9664 | `	/* Create the switch blocks container */` |
|      30 |  9665 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9666 | `	if( pSwitch == 0 ){` |
|       - |  9667 | `		/* Abort compilation */` |
|     ! 0 |  9668 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9669 | `		return SXERR_ABORT;` |
|       - |  9670 | `	}` |
|       - |  9671 | `	/* Zero the structure */` |
|      30 |  9672 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9673 | `	/* Initialize fields */` |
|      30 |  9674 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9675 | `	/* Emit the switch instruction */` |
|      30 |  9676 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9677 | `	/* Compile case blocks */` |
|      96 |  9678 | `	for(;;){` |
|       - |  9679 | `		sxu32 nKwrd;` |
|     112 |  9680 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9681 | `			/* No more input to process */` |
|     ! 0 |  9682 | `			break;` |
|       - |  9683 | `		}` |
|     112 |  9684 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9685 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9686 | `				/* Unexpected token */` |
|     ! 0 |  9687 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9688 | `					&pGen->pIn->sData);` |
|     ! 0 |  9689 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9690 | `					return SXERR_ABORT;` |
|       - |  9691 | `				}` |
|       - |  9692 | `				/* FALL THROUGH */` |
|     ! 0 |  9693 | `			}` |
|       - |  9694 | `			/* Block compiled */` |
|     ! 0 |  9695 | `			break;` |
|       - |  9696 | `		}` |
|       - |  9697 | `		/* Extract the keyword */` |
|     112 |  9698 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9699 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9700 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9701 | `				/* Unexpected token */` |
|     ! 0 |  9702 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9703 | `					&pGen->pIn->sData);` |
|     ! 0 |  9704 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9705 | `					return SXERR_ABORT;` |
|       - |  9706 | `				}` |
|       - |  9707 | `				/* FALL THROUGH */` |
|     ! 0 |  9708 | `			}` |
|       - |  9709 | `			/* Block compiled */` |
|       3 |  9710 | `			break;` |
|       - |  9711 | `		}` |
|     110 |  9712 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9713 | `			/*` |
|       - |  9714 | `			 * Accroding to the PHP language reference manual` |
|       - |  9715 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9716 | `			 *  that wasn't matched by the other cases.` |
|       - |  9717 | `			 */` |
|      22 |  9718 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9719 | `				/* Default case already compiled */` |
|     ! 0 |  9720 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9721 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9722 | `					return SXERR_ABORT;` |
|       - |  9723 | `				}` |
|     ! 0 |  9724 | `			}` |
|      22 |  9725 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9726 | `			/* Compile the default block */` |
|      22 |  9727 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9728 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9729 | `				return SXERR_ABORT;` |
|      22 |  9730 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9731 | `				break;` |
|       1 |  9732 | `			}` |
|      91 |  9733 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9734 | `			ph7_case_expr sCase;` |
|       - |  9735 | `			/* Standard case block */` |
|      90 |  9736 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9737 | `			/* initialize the structure */` |
|      90 |  9738 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9739 | `			/* Compile the case expression */` |
|      90 |  9740 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9741 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9742 | `				return SXERR_ABORT;` |
|       - |  9743 | `			}` |
|       - |  9744 | `			/* Compile the case block */` |
|      90 |  9745 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9746 | `			/* Insert in the switch container */` |
|      90 |  9747 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9748 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9749 | `				return SXERR_ABORT;` |
|      90 |  9750 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9751 | `				break;` |
|       - |  9752 | `			}` |
|      42 |  9753 | `		}else{` |
|       - |  9754 | `			/* Unexpected token */` |
|     ! 0 |  9755 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9756 | `				&pGen->pIn->sData);` |
|     ! 0 |  9757 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9758 | `				return SXERR_ABORT;` |
|       - |  9759 | `			}` |
|     ! 0 |  9760 | `			break;` |
|       - |  9761 | `		}` |
|       2 |  9762 | `	}` |
|       - |  9763 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9764 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9765 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9766 | `	/* Release the loop block */` |
|      30 |  9767 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9768 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9769 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9770 | `		pGen->pIn++;` |
|      14 |  9771 | `	}` |
|       - |  9772 | `	/* Statement successfully compiled */` |
|      30 |  9773 | `	return SXRET_OK;` |
|     ! 0 |  9774 | `Synchronize:` |
|       - |  9775 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9776 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9777 | `		pGen->pIn++;` |
|     ! 0 |  9778 | `	}` |
|     ! 0 |  9779 | `	return SXRET_OK;` |
|      16 |  9780 |  |
|       - |  9781 | `/*` |
|       - |  9782 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9783 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9784 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9785 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9786 | ` */` |
|       - |  9787 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9788 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9789 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9790 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9791 |  |
|       - |  9792 | `/*` |
|       - |  9793 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9794 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9795 | ` * patched entries from the pending set.` |
|       - |  9796 | ` */` |
| 2133938 |  9797 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       2 |  9798 |  |
| 2133940 |  9799 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9800 | `	sxu32 nTarget;` |
|       - |  9801 | `	sxu32 *aIdx;` |
|       - |  9802 | `	sxu32 i;` |
| 2133940 |  9803 | `	if( nCur <= nBaseline ){` |
| 2133850 |  9804 | `		return;` |
|       - |  9805 | `	}` |
|      92 |  9806 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      92 |  9807 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     190 |  9808 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     100 |  9809 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     100 |  9810 | `		if( pInstr ){` |
|     100 |  9811 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9812 | `		}` |
|      51 |  9813 | `	}` |
|      92 |  9814 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1066971 |  9815 |  |
|       - |  9816 |  |
|       - |  9817 | `/*` |
|       - |  9818 | ` * Generate bytecode for a given expression tree.` |
|       - |  9819 | ` * If something goes wrong while generating bytecode` |
|       - |  9820 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9821 | ` * this function takes care of generating the appropriate` |
|       - |  9822 | ` * error message.` |
|       - |  9823 | ` */` |
| 2875588 |  9824 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9825 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9826 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9827 | `	sxi32 iFlags /* Control flags */` |
|       - |  9828 | `	)` |
|       2 |  9829 |  |
|       - |  9830 | `	VmInstr *pInstr;` |
|       - |  9831 | `	sxu32 nJmpIdx;` |
| 2875590 |  9832 | `	sxi32 iP1 = 0;` |
| 2875590 |  9833 | `	sxu32 iP2 = 0;` |
| 2875590 |  9834 | `	void *p3  = 0;` |
|       - |  9835 | `	sxi32 iVmOp;` |
|       - |  9836 | `	sxi32 rc;` |
| 2875590 |  9837 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 2875590 |  9838 | `	sxu32 nRhsNsBase = 0;` |
| 2875590 |  9839 | `	if( pNode->xCode ){` |
|       - |  9840 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9841 | `		/* Compile node */` |
| 1780904 |  9842 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1780904 |  9843 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1780904 |  9844 | `		RE_SWAP_DELIMITER(pGen);` |
| 1780904 |  9845 | `		return rc;` |
|       - |  9846 | `	}` |
| 1094688 |  9847 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9848 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9849 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9850 | `		return SXERR_ABORT;` |
|       - |  9851 | `	}` |
| 1094688 |  9852 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1094688 |  9853 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9854 | `		sxu32 nJmp = 0;` |
|       - |  9855 | `		sxu32 nNcNsBase;` |
|       - |  9856 | `		VmInstr *pInstrFix;` |
|       - |  9857 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9858 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9859 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9860 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9861 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9862 | `		if( pNode->pRight ){` |
|      47 |  9863 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9864 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9865 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9866 | `				return rc;` |
|       - |  9867 | `			}` |
|      47 |  9868 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - |  9869 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9870 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9871 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9872 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9873 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9874 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9875 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9876 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9877 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9878 | `				pInstrFix->iP2 = 3;` |
|       9 |  9879 | `			}` |
|      23 |  9880 | `		}` |
|       - |  9881 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9882 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9883 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9884 | `		if( pNode->pLeft ){` |
|      47 |  9885 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9886 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9887 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9888 | `				return rc;` |
|       - |  9889 | `			}` |
|      47 |  9890 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      23 |  9891 | `		}` |
|       - |  9892 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9893 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9894 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9895 | `		if( nJmp > 0 ){` |
|      47 |  9896 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9897 | `			if( pInstrFix ){` |
|      47 |  9898 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9899 | `			}` |
|      23 |  9900 | `		}` |
|      47 |  9901 | `		return SXRET_OK;` |
|       - |  9902 | `	}` |
| 1094642 |  9903 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9904 | `		sxu32 nJz,nJmp;` |
|       - |  9905 | `		sxu32 nTernaryNsBase;` |
|       - |  9906 | `		/* Ternary operator require special handling */` |
|       - |  9907 | `		/* Phase#1: Compile the condition */` |
|    2052 |  9908 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2052 |  9909 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2052 |  9910 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9911 | `			return rc;` |
|       - |  9912 | `		}` |
|       - |  9913 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - |  9914 | `		 * compiling the condition must short-circuit to the end of the` |
|       - |  9915 | `		 * condition expression, not leak past the ternary. */` |
|    2052 |  9916 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2052 |  9917 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2052 |  9918 | `		if( pNode->pLeft ){` |
|       - |  9919 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9920 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1984 |  9921 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9922 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1984 |  9923 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    1984 |  9924 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1984 |  9925 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9926 | `				return rc;` |
|       - |  9927 | `			}` |
|    1984 |  9928 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     993 |  9929 | `		}else{` |
|       - |  9930 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9931 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9932 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9933 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9934 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9935 | `		}` |
|       - |  9936 | `		/* Phase#4: Emit the unconditional jump */` |
|    2052 |  9937 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9938 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2052 |  9939 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2052 |  9940 | `		if( pInstr ){` |
|    2052 |  9941 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1025 |  9942 | `		}` |
|    2052 |  9943 | `		if( !pNode->pLeft ){` |
|       - |  9944 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9945 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9946 | `		}` |
|       - |  9947 | `		/* Phase#6: Compile the 'else' expression */` |
|    2052 |  9948 | `		if( pNode->pRight ){` |
|    2052 |  9949 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2052 |  9950 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2052 |  9951 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9952 | `				return rc;` |
|       - |  9953 | `			}` |
|    2052 |  9954 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1025 |  9955 | `		}` |
|    2052 |  9956 | `		if( nJmp > 0 ){` |
|       - |  9957 | `			/* Phase#7: Fix the unconditional jump */` |
|    2052 |  9958 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2052 |  9959 | `			if( pInstr ){` |
|    2052 |  9960 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1025 |  9961 | `			}` |
|    1025 |  9962 | `		}` |
|       - |  9963 | `		/* All done */` |
|    2052 |  9964 | `		return SXRET_OK;` |
|       - |  9965 | `	}` |
| 1092592 |  9966 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - |  9967 | `	/* Generate code for the left tree */` |
| 1092592 |  9968 | `	if( pNode->pLeft ){` |
| 1092554 |  9969 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1092554 |  9970 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - |  9971 | `			ph7_expr_node **apNode;` |
|  346920 |  9972 | `			int hasSpread = 0;` |
|  346920 |  9973 | `			int hasNamed = 0;` |
|       - |  9974 | `			sxi32 nArgs;` |
|       - |  9975 | `			sxi32 n;` |
|       - |  9976 | `			/* Recurse and generate bytecodes for function arguments */` |
|  346920 |  9977 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  346920 |  9978 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - |  9979 | `			/* Validate: no positional arguments after named arguments */` |
|       - |  9980 | `			{` |
|  346920 |  9981 | `				int seenNamed = 0;` |
|  686876 |  9982 | `				for( n = 0; n < nArgs; ++n ){` |
|  339960 |  9983 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     176 |  9984 | `						seenNamed = 1;` |
|     176 |  9985 | `						hasNamed = 1;` |
|  339873 |  9986 | `					}else if( seenNamed && !(apNode[n]->iFlags & EXPR_NODE_SPREAD) ){` |
|       3 |  9987 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - |  9988 | `							"Cannot use positional argument after named argument");` |
|       3 |  9989 | `						return SXERR_SYNTAX;` |
|       - |  9990 | `					}` |
|  169980 |  9991 | `				}` |
|       - |  9992 | `			}` |
|       - |  9993 | `			/* Read-only load */` |
|  346918 |  9994 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  686872 |  9995 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  339956 |  9996 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  339956 |  9997 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  339956 |  9998 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9999 | `					return rc;` |
|       - | 10000 | `				}` |
|       - | 10001 | `				/* Each argument is an independent nullsafe scope. */` |
|  339956 | 10002 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  339956 | 10003 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10004 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 | 10005 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 | 10006 | `					hasSpread = 1;` |
|       9 | 10007 | `				}` |
|  169979 | 10008 | `			}` |
|       - | 10009 | `			/* Total number of given arguments */` |
|  346918 | 10010 | `			iP1 = nArgs;` |
|  346918 | 10011 | `			iP2 = hasSpread;` |
|       - | 10012 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10013 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  346918 | 10014 | `			if( hasNamed ){` |
|      94 | 10015 | `				sxu32 nStrBytes = 0;` |
|       - | 10016 | `				char *zBuf;` |
|     278 | 10017 | `				for( n = 0; n < nArgs; ++n ){` |
|     186 | 10018 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 | 10019 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      86 | 10020 | `					}` |
|      94 | 10021 | `				}` |
|       - | 10022 | `				{` |
|      94 | 10023 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      94 | 10024 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      92 | 10025 | `					&pGen->pVm->sAllocator, mapSize);` |
|      94 | 10026 | `				if( pMap ){` |
|      94 | 10027 | `					SyZero(pMap, mapSize);` |
|      94 | 10028 | `					pMap->bHasNamed = 1;` |
|      94 | 10029 | `					pMap->nTotal = (sxu32)nArgs;` |
|      94 | 10030 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      94 | 10031 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     278 | 10032 | `					for( n = 0; n < nArgs; ++n ){` |
|     186 | 10033 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 | 10034 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     174 | 10035 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     174 | 10036 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     174 | 10037 | `							zBuf += nb;` |
|      86 | 10038 | `						}` |
|       - | 10039 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      94 | 10040 | `					}` |
|      94 | 10041 | `					p3 = (void *)pMap;` |
|      46 | 10042 | `				}` |
|       - | 10043 | `				}` |
|      46 | 10044 | `			}` |
|       - | 10045 | `			/* Remove stale flags now */` |
|  346918 | 10046 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  173458 | 10047 | `		}` |
| 1092552 | 10048 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1092552 | 10049 | `		if( rc != SXRET_OK ){` |
|      31 | 10050 | `			return rc;` |
|       - | 10051 | `		}` |
| 1092522 | 10052 | `		if( !bIsChainOp ){` |
|       - | 10053 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10054 | `			 * target the end of that LHS chain, which is right here. */` |
|  510730 | 10055 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  255364 | 10056 | `		}` |
| 1092522 | 10057 | `		if( iVmOp == PH7_OP_CALL ){` |
|  346918 | 10058 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  346918 | 10059 | `			if( pInstr ){` |
|  346918 | 10060 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  346016 | 10061 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10062 | `					sxu32 nQual;` |
|  346016 | 10063 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10064 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10065 | `					 * so the later NEW handler (if any) can see it. */` |
|  346016 | 10066 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10067 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10068 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10069 | `					 * imports — class imports must NOT affect function` |
|       - | 10070 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10071 | `					 * before NEW; we store the original literal index in the` |
|       - | 10072 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10073 | `					 * the unqualified name and re-qualify with class imports. */` |
|  346016 | 10074 | `					if( bAbsolute ){` |
|      20 | 10075 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      11 | 10076 | `					}else{` |
|  345998 | 10077 | `						int fromImport = 0;` |
|  345998 | 10078 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  345998 | 10079 | `						pInstr->iP2 = (sxi32)nQual;` |
|  345998 | 10080 | `						if( nQual != nOrig ){` |
|       - | 10081 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10082 | `							 * NEW handler can recover the unqualified name. */` |
|      74 | 10083 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      74 | 10084 | `							if( !fromImport ){` |
|       - | 10085 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      64 | 10086 | `								if( p3 == 0 ){` |
|      64 | 10087 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10088 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      64 | 10089 | `									if( pMap ){` |
|      64 | 10090 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      64 | 10091 | `										p3 = (void *)pMap;` |
|      31 | 10092 | `									}` |
|      31 | 10093 | `								}` |
|      64 | 10094 | `								if( p3 ){` |
|      64 | 10095 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10096 | `								}` |
|      31 | 10097 | `							}` |
|      36 | 10098 | `						}` |
|       2 | 10099 | `					}` |
|  173911 | 10100 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10101 | `					/* Method call,flag that */` |
|     748 | 10102 | `					pInstr->iP2 = 1;` |
|     373 | 10103 | `				}` |
|  173460 | 10104 | `			}` |
|  919064 | 10105 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10106 | `			ph7_expr_node **apNode;` |
|       - | 10107 | `			sxi32 n;` |
|       - | 10108 | `			/* Recurse and generate bytecodes for array index */` |
|   75046 | 10109 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  135390 | 10110 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   60346 | 10111 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   60346 | 10112 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   60346 | 10113 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10114 | `					return rc;` |
|       - | 10115 | `				}` |
|       - | 10116 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   60346 | 10117 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   30174 | 10118 | `			}` |
|   75046 | 10119 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   60346 | 10120 | `				iP1 = 1; /* Node have an index associated with it */` |
|   30172 | 10121 | `			}` |
|   75046 | 10122 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10123 | `				/* Create an empty entry when the desired index is not found */` |
|   29660 | 10124 | `				iP2 = 1;` |
|   14831 | 10125 | `			}` |
|  708084 | 10126 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10127 | `			/* POP the left node */` |
|      32 | 10128 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10129 | `		}` |
|  546260 | 10130 | `	}` |
| 1092560 | 10131 | `	rc = SXRET_OK;` |
| 1092560 | 10132 | `	nJmpIdx = 0;` |
|       - | 10133 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10134 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10135 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1092560 | 10136 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     270 | 10137 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     270 | 10138 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     270 | 10139 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     270 | 10140 | `			int isSpecial = 0;` |
|     270 | 10141 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     182 | 10142 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     182 | 10143 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     195 | 10144 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     160 | 10145 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      83 | 10146 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      90 | 10147 | `					isSpecial = 1;` |
|      44 | 10148 | `				}` |
|     112 | 10149 | `			}` |
|     314 | 10150 | `			pInstr->iP1 = 0;` |
|     314 | 10151 | `			if( !isSpecial ){` |
|     138 | 10152 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10153 | `			}` |
|       - | 10154 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10155 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     226 | 10156 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     138 | 10157 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     138 | 10158 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10159 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 10160 | `					return SXRET_OK;` |
|       - | 10161 | `				}` |
|      47 | 10162 | `			}` |
|      91 | 10163 | `		}` |
|     167 | 10164 | `	}` |
|       - | 10165 | `	/* Generate code for the right tree */` |
| 1092482 | 10166 | `	if( pNode->pRight ){` |
|  603788 | 10167 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10168 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9200 | 10169 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  599189 | 10170 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10171 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3076 | 10172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  593053 | 10173 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10174 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      84 | 10175 | `			iVmOp = 0; /* No binary operator to emit */` |
|      84 | 10176 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  591524 | 10177 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10178 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10179 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10180 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10181 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10182 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10183 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     100 | 10184 | `			sxu32 nNsJmp = 0;` |
|     100 | 10185 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     100 | 10186 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  591385 | 10187 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  245244 | 10188 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  122621 | 10189 | `		}` |
|  603788 | 10190 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  603788 | 10191 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  603788 | 10192 | `		if( !bIsChainOp ){` |
|       - | 10193 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10194 | `			 * operator instruction is emitted. */` |
|  443998 | 10195 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  221998 | 10196 | `		}` |
|  603788 | 10197 | `		if( iVmOp == PH7_OP_STORE ){` |
|  242134 | 10198 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  242108 | 10199 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10200 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10201 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10202 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10203 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10204 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10205 | `				 */` |
|      54 | 10206 | `				iVmOp = 0;` |
|  242108 | 10207 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  242082 | 10208 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10209 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   67596 | 10210 | `					iP2 = 1;` |
|   33799 | 10211 | `				}else{` |
|  174488 | 10212 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10213 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   29598 | 10214 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   29598 | 10215 | `						iP1 = pInstr->iP1;` |
|   14800 | 10216 | `					}else{` |
|  144892 | 10217 | `						p3 = pInstr->p3;` |
|       - | 10218 | `					}` |
|       - | 10219 | `					/* POP the last dynamic load instruction */` |
|  174488 | 10220 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10221 | `				}` |
|  121042 | 10222 | `			}` |
|  482722 | 10223 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 10224 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 10225 | `			if( pInstr ){` |
|      48 | 10226 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10227 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10228 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10229 | `					 */` |
|      15 | 10230 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10231 | `					iP1 = pInstr->iP1;` |
|      15 | 10232 | `					iP2 = pInstr->iP2;` |
|      15 | 10233 | `					p3  = pInstr->p3;` |
|       8 | 10234 | `				}else{` |
|      34 | 10235 | `					p3 = pInstr->p3;` |
|       - | 10236 | `				}` |
|      23 | 10237 | `			}` |
|      23 | 10238 | `		}` |
|  301893 | 10239 | `	}` |
| 1092482 | 10240 | `	if( iVmOp > 0 ){` |
| 1092318 | 10241 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11966 | 10242 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10243 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8788 | 10244 | `				iP1 = 1;` |
|    4395 | 10245 | `			}` |
| 1086336 | 10246 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10247 | `			/* Namespace-qualify the class name for NEW */ {` |
|   15432 | 10248 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   15432 | 10249 | `				VmInstr *pCallInstr = 0;` |
|   15432 | 10250 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   15416 | 10251 | `					pCallInstr = pPeek;` |
|   15416 | 10252 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7707 | 10253 | `				}` |
|   15432 | 10254 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   15430 | 10255 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10256 | `					sxu32 nLitForClass;` |
|       - | 10257 | `					/* If the CALL handler already qualified the name using` |
|       - | 10258 | `					 * function imports, recover the original unqualified` |
|       - | 10259 | `					 * literal so we can re-qualify with class imports. */` |
|   15430 | 10260 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 | 10261 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 | 10262 | `					}else{` |
|   15398 | 10263 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10264 | `					}` |
|   15430 | 10265 | `					pPeek->iP1 = 0;` |
|   15430 | 10266 | `					if( !bAbsolute ){` |
|   15414 | 10267 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7708 | 10268 | `					}else{` |
|      18 | 10269 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10270 | `					}` |
|    7714 | 10271 | `				}` |
|       - | 10272 | `			}` |
|   15432 | 10273 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   15432 | 10274 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10275 | `				VmInstr *pPrev;` |
|   15416 | 10276 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   15416 | 10277 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10278 | `					/* Pop the call instruction, preserve named-arg map */` |
|   15416 | 10279 | `					iP1 = pInstr->iP1;` |
|   15416 | 10280 | `					if( pInstr->p3 ){` |
|      38 | 10281 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      18 | 10282 | `					}` |
|   15416 | 10283 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7707 | 10284 | `				}` |
|    7709 | 10285 | `			}` |
| 1072639 | 10286 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10287 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10288 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|      88 | 10289 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      88 | 10290 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      88 | 10291 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      88 | 10292 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|      88 | 10293 | `				int isSpecialIs = 0;` |
|      88 | 10294 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      84 | 10295 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      84 | 10296 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      87 | 10297 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      79 | 10298 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      42 | 10299 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 10300 | `						isSpecialIs = 1;` |
|       5 | 10301 | `					}` |
|      42 | 10302 | `				}` |
|      90 | 10303 | `				pInstr->iP1 = 0;` |
|      90 | 10304 | `				if( !isSpecialIs && !bAbsolute ){` |
|      68 | 10305 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      33 | 10306 | `				}` |
|      44 | 10307 | `			}` |
| 1064884 | 10308 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10309 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10310 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10311 | `			 * should not trigger constant lookup. */` |
|  159792 | 10312 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  159792 | 10313 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  159754 | 10314 | `				pInstr->iP1 = 0;` |
|   79876 | 10315 | `			}` |
|  159792 | 10316 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10317 | `				/* Static member access,remember that */` |
|     192 | 10318 | `				iP1 = 1;` |
|     192 | 10319 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     192 | 10320 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      32 | 10321 | `					p3 = pInstr->p3;` |
|      32 | 10322 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      15 | 10323 | `				}` |
|      95 | 10324 | `			}` |
|   79895 | 10325 | `		}` |
|       - | 10326 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1092316 | 10327 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  546157 | 10328 | `	}` |
| 1092480 | 10329 | `	if( nJmpIdx > 0 ){` |
|       - | 10330 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   12356 | 10331 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   12356 | 10332 | `		if( pInstr ){` |
|   12356 | 10333 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6177 | 10334 | `		}` |
|    6177 | 10335 | `	}` |
| 1092480 | 10336 | `	return rc;` |
| 1437777 | 10337 |  |
|       - | 10338 | `/*` |
|       - | 10339 | ` * Compile a PHP expression.` |
|       - | 10340 | ` * According to the PHP language reference manual:` |
|       - | 10341 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10342 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10343 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10344 | ` *  is "anything that has a value".` |
|       - | 10345 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10346 | ` * function takes care of generating the appropriate error` |
|       - | 10347 | ` * message.` |
|       - | 10348 | ` */` |
|  772934 | 10349 | `static sxi32 PH7_CompileExpr(` |
|       - | 10350 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10351 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10352 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10353 | `	)` |
|       2 | 10354 |  |
|       - | 10355 | `	ph7_expr_node *pRoot;` |
|       - | 10356 | `	SySet sExprNode;` |
|       - | 10357 | `	SyToken *pEnd;` |
|       - | 10358 | `	sxi32 nExpr;` |
|       - | 10359 | `	sxi32 iNest;` |
|       - | 10360 | `	sxi32 rc;` |
|       - | 10361 | `	sxu32 nNullsafeBase;` |
|       - | 10362 | `	/* Initialize worker variables */` |
|  772936 | 10363 | `	nExpr = 0;` |
|  772936 | 10364 | `	pRoot = 0;` |
|       - | 10365 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10366 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  772936 | 10367 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  772936 | 10368 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  772936 | 10369 | `	SySetAlloc(&sExprNode,0x10);` |
|  772936 | 10370 | `	rc = SXRET_OK;` |
|       - | 10371 | `	/* Delimit the expression */` |
|  772936 | 10372 | `	pEnd = pGen->pIn;` |
|  772936 | 10373 | `	iNest = 0;` |
| 5173058 | 10374 | `	while( pEnd < pGen->pEnd ){` |
| 4910228 | 10375 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10376 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     330 | 10377 | `			iNest++;` |
| 4910064 | 10378 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     338 | 10379 | `			iNest--;` |
| 4909732 | 10380 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  510320 | 10381 | `			if( iNest <= 0 ){` |
|  510106 | 10382 | `				break;` |
|       - | 10383 | `			}` |
|     107 | 10384 | `		}` |
| 4400124 | 10385 | `		pEnd++;` |
|       2 | 10386 | `	}` |
|  772936 | 10387 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   17916 | 10388 | `		SyToken *pEnd2 = pGen->pIn;` |
|   17916 | 10389 | `		iNest = 0;` |
|       - | 10390 | `		/* Stop at the first comma */` |
|   35880 | 10391 | `		while( pEnd2 < pEnd ){` |
|   17970 | 10392 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      16 | 10393 | `				iNest++;` |
|   17963 | 10394 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      16 | 10395 | `				iNest--;` |
|   17949 | 10396 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      13 | 10397 | `				if( iNest <= 0 ){` |
|       5 | 10398 | `					break;` |
|       - | 10399 | `				}` |
|       4 | 10400 | `			}` |
|   17966 | 10401 | `			pEnd2++;` |
|       2 | 10402 | `		}` |
|   17916 | 10403 | `		if( pEnd2 <pEnd ){` |
|       5 | 10404 | `			pEnd = pEnd2;` |
|       2 | 10405 | `		}` |
|    8957 | 10406 | `	}` |
|  772936 | 10407 | `	if( pEnd > pGen->pIn ){` |
|  772926 | 10408 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10409 | `		/* Swap delimiter */` |
|  772926 | 10410 | `		pGen->pEnd = pEnd;` |
|       - | 10411 | `		/* Try to get an expression tree */` |
|  772926 | 10412 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  772926 | 10413 | `		if( rc == SXRET_OK && pRoot ){` |
|  772744 | 10414 | `			rc = SXRET_OK;` |
|  772744 | 10415 | `			if( xTreeValidator ){` |
|       - | 10416 | `				/* Call the upper layer validator callback */` |
|   21780 | 10417 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   10889 | 10418 | `			}` |
|  772744 | 10419 | `			if( rc != SXERR_ABORT ){` |
|       - | 10420 | `				/* Generate code for the given tree */` |
|  772744 | 10421 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10422 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10423 | `				 * expression so they short-circuit to its end. */` |
|  772744 | 10424 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  386371 | 10425 | `			}` |
|  772744 | 10426 | `			nExpr = 1;` |
|  386371 | 10427 | `		}` |
|       - | 10428 | `		/* Release the whole tree */` |
|  772926 | 10429 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10430 | `		/* Synchronize token stream */` |
|  772926 | 10431 | `		pGen->pEnd = pTmp;` |
|  772926 | 10432 | `		pGen->pIn  = pEnd;` |
|  772926 | 10433 | `		if( rc == SXERR_ABORT ){` |
|      11 | 10434 | `			SySetRelease(&sExprNode);` |
|      11 | 10435 | `			return SXERR_ABORT;` |
|       - | 10436 | `		}` |
|  386457 | 10437 | `	}` |
|  772926 | 10438 | `	SySetRelease(&sExprNode);` |
|  772926 | 10439 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  386469 | 10440 |  |
|       - | 10441 | `/*` |
|       - | 10442 | ` * Return a pointer to the node construct handler associated` |
|       - | 10443 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10444 | ` */` |
|  194412 | 10445 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 10446 |  |
|  194414 | 10447 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10448 | `		/* Numeric literal: Either real or integer */` |
|  102932 | 10449 | `		return PH7_CompileNumLiteral;` |
|   91484 | 10450 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10451 | `		/* Double quoted string */` |
|   17130 | 10452 | `		return PH7_CompileString;` |
|   74356 | 10453 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10454 | `		/* Single quoted string */` |
|   74244 | 10455 | `		return PH7_CompileSimpleString;` |
|     114 | 10456 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10457 | `		/* Heredoc */` |
|      66 | 10458 | `		return PH7_CompileHereDoc;` |
|      50 | 10459 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10460 | `		/* Nowdoc */` |
|      44 | 10461 | `		return PH7_CompileNowDoc;` |
|       7 | 10462 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10463 | `		/* Backtick quoted string */` |
|       5 | 10464 | `		return PH7_CompileBacktic;` |
|       - | 10465 | `	}` |
|       3 | 10466 | `	return 0;` |
|   97208 | 10467 |  |
|       - | 10468 | `/*` |
|       - | 10469 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10470 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10471 | ` * in write context" parse error.` |
|       - | 10472 | ` */` |
|    6454 | 10473 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       2 | 10474 |  |
|       - | 10475 | `	sxi32 rc;` |
|    6456 | 10476 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6454 | 10477 | `		return SXRET_OK;` |
|       - | 10478 | `	}` |
|       5 | 10479 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10480 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10481 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10482 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3229 | 10483 |  |
|       - | 10484 | `/*` |
|       - | 10485 | ` * Compile an unset() statement.` |
|       - | 10486 | ` * unset($var, $arr[$key], ...);` |
|       - | 10487 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10488 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10489 | ` * parent array before extracting the element to unset.` |
|       - | 10490 | ` */` |
|    2798 | 10491 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 10492 |  |
|    2800 | 10493 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2800 | 10494 | `	sxu32 nIdx = 0;` |
|       - | 10495 | `	SyString sName;` |
|       - | 10496 | `	sxi32 rc;` |
|       - | 10497 | `	/* Jump the 'unset' keyword */` |
|    2800 | 10498 | `	pGen->pIn++;` |
|       - | 10499 | `	/* Save delimiter */` |
|    2800 | 10500 | `	pTmp = pGen->pEnd;` |
|       - | 10501 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2800 | 10502 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2800 | 10503 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10504 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10505 | `		SyToken *pClose;` |
|    2800 | 10506 | `		pGen->pIn++;   /* Skip '(' */` |
|    2800 | 10507 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2800 | 10508 | `		pEnd = pClose; /* Stop at ')' */` |
|    1399 | 10509 | `	}` |
|    2800 | 10510 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10511 | `	/* Resolve the 'unset' builtin name once */` |
|    2800 | 10512 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     336 | 10513 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     336 | 10514 | `		if( pObj == 0 ){` |
|     ! 0 | 10515 | `			return SXERR_ABORT;` |
|       - | 10516 | `		}` |
|     336 | 10517 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     336 | 10518 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     167 | 10519 | `	}` |
|       - | 10520 | `	/* Compile each comma-separated argument */` |
|    9256 | 10521 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6458 | 10522 | `		if( pGen->pIn < pNext ){` |
|    6458 | 10523 | `			pGen->pEnd = pNext;` |
|    6458 | 10524 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10525 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,` |
|       - | 10526 | `				GenStateUnsetValidator);` |
|    6458 | 10527 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10528 | `				return SXERR_ABORT;` |
|       - | 10529 | `			}` |
|    6458 | 10530 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10531 | `				/* Emit call for this single argument */` |
|    6456 | 10532 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6456 | 10533 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6456 | 10534 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3227 | 10535 | `			}` |
|    3228 | 10536 | `		}` |
|       - | 10537 | `		/* Jump trailing commas */` |
|   10118 | 10538 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3662 | 10539 | `			pNext++;` |
|       2 | 10540 | `		}` |
|    6458 | 10541 | `		pGen->pIn = pNext;` |
|       2 | 10542 | `	}` |
|       - | 10543 | `	/* Skip past the closing ')' if present */` |
|    2800 | 10544 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2800 | 10545 | `		pGen->pIn++;` |
|    1399 | 10546 | `	}` |
|       - | 10547 | `	/* Restore token stream */` |
|    2800 | 10548 | `	pGen->pEnd = pTmp;` |
|    2800 | 10549 | `	return SXRET_OK;` |
|    1401 | 10550 |  |
|       - | 10551 | `/*` |
|       - | 10552 | ` * PHP Language construct table.` |
|       - | 10553 | ` */` |
|       - | 10554 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10555 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10556 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10557 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10558 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10559 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10560 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10561 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10562 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10563 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10564 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10565 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10566 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10567 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10568 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10569 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10570 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10571 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10572 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10573 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10574 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10575 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10576 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10577 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10578 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10579 | `};` |
|       - | 10580 | `/*` |
|       - | 10581 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10582 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10583 | ` */` |
|  463432 | 10584 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10585 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10586 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10587 | `	)` |
|       2 | 10588 |  |
|  463434 | 10589 | `	sxu32 n = 0;` |
| 1963864 | 10590 | `	for(;;){` |
| 3927730 | 10591 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   53662 | 10592 | `			break;` |
|       - | 10593 | `		}` |
| 3874070 | 10594 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  409774 | 10595 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10596 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10597 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10598 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10599 | `					return 0;` |
|       - | 10600 | `				}` |
|     ! 0 | 10601 | `			}` |
|  409772 | 10602 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 10603 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 10604 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10605 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10606 | `				return 0;` |
|       - | 10607 | `			}` |
|       - | 10608 | `			/* Return a pointer to the handler.` |
|       - | 10609 | `			*/` |
|  409774 | 10610 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10611 | `		}` |
| 3464298 | 10612 | `		n++;` |
|       2 | 10613 | `	}` |
|   53662 | 10614 | `	if( pLookahed ){` |
|   53662 | 10615 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   11748 | 10616 | `			return PH7_CompileClassInterface;` |
|   41916 | 10617 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   41704 | 10618 | `			return PH7_CompileClass;` |
|     214 | 10619 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 | 10620 | `			return PH7_CompileTrait;` |
|     158 | 10621 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      21 | 10622 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      20 | 10623 | `				return PH7_CompileAbstractClass;` |
|     140 | 10624 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 10625 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10626 | `				return PH7_CompileFinalClass;` |
|       - | 10627 | `		}` |
|      69 | 10628 | `	}` |
|       - | 10629 | `	/* Not a language construct */` |
|     140 | 10630 | `	return 0;` |
|  231718 | 10631 |  |
|       - | 10632 | `/*` |
|       - | 10633 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10634 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10635 | ` */` |
|     138 | 10636 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 10637 |  |
|       - | 10638 | `	int rc;` |
|     140 | 10639 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     140 | 10640 | `	if( rc == FALSE ){` |
|      44 | 10641 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 | 10642 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10643 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10644 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10645 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10646 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10647 | `			*/` |
|       - | 10648 | `			){` |
|      38 | 10649 | `				rc = TRUE;` |
|      18 | 10650 | `		}` |
|      22 | 10651 | `	}` |
|     140 | 10652 | `	return rc;` |
|       2 | 10653 |  |
|       - | 10654 | `/*` |
|       - | 10655 | ` * Compile a PHP chunk.` |
|       - | 10656 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10657 | ` * takes care of generating the appropriate error message.` |
|       - | 10658 | ` */` |
|  625706 | 10659 | `static sxi32 GenStateCompileChunk(` |
|       - | 10660 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10661 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10662 | `	)` |
|       2 | 10663 |  |
|       - | 10664 | `	ProcLangConstruct xCons;` |
|       - | 10665 | `	sxi32 rc;` |
|  625708 | 10666 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  428783 | 10667 | `	for(;;){` |
|  741638 | 10668 | `		int bStmtIsDeclare = 0;` |
|  741638 | 10669 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10670 | `			/* No more input to process */` |
|   12436 | 10671 | `			break;` |
|       - | 10672 | `		}` |
|       - | 10673 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 10674 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  729204 | 10675 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  463434 | 10676 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  463434 | 10677 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      26 | 10678 | `				bStmtIsDeclare = 1;` |
|      12 | 10679 | `			}` |
|  231716 | 10680 | `		}` |
|  729204 | 10681 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 10682 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 10683 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  115918 | 10684 | `			pGen->bStrictTypesLocked = 1;` |
|   57958 | 10685 | `		}` |
|  729204 | 10686 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10687 | `			/* Compile block */` |
|      18 | 10688 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      18 | 10689 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10690 | `				break;` |
|       - | 10691 | `			}` |
|      10 | 10692 | `		}else{` |
|  729188 | 10693 | `			xCons = 0;` |
|  729188 | 10694 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  463434 | 10695 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10696 | `				/* Try to extract a language construct handler */` |
|  463434 | 10697 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  463434 | 10698 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10699 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10700 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10701 | `						&pGen->pIn->sData);` |
|       9 | 10702 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10703 | `						break;` |
|       - | 10704 | `					}` |
|       - | 10705 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10706 | `					 * this erroneous statement.` |
|       - | 10707 | `					 */` |
|       9 | 10708 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10709 | `				}` |
|  497472 | 10710 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   43462 | 10711 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10712 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10713 | `				xCons = PH7_CompileLabel;` |
|      56 | 10714 | `			}` |
|  729188 | 10715 | `			if( xCons == 0 ){` |
|       - | 10716 | `				/* Assume an expression an try to compile it */` |
|  265774 | 10717 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  265774 | 10718 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10719 | `					/* Pop l-value */` |
|  265624 | 10720 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  132811 | 10721 | `				}` |
|  132888 | 10722 | `			}else{` |
|       - | 10723 | `				/* Go compile the sucker */` |
|  463416 | 10724 | `				rc = xCons(&(*pGen));` |
|       - | 10725 | `			}` |
|  729188 | 10726 | `			if( rc == SXERR_ABORT ){` |
|       - | 10727 | `				/* Request to abort compilation */` |
|      11 | 10728 | `				break;` |
|       - | 10729 | `			}` |
|       - | 10730 | `		}` |
|       - | 10731 | `		/* Ignore trailing semi-colons ';' */` |
| 1215910 | 10732 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  486718 | 10733 | `			pGen->pIn++;` |
|       2 | 10734 | `		}` |
|  729194 | 10735 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10736 | `			/* Compile a single statement and return */` |
|  613264 | 10737 | `			break;` |
|       - | 10738 | `		}` |
|       - | 10739 | `		/* LOOP ONE */` |
|       - | 10740 | `		/* LOOP TWO */` |
|       - | 10741 | `		/* LOOP THREE */` |
|       - | 10742 | `		/* LOOP FOUR */` |
|       2 | 10743 | `	}` |
|       - | 10744 | `	/* Return compilation status */` |
|  625708 | 10745 | `	return rc;` |
|       2 | 10746 |  |
|       - | 10747 | `/*` |
|       - | 10748 | ` * Compile a Raw PHP chunk.` |
|       - | 10749 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10750 | ` * takes care of generating the appropriate error message.` |
|       - | 10751 | ` */` |
|   12446 | 10752 | `static sxi32 PH7_CompilePHP(` |
|       - | 10753 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10754 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10755 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10756 | `	)` |
|       2 | 10757 |  |
|   12448 | 10758 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10759 | `	sxi32 rc;` |
|       - | 10760 | `	/* Reset the token set */` |
|   12448 | 10761 | `	SySetReset(&(*pTokenSet));` |
|       - | 10762 | `	/* Mark as the default token set */` |
|   12448 | 10763 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10764 | `	/* Advance the stream cursor */` |
|   12448 | 10765 | `	pGen->pRawIn++;` |
|       - | 10766 | `	/* Tokenize the PHP chunk first */` |
|   12448 | 10767 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10768 | `	/* Point to the head and tail of the token stream. */` |
|   12448 | 10769 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   12448 | 10770 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   12448 | 10771 | `	if( is_expr ){` |
|     ! 0 | 10772 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10773 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10774 | `			/* A simple expression,compile it */` |
|     ! 0 | 10775 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10776 | `		}` |
|       - | 10777 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10778 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10779 | `		return SXRET_OK;` |
|       - | 10780 | `	}` |
|   12448 | 10781 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10782 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10783 | `		/*` |
|       - | 10784 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10785 | `		 * According to the PHP reference manual:` |
|       - | 10786 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10787 | `		 *  immediately follow` |
|       - | 10788 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10789 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10790 | `		 * Symisc extension:` |
|       - | 10791 | `		 *   This short syntax works with all PHP opening` |
|       - | 10792 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10793 | `		 *   only short tag.` |
|       - | 10794 | `		 */` |
|       - | 10795 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10796 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10797 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 10798 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 10799 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 10800 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 10801 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 10802 | `		}` |
|       3 | 10803 | `		return SXRET_OK;` |
|       - | 10804 | `	}` |
|       - | 10805 | `	/* Compile the PHP chunk */` |
|   12446 | 10806 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 10807 | `	/* Fix exceptions jumps */` |
|   12446 | 10808 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10809 | `	/* Fix gotos now, the jump destination is resolved */` |
|   12446 | 10810 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 10811 | `		rc = SXERR_ABORT;` |
|       1 | 10812 | `	}` |
|       - | 10813 | `	/* Reset container */` |
|   12446 | 10814 | `	SySetReset(&pGen->aGoto);` |
|   12446 | 10815 | `	SySetReset(&pGen->aLabel);` |
|   12446 | 10816 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 10817 | `	/* Compilation result */` |
|   12446 | 10818 | `	return rc;` |
|    6225 | 10819 |  |
|       - | 10820 | `/*` |
|       - | 10821 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 10822 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 10823 | ` * This is the only compile interface exported from this file.` |
|       - | 10824 | ` */` |
|   14786 | 10825 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 10826 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 10827 | `	SyString *pScript,  /* Script to compile */` |
|       - | 10828 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 10829 | `	)` |
|       2 | 10830 |  |
|       - | 10831 | `	SySet aPhpToken,aRawToken;` |
|       - | 10832 | `	ph7_gen_state *pCodeGen;` |
|       - | 10833 | `	ph7_value *pRawObj;` |
|       - | 10834 | `	sxu32 nObjIdx;` |
|       - | 10835 | `	sxi32 nRawObj;` |
|       - | 10836 | `	int is_expr;` |
|       - | 10837 | `	sxi8 bSavedStrict;` |
|       - | 10838 | `	sxi8 bSavedStrictLocked;` |
|       - | 10839 | `	sxi32 rc;` |
|   14788 | 10840 | `	if( pScript->nByte < 1 ){` |
|       - | 10841 | `		/* Nothing to compile */` |
|     ! 0 | 10842 | `		return PH7_OK;` |
|       - | 10843 | `	}` |
|       - | 10844 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 10845 | `	 * file's flags so include/require restore them on return. */` |
|   14788 | 10846 | `	pCodeGen = &pVm->sCodeGen;` |
|   14788 | 10847 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   14788 | 10848 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   14788 | 10849 | `	pCodeGen->bStrictTypes = 0;` |
|   14788 | 10850 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 10851 | `	/* Initialize the tokens containers */` |
|   14788 | 10852 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14788 | 10853 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14788 | 10854 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14788 | 10855 | `	is_expr = 0;` |
|   14788 | 10856 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 10857 | `		SyToken sTmp;` |
|       - | 10858 | `		/* PHP only: -*/` |
|    2956 | 10859 | `		sTmp.nLine = 1;` |
|    2956 | 10860 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2956 | 10861 | `		sTmp.pUserData = 0;` |
|    2956 | 10862 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2956 | 10863 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2956 | 10864 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 10865 | `			/* A simple PHP expression */` |
|     ! 0 | 10866 | `			is_expr = 1;` |
|     ! 0 | 10867 | `		}` |
|    1479 | 10868 | `	}else{` |
|       - | 10869 | `		/* Tokenize raw text */` |
|   11834 | 10870 | `		SySetAlloc(&aRawToken,32);` |
|   11834 | 10871 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 10872 | `	}` |
|       - | 10873 | `	/* Process high-level tokens */` |
|   14788 | 10874 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14788 | 10875 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14788 | 10876 | `	rc = PH7_OK;` |
|   14788 | 10877 | `	if( is_expr ){` |
|       - | 10878 | `		/* Compile the expression */` |
|     ! 0 | 10879 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 10880 | `		goto cleanup;` |
|       - | 10881 | `	}` |
|   14788 | 10882 | `	nObjIdx = 0;` |
|       - | 10883 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 10884 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 10885 | `	 * preventing namespace bleeding across include()d files. */` |
|   14788 | 10886 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 10887 | `	/* Start the compilation process */` |
|   13313 | 10888 | `	for(;;){` |
|   39062 | 10889 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14776 | 10890 | `			break; /* No more tokens to process */` |
|       - | 10891 | `		}` |
|   24288 | 10892 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 10893 | `			/* Compile the PHP chunk */` |
|   12448 | 10894 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   12448 | 10895 | `			if( rc == SXERR_ABORT ){` |
|      13 | 10896 | `				break;` |
|       - | 10897 | `			}` |
|   12436 | 10898 | `			continue;` |
|       - | 10899 | `		}` |
|       - | 10900 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11842 | 10901 | `		nRawObj = 0;` |
|   23682 | 10902 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 10903 | `			/* Consume the raw chunk without any processing */` |
|   11842 | 10904 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11842 | 10905 | `			if( pRawObj == 0 ){` |
|     ! 0 | 10906 | `				rc = SXERR_MEM;` |
|     ! 0 | 10907 | `				break;` |
|       - | 10908 | `			}` |
|       - | 10909 | `			/* Mark as constant and emit the load constant instruction */` |
|   11842 | 10910 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11842 | 10911 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11842 | 10912 | `			++nRawObj;` |
|   11842 | 10913 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 10914 | `		}` |
|   11842 | 10915 | `		if( nRawObj > 0 ){` |
|       - | 10916 | `			/* Emit the consume instruction */` |
|   11842 | 10917 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5920 | 10918 | `		}` |
|    7395 | 10919 | `	}` |
|    7393 | 10920 | `cleanup:` |
|   14788 | 10921 | `	SySetRelease(&aRawToken);` |
|   14788 | 10922 | `	SySetRelease(&aPhpToken);` |
|       - | 10923 | `	/* Restore outer file's strict_types scope */` |
|   14788 | 10924 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   14788 | 10925 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   14788 | 10926 | `	return rc;` |
|    7395 | 10927 |  |
|       - | 10928 | `/*` |
|       - | 10929 | ` * Utility routines.Initialize the code generator.` |
|       - | 10930 | ` */` |
|    2926 | 10931 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 10932 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10933 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10934 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10935 | `	)` |
|       2 | 10936 |  |
|    2928 | 10937 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10938 | `	/* Zero the structure */` |
|    2928 | 10939 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 10940 | `	/* Initial state */` |
|    2928 | 10941 | `	pGen->pVm  = &(*pVm);` |
|    2928 | 10942 | `	pGen->xErr = xErr;` |
|    2928 | 10943 | `	pGen->pErrData = pErrData;` |
|    2928 | 10944 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2928 | 10945 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2928 | 10946 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    2928 | 10947 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2928 | 10948 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 10949 | `	/* Error log buffer */` |
|    2928 | 10950 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 10951 | `	/* General purpose working buffer */` |
|    2928 | 10952 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 10953 | `	/* Namespace state */` |
|    2928 | 10954 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2928 | 10955 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2928 | 10956 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2928 | 10957 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10958 | `	/* Create the global scope */` |
|    2928 | 10959 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 10960 | `	/* Point to the global scope */` |
|    2928 | 10961 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2928 | 10962 | `	return SXRET_OK;` |
|       2 | 10963 |  |
|       - | 10964 | `/*` |
|       - | 10965 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 10966 | ` */` |
|   17398 | 10967 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 10968 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10969 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10970 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10971 | `	)` |
|       2 | 10972 |  |
|   17400 | 10973 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10974 | `	GenBlock *pBlock,*pParent;` |
|       - | 10975 | `	/* Reset state */` |
|   17400 | 10976 | `	SySetReset(&pGen->aLabel);` |
|   17400 | 10977 | `	SySetReset(&pGen->aGoto);` |
|   17400 | 10978 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   17400 | 10979 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   17400 | 10980 | `	SyBlobRelease(&pGen->sWorker);` |
|   17400 | 10981 | `	SyBlobRelease(&pGen->sNamespace);` |
|   17400 | 10982 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   17400 | 10983 | `	SyHashRelease(&pGen->hUseImports);` |
|   17400 | 10984 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   17400 | 10985 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   17400 | 10986 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   17400 | 10987 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   17400 | 10988 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10989 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 10990 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 10991 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 10992 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 10993 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 10994 | `	 * number of unique names, which is acceptable. */` |
|       - | 10995 | `	/* Point to the global scope */` |
|   17400 | 10996 | `	pBlock = pGen->pCurrent;` |
|   17400 | 10997 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 10998 | `		pParent = pBlock->pParent;` |
|     ! 0 | 10999 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11000 | `		pBlock = pParent;` |
|     ! 0 | 11001 | `	}` |
|   17400 | 11002 | `	pGen->xErr = xErr;` |
|   17400 | 11003 | `	pGen->pErrData = pErrData;` |
|   17400 | 11004 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   17400 | 11005 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   17400 | 11006 | `	pGen->pIn = pGen->pEnd = 0;` |
|   17400 | 11007 | `	pGen->nErr = 0;` |
|   17400 | 11008 | `	return SXRET_OK;` |
|       2 | 11009 |  |
|       - | 11010 | `/*` |
|       - | 11011 | ` * Generate a compile-time error message.` |
|       - | 11012 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11013 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11014 | ` * abort compilation immediately.` |
|       - | 11015 | ` */` |
|     570 | 11016 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 11017 |  |
|     572 | 11018 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     572 | 11019 | `	const char *zErr = "Error";` |
|       - | 11020 | `	SyString *pFile;` |
|       - | 11021 | `	va_list ap;` |
|       - | 11022 | `	sxi32 rc;` |
|       - | 11023 | `	/* Reset the working buffer */` |
|     572 | 11024 | `	SyBlobReset(pWorker);` |
|       - | 11025 | `	/* Peek the processed file path if available */` |
|     572 | 11026 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     572 | 11027 | `	if( nErrType == E_ERROR ){` |
|       - | 11028 | `		/* Increment the error counter */` |
|     470 | 11029 | `		pGen->nErr++;` |
|     470 | 11030 | `		if( pGen->nErr > 15 ){` |
|       - | 11031 | `			/* Error count limit reached */` |
|       5 | 11032 | `			if( pGen->xErr ){` |
|       5 | 11033 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11034 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11035 | `				if( pFile ){` |
|       5 | 11036 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11037 | `				}` |
|       5 | 11038 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11039 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11040 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11041 | `				}` |
|       2 | 11042 | `			}` |
|       - | 11043 | `			/* Abort immediately */` |
|       5 | 11044 | `			return SXERR_ABORT;` |
|       - | 11045 | `		}` |
|     232 | 11046 | `	}` |
|     568 | 11047 | `	if( pGen->xErr == 0 ){` |
|       - | 11048 | `		/* No available error consumer,return immediately */` |
|       3 | 11049 | `		return SXRET_OK;` |
|       - | 11050 | `	}` |
|     565 | 11051 | `	switch(nErrType){` |
|     463 | 11052 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 11053 | `	case E_WARNING: zErr = "Warning";     break;` |
|      69 | 11054 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 11055 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11056 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11057 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11058 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11059 | `	default:` |
|     ! 0 | 11060 | `		break;` |
|       - | 11061 | `	}` |
|     565 | 11062 | `	rc = SXRET_OK;` |
|       - | 11063 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     565 | 11064 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     565 | 11065 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     565 | 11066 | `	va_start(ap,zFormat);` |
|     565 | 11067 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     565 | 11068 | `	va_end(ap);` |
|     565 | 11069 | `	if( pFile ){` |
|     565 | 11070 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     282 | 11071 | `	}` |
|       - | 11072 | `	/* Append a new line */` |
|     565 | 11073 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     565 | 11074 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11075 | `		/* Consume the generated error message */` |
|     565 | 11076 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     282 | 11077 | `	}` |
|     565 | 11078 | `	return rc;` |
|     287 | 11079 |  |
|       - | 11080 |  |
