# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5658/7027 lines (80.52%)

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
|       - |    94 | `#define EXPR_FLAG_LOAD_IDX_ISSET    0x008 /* LOAD_IDX argument is the LHS of isset() — emit iP2=4 (offsetExists) */` |
|       - |    95 | `#define EXPR_FLAG_LOAD_IDX_UNSET    0x010 /* LOAD_IDX argument is the LHS of unset() — emit iP2=5 (offsetUnset) */` |
|       - |    96 | `#define EXPR_FLAG_LOAD_IDX_EMPTY    0x020 /* LOAD_IDX argument is the LHS of empty() — emit iP2=6 (offsetExists+offsetGet) */` |
|       - |    97 | `/* Forward declaration */` |
|       - |    98 | `static sxi32 PH7_CompileExpr(ph7_gen_state *pGen,sxi32 iFlags,sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *));` |
|       - |    99 | `/*` |
|       - |   100 | ` * Local utility routines used in the code generation phase.` |
|       - |   101 | ` */` |
|       - |   102 | `/*` |
|       - |   103 | ` * Check if the given name refer to a valid label.` |
|       - |   104 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|       - |   105 | ` * Any other return value indicates no such label.` |
|       - |   106 | ` */` |
|     148 |   107 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|       5 |   108 |  |
|       - |   109 | `	Label *aLabel;` |
|       - |   110 | `	sxu32 n;` |
|       - |   111 | `	/* Perform a linear scan on the label table */` |
|     153 |   112 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     333 |   113 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     277 |   114 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|       - |   115 | `			/* Jump destination found */` |
|      96 |   116 | `			aLabel[n].bRef = TRUE;` |
|      96 |   117 | `			if( ppOut ){` |
|      96 |   118 | `				*ppOut = &aLabel[n];` |
|      46 |   119 | `			}` |
|      96 |   120 | `			return SXRET_OK;` |
|       - |   121 | `		}` |
|      93 |   122 | `	}` |
|       - |   123 | `	/* No such destination */` |
|      60 |   124 | `	return SXERR_NOTFOUND;` |
|      79 |   125 |  |
|       - |   126 | `/*` |
|       - |   127 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   128 | ` * compiled blocks.` |
|       - |   129 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   130 | ` */` |
|    3790 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3795 |   133 | `	GenBlock *pBlock = pCurrent;` |
|   10787 |   134 | `	for(;;){` |
|   21579 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3687 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3687 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3661 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   17923 |   143 | `		pBlock = pBlock->pParent;` |
|   17923 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1900 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  829116 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  829121 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  829121 |   165 | `	pBlock->pUserData   = pUserData;` |
|  829121 |   166 | `	pBlock->pGen        = pGen;` |
|  829121 |   167 | `	pBlock->iFlags      = iType;` |
|  829121 |   168 | `	pBlock->pParent     = 0;` |
|  829121 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  829121 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  829121 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  825604 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  825609 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  825609 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  825609 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  825609 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  825609 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  825609 |   203 | `	pGen->pCurrent = pBlock;` |
|  825609 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  401007 |   206 | `		*ppBlock = pBlock;` |
|  200501 |   207 | `	}` |
|  825609 |   208 | `	return SXRET_OK;` |
|  412807 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  825596 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  825601 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  825601 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  825601 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  825596 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  825601 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  825601 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  825601 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  825601 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  825596 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  825601 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  825601 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  825601 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  825601 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  825601 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  825601 |   247 | `	return SXRET_OK;` |
|  412803 |   248 |  |
|       - |   249 | `/*` |
|       - |   250 | ` * Emit a forward jump.` |
|       - |   251 | ` * Notes on forward jumps` |
|       - |   252 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   253 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   254 | ` *  generation of forward jumps.` |
|       - |   255 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   256 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   257 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|       - |   258 | ` */` |
|  234400 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  234405 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  234405 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  234405 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  234405 |   268 | `	return rc;` |
|       5 |   269 |  |
|       - |   270 | `/*` |
|       - |   271 | ` * Fix a forward jump now the jump destination is resolved.` |
|       - |   272 | ` * Return the total number of fixed jumps.` |
|       - |   273 | ` * Notes on forward jumps:` |
|       - |   274 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|       - |   275 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|       - |   276 | ` *  generation of forward jumps.` |
|       - |   277 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|       - |   278 | ` *  are emitted, we record each forward jump in an instance of the following` |
|       - |   279 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|       - |   280 | ` */` |
|  576564 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  576569 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
| 1038377 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  461813 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  184133 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  277685 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   43287 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  234403 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  234403 |   301 | `		if( pInstr ){` |
|  234403 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  234403 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  234403 |   305 | `			aFix[n].nJumpType = -1;` |
|  117199 |   306 | `		}` |
|  117204 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  576569 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  234096 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  234101 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  234247 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     153 |   329 | `		pJump = &aJumps[n];` |
|       - |   330 | `		/* Extract the target label */` |
|     153 |   331 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     153 |   332 | `		if( rc != SXRET_OK ){` |
|       - |   333 | `			/* No such label */` |
|      60 |   334 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      60 |   335 | `			if( rc == SXERR_ABORT ){` |
|       3 |   336 | `				return SXERR_ABORT;` |
|       - |   337 | `			}` |
|      58 |   338 | `			continue;` |
|       - |   339 | `		}` |
|       - |   340 | `		/* Make sure the target label is reachable */` |
|      96 |   341 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      10 |   342 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      10 |   343 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   344 | `				return SXERR_ABORT;` |
|       - |   345 | `			}` |
|       4 |   346 | `		}` |
|       - |   347 | `		/* Fix the jump now the destination is resolved */` |
|      96 |   348 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      96 |   349 | `		if( pInstr ){` |
|      96 |   350 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   351 | `		}` |
|      50 |   352 | `	}` |
|  234099 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  234231 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  234099 |   361 | `	return SXRET_OK;` |
|  117053 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  740590 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  740595 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  740595 |   370 | `	if( pEntry == 0 ){` |
|  322455 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  418145 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  418145 |   374 | `	return SXRET_OK;` |
|  370300 |   375 |  |
|       - |   376 | `/*` |
|       - |   377 | ` * Install a given constant index in the literal table.` |
|       - |   378 | ` * In order to be installed, the ph7_value must be of type string.` |
|       - |   379 | ` *` |
|       - |   380 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|       - |   381 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|       - |   382 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|       - |   383 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|       - |   384 | ` * many "" literals appear in user code.` |
|       - |   385 | ` */` |
|  322450 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  322455 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  322455 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  161225 |   390 | `	}` |
|  322455 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  123152 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  123157 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  123157 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  123157 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  123157 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  123157 |   411 | `	return pObj;` |
|   61581 |   412 |  |
|       - |   413 | `/*` |
|       - |   414 | ` * Implementation of the PHP language constructs.` |
|       - |   415 | ` */` |
|       - |   416 | `/*` |
|       - |   417 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|       - |   418 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|       - |   419 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|       - |   420 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|       - |   421 | ` *` |
|       - |   422 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|       - |   423 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|       - |   424 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|       - |   425 | ` * surrounding callsites' zero-check fallback pattern.` |
|       - |   426 | ` */` |
|  442684 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  442689 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  221347 |   439 |  |
|       - |   440 | `/* Forward declaration */` |
|       - |   441 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |   442 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|       - |   443 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|       - |   444 | `/* Forward decl: union type parser is defined later in this file. */` |
|       - |   445 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |   446 | `	ph7_gen_state *pGen,` |
|       - |   447 | `	sxu32 *pnType,` |
|       - |   448 | `	SyString *pClass,` |
|       - |   449 | `	SySet *pAlts,` |
|       - |   450 | `	sxi32 *piTypeFlags,` |
|       - |   451 | `	SyString *pTypeText,` |
|       - |   452 | `	int iNullableFlag,` |
|       - |   453 | `	int iUnionFlag,` |
|       - |   454 | `	int bAllowVoid,` |
|       - |   455 | `	sxu32 nLine` |
|       - |   456 | `);` |
|       - |   457 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |   458 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |   459 | `/*` |
|       - |   460 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |   461 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |   462 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |   463 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |   464 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |   465 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |   466 | ` * inputs like a thousand-digit number.` |
|       - |   467 | ` */` |
|       - |   468 | `#define GEN_NUM_SCRATCH 128` |
|       - |   469 | `/*` |
|       - |   470 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |   471 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |   472 | ` *   base  2 => 0 or 1` |
|       - |   473 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |   474 | ` *              decimal scan in the lexer)` |
|       - |   475 | ` */` |
|    1076 |   476 | `static int GenStateIsBaseDigit(int c, int base)` |
|       5 |   477 |  |
|    1081 |   478 | `	if( base == 16 ){ return SyisHex(c); }` |
|     982 |   479 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     703 |   480 | `	return SyisDigit(c);` |
|     543 |   481 |  |
|       - |   482 | `/*` |
|       - |   483 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |   484 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |   485 | ` * the exact wording PHP uses:` |
|       - |   486 | ` *` |
|       - |   487 | ` *   syntax error, unexpected identifier "X"` |
|       - |   488 | ` *` |
|       - |   489 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |   490 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |   491 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |   492 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |   493 | ` * no forward rescan needed.` |
|       - |   494 | ` *` |
|       - |   495 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |   496 | ` * returns 0 when it is well-formed.` |
|       - |   497 | ` */` |
|  123816 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  123821 |   501 | `	const char *z = pRaw->zString;` |
|  123821 |   502 | `	sxu32 n = pRaw->nByte;` |
|  123821 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  123821 |   505 | `	if( n < 2 ) return 0;` |
|   10283 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|   10248 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   37221 |   511 | `	for( i = 0; i < n; ++i ){` |
|   26957 |   512 | `		if( z[i] != '_' ) continue;` |
|     814 |   513 | `		if( i > 0 && i + 1 < n` |
|     543 |   514 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     543 |   515 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |   516 | `			continue; /* well-placed separator */` |
|       - |   517 | `		}` |
|       - |   518 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   519 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      18 |   520 | `		start = i;` |
|      23 |   521 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |   522 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       6 |   523 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |   524 | `		}` |
|      18 |   525 | `		*pBadStart = &z[start];` |
|      18 |   526 | `		*pBadLen = n - start;` |
|      18 |   527 | `		return 1;` |
|     ! 0 |   528 | `	}` |
|   10269 |   529 | `	return 0;` |
|   61913 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  123816 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  123821 |   541 | `	const char *zBad = 0;` |
|  123821 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  123821 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  123807 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   61913 |   555 |  |
|       - |   556 | `/*` |
|       - |   557 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |   558 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |   559 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |   560 | ` *` |
|       - |   561 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |   562 | ` * and *pzAlloc is set to NULL.` |
|       - |   563 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |   564 | ` * and *pzAlloc is set to NULL.` |
|       - |   565 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |   566 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |   567 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |   568 | ` *` |
|       - |   569 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |   570 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |   571 | ` */` |
|  123802 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  123807 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  123807 |   581 | `	*pzAlloc = 0;` |
|  262217 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  138667 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   69210 |   584 | `	}` |
|  123807 |   585 | `	if( !hasUnderscore ){` |
|  123555 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  123555 |   587 | `		return SXRET_OK;` |
|       - |   588 | `	}` |
|     253 |   589 | `	if( pToken->nByte <= nScratch ){` |
|     251 |   590 | `		zBuf = zScratch;` |
|     126 |   591 | `	}else{` |
|       3 |   592 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |   593 | `		if( zBuf == 0 ){` |
|     ! 0 |   594 | `			return SXERR_ABORT;` |
|       - |   595 | `		}` |
|       3 |   596 | `		*pzAlloc = zBuf;` |
|       - |   597 | `	}` |
|     253 |   598 | `	j = 0;` |
|    2895 |   599 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |   600 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |   601 | `	}` |
|     253 |   602 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |   603 | `	return SXRET_OK;` |
|   61906 |   604 |  |
|       - |   605 | `/*` |
|       - |   606 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |   607 | ` * Notes on the integer type.` |
|       - |   608 | ` *  According to the PHP language reference manual` |
|       - |   609 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |   610 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |   611 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |   612 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |   613 | ` * Symisc eXtension to the integer type.` |
|       - |   614 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |   615 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |   616 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |   617 | ` *  [i.e: either 32bit or 64bit].` |
|       - |   618 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |   619 | ` *  documentation.` |
|       - |   620 | ` */` |
|  123788 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  123793 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  123793 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  123793 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   61894 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  123793 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  123793 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  185672 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   61889 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  123783 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  123783 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  123157 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  123157 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  123157 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  123157 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   61581 |   649 | `	}else{` |
|       - |   650 | `		/* Real number */` |
|       - |   651 | `		ph7_value *pObj;` |
|       - |   652 | `		/* Reserve a new constant */` |
|     630 |   653 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     630 |   654 | `		if( pObj == 0 ){` |
|     ! 0 |   655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   656 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   657 | `			return SXERR_ABORT;` |
|       - |   658 | `		}` |
|     630 |   659 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     630 |   660 | `		PH7_MemObjToReal(pObj);` |
|       - |   661 | `	}` |
|  123783 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  123783 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  123783 |   666 | `	return SXRET_OK;` |
|   61899 |   667 |  |
|       - |   668 | `/*` |
|       - |   669 | ` * Compile a single quoted string.` |
|       - |   670 | ` * According to the PHP language reference manual:` |
|       - |   671 | ` *` |
|       - |   672 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   673 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   674 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   675 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   676 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   677 | ` *` |
|       - |   678 | ` */` |
|   88616 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   88621 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   88621 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   88621 |   687 | `	zIn  = pStr->zString;` |
|   88621 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   88621 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    7195 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7195 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   81431 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   32097 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   32097 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   49339 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   49339 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   49339 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   49389 |   711 | `	for(;;){` |
|   98783 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   49339 |   714 | `			break;` |
|       - |   715 | `		}` |
|   49449 |   716 | `		zCur = zIn;` |
|  778845 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  729401 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   49449 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   49425 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   24710 |   723 | `		}` |
|   49449 |   724 | `		zIn++;` |
|   49449 |   725 | `		if( zIn < zEnd ){` |
|     132 |   726 | `			if( zIn[0] == '\\' ){` |
|       - |   727 | `				/* A literal backslash */` |
|      23 |   728 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     121 |   729 | `			}else if( zIn[0] == '\'' ){` |
|       - |   730 | `				/* A single quote */` |
|      11 |   731 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   732 | `			}else{` |
|       - |   733 | `				/* verbatim copy */` |
|     100 |   734 | `				zIn--;` |
|     100 |   735 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|     100 |   736 | `				zIn++;` |
|       - |   737 | `			}` |
|      65 |   738 | `		}` |
|       - |   739 | `		/* Advance the stream cursor */` |
|   49449 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   49339 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   49339 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   49339 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   24667 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   49339 |   749 | `	return SXRET_OK;` |
|   44313 |   750 |  |
|       - |   751 | `/*` |
|       - |   752 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   753 | ` *` |
|       - |   754 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   755 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   756 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   757 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   758 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   759 | ` *` |
|       - |   760 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   761 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   762 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   763 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   764 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   765 | ` *     whitespace.` |
|       - |   766 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   767 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   768 | ` */` |
|     110 |   769 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       5 |   770 |  |
|     115 |   771 | `	SyString *pIn = &pGen->pIn->sData;` |
|     115 |   772 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   773 | `	const char *zPrefix;` |
|       - |   774 | `	const char *z, *zEnd;` |
|       - |   775 | `	char *zBuf, *zDst;` |
|     115 |   776 | `	if( nIndent == 0 ){` |
|       - |   777 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      69 |   778 | `		*pOut = *pIn;` |
|      69 |   779 | `		return SXRET_OK;` |
|       - |   780 | `	}` |
|       - |   781 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   782 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   783 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   784 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   785 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   786 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      48 |   787 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      48 |   788 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   789 | `		zPrefix += 2;` |
|     ! 0 |   790 | `	}else{` |
|      48 |   791 | `		zPrefix += 1;` |
|       - |   792 | `	}` |
|       - |   793 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      48 |   794 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      48 |   795 | `	if( zBuf == 0 ){` |
|     ! 0 |   796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   797 | `		return SXERR_ABORT;` |
|       - |   798 | `	}` |
|      48 |   799 | `	zDst = zBuf;` |
|      48 |   800 | `	z = pIn->zString;` |
|      48 |   801 | `	zEnd = z + pIn->nByte;` |
|     130 |   802 | `	while( z < zEnd ){` |
|      72 |   803 | `		const char *zLine = z;` |
|       - |   804 | `		sxu32 nLine;` |
|       - |   805 | `		int bEmpty;` |
|     800 |   806 | `		while( z < zEnd && z[0] != '\n' ){` |
|     732 |   807 | `			z++;` |
|       4 |   808 | `		}` |
|      72 |   809 | `		nLine = (sxu32)(z - zLine);` |
|      72 |   810 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      72 |   811 | `		if( !bEmpty ){` |
|       - |   812 | `			sxu32 i;` |
|      68 |   813 | `			if( nLine < nIndent ){` |
|     ! 0 |   814 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   815 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   816 | `					nIndent);` |
|     ! 0 |   817 | `				return SXERR_ABORT;` |
|       - |   818 | `			}` |
|     270 |   819 | `			for( i = 0; i < nIndent; i++ ){` |
|     214 |   820 | `				if( zLine[i] != zPrefix[i] ){` |
|      11 |   821 | `					unsigned char c = (unsigned char)zLine[i];` |
|      11 |   822 | `					if( c == ' ' \|\| c == '\t' ){` |
|       6 |   823 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   824 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       4 |   825 | `					}else{` |
|       8 |   826 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   827 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   828 | `							nIndent);` |
|       - |   829 | `					}` |
|      11 |   830 | `					return SXERR_ABORT;` |
|       - |   831 | `				}` |
|     104 |   832 | `			}` |
|      57 |   833 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   834 | `			zDst += nLine - nIndent;` |
|      33 |   835 | `		}else if( nLine == 1 ){` |
|       - |   836 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   837 | `			*zDst++ = '\r';` |
|     ! 0 |   838 | `		}` |
|      61 |   839 | `		if( z < zEnd ){` |
|      25 |   840 | `			*zDst++ = '\n';` |
|      25 |   841 | `			z++;` |
|      12 |   842 | `		}` |
|       1 |   843 | `	}` |
|      37 |   844 | `	pOut->zString = zBuf;` |
|      37 |   845 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   846 | `	return SXRET_OK;` |
|      60 |   847 |  |
|       - |   848 | `/*` |
|       - |   849 | ` * Compile a nowdoc string.` |
|       - |   850 | ` * According to the PHP language reference manual:` |
|       - |   851 | ` *` |
|       - |   852 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   853 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   854 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   855 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   856 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   857 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   858 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   859 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   860 | ` *  of the closing identifier.` |
|       - |   861 | ` */` |
|      46 |   862 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |   863 |  |
|       - |   864 | `	SyString sStripped;` |
|       - |   865 | `	SyString *pStr;` |
|       - |   866 | `	ph7_value *pObj;` |
|       - |   867 | `	sxu32 nIdx;` |
|       - |   868 | `	sxi32 rc;` |
|      50 |   869 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      50 |   870 | `	if( rc != SXRET_OK ){` |
|       6 |   871 | `		return rc;` |
|       - |   872 | `	}` |
|      44 |   873 | `	pStr = &sStripped;` |
|      44 |   874 | `	nIdx = 0; /* Prevent compiler warning */` |
|      44 |   875 | `	if( pStr->nByte <= 0 ){` |
|       - |   876 | `		/* Empty string,load NULL */` |
|       7 |   877 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   878 | `		return SXRET_OK;` |
|       - |   879 | `	}` |
|       - |   880 | `	/* Reserve a new constant */` |
|      38 |   881 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      38 |   882 | `	if( pObj == 0 ){` |
|     ! 0 |   883 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   884 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   885 | `		return SXERR_ABORT;` |
|       - |   886 | `	}` |
|       - |   887 | `	/* No processing is done here, simply a memcpy() operation */` |
|      38 |   888 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   889 | `	/* Emit the load constant instruction */` |
|      38 |   890 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   891 | `	/* Node successfully compiled */` |
|      38 |   892 | `	return SXRET_OK;` |
|      27 |   893 |  |
|       - |   894 | `/*` |
|       - |   895 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |   896 | ` * According to the PHP language reference manual` |
|       - |   897 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |   898 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |   899 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |   900 | ` *  property in a string with a minimum of effort.` |
|       - |   901 | ` *  Simple syntax` |
|       - |   902 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |   903 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |   904 | ` *   the end of the name.` |
|       - |   905 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |   906 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |   907 | ` *   as to simple variables.` |
|       - |   908 | ` *  Complex (curly) syntax` |
|       - |   909 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |   910 | ` *   of complex expressions.` |
|       - |   911 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |   912 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |   913 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |   914 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |   915 | ` */` |
|    2210 |   916 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   917 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   918 | `	sxu32 nLine,         /* Line number */` |
|       - |   919 | `	const char *zIn,     /* Raw expression */` |
|       - |   920 | `	const char *zEnd     /* End of the expression */` |
|       - |   921 | `	)` |
|       5 |   922 |  |
|       - |   923 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   924 | `	SySet sToken;` |
|       - |   925 | `	sxi32 rc;` |
|       - |   926 | `	/* Initialize the token set */` |
|    2215 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2215 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2215 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2215 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2215 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2215 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2215 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2215 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2215 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2215 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2215 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2215 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   24112 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   24117 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   24117 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   24117 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   24117 |   960 | `	(*pCount)++;` |
|   24117 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   24117 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   24117 |   964 | `	return pConstObj;` |
|   12061 |   965 |  |
|       - |   966 | `/*` |
|       - |   967 | ` * Compile a double quoted/heredoc string.` |
|       - |   968 | ` * According to the PHP language reference manual` |
|       - |   969 | ` * Heredoc` |
|       - |   970 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |   971 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |   972 | ` *  to close the quotation.` |
|       - |   973 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |   974 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |   975 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |   976 | ` *  Warning` |
|       - |   977 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |   978 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |   979 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |   980 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |   981 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |   982 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |   983 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |   984 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |   985 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |   986 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |   987 | ` * Double quoted` |
|       - |   988 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |   989 | ` *  Escaped characters Sequence 	Meaning` |
|       - |   990 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |   991 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |   992 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |   993 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |   994 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |   995 | ` *  \\ backslash` |
|       - |   996 | ` *  \$ dollar sign` |
|       - |   997 | ` *  \" double-quote` |
|       - |   998 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |   999 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |  1000 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |  1001 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |  1002 | ` * See string parsing for details.` |
|       - |  1003 | ` */` |
|   22650 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   22655 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   22655 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   22655 |  1012 | `	zIn  = pStr->zString;` |
|   22655 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   22655 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     313 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     313 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   22347 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   22347 |  1024 | `	iCons = 0;` |
|   12276 |  1025 | `	for(;;){` |
|   36763 |  1026 | `		zCur = zIn;` |
|  175345 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  140797 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  140673 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2090 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1046 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  138587 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   36763 |  1036 | `		if( zIn > zCur ){` |
|   17181 |  1037 | `			if( pObj == 0 ){` |
|   16707 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   16707 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    8351 |  1042 | `			}` |
|   17181 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8588 |  1044 | `		}` |
|   36763 |  1045 | `		if( zIn >= zEnd ){` |
|   22347 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   14421 |  1048 | `		if( zIn[0] == '\\' ){` |
|   12211 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   12211 |  1051 | `			zIn++;` |
|   12211 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   12211 |  1055 | `			if( pObj == 0 ){` |
|    7415 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7415 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3705 |  1060 | `			}` |
|   12211 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   12211 |  1062 | `			switch( zIn[0] ){` |
|       7 |  1063 | `			case '$':` |
|       - |  1064 | `				/* Dollar sign */` |
|      15 |  1065 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|      15 |  1066 | `				break;` |
|      49 |  1067 | `			case '\\':` |
|       - |  1068 | `				/* A literal backslash */` |
|     102 |  1069 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|     102 |  1070 | `				break;` |
|       2 |  1071 | `			case 'a':` |
|       - |  1072 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  1073 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  1074 | `				break;` |
|       2 |  1075 | `			case 'b':` |
|       - |  1076 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  1077 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  1078 | `				break;` |
|       4 |  1079 | `			case 'f':` |
|       - |  1080 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1081 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1082 | `				break;` |
|    5620 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11245 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11245 |  1086 | `				break;` |
|      19 |  1087 | `			case 'r':` |
|       - |  1088 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      43 |  1089 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      43 |  1090 | `				break;` |
|      24 |  1091 | `			case 't':` |
|       - |  1092 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      53 |  1093 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      53 |  1094 | `				break;` |
|       3 |  1095 | `			case 'v':` |
|       - |  1096 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1097 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1098 | `				break;` |
|       1 |  1099 | `			case '\'':` |
|       - |  1100 | `				/* Single quote */` |
|       3 |  1101 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1102 | `				break;` |
|     108 |  1103 | `			case '"':` |
|       - |  1104 | `				/* Double quote */` |
|     221 |  1105 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     221 |  1106 | `				break;` |
|      10 |  1107 | `			case '0':` |
|       - |  1108 | `				/* NUL byte */` |
|      21 |  1109 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      21 |  1110 | `				break;` |
|     228 |  1111 | `			case 'x':` |
|     457 |  1112 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1113 | `					int c;` |
|       - |  1114 | `					/* Hex digit */` |
|     443 |  1115 | `					c = SyHexToint(zIn[1]) << 4;` |
|     443 |  1116 | `					if( &zIn[2] < zEnd ){` |
|     443 |  1117 | `						c +=  SyHexToint(zIn[2]);` |
|     221 |  1118 | `					}` |
|       - |  1119 | `					/* Output char */` |
|     443 |  1120 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     443 |  1121 | `					n += sizeof(char) * 2;` |
|     222 |  1122 | `				}else{` |
|       - |  1123 | `					/* Output literal character  */` |
|      15 |  1124 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1125 | `				}` |
|     457 |  1126 | `				break;` |
|      15 |  1127 | `			case 'o':` |
|      31 |  1128 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  1129 | `					/* Octal digit stream */` |
|       - |  1130 | `					int c;` |
|      21 |  1131 | `					c = 0;` |
|      21 |  1132 | `					zIn++;` |
|      61 |  1133 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  1134 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  1135 | `							break;` |
|       - |  1136 | `						}` |
|      41 |  1137 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  1138 | `					}` |
|      21 |  1139 | `					if ( c > 0 ){` |
|      15 |  1140 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  1141 | `					}` |
|      21 |  1142 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  1143 | `				}else{` |
|       - |  1144 | `					/* Output literal character  */` |
|      11 |  1145 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  1146 | `				}` |
|      31 |  1147 | `				break;` |
|      11 |  1148 | `			default:` |
|       - |  1149 | `				/* Output without a slash */` |
|      23 |  1150 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  1151 | `				break;` |
|       - |  1152 | `			}` |
|       - |  1153 | `			/* Advance the stream cursor */` |
|   12211 |  1154 | `			zIn += n;` |
|   12211 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2215 |  1157 | `		if( zIn[0] == '{' ){` |
|       - |  1158 | `			/* Curly syntax */` |
|       - |  1159 | `			const char *zExpr;` |
|     131 |  1160 | `			sxi32 iNest = 1;` |
|     131 |  1161 | `			zIn++;` |
|     131 |  1162 | `			zExpr = zIn;` |
|       - |  1163 | `			/* Synchronize with the next closing curly braces */` |
|    1359 |  1164 | `			while( zIn < zEnd ){` |
|    1359 |  1165 | `				if( zIn[0] == '{' ){` |
|       - |  1166 | `					/* Increment nesting level */` |
|       9 |  1167 | `					iNest++;` |
|    1355 |  1168 | `				}else if(zIn[0] == '}' ){` |
|       - |  1169 | `					/* Decrement nesting level */` |
|     139 |  1170 | `					iNest--;` |
|     139 |  1171 | `					if( iNest <= 0 ){` |
|     131 |  1172 | `						break;` |
|       - |  1173 | `					}` |
|       4 |  1174 | `				}` |
|    1231 |  1175 | `				zIn++;` |
|       3 |  1176 | `			}` |
|       - |  1177 | `			/* Process the expression */` |
|     131 |  1178 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     131 |  1179 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1180 | `				return SXERR_ABORT;` |
|       - |  1181 | `			}` |
|     131 |  1182 | `			if( rc != SXERR_EMPTY ){` |
|     131 |  1183 | `				++iCons;` |
|      64 |  1184 | `			}` |
|     131 |  1185 | `			if( zIn < zEnd ){` |
|       - |  1186 | `				/* Jump the trailing curly */` |
|     131 |  1187 | `				zIn++;` |
|      64 |  1188 | `			}` |
|      67 |  1189 | `		}else{` |
|       - |  1190 | `			/* Simple syntax */` |
|    2087 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|    1050 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    4187 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2087 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|    1050 |  1198 | `				for(;;){` |
|   11677 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8527 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    2105 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    2105 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    2105 |  1212 | `				if( zIn >= zEnd ){` |
|     197 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1913 |  1215 | `				if( zIn[0] == '[' ){` |
|      12 |  1216 | `					sxi32 iSquare = 1;` |
|      12 |  1217 | `					zIn++;` |
|      28 |  1218 | `					while( zIn < zEnd ){` |
|      28 |  1219 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1220 | `							iSquare++;` |
|      28 |  1221 | `						}else if (zIn[0] == ']' ){` |
|      12 |  1222 | `							iSquare--;` |
|      12 |  1223 | `							if( iSquare <= 0 ){` |
|      12 |  1224 | `								break;` |
|       - |  1225 | `							}` |
|     ! 0 |  1226 | `						}` |
|      18 |  1227 | `						zIn++;` |
|       2 |  1228 | `					}` |
|      12 |  1229 | `					if( zIn < zEnd ){` |
|      12 |  1230 | `						zIn++;` |
|       5 |  1231 | `					}` |
|      12 |  1232 | `					break;` |
|    1903 |  1233 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1234 | `					sxi32 iCurly = 1;` |
|       6 |  1235 | `					zIn++;` |
|      18 |  1236 | `					while( zIn < zEnd ){` |
|      16 |  1237 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1238 | `							iCurly++;` |
|      16 |  1239 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1240 | `							iCurly--;` |
|       3 |  1241 | `							if( iCurly <= 0 ){` |
|       3 |  1242 | `								break;` |
|       - |  1243 | `							}` |
|     ! 0 |  1244 | `						}` |
|      14 |  1245 | `						zIn++;` |
|       2 |  1246 | `					}` |
|       6 |  1247 | `					if( zIn < zEnd ){` |
|       3 |  1248 | `						zIn++;` |
|       1 |  1249 | `					}` |
|       6 |  1250 | `					break;` |
|    1899 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      21 |  1253 | `					zIn += 2;` |
|    1890 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     943 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       3 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    2087 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2087 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    2087 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    2085 |  1267 | `				++iCons;` |
|    1040 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2215 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   22347 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1655 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     825 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   22347 |  1278 | `	return SXRET_OK;` |
|   11330 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   22590 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   22595 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   11295 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   22595 |  1290 | `	return rc;` |
|       5 |  1291 |  |
|       - |  1292 | `/*` |
|       - |  1293 | ` * Compile a Heredoc string.` |
|       - |  1294 | ` *  See the block-comment above for more information.` |
|       - |  1295 | ` */` |
|      64 |  1296 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  1297 |  |
|       - |  1298 | `	SyString sOrig, sStripped;` |
|       - |  1299 | `	sxi32 rc;` |
|      68 |  1300 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      68 |  1301 | `	if( rc != SXRET_OK ){` |
|       6 |  1302 | `		return rc;` |
|       - |  1303 | `	}` |
|       - |  1304 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1305 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1306 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1307 | `	 * unaffected, including on the error path. */` |
|      63 |  1308 | `	sOrig = pGen->pIn->sData;` |
|      63 |  1309 | `	pGen->pIn->sData = sStripped;` |
|      63 |  1310 | `	rc = GenStateCompileString(&(*pGen));` |
|      63 |  1311 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1312 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      63 |  1313 | `	return rc;` |
|      36 |  1314 |  |
|       - |  1315 | `/*` |
|       - |  1316 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1317 | ` *  Notes on array entries.` |
|       - |  1318 | ` *  According to the PHP language reference manual` |
|       - |  1319 | ` *  An array can be created by the array() language construct.` |
|       - |  1320 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1321 | ` *  array(  key =>  value` |
|       - |  1322 | ` *    , ...` |
|       - |  1323 | ` *    )` |
|       - |  1324 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1325 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1326 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1327 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1328 | ` *  contain integer and string indices.` |
|       - |  1329 | ` *  A value can be any PHP type.` |
|       - |  1330 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1331 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1332 | ` *  is specified, that value will be overwritten.` |
|       - |  1333 | ` */` |
|   21418 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1335 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1336 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1337 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1338 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1339 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1340 | `	)` |
|       5 |  1341 |  |
|       - |  1342 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1343 | `	sxi32 rc;` |
|       - |  1344 | `	/* Swap token stream */` |
|   21423 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   21423 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   21423 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   21423 |  1350 | `	return rc;` |
|       5 |  1351 |  |
|       - |  1352 | `/*` |
|       - |  1353 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1354 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1355 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1356 | ` * error message.` |
|       - |  1357 | ` * See the routine responible of compiling the array language construct` |
|       - |  1358 | ` * for more inforation.` |
|       - |  1359 | ` */` |
|      36 |  1360 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1361 |  |
|      40 |  1362 | `	sxi32 rc = SXRET_OK;` |
|      40 |  1363 | `	if( pRoot->pOp ){` |
|      19 |  1364 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1365 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      16 |  1366 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1367 | `			/* Unexpected expression */` |
|      13 |  1368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1369 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      13 |  1370 | `			if( rc != SXERR_ABORT ){` |
|      13 |  1371 | `				rc = SXERR_INVALID;` |
|       5 |  1372 | `			}` |
|       9 |  1373 | `		}` |
|      31 |  1374 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1375 | `		/* Unexpected expression */` |
|       3 |  1376 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1377 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1378 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1379 | `			rc = SXERR_INVALID;` |
|       1 |  1380 | `		}` |
|       1 |  1381 | `	}` |
|      40 |  1382 | `	return rc;` |
|       4 |  1383 |  |
|       - |  1384 | `/*` |
|       - |  1385 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1386 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1387 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1388 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1389 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1390 | ` */` |
|   23748 |  1391 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1392 |  |
|   23753 |  1393 | `	SyToken *pCur = pStart;` |
|   23753 |  1394 | `	sxi32 iNest = 0;` |
|   67255 |  1395 | `	while( pCur < pEnd ){` |
|   48887 |  1396 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5381 |  1397 | `			return pCur;` |
|       - |  1398 | `		}` |
|       - |  1399 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1400 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1401 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1402 | `		 */` |
|   43511 |  1403 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      95 |  1404 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      95 |  1405 | `			SyToken *pFn = pCur;` |
|      92 |  1406 | `			if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pEnd` |
|     ! 0 |  1407 | `				&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       3 |  1408 | `				&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1409 | `				pFn = &pCur[1];` |
|     ! 0 |  1410 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1411 | `			}` |
|      95 |  1412 | `			if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1413 | `				pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1414 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1415 | `					pCur++;` |
|     ! 0 |  1416 | `				}` |
|       5 |  1417 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1418 | `					pCur++;` |
|       5 |  1419 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1420 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1421 | `					if( pCur < pEnd ){` |
|       5 |  1422 | `						pCur++;` |
|       2 |  1423 | `					}` |
|       2 |  1424 | `				}` |
|       5 |  1425 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1426 | `					pCur++;` |
|     ! 0 |  1427 | `					if( pCur < pEnd && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1428 | `						&& pCur->sData.nByte == 1` |
|     ! 0 |  1429 | `						&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1430 | `						pCur++;` |
|     ! 0 |  1431 | `					}` |
|     ! 0 |  1432 | `					if( pCur < pEnd` |
|     ! 0 |  1433 | `						&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1434 | `						pCur++;` |
|     ! 0 |  1435 | `					}` |
|     ! 0 |  1436 | `				}` |
|       - |  1437 | `				/* The rest of the entry is the arrow-function body — no outer` |
|       - |  1438 | `				 * key to extract. */` |
|       5 |  1439 | `				return pEnd;` |
|       - |  1440 | `			}` |
|       - |  1441 | `			/* Match expression (PHP 8.0): the '=>' inside match arms is not an` |
|       - |  1442 | `			 * entry separator. Skip past the full match span. */` |
|      91 |  1443 | `			if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1444 | `				pCur++; /* past 'match' */` |
|       3 |  1445 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1446 | `					pCur++;` |
|       3 |  1447 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1448 | `						PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1449 | `					if( pCur < pEnd ){` |
|       3 |  1450 | `						pCur++;` |
|       1 |  1451 | `					}` |
|       1 |  1452 | `				}` |
|       3 |  1453 | `				if( pCur < pEnd && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1454 | `					pCur++;` |
|       3 |  1455 | `					PH7_DelimitNestedTokens(pCur,pEnd,` |
|       - |  1456 | `						PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1457 | `					if( pCur < pEnd ){` |
|       3 |  1458 | `						pCur++;` |
|       1 |  1459 | `					}` |
|       1 |  1460 | `				}` |
|       3 |  1461 | `				continue;` |
|       - |  1462 | `			}` |
|      43 |  1463 | `		}` |
|   43505 |  1464 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     326 |  1465 | `			iNest++;` |
|   43344 |  1466 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1467 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1468 | `			 * parser will shortly detect any syntax error. */` |
|     326 |  1469 | `			iNest--;` |
|     161 |  1470 | `		}` |
|   43505 |  1471 | `		pCur++;` |
|       5 |  1472 | `	}` |
|   18373 |  1473 | `	return pEnd;` |
|   11879 |  1474 |  |
|       - |  1475 | `/*` |
|       - |  1476 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1477 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1478 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1479 | ` */` |
|   30866 |  1480 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1481 |  |
|       - |  1482 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1483 | `	SyToken *pKey,*pCur;` |
|   30871 |  1484 | `	sxi32 iEmitRef = 0;` |
|   30871 |  1485 | `	sxi32 iSpread = 0;` |
|   30871 |  1486 | `	sxi32 nPair = 0;` |
|       - |  1487 | `	sxi32 rc;` |
|   30871 |  1488 | `	xValidator = 0;` |
|   25250 |  1489 | `	for(;;){` |
|       - |  1490 | `		/* Jump leading commas */` |
|   57275 |  1491 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6775 |  1492 | `			pGen->pIn++;` |
|       5 |  1493 | `		}` |
|   50505 |  1494 | `		pCur = pGen->pIn;` |
|   50505 |  1495 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1496 | `			/* No more entry to process */` |
|   30855 |  1497 | `			break;` |
|       - |  1498 | `		}` |
|   19655 |  1499 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1500 | `			continue;` |
|       - |  1501 | `		}` |
|       - |  1502 | `		/* Compile the key if available */` |
|   19655 |  1503 | `		pKey = pCur;` |
|   19655 |  1504 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   19655 |  1505 | `		rc = SXERR_EMPTY;` |
|   19655 |  1506 | `		if( pCur < pGen->pIn ){` |
|    1605 |  1507 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1508 | `				/* Missing value */` |
|      12 |  1509 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      12 |  1510 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1511 | `					return SXERR_ABORT;` |
|       - |  1512 | `				}` |
|      12 |  1513 | `				return SXRET_OK;` |
|       - |  1514 | `			}` |
|       - |  1515 | `			/* Compile the expression holding the key */` |
|    1595 |  1516 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1517 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1595 |  1518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1519 | `				return SXERR_ABORT;` |
|       - |  1520 | `			}` |
|    1595 |  1521 | `			pCur++; /* Jump the '=>' operator */` |
|   18850 |  1522 | `		}else if( pKey == pCur ){` |
|       - |  1523 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1524 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1525 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1526 | `		}else{` |
|       - |  1527 | `			/* Reset back the cursor and point to the entry value */` |
|   18055 |  1528 | `			pCur = pKey;` |
|       - |  1529 | `		}` |
|   19645 |  1530 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1531 | `			/* No available key,load NULL */` |
|   18057 |  1532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9026 |  1533 | `		}` |
|   19645 |  1534 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1535 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      44 |  1536 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      44 |  1537 | `			iEmitRef = 1;` |
|      44 |  1538 | `			pCur++; /* Jump the '&' token */` |
|      44 |  1539 | `			if( pCur >= pGen->pIn ){` |
|       - |  1540 | `				/* Missing value */` |
|       3 |  1541 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1542 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1543 | `					return SXERR_ABORT;` |
|       - |  1544 | `				}` |
|       3 |  1545 | `				return SXRET_OK;` |
|       - |  1546 | `			}` |
|      19 |  1547 | `		}` |
|       - |  1548 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1549 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1550 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1551 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1552 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   19643 |  1553 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   19643 |  1554 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1555 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1556 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1557 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1558 | `			 * output is engine-portable. */` |
|       6 |  1559 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1560 | `				"syntax error, unexpected token \"...\"");` |
|       6 |  1561 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1562 | `				return SXERR_ABORT;` |
|       - |  1563 | `			}` |
|       6 |  1564 | `			return SXRET_OK;` |
|       - |  1565 | `		}` |
|       - |  1566 | `		/* Compile indice value */` |
|   19639 |  1567 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   19639 |  1568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1569 | `			return SXERR_ABORT;` |
|       - |  1570 | `		}` |
|   19639 |  1571 | `		if( iSpread ){` |
|       - |  1572 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   19608 |  1574 | `		}else if( iEmitRef ){` |
|       - |  1575 | `			/* Emit the load reference instruction */` |
|      40 |  1576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1577 | `		}` |
|   19639 |  1578 | `		xValidator = 0;` |
|   19639 |  1579 | `		iEmitRef = 0;` |
|   19639 |  1580 | `		iSpread = 0;` |
|   19639 |  1581 | `		nPair++;` |
|       5 |  1582 | `	}` |
|       - |  1583 | `	/* Emit the load map instruction */` |
|   30855 |  1584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1585 | `	/* Node successfully compiled */` |
|   30855 |  1586 | `	return SXRET_OK;` |
|   15438 |  1587 |  |
|       - |  1588 | `/*` |
|       - |  1589 | ` * Compile the 'array' language construct.` |
|       - |  1590 | ` *	 According to the PHP language reference manual` |
|       - |  1591 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1592 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1593 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1594 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1595 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1596 | ` */` |
|   29904 |  1597 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1598 |  |
|       - |  1599 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   29909 |  1600 | `	pGen->pIn += 2;` |
|   29909 |  1601 | `	pGen->pEnd--;` |
|   14952 |  1602 | `	SXUNUSED(iCompileFlag);` |
|   29909 |  1603 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1604 |  |
|       - |  1605 | `/*` |
|       - |  1606 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1607 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1608 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1609 | ` */` |
|     962 |  1610 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1611 |  |
|       - |  1612 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     967 |  1613 | `	pGen->pIn++;` |
|     967 |  1614 | `	pGen->pEnd--;` |
|     481 |  1615 | `	SXUNUSED(iCompileFlag);` |
|     967 |  1616 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1617 |  |
|       - |  1618 | `/*` |
|       - |  1619 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1620 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1621 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1622 | ` * error message.` |
|       - |  1623 | ` * See the routine responible of compiling the list language construct` |
|       - |  1624 | ` * for more inforation.` |
|       - |  1625 | ` */` |
|     158 |  1626 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       4 |  1627 |  |
|     162 |  1628 | `	sxi32 rc = SXRET_OK;` |
|     162 |  1629 | `	if( pRoot->pOp ){` |
|       4 |  1630 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|       2 |  1631 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1632 | `				/* Unexpected expression */` |
|     ! 0 |  1633 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1634 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1635 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1636 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1637 | `				}` |
|       1 |  1638 | `		}` |
|     160 |  1639 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1640 | `		/* Unexpected expression */` |
|       6 |  1641 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1642 | `			"list(): Expecting a variable not an expression");` |
|       6 |  1643 | `		if( rc != SXERR_ABORT ){` |
|       6 |  1644 | `			rc = SXERR_INVALID;` |
|       2 |  1645 | `		}` |
|       2 |  1646 | `	}` |
|     162 |  1647 | `	return rc;` |
|       4 |  1648 |  |
|       - |  1649 | `/*` |
|       - |  1650 | ` * Compile the 'list' language construct.` |
|       - |  1651 | ` *  According to the PHP language reference` |
|       - |  1652 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1653 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1654 | ` *  Description` |
|       - |  1655 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1656 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1657 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1658 | ` *  Parameters` |
|       - |  1659 | ` *   $varname: A variable.` |
|       - |  1660 | ` *  Return Values` |
|       - |  1661 | ` *   The assigned array.` |
|       - |  1662 | ` */` |
|       - |  1663 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1664 | `struct NestedListEntry {` |
|       - |  1665 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1666 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1667 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1668 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1669 | `};` |
|       - |  1670 | `/*` |
|       - |  1671 | ` * Compile the body of a *keyed* list/short-list destructuring (PHP 7.1), where` |
|       - |  1672 | `` * every entry has the form `keyExpr => target`. The source array is on the stack`` |
|       - |  1673 | ` * top on entry and remains there on exit, mirroring the positional LOAD_LIST` |
|       - |  1674 | ` * path so the caller's teardown is unchanged. For each entry: DUP the source,` |
|       - |  1675 | ` * push the key, LOAD_IDX to fetch source[key] (NULL on a missing key, silently,` |
|       - |  1676 | ` * like a normal subscript read), then assign the fetched value to the target — a` |
|       - |  1677 | ` * nested [...]/list() recurses, a simple lvalue uses the same STORE fold as a` |
|       - |  1678 | ` * normal assignment (the value sits below the lvalue-load, exactly as in` |
|       - |  1679 | ` * GenStateEmitExprCode where the assignment RHS precedes the LHS load).` |
|       - |  1680 | ` */` |
|      22 |  1681 | `static sxi32 GenStateCompileKeyedListBody(ph7_gen_state *pGen)` |
|       1 |  1682 |  |
|       - |  1683 | `	SyToken *pNext;` |
|       - |  1684 | `	sxi32 rc;` |
|      53 |  1685 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|       - |  1686 | `		SyToken *pArrow,*pTarget;` |
|       - |  1687 | ``		/* Split `keyExpr => target` at the top-level '=>' */`` |
|      31 |  1688 | `		pArrow = GenStateFindTopLevelArrow(pGen->pIn,pNext);` |
|      31 |  1689 | `		pTarget = &pArrow[1];` |
|      31 |  1690 | `		if( pArrow <= pGen->pIn \|\| pTarget >= pNext ){` |
|       - |  1691 | ``			/* Empty key (`[ => $v]`) or empty value (`["k" =>]`): PHP rejects`` |
|       - |  1692 | `			 * both. Reject rather than silently emitting unbalanced bytecode. */` |
|     ! 0 |  1693 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1694 | `				"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1695 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1696 | `		}` |
|       - |  1697 | `		/* DUP the source array (it is on the stack top) */` |
|      31 |  1698 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1699 | `		/* Compile the key expression; it is pushed above the DUP'd source */` |
|      31 |  1700 | `		rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pArrow,EXPR_FLAG_RDONLY_LOAD,0);` |
|      31 |  1701 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1702 | `			return SXERR_ABORT;` |
|       - |  1703 | `		}` |
|       - |  1704 | `		/* LOAD_IDX: pop the key, replace the DUP'd source with source[key].` |
|       - |  1705 | `		 * iP2=0 is a read context: a missing key loads NULL silently, matching a` |
|       - |  1706 | ``		 * normal `$arr[$k]` read. (PHP also emits an "Undefined array key"`` |
|       - |  1707 | `		 * warning here; PHL omits it, like its other subscript reads — §3.7.) */` |
|      31 |  1708 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,0,0,0);` |
|      31 |  1709 | `		if( pTarget < pNext && ( (pTarget->nType & PH7_TK_OSB)` |
|      28 |  1710 | `			\|\| ( (pTarget->nType & PH7_TK_KEYWORD)` |
|      15 |  1711 | `				&& SX_PTR_TO_INT(pTarget->pUserData) == PH7_TKWRD_LIST ) ) ){` |
|       - |  1712 | `			/* Nested destructuring:  ["k" => [ ... ]]  or  ["k" => list( ... )].` |
|       - |  1713 | `			 * Treat source[key] as the inner body's source, then drop the` |
|       - |  1714 | `			 * leftover it leaves behind (mirrors the positional nested path). */` |
|       5 |  1715 | `			sxi32 isShort = (pTarget->nType & PH7_TK_OSB) != 0;` |
|       5 |  1716 | `			SyToken *pSavedIn = pGen->pIn;` |
|       5 |  1717 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       5 |  1718 | `			pGen->pIn = pTarget;` |
|       5 |  1719 | `			pGen->pEnd = pNext;` |
|       5 |  1720 | `			rc = isShort ? PH7_CompileShortList(&(*pGen),0)` |
|       2 |  1721 | `			             : PH7_CompileList(&(*pGen),0);` |
|       5 |  1722 | `			pGen->pIn = pSavedIn;` |
|       5 |  1723 | `			pGen->pEnd = pSavedEnd;` |
|       5 |  1724 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1725 | `				return SXERR_ABORT;` |
|       - |  1726 | `			}` |
|       5 |  1727 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       3 |  1728 | `		}else{` |
|       - |  1729 | `			/* Simple lvalue target ($v / $o->p / $a[i] / Cls::$s). source[key]` |
|       - |  1730 | `			 * is already on the stack as the value; compiling the target appends` |
|       - |  1731 | `			 * its lvalue-load, which we fold into a STORE just as a normal` |
|       - |  1732 | `			 * assignment does. */` |
|       - |  1733 | `			VmInstr *pInstr;` |
|      27 |  1734 | `			sxi32 iVmOp = PH7_OP_STORE;` |
|      27 |  1735 | `			sxi32 iP1 = 0, iP2 = 0;` |
|      27 |  1736 | `			void *p3 = 0;` |
|      27 |  1737 | `			rc = GenStateCompileArrayEntry(&(*pGen),pTarget,pNext,` |
|       - |  1738 | `				EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|      27 |  1739 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  1740 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1741 | `			}` |
|      27 |  1742 | `			if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|      27 |  1743 | `				if( pInstr->iOp == PH7_OP_MEMBER ){` |
|       3 |  1744 | `					iP2 = 1; /* member store: keep MEMBER, store value below it */` |
|      26 |  1745 | `				}else if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       3 |  1746 | `					iVmOp = PH7_OP_STORE_IDX;` |
|       3 |  1747 | `					iP1 = pInstr->iP1;` |
|       3 |  1748 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       2 |  1749 | `				}else{` |
|      23 |  1750 | `					p3 = pInstr->p3; /* named store: $v = value */` |
|      23 |  1751 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  1752 | `				}` |
|      13 |  1753 | `			}` |
|      27 |  1754 | `			PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|       - |  1755 | `			/* STORE leaves the assigned value on the stack top; drop it so the` |
|       - |  1756 | `			 * source array is back on top for the next entry. */` |
|      27 |  1757 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       - |  1758 | `		}` |
|      31 |  1759 | `		pGen->pIn = &pNext[1];` |
|       1 |  1760 | `	}` |
|      23 |  1761 | `	return SXRET_OK;` |
|      12 |  1762 |  |
|       - |  1763 | `/*` |
|       - |  1764 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1765 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1766 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1767 | ` */` |
|      98 |  1768 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       4 |  1769 |  |
|       - |  1770 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1771 | `	SyToken *pNext;` |
|       - |  1772 | `	SyToken *pClassifyIn;` |
|     102 |  1773 | `	sxi32 nKeyed = 0, nPositional = 0, nEmpty = 0;` |
|       - |  1774 | `	sxi32 nExpr;` |
|       - |  1775 | `	sxi32 rc;` |
|       - |  1776 | ``	/* First pass: classify entries as keyed (`k => v`), positional, or empty`` |
|       - |  1777 | `	 * skip slots ([,]). A list level must be entirely keyed or entirely` |
|       - |  1778 | `	 * positional — PHP fatals on a mix, and on an empty slot inside a keyed` |
|       - |  1779 | `	 * list. */` |
|     102 |  1780 | `	pClassifyIn = pGen->pIn;` |
|     290 |  1781 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     192 |  1782 | `		if( pGen->pIn >= pNext ){` |
|      13 |  1783 | `			nEmpty++;` |
|     186 |  1784 | `		}else if( GenStateFindTopLevelArrow(pGen->pIn,pNext) < pNext ){` |
|      31 |  1785 | `			nKeyed++;` |
|      16 |  1786 | `		}else{` |
|     150 |  1787 | `			nPositional++;` |
|       - |  1788 | `		}` |
|     192 |  1789 | `		pGen->pIn = &pNext[1];` |
|       4 |  1790 | `	}` |
|     102 |  1791 | `	pGen->pIn = pClassifyIn;` |
|     102 |  1792 | `	if( nKeyed > 0 && nEmpty > 0 ){` |
|     ! 0 |  1793 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1794 | `			"Cannot use empty array entries in keyed array assignment");` |
|     ! 0 |  1795 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1796 | `	}` |
|     102 |  1797 | `	if( nKeyed > 0 && nPositional > 0 ){` |
|     ! 0 |  1798 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  1799 | `			"Cannot mix keyed and unkeyed array entries in assignments");` |
|     ! 0 |  1800 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|       - |  1801 | `	}` |
|     102 |  1802 | `	if( nKeyed > 0 ){` |
|      23 |  1803 | `		return GenStateCompileKeyedListBody(pGen);` |
|       - |  1804 | `	}` |
|      80 |  1805 | `	nExpr = 0;` |
|      80 |  1806 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     238 |  1807 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     162 |  1808 | `		if( pGen->pIn < pNext ){` |
|       - |  1809 | `			/* Check for nested list() */` |
|     150 |  1810 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1811 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1812 | `				/* Record this nested list for post-processing */` |
|       3 |  1813 | `				SyToken *pListEnd = 0;` |
|       3 |  1814 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1815 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1816 | `				}` |
|       3 |  1817 | `				if( pListEnd ){` |
|       - |  1818 | `					struct NestedListEntry sEntry;` |
|       3 |  1819 | `					sEntry.nIndex = nExpr;` |
|       3 |  1820 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1821 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1822 | `					sEntry.isShort = 0;` |
|       3 |  1823 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1824 | `				}` |
|       - |  1825 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1826 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     149 |  1827 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1828 | `				/* Nested short destructuring [...] */` |
|      13 |  1829 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1830 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1831 | `				if( pBracketEnd ){` |
|       - |  1832 | `					struct NestedListEntry sEntry;` |
|      13 |  1833 | `					sEntry.nIndex = nExpr;` |
|      13 |  1834 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1835 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1836 | `					sEntry.isShort = 1;` |
|      13 |  1837 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1838 | `				}` |
|       - |  1839 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1840 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1841 | `			}else{` |
|       - |  1842 | `				/* Compile the expression holding the variable */` |
|     136 |  1843 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     136 |  1844 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1845 | `					SySetRelease(&sNested);` |
|     ! 0 |  1846 | `					return SXRET_OK;` |
|       - |  1847 | `				}` |
|       - |  1848 | `			}` |
|      77 |  1849 | `		}else{` |
|       - |  1850 | `			/* Empty entry,load NULL */` |
|      13 |  1851 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1852 | `		}` |
|     162 |  1853 | `		nExpr++;` |
|       - |  1854 | `		/* Advance the stream cursor */` |
|     162 |  1855 | `		pGen->pIn = &pNext[1];` |
|       4 |  1856 | `	}` |
|       - |  1857 | `	/* Emit the LOAD_LIST instruction */` |
|      80 |  1858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1859 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1860 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1861 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1862 | `	 */` |
|      80 |  1863 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1864 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1865 | `		sxu32 i;` |
|      27 |  1866 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1867 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1868 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1869 | `			ph7_value *pIdx;` |
|       - |  1870 | `			sxu32 nConstIdx;` |
|       - |  1871 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1872 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1873 | `			/* Push the integer index for this nested entry */` |
|      15 |  1874 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1875 | `			if( pIdx == 0 ){` |
|     ! 0 |  1876 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1877 | `				SySetRelease(&sNested);` |
|     ! 0 |  1878 | `				return SXERR_ABORT;` |
|       - |  1879 | `			}` |
|      15 |  1880 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1881 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1882 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1883 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1884 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1885 | `			 */` |
|      15 |  1886 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1887 | `			/* Recursively compile the inner list */` |
|      15 |  1888 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1889 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1890 | `			if( apNested[i].isShort ){` |
|      13 |  1891 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1892 | `			}else{` |
|       3 |  1893 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1894 | `			}` |
|      15 |  1895 | `			pGen->pIn = pSavedIn;` |
|      15 |  1896 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1897 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1898 | `				SySetRelease(&sNested);` |
|     ! 0 |  1899 | `				return SXERR_ABORT;` |
|       - |  1900 | `			}` |
|       - |  1901 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1902 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1903 | `		}` |
|       6 |  1904 | `	}` |
|      80 |  1905 | `	SySetRelease(&sNested);` |
|       - |  1906 | `	/* Node successfully compiled */` |
|      80 |  1907 | `	return SXRET_OK;` |
|      53 |  1908 |  |
|      34 |  1909 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1910 |  |
|       - |  1911 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      36 |  1912 | `	pGen->pIn += 2;` |
|      36 |  1913 | `	pGen->pEnd--;` |
|      17 |  1914 | `	SXUNUSED(iCompileFlag);` |
|      36 |  1915 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1916 |  |
|      64 |  1917 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  1918 |  |
|       - |  1919 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      67 |  1920 | `	pGen->pIn++;` |
|      67 |  1921 | `	pGen->pEnd--;` |
|      32 |  1922 | `	SXUNUSED(iCompileFlag);` |
|      67 |  1923 | `	return GenStateCompileListBody(pGen);` |
|       3 |  1924 |  |
|       - |  1925 | `/* Forward declarations */` |
|       - |  1926 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1927 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1928 | `static int GenStateIsReadonly(SyToken *pTok);` |
|       - |  1929 | `static sxi32 GenStateValidateMemberType(ph7_gen_state *pGen,ph7_class *pClass,const SyString *pMemberName,` |
|       - |  1930 | `	sxu32 nType,const SyString *pTypeClass,const SyString *pTypeText,SySet *pUnionAlts,const char *zErrFmt,sxu32 nLine);` |
|       - |  1931 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1932 | `/*` |
|       - |  1933 | ` * Compile an annoynmous function or a closure.` |
|       - |  1934 | ` * According to the PHP language reference` |
|       - |  1935 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1936 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1937 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1938 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1939 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1940 | ` *  Example Anonymous function variable assignment example` |
|       - |  1941 | ` * <?php` |
|       - |  1942 | ` * $greet = function($name)` |
|       - |  1943 | ` * {` |
|       - |  1944 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1945 | ` * };` |
|       - |  1946 | ` * $greet('World');` |
|       - |  1947 | ` * $greet('PHP');` |
|       - |  1948 | ` * ?>` |
|       - |  1949 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1950 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1951 | ` */` |
|     256 |  1952 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1953 |  |
|       - |  1954 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1955 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1956 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1957 | `							  * one thread is allowed to compile the script.` |
|       - |  1958 | `						      */` |
|       - |  1959 | `	ph7_value *pObj;` |
|       - |  1960 | `	SyString sName;` |
|       - |  1961 | `	sxu32 nIdx;` |
|       - |  1962 | `	sxu32 nLen;` |
|       - |  1963 | `	sxi32 rc;` |
|       - |  1964 |  |
|     261 |  1965 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     261 |  1966 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1967 | `		pGen->pIn++;` |
|     ! 0 |  1968 | `	}` |
|       - |  1969 | `	/* Reserve a constant for the lambda */` |
|     261 |  1970 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     261 |  1971 | `	if( pObj == 0 ){` |
|     ! 0 |  1972 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1973 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1974 | `		return SXERR_ABORT;` |
|       - |  1975 | `	}` |
|       - |  1976 | `	/* Generate a unique name */` |
|     261 |  1977 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1978 | `	/* Make sure the generated name is unique */` |
|     261 |  1979 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1980 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1981 | `	}` |
|     261 |  1982 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     261 |  1983 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1984 | `	/* Compile the lambda body */` |
|     261 |  1985 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     261 |  1986 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1987 | `		return SXERR_ABORT;` |
|       - |  1988 | `	}` |
|     261 |  1989 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1990 | `		/* Emit the load closure instruction */` |
|      21 |  1991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      13 |  1992 | `	}else{` |
|       - |  1993 | `		/* Emit the load constant instruction */` |
|     245 |  1994 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1995 | `	}` |
|       - |  1996 | `	/* Node successfully compiled */` |
|     261 |  1997 | `	return SXRET_OK;` |
|     133 |  1998 |  |
|       - |  1999 | `/*` |
|       - |  2000 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  2001 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  2002 | ` * enclosing arrow level, or has already been captured.` |
|       - |  2003 | ` */` |
|     150 |  2004 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  2005 | `	ph7_gen_state *pGen,` |
|       - |  2006 | `	ph7_vm_func *pFunc,` |
|       - |  2007 | `	const char *zName,` |
|       - |  2008 | `	sxu32 nByte,` |
|       - |  2009 | `	SyString *aShadow,` |
|       - |  2010 | `	sxu32 nShadow)` |
|       2 |  2011 |  |
|       - |  2012 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2013 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2014 | `	sxu32 n, nEnv;` |
|       - |  2015 | `	char *zDup;` |
|     152 |  2016 | `	if( nByte == 0 ){` |
|     ! 0 |  2017 | `		return SXRET_OK;` |
|       - |  2018 | `	}` |
|     150 |  2019 | `	if( nByte == sizeof("this")-1` |
|      81 |  2020 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2021 | `		return SXRET_OK;` |
|       - |  2022 | `	}` |
|     182 |  2023 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     128 |  2024 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     125 |  2025 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      98 |  2026 | `			return SXRET_OK;` |
|       - |  2027 | `		}` |
|      17 |  2028 | `	}` |
|      53 |  2029 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  2030 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  2031 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2032 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2033 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2034 | `			return SXRET_OK;` |
|       - |  2035 | `		}` |
|      15 |  2036 | `	}` |
|      53 |  2037 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  2038 | `	if( zDup == 0 ){` |
|     ! 0 |  2039 | `		return SXERR_ABORT;` |
|       - |  2040 | `	}` |
|      53 |  2041 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  2042 | `	sEnv.iFlags = 0;` |
|      53 |  2043 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  2044 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  2045 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  2046 | `	return SXRET_OK;` |
|      77 |  2047 |  |
|       - |  2048 | `/*` |
|       - |  2049 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2050 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2051 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2052 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2053 | ` */` |
|      14 |  2054 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2055 | `	ph7_gen_state *pGen,` |
|       - |  2056 | `	ph7_vm_func *pFunc,` |
|       - |  2057 | `	const char *zIn,` |
|       - |  2058 | `	const char *zEnd,` |
|       - |  2059 | `	SyString *aShadow,` |
|       - |  2060 | `	sxu32 nShadow)` |
|       1 |  2061 |  |
|       - |  2062 | `	sxi32 rc;` |
|     159 |  2063 | `	while( zIn < zEnd ){` |
|     145 |  2064 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  2065 | `			zIn++;` |
|     ! 0 |  2066 | `			if( zIn < zEnd ){` |
|     ! 0 |  2067 | `				zIn++;` |
|     ! 0 |  2068 | `			}` |
|     ! 0 |  2069 | `			continue;` |
|       - |  2070 | `		}` |
|     144 |  2071 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  2072 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  2073 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2074 | `			const char *zName;` |
|      13 |  2075 | `			zIn++; /* skip '$' */` |
|      13 |  2076 | `			zName = zIn;` |
|      39 |  2077 | `			while( zIn < zEnd ){` |
|      35 |  2078 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  2079 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2080 | `					zIn++;` |
|     ! 0 |  2081 | `					while( zIn < zEnd` |
|     ! 0 |  2082 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2083 | `						zIn++;` |
|     ! 0 |  2084 | `					}` |
|     ! 0 |  2085 | `					continue;` |
|       - |  2086 | `				}` |
|      35 |  2087 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  2088 | `					break;` |
|       - |  2089 | `				}` |
|      27 |  2090 | `				zIn++;` |
|       1 |  2091 | `			}` |
|      13 |  2092 | `			if( zIn > zName ){` |
|      19 |  2093 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  2094 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  2095 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2096 | `					return SXERR_ABORT;` |
|       - |  2097 | `				}` |
|       6 |  2098 | `			}` |
|      13 |  2099 | `			continue;` |
|       - |  2100 | `		}` |
|     133 |  2101 | `		zIn++;` |
|       1 |  2102 | `	}` |
|      15 |  2103 | `	return SXRET_OK;` |
|       8 |  2104 |  |
|       - |  2105 | `/*` |
|       - |  2106 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2107 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2108 | ` *   - plain $<id> pairs` |
|       - |  2109 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2110 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2111 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2112 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2113 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2114 | ` *     are never mistakenly captured.` |
|       - |  2115 | ` */` |
|     138 |  2116 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2117 | `	ph7_gen_state *pGen,` |
|       - |  2118 | `	ph7_vm_func *pFunc,` |
|       - |  2119 | `	SyToken *pStart,` |
|       - |  2120 | `	SyToken *pEnd,` |
|       - |  2121 | `	SyString *aShadow,` |
|       - |  2122 | `	sxu32 nShadow)` |
|       2 |  2123 |  |
|     140 |  2124 | `	SyToken *pScan = pStart;` |
|       - |  2125 | `	sxi32 rc;` |
|     516 |  2126 | `	while( pScan < pEnd ){` |
|     378 |  2127 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  2128 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  2129 | `				pScan->sData.zString,` |
|      14 |  2130 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  2131 | `				aShadow,nShadow);` |
|      15 |  2132 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2133 | `				return SXERR_ABORT;` |
|       - |  2134 | `			}` |
|      15 |  2135 | `			pScan++;` |
|      15 |  2136 | `			continue;` |
|       - |  2137 | `		}` |
|     364 |  2138 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2139 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2140 | `			SyToken *pFnKw = pScan;` |
|      20 |  2141 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2142 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2143 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2144 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2145 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2146 | `			}` |
|      21 |  2147 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2148 | `				SyToken *pInnerSigStart;` |
|       - |  2149 | `				SyToken *pInnerSigEnd;` |
|       - |  2150 | `				SyToken *pInnerBodyEnd;` |
|       - |  2151 | `				SyString *aInnerShadow;` |
|       - |  2152 | `				sxu32 nInnerShadow;` |
|       - |  2153 | `				sxu32 nInnerParamMax;` |
|       - |  2154 | `				SyToken *p;` |
|       - |  2155 | `				int iNestInner;` |
|      19 |  2156 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2157 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2158 | `					pScan++;` |
|     ! 0 |  2159 | `				}` |
|      19 |  2160 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2161 | `					pScan++;` |
|     ! 0 |  2162 | `					continue;` |
|       - |  2163 | `				}` |
|      19 |  2164 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2165 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2166 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2167 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2168 | `					pScan = pEnd;` |
|     ! 0 |  2169 | `					continue;` |
|       - |  2170 | `				}` |
|       - |  2171 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2172 | `				nInnerParamMax = 0;` |
|      57 |  2173 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2174 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2175 | `						nInnerParamMax++;` |
|       6 |  2176 | `					}` |
|      20 |  2177 | `				}` |
|      19 |  2178 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2179 | `					&pGen->pVm->sAllocator,` |
|      18 |  2180 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2181 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2182 | `					return SXERR_ABORT;` |
|       - |  2183 | `				}` |
|      19 |  2184 | `				nInnerShadow = 0;` |
|      25 |  2185 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2186 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2187 | `				}` |
|      57 |  2188 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2189 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2190 | `						continue;` |
|       - |  2191 | `					}` |
|      13 |  2192 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2193 | `						break;` |
|       - |  2194 | `					}` |
|      13 |  2195 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2196 | `						continue;` |
|       - |  2197 | `					}` |
|      13 |  2198 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2199 | `				}` |
|      19 |  2200 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2201 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2202 | `					pScan++;` |
|     ! 0 |  2203 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2204 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2205 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2206 | `						pScan++;` |
|     ! 0 |  2207 | `					}` |
|     ! 0 |  2208 | `					if( pScan < pEnd` |
|     ! 0 |  2209 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2210 | `						pScan++;` |
|     ! 0 |  2211 | `					}` |
|     ! 0 |  2212 | `				}` |
|      19 |  2213 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2214 | `					pScan++; /* past '=>' */` |
|       9 |  2215 | `				}` |
|      19 |  2216 | `				pInnerBodyEnd = pScan;` |
|      19 |  2217 | `				iNestInner = 0;` |
|     131 |  2218 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2219 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2220 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2221 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2222 | `						break;` |
|       - |  2223 | `					}` |
|     113 |  2224 | `					if( pInnerBodyEnd->nType &` |
|       - |  2225 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2226 | `						iNestInner++;` |
|     112 |  2227 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2228 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2229 | `						iNestInner--;` |
|       1 |  2230 | `					}` |
|     113 |  2231 | `					pInnerBodyEnd++;` |
|       1 |  2232 | `				}` |
|       - |  2233 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2234 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2235 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2236 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2237 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2238 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2239 | `				 *` |
|       - |  2240 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2241 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2242 | `				 * range after the '=' sign. */` |
|       - |  2243 | `				{` |
|      19 |  2244 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2245 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2246 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2247 | `						SyToken *pEq = 0;` |
|      13 |  2248 | `						int iNestArg = 0;` |
|      49 |  2249 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2250 | `							if( iNestArg == 0` |
|      39 |  2251 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2252 | `								break;` |
|       - |  2253 | `							}` |
|      37 |  2254 | `							if( pArgEnd->nType &` |
|       - |  2255 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2256 | `								iNestArg++;` |
|      37 |  2257 | `							}else if( pArgEnd->nType &` |
|       - |  2258 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2259 | `								iNestArg--;` |
|     ! 0 |  2260 | `							}` |
|      36 |  2261 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2262 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2263 | `								pEq = pArgEnd;` |
|       3 |  2264 | `							}` |
|      37 |  2265 | `							pArgEnd++;` |
|       1 |  2266 | `						}` |
|      13 |  2267 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2268 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2269 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2270 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2271 | `								return SXERR_ABORT;` |
|       - |  2272 | `							}` |
|       3 |  2273 | `						}` |
|      13 |  2274 | `						pArgStart = pArgEnd;` |
|      12 |  2275 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2276 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2277 | `							pArgStart++;` |
|       1 |  2278 | `						}` |
|       1 |  2279 | `					}` |
|       - |  2280 | `				}` |
|      28 |  2281 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2282 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2283 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2284 | `					return SXERR_ABORT;` |
|       - |  2285 | `				}` |
|      19 |  2286 | `				pScan = pInnerBodyEnd;` |
|      19 |  2287 | `				continue;` |
|       - |  2288 | `			}` |
|       1 |  2289 | `		}` |
|     346 |  2290 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     208 |  2291 | `			pScan++;` |
|     208 |  2292 | `			continue;` |
|       - |  2293 | `		}` |
|       - |  2294 | `		{` |
|       - |  2295 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     140 |  2296 | `			SyToken *pDollar = pScan;` |
|     207 |  2297 | `			while( &pDollar[1] < pEnd` |
|     140 |  2298 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2299 | `				pDollar++;` |
|     ! 0 |  2300 | `			}` |
|     140 |  2301 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2302 | `				break;` |
|       - |  2303 | `			}` |
|     140 |  2304 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2305 | `				pScan = pDollar + 1;` |
|     ! 0 |  2306 | `				continue;` |
|       - |  2307 | `			}` |
|     209 |  2308 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     138 |  2309 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      69 |  2310 | `				aShadow,nShadow);` |
|     140 |  2311 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2312 | `				return SXERR_ABORT;` |
|       - |  2313 | `			}` |
|     140 |  2314 | `			pScan = pDollar + 2;` |
|       - |  2315 | `		}` |
|       2 |  2316 | `	}` |
|     140 |  2317 | `	return SXRET_OK;` |
|      71 |  2318 |  |
|       - |  2319 | `/*` |
|       - |  2320 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2321 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2322 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2323 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2324 | ` * $this is also made available.` |
|       - |  2325 | ` */` |
|     120 |  2326 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2327 |  |
|       - |  2328 | `	ph7_vm_func *pFunc;` |
|       - |  2329 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2330 | `	GenBlock *pBlock;` |
|       - |  2331 | `	SySet *pInstrContainer;` |
|       - |  2332 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2333 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2334 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2335 | `	SyToken *pSavedEnd;` |
|       - |  2336 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2337 | `	char zName[512];` |
|       - |  2338 | `	static int iCnt = 1;` |
|       - |  2339 | `	char *zDup;` |
|       - |  2340 | `	sxu32 nLen;` |
|       - |  2341 | `	sxu32 nLine;` |
|     124 |  2342 | `	sxi32 iFlags = 0;` |
|     124 |  2343 | `	int bStatic = 0;` |
|       - |  2344 | `	sxi32 rc;` |
|       - |  2345 | `	sxu32 n;` |
|      60 |  2346 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2347 |  |
|     124 |  2348 | `	nLine = pGen->pIn->nLine;` |
|       - |  2349 | `	/* Optional 'static' prefix */` |
|     120 |  2350 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     124 |  2351 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2352 | `		bStatic = 1;` |
|       3 |  2353 | `		pGen->pIn++;` |
|       1 |  2354 | `	}` |
|       - |  2355 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     120 |  2356 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     124 |  2357 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2358 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2359 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2360 | `		return SXERR_SYNTAX;` |
|       - |  2361 | `	}` |
|     124 |  2362 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2363 | `	/* Optional '&' — return by reference */` |
|     124 |  2364 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2365 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2366 | `		pGen->pIn++;` |
|     ! 0 |  2367 | `	}` |
|       - |  2368 | `	/* Expect '(' */` |
|     124 |  2369 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2370 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2371 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2372 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2373 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2374 | `		}else{` |
|     ! 0 |  2375 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2376 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2377 | `		}` |
|       3 |  2378 | `		return SXERR_SYNTAX;` |
|       - |  2379 | `	}` |
|     121 |  2380 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2381 | `	/* Delimit the parameter list */` |
|     121 |  2382 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     121 |  2383 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2384 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2385 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2386 | `		return SXERR_SYNTAX;` |
|       - |  2387 | `	}` |
|       - |  2388 | `	/* Allocate the function state */` |
|     119 |  2389 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     119 |  2390 | `	if( pFunc == 0 ){` |
|     ! 0 |  2391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2392 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2393 | `		return SXERR_ABORT;` |
|       - |  2394 | `	}` |
|       - |  2395 | `	/* Generate a unique lambda name */` |
|     119 |  2396 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     221 |  2397 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     104 |  2398 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2399 | `	}` |
|     119 |  2400 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     119 |  2401 | `	if( zDup == 0 ){` |
|     ! 0 |  2402 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2403 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2404 | `		return SXERR_ABORT;` |
|       - |  2405 | `	}` |
|     119 |  2406 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2407 | `	/* Collect function arguments */` |
|     119 |  2408 | `	if( pGen->pIn < pSigEnd ){` |
|      89 |  2409 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      89 |  2410 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2411 | `			return SXERR_ABORT;` |
|       - |  2412 | `		}` |
|      43 |  2413 | `	}` |
|       - |  2414 | `	/* Point past ')' and parse optional return type */` |
|     119 |  2415 | `	pGen->pIn = &pSigEnd[1];` |
|     119 |  2416 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     119 |  2417 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2418 | `		return SXERR_ABORT;` |
|     119 |  2419 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2420 | `		return SXERR_SYNTAX;` |
|       - |  2421 | `	}` |
|       - |  2422 | `	/* Expect '=>' */` |
|     119 |  2423 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2424 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2425 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2426 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2427 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2428 | `		}else{` |
|     ! 0 |  2429 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2430 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2431 | `		}` |
|       3 |  2432 | `		return SXERR_SYNTAX;` |
|       - |  2433 | `	}` |
|     116 |  2434 | `	pGen->pIn++; /* Jump '=>' */` |
|     116 |  2435 | `	pBodyStart = pGen->pIn;` |
|     116 |  2436 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2437 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2438 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2439 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2440 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     116 |  2441 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2442 | `	{` |
|     116 |  2443 | `		SyString *aShadow = 0;` |
|     116 |  2444 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     116 |  2445 | `		if( nShadow > 0 ){` |
|      86 |  2446 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      84 |  2447 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      86 |  2448 | `			if( aShadow == 0 ){` |
|     ! 0 |  2449 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2450 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2451 | `				return SXERR_ABORT;` |
|       - |  2452 | `			}` |
|     188 |  2453 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     104 |  2454 | `				aShadow[n] = aArgs[n].sName;` |
|      53 |  2455 | `			}` |
|      42 |  2456 | `		}` |
|     173 |  2457 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      57 |  2458 | `			aShadow,nShadow);` |
|     116 |  2459 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2460 | `			return SXERR_ABORT;` |
|       - |  2461 | `		}` |
|       - |  2462 | `	}` |
|       - |  2463 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2464 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2465 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2466 | `	 * $this. */` |
|     116 |  2467 | `	if( !bStatic ){` |
|       - |  2468 | `		char *zThisDup;` |
|     114 |  2469 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     114 |  2470 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2471 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2472 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2473 | `			return SXERR_ABORT;` |
|       - |  2474 | `		}` |
|     114 |  2475 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     114 |  2476 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     114 |  2477 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     114 |  2478 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     114 |  2479 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      56 |  2480 | `	}` |
|       - |  2481 | `	/* Arrow functions are always closures */` |
|     116 |  2482 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2483 | `	/* Compile the body expression as an implicit return */` |
|     173 |  2484 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      57 |  2485 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     116 |  2486 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2487 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2488 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2489 | `		return SXERR_ABORT;` |
|       - |  2490 | `	}` |
|     116 |  2491 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     116 |  2492 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     116 |  2493 | `	pSavedEnd = pGen->pEnd;` |
|     116 |  2494 | `	pGen->pIn = pBodyStart;` |
|     116 |  2495 | `	pGen->pEnd = pBodyEnd;` |
|     116 |  2496 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     116 |  2497 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2498 | `		return SXERR_ABORT;` |
|       - |  2499 | `	}` |
|       - |  2500 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2501 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2502 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2503 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     116 |  2504 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     116 |  2505 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     116 |  2506 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     116 |  2507 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     116 |  2508 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2509 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     116 |  2510 | `	pGen->pIn = pBodyEnd;` |
|     116 |  2511 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2512 | `	/* Emit the load-closure instruction */` |
|     116 |  2513 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     116 |  2514 | `	return SXRET_OK;` |
|      64 |  2515 |  |
|       - |  2516 | `/*` |
|       - |  2517 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2518 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2519 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2520 | ` * expression's value.` |
|       - |  2521 | ` */` |
|     346 |  2522 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2523 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2524 |  |
|       - |  2525 | `	SySet *pInstrContainer;` |
|       - |  2526 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2527 | `	GenBlock *pArmBlock;` |
|       - |  2528 | `	sxi32 rc;` |
|     349 |  2529 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2530 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2531 | `	pGen->pIn  = pStart;` |
|     349 |  2532 | `	pGen->pEnd = pStop;` |
|     349 |  2533 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2534 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2535 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2536 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2537 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2538 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2539 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2540 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2541 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2542 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2543 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2544 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2545 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2546 | `		return SXERR_ABORT;` |
|       - |  2547 | `	}` |
|     349 |  2548 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2549 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2550 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2551 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2552 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2553 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2554 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2555 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2556 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2557 | `		return SXERR_ABORT;` |
|       - |  2558 | `	}` |
|     349 |  2559 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2560 | `		return SXERR_EMPTY;` |
|       - |  2561 | `	}` |
|     349 |  2562 | `	return SXRET_OK;` |
|     176 |  2563 |  |
|       - |  2564 | `/*` |
|       - |  2565 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2566 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2567 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2568 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2569 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2570 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2571 | ` */` |
|       - |  2572 | `/*` |
|       - |  2573 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2574 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2575 | ` * caller can bail out of the current expression.` |
|       - |  2576 | ` */` |
|       2 |  2577 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2578 |  |
|       - |  2579 | `	va_list ap;` |
|       - |  2580 | `	sxi32 rc;` |
|       - |  2581 | `	SyBlob sMsg;` |
|       3 |  2582 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2583 | `	va_start(ap,zFmt);` |
|       3 |  2584 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2585 | `	va_end(ap);` |
|       3 |  2586 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2587 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2588 | `	SyBlobRelease(&sMsg);` |
|       3 |  2589 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2590 | `		return SXERR_ABORT;` |
|       - |  2591 | `	}` |
|       3 |  2592 | `	return SXERR_SYNTAX;` |
|       2 |  2593 |  |
|       - |  2594 | `/*` |
|       - |  2595 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2596 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2597 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2598 | ` */` |
|     348 |  2599 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2600 |  |
|     352 |  2601 | `	SyToken *pCur = pStart;` |
|     352 |  2602 | `	int iNest = 0;` |
|     814 |  2603 | `	while( pCur < pEnd ){` |
|     780 |  2604 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2605 | `			iNest++;` |
|     774 |  2606 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2607 | `			iNest--;` |
|     762 |  2608 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2609 | `			return pCur;` |
|       - |  2610 | `		}` |
|     466 |  2611 | `		pCur++;` |
|       4 |  2612 | `	}` |
|      37 |  2613 | `	return pEnd;` |
|     178 |  2614 |  |
|      70 |  2615 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2616 |  |
|       - |  2617 | `	ph7_match *pMatch;` |
|       - |  2618 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2619 | `	int bHasDefault = 0;` |
|       - |  2620 | `	sxu32 nLine;` |
|       - |  2621 | `	sxi32 rc;` |
|      35 |  2622 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2623 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2624 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2625 | `	/* Expect '(' */` |
|      75 |  2626 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2627 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2628 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2629 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2630 | `	}` |
|      75 |  2631 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2632 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2633 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2634 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2635 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2636 | `	}` |
|      75 |  2637 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2638 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2639 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2640 | `	}` |
|       - |  2641 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2642 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2643 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2644 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2645 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2646 | `		return SXERR_ABORT;` |
|       - |  2647 | `	}` |
|      75 |  2648 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2649 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2650 | `	/* Expect '{' */` |
|      75 |  2651 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2652 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2653 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2654 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2655 | `	}` |
|      75 |  2656 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2657 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2658 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2659 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2660 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2661 | `	}` |
|       - |  2662 | `	/* Allocate ph7_match container */` |
|      75 |  2663 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2664 | `	if( pMatch == 0 ){` |
|     ! 0 |  2665 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2666 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2667 | `		return SXERR_ABORT;` |
|       - |  2668 | `	}` |
|      75 |  2669 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2670 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2671 | `	/* Iterate arms */` |
|     253 |  2672 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2673 | `		ph7_match_arm sArm;` |
|       - |  2674 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2675 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2676 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2677 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2678 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2679 | `		/* 'default' arm? */` |
|     182 |  2680 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2681 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2682 | `			if( bHasDefault ){` |
|       3 |  2683 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2684 | `					"Match expressions may only contain one default arm");` |
|       4 |  2685 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2686 | `			}` |
|      20 |  2687 | `			sArm.bDefault = 1;` |
|      20 |  2688 | `			bHasDefault = 1;` |
|      20 |  2689 | `			pGen->pIn++;` |
|      20 |  2690 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2691 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2692 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2693 | `			}` |
|      20 |  2694 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2695 | `		}else{` |
|       - |  2696 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2697 | `			pCondStart = pGen->pIn;` |
|     166 |  2698 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2699 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2700 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2701 | `				SySet sCondBc;` |
|       9 |  2702 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2703 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2704 | `						"syntax error, empty match condition expression");` |
|       - |  2705 | `				}` |
|       9 |  2706 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2707 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2708 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2709 | `					return SXERR_ABORT;` |
|       - |  2710 | `				}` |
|       9 |  2711 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2712 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2713 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2714 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2715 | `			}` |
|     166 |  2716 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2717 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2718 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2719 | `			}` |
|     163 |  2720 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2721 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2722 | `					"syntax error, empty match condition expression");` |
|       - |  2723 | `			}` |
|       - |  2724 | `			{` |
|       - |  2725 | `				SySet sCondBc;` |
|     163 |  2726 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2727 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2728 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2729 | `					return SXERR_ABORT;` |
|       - |  2730 | `				}` |
|     163 |  2731 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2732 | `			}` |
|     163 |  2733 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2734 | `		}` |
|       - |  2735 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2736 | `		pResStart = pGen->pIn;` |
|     181 |  2737 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2738 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2739 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2740 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2741 | `		}` |
|     181 |  2742 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2743 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2744 | `			return SXERR_ABORT;` |
|       - |  2745 | `		}` |
|     181 |  2746 | `		pGen->pIn = pResEnd;` |
|     181 |  2747 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2748 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2749 | `		}` |
|     181 |  2750 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2751 | `	}` |
|      69 |  2752 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2753 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2754 | `	return SXRET_OK;` |
|      40 |  2755 |  |
|       - |  2756 | `/*` |
|       - |  2757 | ` * Compile a backtick quoted string.` |
|       - |  2758 | ` */` |
|       4 |  2759 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2760 |  |
|       - |  2761 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2762 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2763 | `	 */` |
|       8 |  2764 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2765 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2766 | `		ph7_lib_version()` |
|       - |  2767 | `		);` |
|       - |  2768 | `	/* Load NULL */` |
|       6 |  2769 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2770 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2771 | `	/* Node successfully compiled */` |
|       6 |  2772 | `	return SXRET_OK;` |
|       2 |  2773 |  |
|       - |  2774 | `/*` |
|       - |  2775 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2776 | ` * construct.` |
|       - |  2777 | ` */` |
|      80 |  2778 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2779 |  |
|       - |  2780 | `	SyString *pName;` |
|       - |  2781 | `	sxu32 nKeyID;` |
|       - |  2782 | `	sxi32 rc;` |
|       - |  2783 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2784 | `	pName = &pGen->pIn->sData;` |
|      85 |  2785 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2786 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2787 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2788 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2789 | `		/* Compile arguments one after one */` |
|       9 |  2790 | `		pTmp = pGen->pEnd;` |
|       - |  2791 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2792 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2793 | `		 *  mean that the following expression is valid:` |
|       - |  2794 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2795 | `		 */` |
|       9 |  2796 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2797 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2798 | `			if( pGen->pIn < pNext ){` |
|       9 |  2799 | `				pGen->pEnd = pNext;` |
|       9 |  2800 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2801 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2802 | `					return SXERR_ABORT;` |
|       - |  2803 | `				}` |
|       9 |  2804 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2805 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2806 | `					 * without the overhead of a function call.` |
|       - |  2807 | `					 * This is a very powerful optimization that improve` |
|       - |  2808 | `					 * performance greatly.` |
|       - |  2809 | `					 */` |
|       9 |  2810 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2811 | `				}` |
|       4 |  2812 | `			}` |
|       - |  2813 | `			/* Jump trailing commas */` |
|       9 |  2814 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2815 | `				pNext++;` |
|     ! 0 |  2816 | `			}` |
|       9 |  2817 | `			pGen->pIn = pNext;` |
|       1 |  2818 | `		}` |
|       - |  2819 | `		/* Restore token stream */` |
|       9 |  2820 | `		pGen->pEnd = pTmp;` |
|       5 |  2821 | `	}else{` |
|      77 |  2822 | `		sxi32 nArg = 0;` |
|      77 |  2823 | `		sxu32 nIdx = 0;` |
|      77 |  2824 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2825 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2826 | `			return SXERR_ABORT;` |
|      77 |  2827 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2828 | `			nArg = 1;` |
|      36 |  2829 | `		}` |
|      77 |  2830 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2831 | `			ph7_value *pObj;` |
|       - |  2832 | `			/* Emit the call instruction */` |
|      29 |  2833 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2834 | `			if( pObj == 0 ){` |
|     ! 0 |  2835 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2836 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2837 | `				return SXERR_ABORT;` |
|       - |  2838 | `			}` |
|      29 |  2839 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2840 | `			/* Install in the literal table */` |
|      29 |  2841 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2842 | `		}` |
|       - |  2843 | `		/* Emit the call instruction */` |
|      77 |  2844 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2845 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2846 | `	}` |
|       - |  2847 | `	/* Node successfully compiled */` |
|      85 |  2848 | `	return SXRET_OK;` |
|      45 |  2849 |  |
|       - |  2850 | `/*` |
|       - |  2851 | ` * Compile a node holding a variable declaration.` |
|       - |  2852 | ` * According to the PHP language reference` |
|       - |  2853 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2854 | ` *  The variable name is case-sensitive.` |
|       - |  2855 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2856 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2857 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2858 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2859 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2860 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2861 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2862 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2863 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2864 | ` *  the chapter on Expressions.` |
|       - |  2865 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2866 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2867 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2868 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2869 | ` *  is being assigned (the source variable).` |
|       - |  2870 | ` */` |
| 1099010 |  2871 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2872 |  |
| 1099015 |  2873 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2874 | `	sxi32 iVv;` |
|       - |  2875 | `	sxi32 iP1;` |
|       - |  2876 | `	void *p3;` |
|       - |  2877 | `	sxi32 rc;` |
| 1099015 |  2878 | `	iVv = -1; /* Variable variable counter */` |
| 2198037 |  2879 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1099027 |  2880 | `		pGen->pIn++;` |
| 1099027 |  2881 | `		iVv++;` |
|       5 |  2882 | `	}` |
| 1099015 |  2883 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2884 | `		/* Invalid variable name */` |
|     ! 0 |  2885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2886 | `		if( rc == SXERR_ABORT ){` |
|       - |  2887 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2888 | `			return SXERR_ABORT;` |
|       - |  2889 | `		}` |
|     ! 0 |  2890 | `		return SXRET_OK;` |
|       - |  2891 | `	}` |
| 1099015 |  2892 | `	p3  = 0;` |
| 1099015 |  2893 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2894 | `		/* Dynamic variable creation */` |
|      19 |  2895 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2896 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2897 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2898 | `			/* Empty expression */` |
|       3 |  2899 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2900 | `			return SXRET_OK;` |
|       - |  2901 | `		}` |
|       - |  2902 | `		/* Compile the expression holding the variable name */` |
|      16 |  2903 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2904 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2905 | `			return SXERR_ABORT;` |
|      16 |  2906 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2907 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2908 | `			return SXRET_OK;` |
|       - |  2909 | `		}` |
|       7 |  2910 | `	}else{` |
|       - |  2911 | `		SyHashEntry *pEntry;` |
|       - |  2912 | `		SyString *pName;` |
| 1098999 |  2913 | `		char *zName = 0;` |
|       - |  2914 | `		/* Extract variable name */` |
| 1098999 |  2915 | `		pName = &pGen->pIn->sData;` |
|       - |  2916 | `		/* Advance the stream cursor */` |
| 1098999 |  2917 | `		pGen->pIn++;` |
| 1098999 |  2918 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1098999 |  2919 | `		if( pEntry == 0 ){` |
|       - |  2920 | `			/* Duplicate name */` |
|  147573 |  2921 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  147573 |  2922 | `			if( zName == 0 ){` |
|     ! 0 |  2923 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2924 | `				return SXERR_ABORT;` |
|       - |  2925 | `			}` |
|       - |  2926 | `			/* Install in the hashtable */` |
|  147573 |  2927 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   73789 |  2928 | `		}else{` |
|       - |  2929 | `			/* Name already available */` |
|  951431 |  2930 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2931 | `		}` |
| 1098999 |  2932 | `		p3 = (void *)zName;` |
|       - |  2933 | `	}` |
| 1099011 |  2934 | `	iP1 = 0;` |
| 1099011 |  2935 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  400003 |  2936 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2937 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  399985 |  2938 | `			iP1 = 1;` |
|  199990 |  2939 | `		}` |
|  199999 |  2940 | `	}` |
|       - |  2941 | `	/* Emit the load instruction */` |
| 1099011 |  2942 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1099023 |  2943 | `	while( iVv > 0 ){` |
|      13 |  2944 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2945 | `		iVv--;` |
|       1 |  2946 | `	}` |
|       - |  2947 | `	/* Node successfully compiled */` |
| 1099011 |  2948 | `	return SXRET_OK;` |
|  549510 |  2949 |  |
|       - |  2950 | `/*` |
|       - |  2951 | ` * Load a literal.` |
|       - |  2952 | ` */` |
|  773988 |  2953 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2954 |  |
|  773993 |  2955 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2956 | `	ph7_value *pObj;` |
|       - |  2957 | `	SyString *pStr;` |
|       - |  2958 | `	sxu32 nIdx;` |
|       - |  2959 | `	/* Extract token value */` |
|  773993 |  2960 | `	pStr = &pToken->sData;` |
|       - |  2961 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  773993 |  2962 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  164083 |  2963 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2964 | `			/* NULL constant are always indexed at 0 */` |
|   60449 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   60449 |  2966 | `			return SXRET_OK;` |
|  103639 |  2967 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2968 | `			/* TRUE constant are always indexed at 1 */` |
|     671 |  2969 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     671 |  2970 | `			return SXRET_OK;` |
|       5 |  2971 | `		}` |
|  722666 |  2972 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  122534 |  2973 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2974 | `			/* FALSE constant are always indexed at 2 */` |
|   46345 |  2975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   46345 |  2976 | `			return SXRET_OK;` |
|  618558 |  2977 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  109966 |  2978 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2979 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10547 |  2980 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10547 |  2981 | `			if( pObj == 0 ){` |
|     ! 0 |  2982 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2983 | `				return SXERR_ABORT;` |
|       - |  2984 | `			}` |
|   10547 |  2985 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2986 | `			/* Emit the load constant instruction */` |
|   10547 |  2987 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10547 |  2988 | `			return SXRET_OK;` |
|  570809 |  2989 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   35552 |  2990 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2991 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       8 |  2992 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       8 |  2993 | `			if( pObj == 0 ){` |
|     ! 0 |  2994 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2995 | `				return SXERR_ABORT;` |
|       - |  2996 | `			}` |
|       8 |  2997 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2998 | `				SyString sNs;` |
|       8 |  2999 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  3000 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 |  3001 | `			}else{` |
|     ! 0 |  3002 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  3003 | `			}` |
|       8 |  3004 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       8 |  3005 | `			return SXRET_OK;` |
|  569865 |  3006 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   14811 |  3007 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  562455 |  3008 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   18880 |  3009 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  3010 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  3011 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3012 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3013 | `				/* Point to the upper block */` |
|      11 |  3014 | `				pBlock = pBlock->pParent;` |
|       1 |  3015 | `			}` |
|      11 |  3016 | `			if( pBlock == 0 ){` |
|       - |  3017 | `				/* Called in the global scope,load NULL */` |
|       5 |  3018 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3019 | `			}else{` |
|       - |  3020 | `				/* Extract the target function/method */` |
|       7 |  3021 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3022 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3023 | `					/* Not a class method,Load null */` |
|       3 |  3024 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3025 | `				}else{` |
|       5 |  3026 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3027 | `					if( pObj == 0 ){` |
|     ! 0 |  3028 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3029 | `						return SXERR_ABORT;` |
|       - |  3030 | `					}` |
|       5 |  3031 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3032 | `					/* Emit the load constant instruction */` |
|       5 |  3033 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3034 | `				}` |
|       - |  3035 | `			}` |
|      11 |  3036 | `			return SXRET_OK;` |
|       - |  3037 | `	}` |
|       - |  3038 | `	/* Query literal table */` |
|  655985 |  3039 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3040 | `		ph7_value *pLitObj;` |
|       - |  3041 | `		/* Unknown literal,install it in the literal table */` |
|  272649 |  3042 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  272649 |  3043 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3044 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3045 | `			return SXERR_ABORT;` |
|       - |  3046 | `		}` |
|  272649 |  3047 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  272649 |  3048 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  136322 |  3049 | `	}` |
|       - |  3050 | `	/* Emit the load constant instruction */` |
|  655985 |  3051 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  655985 |  3052 | `	return SXRET_OK;` |
|  386999 |  3053 |  |
|       - |  3054 | `/*` |
|       - |  3055 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3056 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3057 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3058 | ` * Otherwise, load the simple literal directly.` |
|       - |  3059 | ` */` |
|  774028 |  3060 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3061 |  |
|       - |  3062 | `	sxi32 rc;` |
|  774033 |  3063 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3064 | `		return SXRET_OK;` |
|       - |  3065 | `	}` |
|       - |  3066 | `	/* Check if this is a multi-token namespace path */` |
|  774033 |  3067 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3068 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      45 |  3069 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      45 |  3070 | `		int isAbsolute = 0;` |
|      45 |  3071 | `		SyBlobReset(pWorker);` |
|       - |  3072 | `		/* Check for leading backslash (absolute path) */` |
|      45 |  3073 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      43 |  3074 | `			isAbsolute = 1;` |
|      43 |  3075 | `			pGen->pIn++; /* Skip leading backslash */` |
|      19 |  3076 | `		}` |
|       - |  3077 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      45 |  3078 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3079 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3080 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3081 | `		}` |
|       - |  3082 | `		/* Collect all path components */` |
|     141 |  3083 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     141 |  3084 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3085 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3086 | `			}else{` |
|      93 |  3087 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3088 | `			}` |
|     141 |  3089 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      45 |  3090 | `				pGen->pIn++;` |
|      45 |  3091 | `				break;` |
|       - |  3092 | `			}` |
|     101 |  3093 | `			pGen->pIn++;` |
|       5 |  3094 | `		}` |
|      45 |  3095 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3096 | `			ph7_value *pObj;` |
|       - |  3097 | `			SyString sPath;` |
|       - |  3098 | `			sxu32 nIdx;` |
|      45 |  3099 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3100 | `			/* Install in the literal table */` |
|      45 |  3101 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      20 |  3102 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  3103 | `				if( pObj == 0 ){` |
|     ! 0 |  3104 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3105 | `					return SXERR_ABORT;` |
|       - |  3106 | `				}` |
|      20 |  3107 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      20 |  3108 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  3109 | `			}` |
|       - |  3110 | `			/* Emit the load constant instruction.` |
|       - |  3111 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3112 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      65 |  3113 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      20 |  3114 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      20 |  3115 | `				nIdx,0,0);` |
|      45 |  3116 | `			return SXRET_OK;` |
|       - |  3117 | `		}` |
|     ! 0 |  3118 | `	}` |
|       - |  3119 | `	/* Single-token literal: load directly */` |
|  773993 |  3120 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  773993 |  3121 | `	return rc;` |
|  387019 |  3122 |  |
|       - |  3123 | `/*` |
|       - |  3124 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3125 | ` */` |
|  774028 |  3126 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3127 |  |
|       - |  3128 | `	sxi32 rc;` |
|  774033 |  3129 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  774033 |  3130 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3131 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3132 | `		return rc;` |
|       - |  3133 | `	}` |
|       - |  3134 | `	/* Node successfully compiled */` |
|  774033 |  3135 | `	return SXRET_OK;` |
|  387019 |  3136 |  |
|       - |  3137 | `/*` |
|       - |  3138 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3139 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3140 | ` */` |
|       8 |  3141 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3142 |  |
|       - |  3143 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3144 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3145 | `		pGen->pIn++;` |
|       1 |  3146 | `	}` |
|       9 |  3147 | `	return SXRET_OK;` |
|       1 |  3148 |  |
|       - |  3149 | `/*` |
|       - |  3150 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3151 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3152 | ` */` |
|     106 |  3153 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3154 |  |
|     111 |  3155 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      29 |  3156 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3157 | `			return TRUE;` |
|      27 |  3158 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3159 | `			return TRUE;` |
|       2 |  3160 | `		}` |
|      95 |  3161 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3162 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3163 | `			return TRUE;` |
|       - |  3164 | `		}` |
|     ! 0 |  3165 | `	}` |
|       - |  3166 | `	/* Not a reserved constant */` |
|     103 |  3167 | `	return FALSE;` |
|      58 |  3168 |  |
|       - |  3169 | `/*` |
|       - |  3170 | ` * Compile the 'const' statement.` |
|       - |  3171 | ` * According to the PHP language reference` |
|       - |  3172 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3173 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3174 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3175 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3176 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3177 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3178 | ` *  Syntax` |
|       - |  3179 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3180 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3181 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3182 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3183 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3184 | ` *  to get a list of all defined constants.` |
|       - |  3185 | ` *` |
|       - |  3186 | ` * Symisc eXtension.` |
|       - |  3187 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3188 | ` *  would allow only simple scalar value.` |
|       - |  3189 | ` *  Example` |
|       - |  3190 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3191 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3192 | ` */` |
|      32 |  3193 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3194 |  |
|       - |  3195 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3196 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3197 | `	SyString *pName;` |
|       - |  3198 | `	sxi32 rc;` |
|      37 |  3199 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3200 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3201 | `		/* Invalid constant name */` |
|       8 |  3202 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       8 |  3203 | `		if( rc == SXERR_ABORT ){` |
|       - |  3204 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3205 | `			return SXERR_ABORT;` |
|       - |  3206 | `		}` |
|       8 |  3207 | `		goto Synchronize;` |
|       - |  3208 | `	}` |
|       - |  3209 | `	/* Peek constant name */` |
|      30 |  3210 | `	pName = &pGen->pIn->sData;` |
|       - |  3211 | `	/* Make sure the constant name isn't reserved */` |
|      30 |  3212 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3213 | `		/* Reserved constant */` |
|      10 |  3214 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3215 | `		if( rc == SXERR_ABORT ){` |
|       - |  3216 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3217 | `			return SXERR_ABORT;` |
|       - |  3218 | `		}` |
|      10 |  3219 | `		goto Synchronize;` |
|       - |  3220 | `	}` |
|      21 |  3221 | `	pGen->pIn++;` |
|      21 |  3222 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3223 | `		/* Invalid statement*/` |
|       6 |  3224 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3225 | `		if( rc == SXERR_ABORT ){` |
|       - |  3226 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3227 | `			return SXERR_ABORT;` |
|       - |  3228 | `		}` |
|       6 |  3229 | `		goto Synchronize;` |
|       - |  3230 | `	}` |
|      15 |  3231 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3232 | `	/* Allocate a new constant value container */` |
|      15 |  3233 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3234 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3235 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3236 | `		return SXERR_ABORT;` |
|       - |  3237 | `	}` |
|      15 |  3238 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3239 | `	/* Swap bytecode container */` |
|      15 |  3240 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3241 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3242 | `	/* Compile constant value */` |
|      15 |  3243 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3244 | `	/* Emit the done instruction */` |
|      15 |  3245 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3246 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3247 | `	if( rc == SXERR_ABORT ){` |
|       - |  3248 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3249 | `		return SXERR_ABORT;` |
|       - |  3250 | `	}` |
|      15 |  3251 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3252 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3253 | `	{` |
|       - |  3254 | `		SyBlob sFQN;` |
|       - |  3255 | `		SyString sFQNStr;` |
|      15 |  3256 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3257 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3258 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3259 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3260 | `		SyBlobRelease(&sFQN);` |
|       - |  3261 | `	}` |
|      15 |  3262 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3263 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3264 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3265 | `	}` |
|      15 |  3266 | `	return SXRET_OK;` |
|       9 |  3267 | `Synchronize:` |
|       - |  3268 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3269 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      42 |  3270 | `		pGen->pIn++;` |
|       4 |  3271 | `	}` |
|      22 |  3272 | `	return SXRET_OK;` |
|      21 |  3273 |  |
|       - |  3274 | `/*` |
|       - |  3275 | ` * Compile the 'continue' statement.` |
|       - |  3276 | ` * According to the PHP language reference` |
|       - |  3277 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3278 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3279 | ` *  iteration.` |
|       - |  3280 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3281 | ` *  the purposes of continue.` |
|       - |  3282 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3283 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3284 | ` *  Note:` |
|       - |  3285 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3286 | ` */` |
|       - |  3287 | `/*` |
|       - |  3288 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3289 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3290 | ` * break/continue crosses a try boundary.` |
|       - |  3291 | ` *` |
|       - |  3292 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3293 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3294 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3295 | ` */` |
|    3652 |  3296 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3297 |  |
|    3657 |  3298 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   21425 |  3299 | `	while( pBlock && pBlock != pTarget ){` |
|   17773 |  3300 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3301 | `			if( pBlock->pUserData ){` |
|       - |  3302 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3303 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3304 | `			}else{` |
|       - |  3305 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3306 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3307 | `				 * exception context from a sub-execution.` |
|       - |  3308 | `				 */` |
|     ! 0 |  3309 | `				break;` |
|       - |  3310 | `			}` |
|       1 |  3311 | `		}` |
|   17773 |  3312 | `		pBlock = pBlock->pParent;` |
|       5 |  3313 | `	}` |
|    3657 |  3314 |  |
|    3556 |  3315 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3316 |  |
|       - |  3317 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3318 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3319 | `	sxu32 nLineLocal;` |
|       - |  3320 | `	sxi32 rc;` |
|    3561 |  3321 | `	nLineLocal = pGen->pIn->nLine;` |
|    3561 |  3322 | `	iLevel = 0;` |
|       - |  3323 | `	/* Jump the 'continue' keyword */` |
|    3561 |  3324 | `	pGen->pIn++;` |
|    3561 |  3325 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3326 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3327 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3328 | `		 */` |
|       - |  3329 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3330 | `		char *zAlloc = 0;` |
|       - |  3331 | `		SyString sNum;` |
|      17 |  3332 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3333 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3334 | `			return SXERR_ABORT;` |
|       - |  3335 | `		}` |
|      17 |  3336 | `		if( rc == SXRET_OK ){` |
|      20 |  3337 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3338 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3339 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3340 | `				return SXERR_ABORT;` |
|       - |  3341 | `			}` |
|      14 |  3342 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3343 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3344 | `		}` |
|      17 |  3345 | `		if( iLevel < 2 ){` |
|       3 |  3346 | `			iLevel = 0;` |
|       1 |  3347 | `		}` |
|      17 |  3348 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3349 | `	}` |
|       - |  3350 | `	/* Point to the target loop */` |
|    3561 |  3351 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3561 |  3352 | `	if( pLoop == 0 ){` |
|       - |  3353 | `		/* Illegal continue */` |
|      12 |  3354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      12 |  3355 | `		if( rc == SXERR_ABORT ){` |
|       - |  3356 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3357 | `			return SXERR_ABORT;` |
|       - |  3358 | `		}` |
|       7 |  3359 | `	}else{` |
|    3551 |  3360 | `		sxu32 nInstrIdx = 0;` |
|       - |  3361 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3551 |  3362 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3551 |  3363 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3364 | `			/* According to the PHP language reference manual` |
|       - |  3365 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3366 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3367 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3368 | `			 */` |
|       5 |  3369 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3370 | `			if( rc == SXRET_OK ){` |
|       5 |  3371 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3372 | `			}` |
|       3 |  3373 | `		}else{` |
|       - |  3374 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3547 |  3375 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3547 |  3376 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3377 | `				JumpFixup sJumpFix;` |
|       - |  3378 | `				/* Post-continue */` |
|      14 |  3379 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3380 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3381 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3382 | `			}` |
|       - |  3383 | `		}` |
|       - |  3384 | `	}` |
|    3561 |  3385 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3386 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3387 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3388 | `	}` |
|       - |  3389 | `	/* Statement successfully compiled */` |
|    3561 |  3390 | `	return SXRET_OK;` |
|    1783 |  3391 |  |
|       - |  3392 | `/*` |
|       - |  3393 | ` * Compile the 'break' statement.` |
|       - |  3394 | ` * According to the PHP language reference` |
|       - |  3395 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3396 | ` *  structure.` |
|       - |  3397 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3398 | ` *  enclosing structures are to be broken out of.` |
|       - |  3399 | ` */` |
|     122 |  3400 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3401 |  |
|       - |  3402 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3403 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3404 | `	sxi32 rc;` |
|     127 |  3405 | `	iLevel = 0;` |
|       - |  3406 | `	/* Jump the 'break' keyword */` |
|     127 |  3407 | `	pGen->pIn++;` |
|     127 |  3408 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3409 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3410 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3411 | `		 */` |
|       - |  3412 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3413 | `		char *zAlloc = 0;` |
|       - |  3414 | `		SyString sNum;` |
|      17 |  3415 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3416 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3417 | `			return SXERR_ABORT;` |
|       - |  3418 | `		}` |
|      17 |  3419 | `		if( rc == SXRET_OK ){` |
|      21 |  3420 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3421 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3422 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3423 | `				return SXERR_ABORT;` |
|       - |  3424 | `			}` |
|      15 |  3425 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3426 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3427 | `		}` |
|      17 |  3428 | `		if( iLevel < 2 ){` |
|       3 |  3429 | `			iLevel = 0;` |
|       1 |  3430 | `		}` |
|      17 |  3431 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3432 | `	}` |
|       - |  3433 | `	/* Extract the target loop */` |
|     127 |  3434 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3435 | `	if( pLoop == 0 ){` |
|       - |  3436 | `		/* Illegal break */` |
|      19 |  3437 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3438 | `		if( rc == SXERR_ABORT ){` |
|       - |  3439 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3440 | `			return SXERR_ABORT;` |
|       - |  3441 | `		}` |
|      11 |  3442 | `	}else{` |
|       - |  3443 | `		sxu32 nInstrIdx;` |
|       - |  3444 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3445 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3446 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3447 | `		if( rc == SXRET_OK ){` |
|       - |  3448 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3449 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3450 | `		}` |
|       - |  3451 | `	}` |
|     127 |  3452 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3453 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3454 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3455 | `	}` |
|       - |  3456 | `	/* Statement successfully compiled */` |
|     127 |  3457 | `	return SXRET_OK;` |
|      66 |  3458 |  |
|       - |  3459 | `/*` |
|       - |  3460 | ` * Compile or record a label.` |
|       - |  3461 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3462 | ` * Example` |
|       - |  3463 | ` *  goto LABEL;` |
|       - |  3464 | ` *   echo 'Foo';` |
|       - |  3465 | ` *  LABEL:` |
|       - |  3466 | ` *   echo 'Bar';` |
|       - |  3467 | ` */` |
|     112 |  3468 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3469 |  |
|       - |  3470 | `	GenBlock *pBlock;` |
|       - |  3471 | `	Label sLabel;` |
|       - |  3472 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3473 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3474 | `	if( pBlock ){` |
|       - |  3475 | `		sxi32 rc;` |
|       8 |  3476 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3477 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3478 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3479 | `			return SXERR_ABORT;` |
|       - |  3480 | `		}` |
|       4 |  3481 | `	}else{` |
|     113 |  3482 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3483 | `		char *zDup;` |
|       - |  3484 | `		/* Initialize label fields */` |
|     113 |  3485 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3486 | `		/* Duplicate label name */` |
|     113 |  3487 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3488 | `		if( zDup == 0 ){` |
|     ! 0 |  3489 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3490 | `			return SXERR_ABORT;` |
|       - |  3491 | `		}` |
|     113 |  3492 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3493 | `		sLabel.bRef  = FALSE;` |
|     113 |  3494 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3495 | `		pBlock = pGen->pCurrent;` |
|     221 |  3496 | `		while( pBlock ){` |
|     133 |  3497 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3498 | `				break;` |
|       - |  3499 | `			}` |
|       - |  3500 | `			/* Point to the upper block */` |
|     113 |  3501 | `			pBlock = pBlock->pParent;` |
|       5 |  3502 | `		}` |
|     113 |  3503 | `		if( pBlock ){` |
|      23 |  3504 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3505 | `		}else{` |
|      93 |  3506 | `			sLabel.pFunc = 0;` |
|       - |  3507 | `		}` |
|       - |  3508 | `		/* Insert in label set */` |
|     113 |  3509 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3510 | `	}` |
|     117 |  3511 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3512 | `	return SXRET_OK;` |
|      61 |  3513 |  |
|       - |  3514 | `/*` |
|       - |  3515 | ` * Compile the so hated 'goto' statement.` |
|       - |  3516 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3517 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3518 | ` * a compiler it has to do this.` |
|       - |  3519 | ` * According to the PHP language reference manual` |
|       - |  3520 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3521 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3522 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3523 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3524 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3525 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3526 | ` *   of a multi-level break` |
|       - |  3527 | ` */` |
|     152 |  3528 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3529 |  |
|       - |  3530 | `	JumpFixup sJump;` |
|       - |  3531 | `	sxi32 rc;` |
|     157 |  3532 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3533 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3534 | `		/* Missing label */` |
|     ! 0 |  3535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3536 | `		if( rc == SXERR_ABORT ){` |
|       - |  3537 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3538 | `			return SXERR_ABORT;` |
|       - |  3539 | `		}` |
|     ! 0 |  3540 | `		return SXRET_OK;` |
|       - |  3541 | `	}` |
|     157 |  3542 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3544 | `		if( rc == SXERR_ABORT ){` |
|       - |  3545 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3546 | `			return SXERR_ABORT;` |
|       - |  3547 | `		}` |
|       3 |  3548 | `	}else{` |
|     153 |  3549 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3550 | `		GenBlock *pBlock;` |
|       - |  3551 | `		char *zDup;` |
|       - |  3552 | `		/* Prepare the jump destination */` |
|     153 |  3553 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3554 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3555 | `		/* Duplicate label name */` |
|     153 |  3556 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3557 | `		if( zDup == 0 ){` |
|     ! 0 |  3558 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3559 | `			return SXERR_ABORT;` |
|       - |  3560 | `		}` |
|     153 |  3561 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3562 | `		pBlock = pGen->pCurrent;` |
|     315 |  3563 | `		while( pBlock ){` |
|     199 |  3564 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3565 | `				break;` |
|       - |  3566 | `			}` |
|       - |  3567 | `			/* Point to the upper block */` |
|     167 |  3568 | `			pBlock = pBlock->pParent;` |
|       5 |  3569 | `		}` |
|     153 |  3570 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       9 |  3571 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       9 |  3572 | `			if( rc == SXERR_ABORT ){` |
|       - |  3573 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3574 | `				return SXERR_ABORT;` |
|       - |  3575 | `			}` |
|       3 |  3576 | `		}` |
|     153 |  3577 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      29 |  3578 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      16 |  3579 | `		}else{` |
|     127 |  3580 | `			sJump.pFunc = 0;` |
|       - |  3581 | `		}` |
|       - |  3582 | `		/* Emit the unconditional jump */` |
|     153 |  3583 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3584 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3585 | `		}` |
|       - |  3586 | `	}` |
|     157 |  3587 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3588 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3589 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3590 | `	}` |
|       - |  3591 | `	/* Statement successfully compiled */` |
|     157 |  3592 | `	return SXRET_OK;` |
|      81 |  3593 |  |
|       - |  3594 | `/*` |
|       - |  3595 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3596 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3597 | ` * failure.` |
|       - |  3598 | ` */` |
|      20 |  3599 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       2 |  3600 |  |
|       - |  3601 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3602 | `	sxu32 nRawObj;` |
|      10 |  3603 | `	sxu32 nObjIdx;` |
|       - |  3604 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3605 | `	 * a PHP block.` |
|       - |  3606 | `	 */` |
|      10 |  3607 | `Consume:` |
|      22 |  3608 | `	nRawObj = nObjIdx = 0;` |
|      22 |  3609 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3610 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3611 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3612 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3613 | `			return SXERR_ABORT;` |
|       - |  3614 | `		}` |
|       - |  3615 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3616 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3617 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3618 | `		++nRawObj;` |
|     ! 0 |  3619 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3620 | `	}` |
|      22 |  3621 | `	if( nRawObj > 0 ){` |
|       - |  3622 | `		/* Emit the consume instruction */` |
|     ! 0 |  3623 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3624 | `	}` |
|      22 |  3625 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3626 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3627 | `		/* Reset the token set */` |
|     ! 0 |  3628 | `		SySetReset(pTokenSet);` |
|       - |  3629 | `		/* Tokenize input */` |
|     ! 0 |  3630 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3631 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3632 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3633 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3634 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3635 | `		/* Advance the stream cursor */` |
|     ! 0 |  3636 | `		pGen->pRawIn++;` |
|       - |  3637 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3638 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3639 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3640 | `			sxi32 rc;` |
|       - |  3641 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3642 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3643 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3644 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3645 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3646 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3647 | `				return SXERR_ABORT;` |
|     ! 0 |  3648 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3649 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3650 | `			}` |
|     ! 0 |  3651 | `			goto Consume;` |
|       - |  3652 | `		}` |
|     ! 0 |  3653 | `	}else{` |
|       - |  3654 | `		/* No more chunks to process */` |
|      22 |  3655 | `		pGen->pIn = pGen->pEnd;` |
|      22 |  3656 | `		return SXERR_EOF;` |
|       - |  3657 | `	}` |
|     ! 0 |  3658 | `	return SXRET_OK;` |
|      12 |  3659 |  |
|       - |  3660 | `/*` |
|       - |  3661 | ` * Compile a PHP block.` |
|       - |  3662 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3663 | ` * optionally delimited by braces {}.` |
|       - |  3664 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3665 | ` * and this function takes care of generating the appropriate error` |
|       - |  3666 | ` * message.` |
|       - |  3667 | ` */` |
|  426286 |  3668 | `static sxi32 PH7_CompileBlock(` |
|       - |  3669 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3670 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3671 | `	)` |
|       5 |  3672 |  |
|       - |  3673 | `	sxi32 rc;` |
|       - |  3674 | `	sxu32 nLine;` |
|  426291 |  3675 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  424607 |  3676 | `		nLine = pGen->pIn->nLine;` |
|  424607 |  3677 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  424607 |  3678 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3679 | `			return SXERR_ABORT;` |
|       - |  3680 | `		}` |
|  424607 |  3681 | `		pGen->pIn++;` |
|       - |  3682 | `		/* Compile until we hit the closing braces '}' */` |
|  579870 |  3683 | `		for(;;){` |
| 1159745 |  3684 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      22 |  3685 | `				rc = GenStateNextChunk(&(*pGen));` |
|      22 |  3686 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3687 | `			 	   return SXERR_ABORT;` |
|       - |  3688 | `				}` |
|      22 |  3689 | `				if( rc == SXERR_EOF ){` |
|       - |  3690 | `					/* No more token to process. Missing closing braces */` |
|      22 |  3691 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      22 |  3692 | `					break;` |
|       - |  3693 | `				}` |
|     ! 0 |  3694 | `			}` |
| 1159725 |  3695 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3696 | `				/* Closing braces found,break immediately*/` |
|  424587 |  3697 | `				pGen->pIn++;` |
|  424587 |  3698 | `				break;` |
|       - |  3699 | `			}` |
|       - |  3700 | `			/* Compile a single statement */` |
|  735143 |  3701 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  735143 |  3702 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3703 | `				return SXERR_ABORT;` |
|       - |  3704 | `			}` |
|       5 |  3705 | `		}` |
|  424607 |  3706 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  213990 |  3707 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3708 | `		pGen->pIn++;` |
|     ! 0 |  3709 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3710 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3711 | `			return SXERR_ABORT;` |
|       - |  3712 | `		}` |
|       - |  3713 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3714 | `		for(;;){` |
|     ! 0 |  3715 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3716 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3717 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3718 | `			 	   return SXERR_ABORT;` |
|       - |  3719 | `				}` |
|     ! 0 |  3720 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3721 | `					/* No more token to process */` |
|     ! 0 |  3722 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3723 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3724 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3725 | `					}` |
|     ! 0 |  3726 | `					break;` |
|       - |  3727 | `				}` |
|     ! 0 |  3728 | `			}` |
|     ! 0 |  3729 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3730 | `				sxi32 nKwrd;` |
|       - |  3731 | `				/* Keyword found */` |
|     ! 0 |  3732 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3733 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3734 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3735 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3736 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3737 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3738 | `						}` |
|     ! 0 |  3739 | `						break;` |
|       - |  3740 | `				}` |
|     ! 0 |  3741 | `			}` |
|       - |  3742 | `			/* Compile a single statement */` |
|     ! 0 |  3743 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3744 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3745 | `				return SXERR_ABORT;` |
|       - |  3746 | `			}` |
|     ! 0 |  3747 | `		}` |
|     ! 0 |  3748 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3749 | `	}else{` |
|       - |  3750 | `		/* Compile a single statement */` |
|    1689 |  3751 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1689 |  3752 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3753 | `			return SXERR_ABORT;` |
|       - |  3754 | `		}` |
|       - |  3755 | `	}` |
|       - |  3756 | `	/* Jump trailing semi-colons ';' */` |
|  426291 |  3757 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3758 | `		pGen->pIn++;` |
|     ! 0 |  3759 | `	}` |
|  426291 |  3760 | `	return SXRET_OK;` |
|  213148 |  3761 |  |
|       - |  3762 | `/*` |
|       - |  3763 | ` * Compile the gentle 'while' statement.` |
|       - |  3764 | ` * According to the PHP language reference` |
|       - |  3765 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3766 | ` *  The basic form of a while statement is:` |
|       - |  3767 | ` *  while (expr)` |
|       - |  3768 | ` *   statement` |
|       - |  3769 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3770 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3771 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3772 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3773 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3774 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3775 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3776 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3777 | ` *  while (expr):` |
|       - |  3778 | ` *    statement` |
|       - |  3779 | ` *   endwhile;` |
|       - |  3780 | ` */` |
|   14168 |  3781 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3782 |  |
|   14173 |  3783 | `	GenBlock *pWhileBlock = 0;` |
|   14173 |  3784 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3785 | `	sxu32 nFalseJump;` |
|       - |  3786 | `	sxu32 nLine;` |
|       - |  3787 | `	sxi32 rc;` |
|   14173 |  3788 | `	nLine = pGen->pIn->nLine;` |
|       - |  3789 | `	/* Jump the 'while' keyword */` |
|   14173 |  3790 | `	pGen->pIn++;` |
|   14173 |  3791 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3792 | `		/* Syntax error */` |
|     ! 0 |  3793 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3794 | `		if( rc == SXERR_ABORT ){` |
|       - |  3795 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3796 | `			return SXERR_ABORT;` |
|       - |  3797 | `		}` |
|     ! 0 |  3798 | `		goto Synchronize;` |
|       - |  3799 | `	}` |
|       - |  3800 | `	/* Jump the left parenthesis '(' */` |
|   14173 |  3801 | `	pGen->pIn++;` |
|       - |  3802 | `	/* Create the loop block */` |
|   14173 |  3803 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14173 |  3804 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3805 | `		return SXERR_ABORT;` |
|       - |  3806 | `	}` |
|       - |  3807 | `	/* Delimit the condition */` |
|   14173 |  3808 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14173 |  3809 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3810 | `		/* Empty expression */` |
|       3 |  3811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3812 | `		if( rc == SXERR_ABORT ){` |
|       - |  3813 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3814 | `			return SXERR_ABORT;` |
|       - |  3815 | `		}` |
|       1 |  3816 | `	}` |
|       - |  3817 | `	/* Swap token streams */` |
|   14173 |  3818 | `	pTmp = pGen->pEnd;` |
|   14173 |  3819 | `	pGen->pEnd = pEnd;` |
|       - |  3820 | `	/* Compile the expression */` |
|   14173 |  3821 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14173 |  3822 | `	if( rc == SXERR_ABORT ){` |
|       - |  3823 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3824 | `		return SXERR_ABORT;` |
|       - |  3825 | `	}` |
|       - |  3826 | `	/* Update token stream */` |
|   14173 |  3827 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3829 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3830 | `			return SXERR_ABORT;` |
|       - |  3831 | `		}` |
|     ! 0 |  3832 | `		pGen->pIn++;` |
|     ! 0 |  3833 | `	}` |
|       - |  3834 | `	/* Synchronize pointers */` |
|   14173 |  3835 | `	pGen->pIn  = &pEnd[1];` |
|   14173 |  3836 | `	pGen->pEnd = pTmp;` |
|       - |  3837 | `	/* Emit the false jump */` |
|   14173 |  3838 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3839 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14173 |  3840 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3841 | `	/* Compile the loop body */` |
|   14173 |  3842 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14173 |  3843 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3844 | `		return SXERR_ABORT;` |
|       - |  3845 | `	}` |
|       - |  3846 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14173 |  3847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3848 | `	/* Fix all jumps now the destination is resolved */` |
|   14173 |  3849 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3850 | `	/* Release the loop block */` |
|   14173 |  3851 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3852 | `	/* Statement successfully compiled */` |
|   14173 |  3853 | `	return SXRET_OK;` |
|     ! 0 |  3854 | `Synchronize:` |
|       - |  3855 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3856 | `	 * compiling this erroneous block.` |
|       - |  3857 | `	 */` |
|     ! 0 |  3858 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3859 | `		pGen->pIn++;` |
|     ! 0 |  3860 | `	}` |
|     ! 0 |  3861 | `	return SXRET_OK;` |
|    7089 |  3862 |  |
|       - |  3863 | `/*` |
|       - |  3864 | ` * Compile the ugly do..while() statement.` |
|       - |  3865 | ` * According to the PHP language reference` |
|       - |  3866 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3867 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3868 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3869 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3870 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3871 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3872 | ` *  would end immediately).` |
|       - |  3873 | ` *  There is just one syntax for do-while loops:` |
|       - |  3874 | ` *  <?php` |
|       - |  3875 | ` *  $i = 0;` |
|       - |  3876 | ` *  do {` |
|       - |  3877 | ` *   echo $i;` |
|       - |  3878 | ` *  } while ($i > 0);` |
|       - |  3879 | ` * ?>` |
|       - |  3880 | ` */` |
|       2 |  3881 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3882 |  |
|       3 |  3883 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3884 | `	GenBlock *pDoBlock = 0;` |
|       - |  3885 | `	sxu32 nLine;` |
|       - |  3886 | `	sxi32 rc;` |
|       3 |  3887 | `	nLine = pGen->pIn->nLine;` |
|       - |  3888 | `	/* Jump the 'do' keyword */` |
|       3 |  3889 | `	pGen->pIn++;` |
|       - |  3890 | `	/* Create the loop block */` |
|       3 |  3891 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3892 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3893 | `		return SXERR_ABORT;` |
|       - |  3894 | `	}` |
|       - |  3895 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3896 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3897 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3898 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3899 | `		return SXERR_ABORT;` |
|       - |  3900 | `	}` |
|       3 |  3901 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3902 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3903 | `	}` |
|       3 |  3904 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3905 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3906 | `			/* Missing 'while' statement */` |
|       3 |  3907 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3908 | `			if( rc == SXERR_ABORT ){` |
|       - |  3909 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3910 | `				return SXERR_ABORT;` |
|       - |  3911 | `			}` |
|       3 |  3912 | `			goto Synchronize;` |
|       - |  3913 | `	}` |
|       - |  3914 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3915 | `	pGen->pIn++;` |
|     ! 0 |  3916 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3917 | `		/* Syntax error */` |
|     ! 0 |  3918 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3919 | `		if( rc == SXERR_ABORT ){` |
|       - |  3920 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3921 | `			return SXERR_ABORT;` |
|       - |  3922 | `		}` |
|     ! 0 |  3923 | `		goto Synchronize;` |
|       - |  3924 | `	}` |
|       - |  3925 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3926 | `	pGen->pIn++;` |
|       - |  3927 | `	/* Delimit the condition */` |
|     ! 0 |  3928 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3929 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3930 | `		/* Empty expression */` |
|     ! 0 |  3931 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3932 | `		if( rc == SXERR_ABORT ){` |
|       - |  3933 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3934 | `			return SXERR_ABORT;` |
|       - |  3935 | `		}` |
|     ! 0 |  3936 | `		goto Synchronize;` |
|       - |  3937 | `	}` |
|       - |  3938 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3939 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3940 | `		JumpFixup *aPost;` |
|       - |  3941 | `		VmInstr *pInstr;` |
|       - |  3942 | `		sxu32 nJumpDest;` |
|       - |  3943 | `		sxu32 n;` |
|     ! 0 |  3944 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3945 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3946 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3947 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3948 | `			if( pInstr ){` |
|       - |  3949 | `				/* Fix */` |
|     ! 0 |  3950 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3951 | `			}` |
|     ! 0 |  3952 | `		}` |
|     ! 0 |  3953 | `	}` |
|       - |  3954 | `	/* Swap token streams */` |
|     ! 0 |  3955 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3956 | `	pGen->pEnd = pEnd;` |
|       - |  3957 | `	/* Compile the expression */` |
|     ! 0 |  3958 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3959 | `	if( rc == SXERR_ABORT ){` |
|       - |  3960 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3961 | `		return SXERR_ABORT;` |
|       - |  3962 | `	}` |
|       - |  3963 | `	/* Update token stream */` |
|     ! 0 |  3964 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3965 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3966 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3967 | `			return SXERR_ABORT;` |
|       - |  3968 | `		}` |
|     ! 0 |  3969 | `		pGen->pIn++;` |
|     ! 0 |  3970 | `	}` |
|     ! 0 |  3971 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3972 | `	pGen->pEnd = pTmp;` |
|       - |  3973 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3974 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3975 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3976 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3977 | `	/* Release the loop block */` |
|     ! 0 |  3978 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3979 | `	/* Statement successfully compiled */` |
|     ! 0 |  3980 | `	return SXRET_OK;` |
|       1 |  3981 | `Synchronize:` |
|       - |  3982 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3983 | `	 * compiling this erroneous block.` |
|       - |  3984 | `	 */` |
|       3 |  3985 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3986 | `		pGen->pIn++;` |
|     ! 0 |  3987 | `	}` |
|       3 |  3988 | `	return SXRET_OK;` |
|       2 |  3989 |  |
|       - |  3990 | `/*` |
|       - |  3991 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3992 | ` * According to the PHP language reference` |
|       - |  3993 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3994 | ` *  The syntax of a for loop is:` |
|       - |  3995 | ` *  for (expr1; expr2; expr3)` |
|       - |  3996 | ` *   statement` |
|       - |  3997 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3998 | ` *  the beginning of the loop.` |
|       - |  3999 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  4000 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  4001 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  4002 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  4003 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  4004 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  4005 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  4006 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  4007 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  4008 | ` *  of using the for truth expression.` |
|       - |  4009 | ` */` |
|   14168 |  4010 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4011 |  |
|   14173 |  4012 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14173 |  4013 | `	GenBlock *pForBlock = 0;` |
|       - |  4014 | `	sxu32 nFalseJump;` |
|       - |  4015 | `	sxu32 nLine;` |
|       - |  4016 | `	sxi32 rc;` |
|   14173 |  4017 | `	nLine = pGen->pIn->nLine;` |
|       - |  4018 | `	/* Jump the 'for' keyword */` |
|   14173 |  4019 | `	pGen->pIn++;` |
|   14173 |  4020 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4021 | `		/* Syntax error */` |
|     ! 0 |  4022 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4023 | `		if( rc == SXERR_ABORT ){` |
|       - |  4024 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4025 | `			return SXERR_ABORT;` |
|       - |  4026 | `		}` |
|     ! 0 |  4027 | `		return SXRET_OK;` |
|       - |  4028 | `	}` |
|       - |  4029 | `	/* Jump the left parenthesis '(' */` |
|   14173 |  4030 | `	pGen->pIn++;` |
|       - |  4031 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14173 |  4032 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14173 |  4033 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4034 | `		/* Empty expression */` |
|     ! 0 |  4035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4036 | `		if( rc == SXERR_ABORT ){` |
|       - |  4037 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4038 | `			return SXERR_ABORT;` |
|       - |  4039 | `		}` |
|       - |  4040 | `		/* Synchronize */` |
|     ! 0 |  4041 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4042 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4043 | `			pGen->pIn++;` |
|     ! 0 |  4044 | `		}` |
|     ! 0 |  4045 | `		return SXRET_OK;` |
|       - |  4046 | `	}` |
|       - |  4047 | `	/* Swap token streams */` |
|   14173 |  4048 | `	pTmp = pGen->pEnd;` |
|   14173 |  4049 | `	pGen->pEnd = pEnd;` |
|       - |  4050 | `	/* Compile initialization expressions if available */` |
|   14173 |  4051 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4052 | `	/* Pop operand lvalues */` |
|   14173 |  4053 | `	if( rc == SXERR_ABORT ){` |
|       - |  4054 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4055 | `		return SXERR_ABORT;` |
|   14173 |  4056 | `	}else if( rc != SXERR_EMPTY ){` |
|   14171 |  4057 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7083 |  4058 | `	}` |
|   14173 |  4059 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4060 | `		/* Syntax error */` |
|     ! 0 |  4061 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4062 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4063 | `		if( rc == SXERR_ABORT ){` |
|       - |  4064 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4065 | `			return SXERR_ABORT;` |
|       - |  4066 | `		}` |
|     ! 0 |  4067 | `		return SXRET_OK;` |
|       - |  4068 | `	}` |
|       - |  4069 | `	/* Jump the trailing ';' */` |
|   14173 |  4070 | `	pGen->pIn++;` |
|       - |  4071 | `	/* Create the loop block */` |
|   14173 |  4072 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14173 |  4073 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4074 | `		return SXERR_ABORT;` |
|       - |  4075 | `	}` |
|       - |  4076 | `	/* Deffer continue jumps */` |
|   14173 |  4077 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4078 | `	/* Compile the condition */` |
|   14173 |  4079 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14173 |  4080 | `	if( rc == SXERR_ABORT ){` |
|       - |  4081 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4082 | `		return SXERR_ABORT;` |
|   14173 |  4083 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4084 | `		/* Emit the false jump */` |
|   14171 |  4085 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4086 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14171 |  4087 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7083 |  4088 | `	}` |
|   14173 |  4089 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4090 | `		/* Syntax error */` |
|       6 |  4091 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4092 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4093 | `		if( rc == SXERR_ABORT ){` |
|       - |  4094 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4095 | `			return SXERR_ABORT;` |
|       - |  4096 | `		}` |
|       6 |  4097 | `		return SXRET_OK;` |
|       - |  4098 | `	}` |
|       - |  4099 | `	/* Jump the trailing ';' */` |
|   14169 |  4100 | `	pGen->pIn++;` |
|       - |  4101 | `	/* Save the post condition stream */` |
|   14169 |  4102 | `	pPostStart = pGen->pIn;` |
|       - |  4103 | `	/* Compile the loop body */` |
|   14169 |  4104 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14169 |  4105 | `	pGen->pEnd = pTmp;` |
|   14169 |  4106 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14169 |  4107 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4108 | `		return SXERR_ABORT;` |
|       - |  4109 | `	}` |
|       - |  4110 | `	/* Fix post-continue jumps */` |
|   14169 |  4111 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4112 | `		JumpFixup *aPost;` |
|       - |  4113 | `		VmInstr *pInstr;` |
|       - |  4114 | `		sxu32 nJumpDest;` |
|       - |  4115 | `		sxu32 n;` |
|      14 |  4116 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4117 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4118 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4119 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4120 | `			if( pInstr ){` |
|       - |  4121 | `				/* Fix jump */` |
|      14 |  4122 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4123 | `			}` |
|       8 |  4124 | `		}` |
|       6 |  4125 | `	}` |
|       - |  4126 | `	/* compile the post-expressions if available */` |
|   14169 |  4127 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4128 | `		pPostStart++;` |
|     ! 0 |  4129 | `	}` |
|   14169 |  4130 | `	if( pPostStart < pEnd ){` |
|       - |  4131 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14169 |  4132 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14169 |  4133 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14169 |  4134 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4135 | `			/* Syntax error */` |
|     ! 0 |  4136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4137 | `			if( rc == SXERR_ABORT ){` |
|       - |  4138 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4139 | `				return SXERR_ABORT;` |
|       - |  4140 | `			}` |
|     ! 0 |  4141 | `			return SXRET_OK;` |
|       - |  4142 | `		}` |
|   14169 |  4143 | `		RE_SWAP_DELIMITER(pGen);` |
|   14169 |  4144 | `		if( rc == SXERR_ABORT ){` |
|       - |  4145 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4146 | `			return SXERR_ABORT;` |
|   14169 |  4147 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4148 | `			/* Pop operand lvalue */` |
|   14169 |  4149 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7082 |  4150 | `		}` |
|    7082 |  4151 | `	}` |
|       - |  4152 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14169 |  4153 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4154 | `	/* Fix all jumps now the destination is resolved */` |
|   14169 |  4155 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4156 | `	/* Release the loop block */` |
|   14169 |  4157 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4158 | `	/* Statement successfully compiled */` |
|   14169 |  4159 | `	return SXRET_OK;` |
|    7089 |  4160 |  |
|       - |  4161 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4162 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4163 | ` * are allowed.` |
|       - |  4164 | ` */` |
|    7596 |  4165 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4166 |  |
|    7601 |  4167 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7601 |  4168 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4169 | `		/* Unexpected expression */` |
|     ! 0 |  4170 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4171 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4172 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4173 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4174 | `		}` |
|     ! 0 |  4175 | `	}` |
|    7601 |  4176 | `	return rc;` |
|       5 |  4177 |  |
|       - |  4178 | `/*` |
|       - |  4179 | ` * Compile the 'foreach' statement.` |
|       - |  4180 | ` * According to the PHP language reference` |
|       - |  4181 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4182 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4183 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4184 | ` *  is a minor but useful extension of the first:` |
|       - |  4185 | ` *  foreach (array_expression as $value)` |
|       - |  4186 | ` *    statement` |
|       - |  4187 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4188 | ` *   statement` |
|       - |  4189 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4190 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4191 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4192 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4193 | ` *  to the variable $key on each loop.` |
|       - |  4194 | ` *  Note:` |
|       - |  4195 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4196 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4197 | ` *  Note:` |
|       - |  4198 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4199 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4200 | ` *  or after the foreach without resetting it.` |
|       - |  4201 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4202 | ` *  of copying the value.` |
|       - |  4203 | ` */` |
|    3892 |  4204 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4205 |  |
|    3897 |  4206 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3897 |  4207 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3897 |  4208 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4209 | `	ph7_foreach_info *pInfo;` |
|       - |  4210 | `	sxu32 nFalseJump;` |
|       - |  4211 | `	VmInstr *pInstr;` |
|       - |  4212 | `	sxu32 nLine;` |
|       - |  4213 | `	sxi32 rc;` |
|    3897 |  4214 | `	nLine = pGen->pIn->nLine;` |
|       - |  4215 | `	/* Jump the 'foreach' keyword */` |
|    3897 |  4216 | `	pGen->pIn++;` |
|    3897 |  4217 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4218 | `		/* Syntax error */` |
|     ! 0 |  4219 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4220 | `		if( rc == SXERR_ABORT ){` |
|       - |  4221 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4222 | `			return SXERR_ABORT;` |
|       - |  4223 | `		}` |
|     ! 0 |  4224 | `		goto Synchronize;` |
|       - |  4225 | `	}` |
|       - |  4226 | `	/* Jump the left parenthesis '(' */` |
|    3897 |  4227 | `	pGen->pIn++;` |
|       - |  4228 | `	/* Create the loop block */` |
|    3897 |  4229 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3897 |  4230 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4231 | `		return SXERR_ABORT;` |
|       - |  4232 | `	}` |
|       - |  4233 | `	/* Delimit the expression */` |
|    3897 |  4234 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3897 |  4235 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4236 | `		/* Empty expression */` |
|     ! 0 |  4237 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4238 | `		if( rc == SXERR_ABORT ){` |
|       - |  4239 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4240 | `			return SXERR_ABORT;` |
|       - |  4241 | `		}` |
|       - |  4242 | `		/* Synchronize */` |
|     ! 0 |  4243 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4244 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4245 | `			pGen->pIn++;` |
|     ! 0 |  4246 | `		}` |
|     ! 0 |  4247 | `		return SXRET_OK;` |
|       - |  4248 | `	}` |
|       - |  4249 | `	/* Compile the array expression */` |
|    3897 |  4250 | `	pCur = pGen->pIn;` |
|   26759 |  4251 | `	while( pCur < pEnd ){` |
|   26759 |  4252 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3911 |  4253 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3911 |  4254 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4255 | `				/* Break with the first 'as' found */` |
|    3897 |  4256 | `				break;` |
|       - |  4257 | `			}` |
|       7 |  4258 | `		}` |
|       - |  4259 | `		/* Advance the stream cursor */` |
|   22867 |  4260 | `		pCur++;` |
|       5 |  4261 | `	}` |
|    3897 |  4262 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4263 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4264 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4265 | `		if( rc == SXERR_ABORT ){` |
|       - |  4266 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4267 | `			return SXERR_ABORT;` |
|       - |  4268 | `		}` |
|     ! 0 |  4269 | `		goto Synchronize;` |
|       - |  4270 | `	}` |
|       - |  4271 | `	/* Swap token streams */` |
|    3897 |  4272 | `	pTmp = pGen->pEnd;` |
|    3897 |  4273 | `	pGen->pEnd = pCur;` |
|    3897 |  4274 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3897 |  4275 | `	if( rc == SXERR_ABORT ){` |
|       - |  4276 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4277 | `		return SXERR_ABORT;` |
|       - |  4278 | `	}` |
|       - |  4279 | `	/* Update token stream */` |
|    3897 |  4280 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4281 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4282 | `		if( rc == SXERR_ABORT ){` |
|       - |  4283 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4284 | `			return SXERR_ABORT;` |
|       - |  4285 | `		}` |
|     ! 0 |  4286 | `		pGen->pIn++;` |
|     ! 0 |  4287 | `	}` |
|    3897 |  4288 | `	pCur++; /* Jump the 'as' keyword */` |
|    3897 |  4289 | `	pGen->pIn = pCur;` |
|    3897 |  4290 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4291 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4292 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4293 | `			return SXERR_ABORT;` |
|       - |  4294 | `		}` |
|     ! 0 |  4295 | `	}` |
|       - |  4296 | `	/* Create the foreach context */` |
|    3897 |  4297 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3897 |  4298 | `	if( pInfo == 0 ){` |
|     ! 0 |  4299 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4300 | `		return SXERR_ABORT;` |
|       - |  4301 | `	}` |
|       - |  4302 | `	/* Zero the structure */` |
|    3897 |  4303 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4304 | `	/* Initialize structure fields */` |
|    3897 |  4305 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4306 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4307 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4308 | `	 * '=>'. */` |
|    3897 |  4309 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3897 |  4310 | `	if( pCur < pEnd ){` |
|       - |  4311 | `		/* Compile the expression holding the key name */` |
|    3721 |  4312 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4313 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4314 | `			if( rc == SXERR_ABORT ){` |
|       - |  4315 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4316 | `				return SXERR_ABORT;` |
|       - |  4317 | `			}` |
|     ! 0 |  4318 | `		}else{` |
|    3721 |  4319 | `			pGen->pEnd = pCur;` |
|    3721 |  4320 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3721 |  4321 | `			if( rc == SXERR_ABORT ){` |
|       - |  4322 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4323 | `				return SXERR_ABORT;` |
|       - |  4324 | `			}` |
|    3721 |  4325 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3721 |  4326 | `			if( pInstr->p3 ){` |
|       - |  4327 | `				/* Record key name */` |
|    3721 |  4328 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1858 |  4329 | `			}` |
|    3721 |  4330 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4331 | `		}` |
|    3721 |  4332 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1858 |  4333 | `	}` |
|    3897 |  4334 | `	pGen->pEnd = pEnd;` |
|    3897 |  4335 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4336 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4337 | `		if( rc == SXERR_ABORT ){` |
|       - |  4338 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4339 | `			return SXERR_ABORT;` |
|       - |  4340 | `		}` |
|     ! 0 |  4341 | `		goto Synchronize;` |
|       - |  4342 | `	}` |
|    3897 |  4343 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4344 | `		pGen->pIn++;` |
|       - |  4345 | `		/* Pass by reference  */` |
|      11 |  4346 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4347 | `	}` |
|       - |  4348 | `	/* Check if the value target is list() */` |
|    3897 |  4349 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4350 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4351 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4352 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4353 | `		 */` |
|       - |  4354 | `		static int iForeachListCnt = 0;` |
|       - |  4355 | `		char zTmp[128];` |
|       - |  4356 | `		sxu32 nLen;` |
|       - |  4357 | `		char *zDup;` |
|      10 |  4358 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4359 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4360 | `		if( zDup == 0 ){` |
|     ! 0 |  4361 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4362 | `			return SXERR_ABORT;` |
|       - |  4363 | `		}` |
|      10 |  4364 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4365 | `		/* Save list() token boundaries */` |
|      10 |  4366 | `		pListStart = pGen->pIn;` |
|       - |  4367 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4368 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4369 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4370 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4371 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4372 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4373 | `				return SXERR_ABORT;` |
|       - |  4374 | `			}` |
|       3 |  4375 | `			goto Synchronize;` |
|       - |  4376 | `		}` |
|       7 |  4377 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4378 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4379 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4381 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4382 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4383 | `				return SXERR_ABORT;` |
|       - |  4384 | `			}` |
|     ! 0 |  4385 | `			goto Synchronize;` |
|       - |  4386 | `		}` |
|       7 |  4387 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4388 | `		pListEnd = pGen->pIn;` |
|       7 |  4389 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3892 |  4390 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4391 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4392 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4393 | `		 */` |
|       - |  4394 | `		static int iForeachShortListCnt = 0;` |
|       - |  4395 | `		char zTmp[128];` |
|       - |  4396 | `		sxu32 nLen;` |
|       - |  4397 | `		char *zDup;` |
|       5 |  4398 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       5 |  4399 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       5 |  4400 | `		if( zDup == 0 ){` |
|     ! 0 |  4401 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4402 | `			return SXERR_ABORT;` |
|       - |  4403 | `		}` |
|       5 |  4404 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4405 | `		/* Save [...] token boundaries */` |
|       5 |  4406 | `		pListStart = pGen->pIn;` |
|       - |  4407 | `		/* Advance past [...] */` |
|       5 |  4408 | `		pGen->pIn++; /* Jump '[' */` |
|       5 |  4409 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       5 |  4410 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4411 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4412 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4413 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4414 | `				return SXERR_ABORT;` |
|       - |  4415 | `			}` |
|     ! 0 |  4416 | `			goto Synchronize;` |
|       - |  4417 | `		}` |
|       5 |  4418 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       5 |  4419 | `		pListEnd = pGen->pIn;` |
|       5 |  4420 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       3 |  4421 | `	}else{` |
|       - |  4422 | `		/* Compile the expression holding the value name */` |
|    3885 |  4423 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3885 |  4424 | `		if( rc == SXERR_ABORT ){` |
|       - |  4425 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4426 | `			return SXERR_ABORT;` |
|       - |  4427 | `		}` |
|    3885 |  4428 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3885 |  4429 | `		if( pInstr->p3 ){` |
|       - |  4430 | `			/* Record value name */` |
|    3885 |  4431 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1940 |  4432 | `		}` |
|       - |  4433 | `	}` |
|       - |  4434 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3895 |  4435 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4436 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3895 |  4437 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4438 | `	/* Record the first instruction to execute */` |
|    3895 |  4439 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4440 | `	/* Emit the FOREACH_STEP instruction */` |
|    3895 |  4441 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4442 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3895 |  4443 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4444 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3895 |  4445 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4446 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4447 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4448 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4449 | `		 */` |
|      11 |  4450 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4451 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4452 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4453 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4454 | `		 */` |
|      11 |  4455 | `		pSavedIn = pGen->pIn;` |
|      11 |  4456 | `		pSavedEnd = pGen->pEnd;` |
|      11 |  4457 | `		pGen->pIn = pListStart;` |
|      11 |  4458 | `		pGen->pEnd = pListEnd;` |
|      11 |  4459 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       5 |  4460 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       3 |  4461 | `		}else{` |
|       7 |  4462 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4463 | `		}` |
|      11 |  4464 | `		pGen->pIn = pSavedIn;` |
|      11 |  4465 | `		pGen->pEnd = pSavedEnd;` |
|      11 |  4466 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4467 | `			return SXERR_ABORT;` |
|       - |  4468 | `		}` |
|       - |  4469 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      11 |  4470 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       5 |  4471 | `	}` |
|       - |  4472 | `	/* Compile the loop body */` |
|    3895 |  4473 | `	pGen->pIn = &pEnd[1];` |
|    3895 |  4474 | `	pGen->pEnd = pTmp;` |
|    3895 |  4475 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3895 |  4476 | `	if( rc == SXERR_ABORT ){` |
|       - |  4477 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4478 | `		return SXERR_ABORT;` |
|       - |  4479 | `	}` |
|       - |  4480 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3895 |  4481 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4482 | `	/* Fix all jumps now the destination is resolved */` |
|    3895 |  4483 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4484 | `	/* Release the loop block */` |
|    3895 |  4485 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4486 | `	/* Statement successfully compiled */` |
|    3895 |  4487 | `	return SXRET_OK;` |
|       1 |  4488 | `Synchronize:` |
|       - |  4489 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4490 | `	 * compiling this erroneous block.` |
|       - |  4491 | `	 */` |
|       3 |  4492 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4493 | `		pGen->pIn++;` |
|     ! 0 |  4494 | `	}` |
|       3 |  4495 | `	return SXRET_OK;` |
|    1951 |  4496 |  |
|       - |  4497 | `/*` |
|       - |  4498 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4499 | ` * According to the PHP language reference` |
|       - |  4500 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4501 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4502 | ` *  that is similar to that of C:` |
|       - |  4503 | ` *  if (expr)` |
|       - |  4504 | ` *   statement` |
|       - |  4505 | ` *  else construct:` |
|       - |  4506 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4507 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4508 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4509 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4510 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4511 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4512 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4513 | ` *  elseif` |
|       - |  4514 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4515 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4516 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4517 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4518 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4519 | ` *   <?php` |
|       - |  4520 | ` *    if ($a > $b) {` |
|       - |  4521 | ` *     echo "a is bigger than b";` |
|       - |  4522 | ` *    } elseif ($a == $b) {` |
|       - |  4523 | ` *     echo "a is equal to b";` |
|       - |  4524 | ` *    } else {` |
|       - |  4525 | ` *     echo "a is smaller than b";` |
|       - |  4526 | ` *    }` |
|       - |  4527 | ` *    ?>` |
|       - |  4528 | ` */` |
|  147274 |  4529 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4530 |  |
|  147279 |  4531 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  147279 |  4532 | `	GenBlock *pCondBlock = 0;` |
|       - |  4533 | `	sxu32 nJumpIdx;` |
|       - |  4534 | `	sxu32 nKeyID;` |
|       - |  4535 | `	sxi32 rc;` |
|       - |  4536 | `	/* Jump the 'if' keyword */` |
|  147279 |  4537 | `	pGen->pIn++;` |
|  147279 |  4538 | `	pToken = pGen->pIn;` |
|       - |  4539 | `	/* Create the conditional block */` |
|  147279 |  4540 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  147279 |  4541 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4542 | `		return SXERR_ABORT;` |
|       - |  4543 | `	}` |
|       - |  4544 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   80718 |  4545 | `	for(;;){` |
|  161441 |  4546 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4547 | `			/* Syntax error */` |
|     ! 0 |  4548 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4549 | `				pToken--;` |
|     ! 0 |  4550 | `			}` |
|     ! 0 |  4551 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4552 | `			if( rc == SXERR_ABORT ){` |
|       - |  4553 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4554 | `				return SXERR_ABORT;` |
|       - |  4555 | `			}` |
|     ! 0 |  4556 | `			goto Synchronize;` |
|       - |  4557 | `		}` |
|       - |  4558 | `		/* Jump the left parenthesis '(' */` |
|  161441 |  4559 | `		pToken++;` |
|       - |  4560 | `		/* Delimit the condition */` |
|  161441 |  4561 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  161441 |  4562 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4563 | `			/* Syntax error */` |
|     ! 0 |  4564 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4565 | `				pToken--;` |
|     ! 0 |  4566 | `			}` |
|     ! 0 |  4567 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4568 | `			if( rc == SXERR_ABORT ){` |
|       - |  4569 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4570 | `				return SXERR_ABORT;` |
|       - |  4571 | `			}` |
|     ! 0 |  4572 | `			goto Synchronize;` |
|       - |  4573 | `		}` |
|       - |  4574 | `		/* Swap token streams */` |
|  161441 |  4575 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4576 | `		/* Compile the condition */` |
|  161441 |  4577 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4578 | `		/* Update token stream */` |
|  161441 |  4579 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4580 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4581 | `			pGen->pIn++;` |
|     ! 0 |  4582 | `		}` |
|  161441 |  4583 | `		pGen->pIn  = &pEnd[1];` |
|  161441 |  4584 | `		pGen->pEnd = pTmp;` |
|  161441 |  4585 | `		if( rc == SXERR_ABORT ){` |
|       - |  4586 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4587 | `			return SXERR_ABORT;` |
|       - |  4588 | `		}` |
|       - |  4589 | `		/* Emit the false jump */` |
|  161441 |  4590 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4591 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  161441 |  4592 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4593 | `		/* Compile the body */` |
|  161441 |  4594 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  161441 |  4595 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4596 | `			return SXERR_ABORT;` |
|       - |  4597 | `		}` |
|  161441 |  4598 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   45005 |  4599 | `			break;` |
|       - |  4600 | `		}` |
|       - |  4601 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   71441 |  4602 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   71441 |  4603 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   45979 |  4604 | `			break;` |
|       - |  4605 | `		}` |
|       - |  4606 | `		/* Emit the unconditional jump */` |
|   25467 |  4607 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4608 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   25467 |  4609 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   25467 |  4610 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18329 |  4611 | `			pToken = &pGen->pIn[1];` |
|   18329 |  4612 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7076 |  4613 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5655 |  4614 | `					break;` |
|       - |  4615 | `			}` |
|    7029 |  4616 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3512 |  4617 | `		}` |
|   14167 |  4618 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4619 | `		/* Synchronize cursors */` |
|   14167 |  4620 | `		pToken = pGen->pIn;` |
|       - |  4621 | `		/* Fix the false jump */` |
|   14167 |  4622 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4623 | `	} /* For(;;) */` |
|       - |  4624 | `	/* Fix the false jump */` |
|  147279 |  4625 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  147279 |  4626 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   57274 |  4627 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4628 | `			/* Compile the else block */` |
|   11305 |  4629 | `			pGen->pIn++;` |
|   11305 |  4630 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11305 |  4631 | `			if( rc == SXERR_ABORT ){` |
|       - |  4632 |  |
|     ! 0 |  4633 | `				return SXERR_ABORT;` |
|       - |  4634 | `			}` |
|    5650 |  4635 | `	}` |
|  147279 |  4636 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4637 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  147279 |  4638 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4639 | `	/* Release the conditional block */` |
|  147279 |  4640 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4641 | `	/* Statement successfully compiled */` |
|  147279 |  4642 | `	return SXRET_OK;` |
|     ! 0 |  4643 | `Synchronize:` |
|       - |  4644 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4645 | `	 */` |
|     ! 0 |  4646 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4647 | `		pGen->pIn++;` |
|     ! 0 |  4648 | `	}` |
|     ! 0 |  4649 | `	return SXRET_OK;` |
|   73642 |  4650 |  |
|       - |  4651 | `/*` |
|       - |  4652 | ` * Compile the global construct.` |
|       - |  4653 | ` * According to the PHP language reference` |
|       - |  4654 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4655 | ` *  to be used in that function.` |
|       - |  4656 | ` *  Example #1 Using global` |
|       - |  4657 | ` *  <?php` |
|       - |  4658 | ` *   $a = 1;` |
|       - |  4659 | ` *   $b = 2;` |
|       - |  4660 | ` *   function Sum()` |
|       - |  4661 | ` *   {` |
|       - |  4662 | ` *    global $a, $b;` |
|       - |  4663 | ` *    $b = $a + $b;` |
|       - |  4664 | ` *   }` |
|       - |  4665 | ` *   Sum();` |
|       - |  4666 | ` *   echo $b;` |
|       - |  4667 | ` *  ?>` |
|       - |  4668 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4669 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4670 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4671 | ` */` |
|      36 |  4672 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4673 |  |
|      41 |  4674 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4675 | `	sxi32 nExpr;` |
|       - |  4676 | `	sxi32 rc;` |
|       - |  4677 | `	/* Jump the 'global' keyword */` |
|      41 |  4678 | `	pGen->pIn++;` |
|      41 |  4679 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4680 | `		/* Nothing to process */` |
|     ! 0 |  4681 | `		return SXRET_OK;` |
|       - |  4682 | `	}` |
|      41 |  4683 | `	pTmp = pGen->pEnd;` |
|      41 |  4684 | `	nExpr = 0;` |
|      87 |  4685 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4686 | `		if( pGen->pIn < pNext ){` |
|      51 |  4687 | `			pGen->pEnd = pNext;` |
|      51 |  4688 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4689 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4690 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4691 | `					return SXERR_ABORT;` |
|       - |  4692 | `				}` |
|     ! 0 |  4693 | `			}else{` |
|      51 |  4694 | `				pGen->pIn++;` |
|      51 |  4695 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4696 | `					/* Emit a warning */` |
|     ! 0 |  4697 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4698 | `				}else{` |
|      51 |  4699 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4700 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4701 | `						return SXERR_ABORT;` |
|      51 |  4702 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4703 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4704 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4705 | `							/* Variable name, not a constant */` |
|      51 |  4706 | `							pLast->iP1 = 0;` |
|      23 |  4707 | `						}` |
|      51 |  4708 | `						nExpr++;` |
|      23 |  4709 | `					}` |
|       - |  4710 | `				}` |
|       - |  4711 | `			}` |
|      23 |  4712 | `		}` |
|       - |  4713 | `		/* Next expression in the stream */` |
|      51 |  4714 | `		pGen->pIn = pNext;` |
|       - |  4715 | `		/* Jump trailing commas */` |
|      61 |  4716 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4717 | `			pGen->pIn++;` |
|       5 |  4718 | `		}` |
|       5 |  4719 | `	}` |
|       - |  4720 | `	/* Restore token stream */` |
|      41 |  4721 | `	pGen->pEnd = pTmp;` |
|      41 |  4722 | `	if( nExpr > 0 ){` |
|       - |  4723 | `		/* Emit the uplink instruction */` |
|      41 |  4724 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4725 | `	}` |
|      41 |  4726 | `	return SXRET_OK;` |
|      23 |  4727 |  |
|       - |  4728 | `/*` |
|       - |  4729 | ` * Compile the return statement.` |
|       - |  4730 | ` * According to the PHP language reference` |
|       - |  4731 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4732 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4733 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4734 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4735 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4736 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4737 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4738 | ` *  from within the main script file, then script execution end.` |
|       - |  4739 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4740 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4741 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4742 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4743 | ` */` |
|  233060 |  4744 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4745 |  |
|  233065 |  4746 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4747 | `	sxi32 rc;` |
|       - |  4748 | `	/* Jump the 'return' keyword */` |
|  233065 |  4749 | `	pGen->pIn++;` |
|  233065 |  4750 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4751 | `		/* Compile the expression */` |
|  233037 |  4752 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  233037 |  4753 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4754 | `			return SXERR_ABORT;` |
|  233037 |  4755 | `		}else if(rc != SXERR_EMPTY ){` |
|  233037 |  4756 | `			nRet = 1;` |
|  116516 |  4757 | `		}` |
|  116516 |  4758 | `	}` |
|       - |  4759 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4760 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4761 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4762 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4763 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  233065 |  4764 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  233065 |  4765 | `	return SXRET_OK;` |
|  116535 |  4766 |  |
|       - |  4767 | `/*` |
|       - |  4768 | ` * Compile a yield expression.` |
|       - |  4769 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4770 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4771 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4772 | ` */` |
|     154 |  4773 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4774 |  |
|       - |  4775 | `	SyToken *pTmp, *pSplit;` |
|     159 |  4776 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     159 |  4777 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4778 | `	sxi32 rc;` |
|      77 |  4779 | `	(void)iCompileFlag;` |
|       - |  4780 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     159 |  4781 | `	pGen->pIn++;` |
|       - |  4782 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4783 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4784 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4785 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4786 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     171 |  4787 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      93 |  4788 | `		&& pGen->pIn->sData.nByte == 4` |
|      39 |  4789 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      38 |  4790 | `		pGen->pIn++; /* Skip 'from' */` |
|      38 |  4791 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      38 |  4792 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4793 | `			return SXERR_ABORT;` |
|       - |  4794 | `		}` |
|      38 |  4795 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4796 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4797 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4798 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4799 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4800 | `				return SXERR_ABORT;` |
|       - |  4801 | `			}` |
|     ! 0 |  4802 | `		}` |
|      38 |  4803 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      38 |  4804 | `		return SXRET_OK;` |
|       - |  4805 | `	}` |
|     125 |  4806 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4807 | `		/* Bare yield — no value */` |
|       3 |  4808 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4809 | `		return SXRET_OK;` |
|       - |  4810 | `	}` |
|       - |  4811 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     123 |  4812 | `	pSplit = 0;` |
|       - |  4813 | `	{` |
|     123 |  4814 | `		SyToken *pCur = pGen->pIn;` |
|     123 |  4815 | `		sxi32 nNest = 0;` |
|     257 |  4816 | `		while( pCur < pGen->pEnd ){` |
|     153 |  4817 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4818 | `				nNest++;` |
|     153 |  4819 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4820 | `				nNest--;` |
|     153 |  4821 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4822 | `				pSplit = pCur;` |
|      16 |  4823 | `				break;` |
|       - |  4824 | `			}` |
|     139 |  4825 | `			pCur++;` |
|       5 |  4826 | `		}` |
|       - |  4827 | `	}` |
|     123 |  4828 | `	pTmp = pGen->pEnd;` |
|     123 |  4829 | `	if( pSplit ){` |
|       - |  4830 | `		/* yield $key => $value */` |
|      16 |  4831 | `		pGen->pEnd = pSplit;` |
|      16 |  4832 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4833 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4834 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4835 | `		pGen->pEnd = pTmp;` |
|      16 |  4836 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4837 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4838 | `		iP1 = 1;` |
|      16 |  4839 | `		iP2 = 1;` |
|       9 |  4840 | `	}else{` |
|       - |  4841 | `		/* yield $value */` |
|     109 |  4842 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     109 |  4843 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     109 |  4844 | `		if( rc != SXERR_EMPTY ){` |
|     109 |  4845 | `			iP1 = 1;` |
|      52 |  4846 | `		}` |
|       - |  4847 | `	}` |
|     123 |  4848 | `	pGen->pEnd = pTmp;` |
|     123 |  4849 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     123 |  4850 | `	return SXRET_OK;` |
|      82 |  4851 |  |
|       - |  4852 | `/*` |
|       - |  4853 | ` * Compile the die/exit language construct.` |
|       - |  4854 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4855 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4856 | ` */` |
|     120 |  4857 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4858 |  |
|     125 |  4859 | `	sxi32 nExpr = 0;` |
|       - |  4860 | `	sxi32 rc;` |
|       - |  4861 | `	/* Jump the die/exit keyword */` |
|     125 |  4862 | `	pGen->pIn++;` |
|     125 |  4863 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4864 | `		/* Compile the expression */` |
|     125 |  4865 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4866 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4867 | `			return SXERR_ABORT;` |
|     125 |  4868 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4869 | `			nExpr = 1;` |
|      60 |  4870 | `		}` |
|      60 |  4871 | `	}` |
|       - |  4872 | `	/* Emit the HALT instruction */` |
|     125 |  4873 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4874 | `	return SXRET_OK;` |
|      65 |  4875 |  |
|       - |  4876 | `/*` |
|       - |  4877 | ` * Compile the 'echo' language construct.` |
|       - |  4878 | ` */` |
|   14226 |  4879 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4880 |  |
|   14231 |  4881 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4882 | `	sxi32 rc;` |
|       - |  4883 | `	/* Jump the 'echo' keyword */` |
|   14231 |  4884 | `	pGen->pIn++;` |
|       - |  4885 | `	/* Compile arguments one after one */` |
|   14231 |  4886 | `	pTmp = pGen->pEnd;` |
|   31101 |  4887 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   16875 |  4888 | `		if( pGen->pIn < pNext ){` |
|   16875 |  4889 | `			pGen->pEnd = pNext;` |
|   16875 |  4890 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   16875 |  4891 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4892 | `				return SXERR_ABORT;` |
|   16875 |  4893 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4894 | `				/* Emit the consume instruction */` |
|   16851 |  4895 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8423 |  4896 | `			}` |
|    8435 |  4897 | `		}` |
|       - |  4898 | `		/* Jump trailing commas */` |
|   19519 |  4899 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2649 |  4900 | `			pNext++;` |
|       5 |  4901 | `		}` |
|   16875 |  4902 | `		pGen->pIn = pNext;` |
|       5 |  4903 | `	}` |
|       - |  4904 | `	/* Restore token stream */` |
|   14231 |  4905 | `	pGen->pEnd = pTmp;` |
|   14231 |  4906 | `	return SXRET_OK;` |
|    7118 |  4907 |  |
|       - |  4908 | `/*` |
|       - |  4909 | ` * Compile the static statement.` |
|       - |  4910 | ` * According to the PHP language reference` |
|       - |  4911 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4912 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4913 | ` *  when program execution leaves this scope.` |
|       - |  4914 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4915 | ` * Symisc eXtension.` |
|       - |  4916 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4917 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4918 | ` *  Example` |
|       - |  4919 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4920 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4921 | ` */` |
|       6 |  4922 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4923 |  |
|       - |  4924 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4925 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4926 | `	GenBlock *pBlock;` |
|       - |  4927 | `	SyString *pName;` |
|       - |  4928 | `	char *zDup;` |
|       - |  4929 | `	sxu32 nLine;` |
|       - |  4930 | `	sxi32 rc;` |
|       - |  4931 | `	/* Jump the static keyword */` |
|       8 |  4932 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4933 | `	pGen->pIn++;` |
|       - |  4934 | `	/* Extract the enclosing function if any */` |
|       8 |  4935 | `	pBlock = pGen->pCurrent;` |
|      14 |  4936 | `	while( pBlock ){` |
|      14 |  4937 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4938 | `			break;` |
|       - |  4939 | `		}` |
|       - |  4940 | `		/* Point to the upper block */` |
|       8 |  4941 | `		pBlock = pBlock->pParent;` |
|       2 |  4942 | `	}` |
|       8 |  4943 | `	if( pBlock == 0 ){` |
|       - |  4944 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4945 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4946 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4947 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4948 | `				return SXERR_ABORT;` |
|       - |  4949 | `			}` |
|     ! 0 |  4950 | `			goto Synchronize;` |
|       - |  4951 | `		}` |
|       - |  4952 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4953 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4954 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4955 | `			return SXERR_ABORT;` |
|     ! 0 |  4956 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4957 | `			/* Emit the POP instruction */` |
|     ! 0 |  4958 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4959 | `		}` |
|     ! 0 |  4960 | `		return SXRET_OK;` |
|       - |  4961 | `	}` |
|       8 |  4962 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4963 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4964 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4965 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4966 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4967 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4968 | `				return SXERR_ABORT;` |
|       - |  4969 | `			}` |
|       3 |  4970 | `			goto Synchronize;` |
|       - |  4971 | `	}` |
|       5 |  4972 | `	pGen->pIn++;` |
|       - |  4973 | `	/* Extract variable name */` |
|       5 |  4974 | `	pName = &pGen->pIn->sData;` |
|       5 |  4975 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4976 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4977 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4978 | `		goto Synchronize;` |
|       - |  4979 | `	}` |
|       - |  4980 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4981 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4982 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4983 | `	/* Duplicate variable name */` |
|       5 |  4984 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4985 | `	if( zDup == 0 ){` |
|     ! 0 |  4986 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4987 | `		return SXERR_ABORT;` |
|       - |  4988 | `	}` |
|       5 |  4989 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4990 | `	/* Check if we have an expression to compile */` |
|       5 |  4991 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4992 | `		SySet *pInstrContainer;` |
|       - |  4993 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4994 | `		 * Static variable can take any complex expression including function` |
|       - |  4995 | `		 * call as their initialization value.` |
|       - |  4996 | `		 * Example:` |
|       - |  4997 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4998 | `		 */` |
|       5 |  4999 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  5000 | `		/* Swap bytecode container */` |
|       5 |  5001 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  5002 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  5003 | `		/* Compile the expression */` |
|       5 |  5004 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5005 | `		/* Emit the done instruction */` |
|       5 |  5006 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  5007 | `		/* Restore default bytecode container */` |
|       5 |  5008 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  5009 | `	}` |
|       - |  5010 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  5011 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  5012 | `	return SXRET_OK;` |
|       1 |  5013 | `Synchronize:` |
|       - |  5014 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5015 | `	 * statement.` |
|       - |  5016 | `	 */` |
|       5 |  5017 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5018 | `		pGen->pIn++;` |
|       1 |  5019 | `	}` |
|       3 |  5020 | `	return SXRET_OK;` |
|       5 |  5021 |  |
|       - |  5022 | `/*` |
|       - |  5023 | ` * Compile the var statement.` |
|       - |  5024 | ` * Symisc Extension:` |
|       - |  5025 | ` *      var statement can be used outside of a class definition.` |
|       - |  5026 | ` */` |
|       4 |  5027 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5028 |  |
|       - |  5029 | `	sxu32 nLine;` |
|       - |  5030 | `	sxi32 rc;` |
|       5 |  5031 | `	nLine = pGen->pIn->nLine;` |
|       - |  5032 | `	/* Jump the 'var' keyword */` |
|       5 |  5033 | `	pGen->pIn++;` |
|       5 |  5034 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5035 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5036 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5037 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5038 | `			pGen->pIn++;` |
|     ! 0 |  5039 | `		}` |
|     ! 0 |  5040 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5041 | `			return SXERR_ABORT;` |
|       - |  5042 | `		}` |
|     ! 0 |  5043 | `	}else{` |
|       - |  5044 | `		/* Compile the expression */` |
|       5 |  5045 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5046 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5047 | `			return SXERR_ABORT;` |
|       5 |  5048 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5049 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5050 | `		}` |
|       - |  5051 | `	}` |
|       5 |  5052 | `	return SXRET_OK;` |
|       3 |  5053 |  |
|       - |  5054 | `/*` |
|       - |  5055 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5056 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5057 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5058 | ` */` |
|       - |  5059 | `/*` |
|       - |  5060 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5061 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5062 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5063 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5064 | ` *` |
|       - |  5065 | ` * Resolution order:` |
|       - |  5066 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5067 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5068 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5069 | ` *` |
|       - |  5070 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5071 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5072 | ` * Returns the (possibly new) literal index.` |
|       - |  5073 | ` */` |
|  434752 |  5074 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5075 |  |
|       - |  5076 | `	ph7_value *pLit;` |
|       - |  5077 | `	const char *zLit;` |
|       - |  5078 | `	SyString sQualified;` |
|       - |  5079 | `	sxu32 nLit;` |
|       - |  5080 | `	sxu32 k;` |
|       - |  5081 | `	sxu32 nNewIdx;` |
|       - |  5082 | `	int hasNsSep;` |
|       - |  5083 | `	SyHashEntry *pImport;` |
|       - |  5084 | `	ph7_value *pNew;` |
|  434757 |  5085 | `	if( pFromImport ){` |
|  415557 |  5086 | `		*pFromImport = 0;` |
|  207776 |  5087 | `	}` |
|  434757 |  5088 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  434757 |  5089 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5090 | `		return nOrigIdx;` |
|       - |  5091 | `	}` |
|  434757 |  5092 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  434757 |  5093 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5094 | `	/* Skip if already qualified (contains backslash) */` |
|  434757 |  5095 | `	hasNsSep = 0;` |
| 4702965 |  5096 | `	for( k = 0; k < nLit; k++ ){` |
| 4268221 |  5097 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2134109 |  5098 | `	}` |
|  434757 |  5099 | `	if( hasNsSep ){` |
|      10 |  5100 | `		return nOrigIdx;` |
|       - |  5101 | `	}` |
|       - |  5102 | `	/* Check use imports first (works even outside namespaces) */` |
|  434749 |  5103 | `	SyBlobReset(&pGen->sWorker);` |
|  434749 |  5104 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  434749 |  5105 | `	if( pImport ){` |
|      41 |  5106 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5107 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5108 | `		if( pFromImport ){` |
|      18 |  5109 | `			*pFromImport = 1;` |
|       8 |  5110 | `		}` |
|      23 |  5111 | `	}else{` |
|  434713 |  5112 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  434623 |  5113 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5114 | `		}` |
|       - |  5115 | `		/* Prepend current namespace */` |
|      95 |  5116 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5117 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5118 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5119 | `	}` |
|       - |  5120 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5121 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5122 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5123 | `		return nNewIdx; /* Already interned */` |
|       - |  5124 | `	}` |
|      79 |  5125 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5126 | `	if( pNew == 0 ){` |
|     ! 0 |  5127 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5128 | `	}` |
|      79 |  5129 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5130 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5131 | `	return nNewIdx;` |
|  217381 |  5132 |  |
|       - |  5133 | `/*` |
|       - |  5134 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5135 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5136 | ` */` |
|   95648 |  5137 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5138 |  |
|       - |  5139 | `	SyHashEntry *pImport;` |
|       - |  5140 | `	/* Check use imports first */` |
|   95653 |  5141 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   95653 |  5142 | `	if( pImport ){` |
|      14 |  5143 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  5144 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  5145 | `		return;` |
|       - |  5146 | `	}` |
|       - |  5147 | `	/* Prepend current namespace if active */` |
|   95641 |  5148 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5149 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5150 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5151 | `	}` |
|   95641 |  5152 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   47829 |  5153 |  |
|       - |  5154 | `/*` |
|       - |  5155 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5156 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5157 | ` * The caller must release pOut when done.` |
|       - |  5158 | ` */` |
|  134756 |  5159 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5160 |  |
|  134761 |  5161 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5162 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5163 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5164 | `	}` |
|  134761 |  5165 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  134761 |  5166 |  |
|       - |  5167 | `/*` |
|       - |  5168 | ` * Compile a namespace statement` |
|       - |  5169 | ` * According to the PHP language reference manual` |
|       - |  5170 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5171 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5172 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5173 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5174 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5175 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5176 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5177 | ` *  programming world.` |
|       - |  5178 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5179 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5180 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5181 | ` *  classes/functions/constants.` |
|       - |  5182 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5183 | ` *  readability of source code.` |
|       - |  5184 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5185 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5186 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5187 | ` *       class MyClass {}` |
|       - |  5188 | ` *       function myfunction() {}` |
|       - |  5189 | ` *       const MYCONST = 1;` |
|       - |  5190 | ` *       $a = new MyClass;` |
|       - |  5191 | ` *       $c = new \my\name\MyClass;` |
|       - |  5192 | ` *       $a = strlen('hi');` |
|       - |  5193 | ` *       $d = namespace\MYCONST;` |
|       - |  5194 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5195 | ` *       echo constant($d);` |
|       - |  5196 | ` * NOTE` |
|       - |  5197 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5198 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5199 | ` */` |
|       - |  5200 | `/*` |
|       - |  5201 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5202 | ` */` |
|      14 |  5203 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5204 |  |
|      17 |  5205 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      11 |  5206 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      11 |  5207 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      11 |  5208 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      11 |  5209 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      11 |  5210 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5211 | `	return "token";` |
|      10 |  5212 |  |
|     106 |  5213 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5214 |  |
|       - |  5215 | `	sxu32 nLine;` |
|       - |  5216 | `	sxi32 rc;` |
|     111 |  5217 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5218 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5219 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5220 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5221 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5222 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5223 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5224 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5225 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5226 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5227 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5228 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5229 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5230 | `		return SXRET_OK;` |
|       - |  5231 | `	}` |
|     111 |  5232 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5233 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5234 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5235 | `		return SXRET_OK;` |
|       - |  5236 | `	}` |
|     111 |  5237 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5238 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5239 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5240 | `		return SXRET_OK;` |
|       - |  5241 | `	}` |
|       - |  5242 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5243 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5244 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5245 | `			/* Append backslash separator */` |
|      26 |  5246 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      26 |  5247 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5248 | `			}` |
|      15 |  5249 | `		}else{` |
|       - |  5250 | `			/* Append identifier */` |
|     131 |  5251 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5252 | `		}` |
|     153 |  5253 | `		pGen->pIn++;` |
|       5 |  5254 | `	}` |
|       - |  5255 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5256 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5257 | `	{` |
|     111 |  5258 | `		char *zNsDup = 0;` |
|     111 |  5259 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5260 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5261 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5262 | `		}` |
|     111 |  5263 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5264 | `	}` |
|     111 |  5265 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5266 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5267 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5268 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5269 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5270 | `			return SXERR_ABORT;` |
|       - |  5271 | `		}` |
|       2 |  5272 | `	}` |
|     111 |  5273 | `	return SXRET_OK;` |
|      58 |  5274 |  |
|       - |  5275 | `/*` |
|       - |  5276 | ` * Compile the 'use' statement` |
|       - |  5277 | ` * According to the PHP language reference manual` |
|       - |  5278 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5279 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5280 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5281 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5282 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5283 | ` *  a function or constant is not supported.` |
|       - |  5284 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5285 | ` * NOTE` |
|       - |  5286 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5287 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5288 | ` */` |
|      68 |  5289 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5290 |  |
|       - |  5291 | `	sxu32 nLine;` |
|       - |  5292 | `	sxi32 rc;` |
|       - |  5293 | `	SyBlob sPath;` |
|       - |  5294 | `	SyString sAlias;` |
|       - |  5295 | `	SyToken *pLast;` |
|       - |  5296 | `	char *zDup;` |
|       - |  5297 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5298 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5299 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5300 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5301 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5302 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5303 | `	iUseType = 0;` |
|      73 |  5304 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5305 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5306 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5307 | `			iUseType = 1;` |
|      16 |  5308 | `			pGen->pIn++;` |
|      23 |  5309 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5310 | `			iUseType = 2;` |
|      16 |  5311 | `			pGen->pIn++;` |
|       7 |  5312 | `		}` |
|      14 |  5313 | `	}` |
|       - |  5314 | `	/* Select target hash tables based on import type */` |
|      73 |  5315 | `	switch( iUseType ){` |
|       7 |  5316 | `		case 1:` |
|      16 |  5317 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5318 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5319 | `			break;` |
|       7 |  5320 | `		case 2:` |
|      16 |  5321 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5322 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5323 | `			break;` |
|      20 |  5324 | `		default:` |
|      45 |  5325 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5326 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5327 | `			break;` |
|       - |  5328 | `	}` |
|      73 |  5329 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5330 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5331 | `	for(;;){` |
|      75 |  5332 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5333 | `			break;` |
|       - |  5334 | `		}` |
|      75 |  5335 | `		SyBlobReset(&sPath);` |
|      75 |  5336 | `		pLast = 0;` |
|       - |  5337 | `		/* Collect the full namespace path */` |
|     261 |  5338 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5339 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5340 | `				pLast = pGen->pIn;` |
|     131 |  5341 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5342 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5343 | `				}` |
|     131 |  5344 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5345 | `			}` |
|     191 |  5346 | `			pGen->pIn++;` |
|       5 |  5347 | `		}` |
|      75 |  5348 | `		if( pLast == 0 ){` |
|       - |  5349 | `			/* Empty path */` |
|       5 |  5350 | `			break;` |
|       - |  5351 | `		}` |
|       - |  5352 | `		/* Default alias is the last component of the path */` |
|      71 |  5353 | `		sAlias = pLast->sData;` |
|       - |  5354 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5355 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5356 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5357 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5358 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5359 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5360 | `				pGen->pIn++;` |
|       8 |  5361 | `			}` |
|       8 |  5362 | `		}` |
|       - |  5363 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5364 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5366 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5367 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5368 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5369 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5370 | `				return SXERR_ABORT;` |
|       - |  5371 | `			}` |
|       2 |  5372 | `		}` |
|       - |  5373 | `		/* Register the import: alias -> FQN.` |
|       - |  5374 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5375 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5376 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5377 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5378 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5379 | `		if( zDup ){` |
|      71 |  5380 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5381 | `			if( pVmHash ){` |
|       - |  5382 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5383 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5384 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5385 | `				if( zAliasDup ){` |
|      43 |  5386 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5387 | `				}` |
|      19 |  5388 | `			}` |
|      71 |  5389 | `			if( iUseType == 2 ){` |
|       - |  5390 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5391 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5392 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5393 | `				if( zAliasDup ){` |
|       - |  5394 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5395 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5396 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5397 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5398 | `					if( azPair ){` |
|      16 |  5399 | `						azPair[0] = zAliasDup;` |
|      16 |  5400 | `						azPair[1] = zDup;` |
|      16 |  5401 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5402 | `					}` |
|       7 |  5403 | `				}` |
|       7 |  5404 | `			}` |
|      33 |  5405 | `		}` |
|       - |  5406 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5407 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5408 | `			pGen->pIn++;` |
|       2 |  5409 | `		}else{` |
|      37 |  5410 | `			break;` |
|       - |  5411 | `		}` |
|       1 |  5412 | `	}` |
|      73 |  5413 | `	SyBlobRelease(&sPath);` |
|      73 |  5414 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5415 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5416 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5417 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5418 | `			return SXERR_ABORT;` |
|       - |  5419 | `		}` |
|       1 |  5420 | `	}` |
|      73 |  5421 | `	return SXRET_OK;` |
|      39 |  5422 |  |
|       - |  5423 | `/*` |
|       - |  5424 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5425 | ` *` |
|       - |  5426 | ` * According to the PHP language reference manual.` |
|       - |  5427 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5428 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5429 | ` *  declare (directive)` |
|       - |  5430 | ` *   statement` |
|       - |  5431 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5432 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5433 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5434 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5435 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5436 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5437 | ` * <?php` |
|       - |  5438 | ` * // these are the same:` |
|       - |  5439 | ` * // you can use this:` |
|       - |  5440 | ` * declare(ticks=1) {` |
|       - |  5441 | ` *   // entire script here` |
|       - |  5442 | ` * }` |
|       - |  5443 | ` * // or you can use this:` |
|       - |  5444 | ` * declare(ticks=1);` |
|       - |  5445 | ` * // entire script here` |
|       - |  5446 | ` * ?>` |
|       - |  5447 | ` *` |
|       - |  5448 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5449 | ` */` |
|       - |  5450 | `/*` |
|       - |  5451 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5452 | ` */` |
|      68 |  5453 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5454 |  |
|     103 |  5455 | `	return SyStringLength(pName) == nWant` |
|      68 |  5456 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5457 |  |
|       - |  5458 |  |
|      40 |  5459 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5460 |  |
|      45 |  5461 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5462 | `	SyToken *pBodyEnd = 0;` |
|       - |  5463 | `	SyToken *pBodyStart;` |
|       - |  5464 | `	SyToken *pCursor;` |
|       - |  5465 | `	int bHasStrictTypes;` |
|       - |  5466 | `	int bBlockForm;` |
|       - |  5467 | `	int bPlacementOk;` |
|       - |  5468 | `	sxi32 rc;` |
|      45 |  5469 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5470 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5471 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5472 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5473 | `			return SXERR_ABORT;` |
|       - |  5474 | `		}` |
|       5 |  5475 | `		goto Synchro;` |
|       - |  5476 | `	}` |
|      41 |  5477 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5478 | `	pBodyStart = pGen->pIn;` |
|       - |  5479 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5480 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5481 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5482 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5483 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5484 | `			return SXERR_ABORT;` |
|       - |  5485 | `		}` |
|     ! 0 |  5486 | `		return SXRET_OK;` |
|       - |  5487 | `	}` |
|       - |  5488 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5489 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5490 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5491 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5492 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5493 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5494 | `			return SXERR_ABORT;` |
|       - |  5495 | `		}` |
|     ! 0 |  5496 | `	}` |
|      41 |  5497 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5498 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5499 | `	bHasStrictTypes = 0;` |
|       - |  5500 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5501 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5502 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5503 | `	pCursor = pBodyStart;` |
|      53 |  5504 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5505 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5506 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5507 | `				bHasStrictTypes = 1;` |
|      37 |  5508 | `				break;` |
|       - |  5509 | `			}` |
|       2 |  5510 | `		}` |
|      14 |  5511 | `		pCursor++;` |
|       2 |  5512 | `	}` |
|      41 |  5513 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5514 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5515 | `			"strict_types declaration must not use block mode");` |
|       3 |  5516 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5517 | `		return SXRET_OK;` |
|       - |  5518 | `	}` |
|      39 |  5519 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5520 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5521 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5522 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5523 | `		return SXRET_OK;` |
|       - |  5524 | `	}` |
|       - |  5525 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5526 | `	pCursor = pBodyStart;` |
|      65 |  5527 | `	while( pCursor < pBodyEnd ){` |
|       - |  5528 | `		SyToken *pNameTok;` |
|       - |  5529 | `		SyToken *pEqTok;` |
|       - |  5530 | `		SyToken *pValTok;` |
|       - |  5531 | `		SyString *pDirName;` |
|       - |  5532 | `		int bIsStrict;` |
|       - |  5533 | `		int iStrictValue;` |
|      37 |  5534 | `		pNameTok = pCursor;` |
|      37 |  5535 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5536 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5537 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5538 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5539 | `			return SXRET_OK;` |
|       - |  5540 | `		}` |
|      37 |  5541 | `		pEqTok = pNameTok + 1;` |
|      37 |  5542 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5543 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5544 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5545 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5546 | `			return SXRET_OK;` |
|       - |  5547 | `		}` |
|      37 |  5548 | `		pValTok = pEqTok + 1;` |
|      37 |  5549 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5550 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5551 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5552 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5553 | `			return SXRET_OK;` |
|       - |  5554 | `		}` |
|      37 |  5555 | `		pDirName = &pNameTok->sData;` |
|      37 |  5556 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5557 | `		if( bIsStrict ){` |
|       - |  5558 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5559 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5560 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5561 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5562 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5563 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5564 | `				return SXRET_OK;` |
|       - |  5565 | `			}` |
|      33 |  5566 | `			iStrictValue = -1;` |
|      33 |  5567 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5568 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5569 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5570 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5571 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5572 | `			}` |
|      33 |  5573 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5574 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5575 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5576 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5577 | `				return SXRET_OK;` |
|       - |  5578 | `			}` |
|      30 |  5579 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5580 | `		}else{` |
|       - |  5581 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5582 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5583 | `			 * behavior don't regress. */` |
|       8 |  5584 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5585 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5586 | `				ph7_lib_version()` |
|       - |  5587 | `				);` |
|       - |  5588 | `		}` |
|      34 |  5589 | `		pCursor = pValTok + 1;` |
|       - |  5590 | `		/* Consume separating comma (or end). */` |
|      34 |  5591 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5592 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5593 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5594 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5595 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5596 | `				return SXRET_OK;` |
|       - |  5597 | `			}` |
|       3 |  5598 | `			pCursor++;` |
|       1 |  5599 | `		}` |
|       4 |  5600 | `	}` |
|       - |  5601 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5602 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5603 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      32 |  5604 | `	return SXRET_OK;` |
|       2 |  5605 | `Synchro:` |
|       - |  5606 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5607 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5608 | `		pGen->pIn++;` |
|       1 |  5609 | `	}` |
|       5 |  5610 | `	return SXRET_OK;` |
|      25 |  5611 |  |
|       - |  5612 | `/*` |
|       - |  5613 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5614 | ` * as follows:` |
|       - |  5615 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5616 | ` * {` |
|       - |  5617 | ` *   return "Making a cup of $type.\n";` |
|       - |  5618 | ` * }` |
|       - |  5619 | ` * Symisc eXtension.` |
|       - |  5620 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5621 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5622 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5623 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5624 | ` *      {` |
|       - |  5625 | ` *       var_dump($a);` |
|       - |  5626 | ` *      }` |
|       - |  5627 | ` *     //call test without args` |
|       - |  5628 | ` *      test();` |
|       - |  5629 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5630 | ` *      Example:` |
|       - |  5631 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5632 | ` * 3 -) Function overloading!!` |
|       - |  5633 | ` *      Example:` |
|       - |  5634 | ` *      function foo($a) {` |
|       - |  5635 | ` *   	  return $a.PHP_EOL;` |
|       - |  5636 | ` *	    }` |
|       - |  5637 | ` *	    function foo($a, $b) {` |
|       - |  5638 | ` *   	  return $a + $b;` |
|       - |  5639 | ` *	    }` |
|       - |  5640 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5641 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5642 | ` *      // Same arg` |
|       - |  5643 | ` *	   function foo(string $a)` |
|       - |  5644 | ` *	   {` |
|       - |  5645 | ` *	     echo "a is a string\n";` |
|       - |  5646 | ` *	     var_dump($a);` |
|       - |  5647 | ` *	   }` |
|       - |  5648 | ` *	  function foo(int $a)` |
|       - |  5649 | ` *	  {` |
|       - |  5650 | ` *	    echo "a is integer\n";` |
|       - |  5651 | ` *	    var_dump($a);` |
|       - |  5652 | ` *	  }` |
|       - |  5653 | ` *	  function foo(array $a)` |
|       - |  5654 | ` *	  {` |
|       - |  5655 | ` * 	    echo "a is an array\n";` |
|       - |  5656 | ` * 	    var_dump($a);` |
|       - |  5657 | ` *	  }` |
|       - |  5658 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5659 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5660 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5661 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5662 | ` * introduced by the PH7 engine.` |
|       - |  5663 | ` */` |
|   66764 |  5664 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5665 |  |
|       - |  5666 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5667 | `	SySet *pInstrContainer;` |
|       - |  5668 | `	sxi32 rc;` |
|       - |  5669 | `	/* Swap token stream */` |
|   66769 |  5670 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   66769 |  5671 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   66769 |  5672 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5673 | `	/* Compile the expression holding the argument value */` |
|   66769 |  5674 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5675 | `	/* Emit the done instruction */` |
|   66769 |  5676 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   66769 |  5677 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   66769 |  5678 | `	RE_SWAP_DELIMITER(pGen);` |
|   66769 |  5679 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5680 | `		return SXERR_ABORT;` |
|       - |  5681 | `	}` |
|   66769 |  5682 | `	return SXRET_OK;` |
|   33387 |  5683 |  |
|       - |  5684 | `/*` |
|       - |  5685 | ` * Collect function arguments one after one.` |
|       - |  5686 | ` * According to the PHP language reference manual.` |
|       - |  5687 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5688 | ` * list of expressions.` |
|       - |  5689 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5690 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5691 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5692 | ` * for more information.` |
|       - |  5693 | ` * Example #1 Passing arrays to functions` |
|       - |  5694 | ` * <?php` |
|       - |  5695 | ` * function takes_array($input)` |
|       - |  5696 | ` * {` |
|       - |  5697 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5698 | ` * }` |
|       - |  5699 | ` * ?>` |
|       - |  5700 | ` * Making arguments be passed by reference` |
|       - |  5701 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5702 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5703 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5704 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5705 | ` * to the argument name in the function definition:` |
|       - |  5706 | ` * Example #2 Passing function parameters by reference` |
|       - |  5707 | ` * <?php` |
|       - |  5708 | ` * function add_some_extra(&$string)` |
|       - |  5709 | ` * {` |
|       - |  5710 | ` *   $string .= 'and something extra.';` |
|       - |  5711 | ` * }` |
|       - |  5712 | ` * $str = 'This is a string, ';` |
|       - |  5713 | ` * add_some_extra($str);` |
|       - |  5714 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5715 | ` * ?>` |
|       - |  5716 | ` *` |
|       - |  5717 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5718 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5719 | ` * on these extension.` |
|       - |  5720 | ` */` |
|   92600 |  5721 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5722 |  |
|       - |  5723 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5724 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5725 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5726 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5727 | `	sxi32 rc;` |
|       - |  5728 |  |
|   92605 |  5729 | `	pIn = pGen->pIn;` |
|   92605 |  5730 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5731 | `	/* Process arguments one after one */` |
|  115786 |  5732 | `	for(;;){` |
|  231577 |  5733 | `		if( pIn >= pEnd ){` |
|       - |  5734 | `			/* No more arguments to process */` |
|   92591 |  5735 | `			break;` |
|       - |  5736 | `		}` |
|  138991 |  5737 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  138991 |  5738 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  138991 |  5739 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  138991 |  5740 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5741 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5742 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5743 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5744 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5745 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5746 | `		{` |
|  138991 |  5747 | `			int bReadonly = 0, bVisSeen = 0;` |
|  138991 |  5748 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  138991 |  5749 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5750 | `				bReadonly = 1;` |
|       3 |  5751 | `				pIn++;` |
|       1 |  5752 | `			}` |
|  138991 |  5753 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   63443 |  5754 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   63443 |  5755 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5756 | `					bVisSeen = 1;` |
|      71 |  5757 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5758 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5759 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5760 | `					pIn++;` |
|      71 |  5761 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5762 | `						bReadonly = 1;` |
|      16 |  5763 | `						pIn++;` |
|       6 |  5764 | `					}` |
|      33 |  5765 | `				}` |
|   31719 |  5766 | `			}` |
|  138991 |  5767 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5768 | `				if( !bCtorCtx ){` |
|       6 |  5769 | `					if( bAbstractCtx ){` |
|       3 |  5770 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5771 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5772 | `					}else{` |
|       3 |  5773 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5774 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5775 | `					}` |
|       6 |  5776 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5777 | `						return SXERR_ABORT;` |
|       - |  5778 | `					}` |
|       6 |  5779 | `					return SXERR_SYNTAX;` |
|       - |  5780 | `				}` |
|      69 |  5781 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5782 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5783 | `				if( bReadonly ){` |
|      18 |  5784 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5785 | `				}` |
|      32 |  5786 | `			}` |
|       - |  5787 | `		}` |
|       - |  5788 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  177765 |  5789 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  110043 |  5790 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   79340 |  5791 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   77549 |  5792 | `			sxu32 nLineLocal = pIn->nLine;` |
|   77549 |  5793 | `			sxi32 iTFlags = 0;` |
|   77549 |  5794 | `			pGen->pIn = pIn;` |
|   77549 |  5795 | `			rc = GenStateParseUnionTypeDecl(` |
|   38772 |  5796 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   38772 |  5797 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5798 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5799 | `				/* bAllowVoid */ 0,` |
|   38772 |  5800 | `						nLineLocal);` |
|   77549 |  5801 | `			pIn = pGen->pIn;` |
|   77549 |  5802 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5803 | `				return SXERR_ABORT;` |
|   77549 |  5804 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5805 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5806 | `				return SXERR_SYNTAX;` |
|   77547 |  5807 | `			}else if( rc == SXERR_SYNTAX ){` |
|       9 |  5808 | `				if( pIn < pEnd ){` |
|      12 |  5809 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5810 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       3 |  5811 | `						&pIn->sData);` |
|       6 |  5812 | `				}else{` |
|     ! 0 |  5813 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5814 | `						"syntax error, unexpected end of file");` |
|       - |  5815 | `				}` |
|       9 |  5816 | `				return SXERR_SYNTAX;` |
|       - |  5817 | `			}` |
|   77541 |  5818 | `			sArg.iFlags \|= iTFlags;` |
|   38768 |  5819 | `		}` |
|  138979 |  5820 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5821 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5822 | `			return rc;` |
|       - |  5823 | `		}` |
|  138979 |  5824 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5825 | `			/* Pass by reference,record that */` |
|    3545 |  5826 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3545 |  5827 | `			pIn++;` |
|    1770 |  5828 | `		}` |
|  138979 |  5829 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5830 | `			/* Variadic parameter: ...$args */` |
|      46 |  5831 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      46 |  5832 | `			pIn++;` |
|      21 |  5833 | `		}` |
|  138979 |  5834 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5835 | `			/* Invalid argument */` |
|     ! 0 |  5836 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5837 | `			return rc;` |
|       - |  5838 | `		}` |
|  138979 |  5839 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5840 | `		/* Copy argument name */` |
|  138979 |  5841 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  138979 |  5842 | `		if( zDup == 0 ){` |
|     ! 0 |  5843 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5844 | `			return SXERR_ABORT;` |
|       - |  5845 | `		}` |
|  138979 |  5846 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  138979 |  5847 | `		pIn++;` |
|  138979 |  5848 | `		if( pIn < pEnd ){` |
|   78035 |  5849 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5850 | `				SyToken *pDefend;` |
|   66771 |  5851 | `				sxi32 iNest = 0;` |
|   66771 |  5852 | `				pIn++; /* Jump the equal sign */` |
|   66771 |  5853 | `				pDefend = pIn;` |
|       - |  5854 | `				/* Process the default value associated with this argument */` |
|  140559 |  5855 | `				while( pDefend < pEnd ){` |
|  108921 |  5856 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   35133 |  5857 | `						break;` |
|       - |  5858 | `					}` |
|   73793 |  5859 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5860 | `						/* Increment nesting level */` |
|    3517 |  5861 | `						iNest++;` |
|   72037 |  5862 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5863 | `						/* Decrement nesting level */` |
|    3517 |  5864 | `						iNest--;` |
|    1756 |  5865 | `					}` |
|   73793 |  5866 | `					pDefend++;` |
|       5 |  5867 | `				}` |
|   66771 |  5868 | `				if( pIn >= pDefend ){` |
|       3 |  5869 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5870 | `					return rc;` |
|       - |  5871 | `				}` |
|       - |  5872 | `				/* Process default value */` |
|   66769 |  5873 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   66769 |  5874 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5875 | `					return rc;` |
|       - |  5876 | `				}` |
|       - |  5877 | `				/* Point beyond the default value */` |
|   66769 |  5878 | `				pIn = pDefend;` |
|   33382 |  5879 | `			}` |
|   78033 |  5880 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5881 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5882 | `				return rc;` |
|       - |  5883 | `			}` |
|   78033 |  5884 | `			pIn++; /* Jump the trailing comma */` |
|   39014 |  5885 | `		}` |
|       - |  5886 | `		/* Append argument signature */` |
|  138977 |  5887 | `		if( sArg.nType > 0 ){` |
|   77487 |  5888 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5889 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14083 |  5890 | `				int marker = 'o';` |
|   14083 |  5891 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14083 |  5892 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7044 |  5893 | `			}else{` |
|       - |  5894 | `				int c;` |
|   63409 |  5895 | `				c = 'n'; /* cc warning */` |
|       - |  5896 | `				/* Type leading character */` |
|   63409 |  5897 | `				switch(sArg.nType){` |
|       3 |  5898 | `				case MEMOBJ_HASHMAP:` |
|       - |  5899 | `					/* Hashmap aka 'array' */` |
|       7 |  5900 | `					c = 'h';` |
|       7 |  5901 | `					break;` |
|    8836 |  5902 | `				case MEMOBJ_INT:` |
|       - |  5903 | `					/* Integer */` |
|   17677 |  5904 | `					c = 'i';` |
|   17677 |  5905 | `					break;` |
|       1 |  5906 | `				case MEMOBJ_BOOL:` |
|       - |  5907 | `					/* Bool */` |
|       3 |  5908 | `					c = 'b';` |
|       3 |  5909 | `					break;` |
|       2 |  5910 | `				case MEMOBJ_REAL:` |
|       - |  5911 | `					/* Float */` |
|       5 |  5912 | `					c = 'f';` |
|       5 |  5913 | `					break;` |
|   22852 |  5914 | `				case MEMOBJ_STRING:` |
|       - |  5915 | `					/* String */` |
|   45709 |  5916 | `					c = 's';` |
|   45709 |  5917 | `					break;` |
|       7 |  5918 | `				case MEMOBJ_OBJ:` |
|       - |  5919 | `					/* Object */` |
|      16 |  5920 | `					c = 'o';` |
|      14 |  5921 | `					break;` |
|       1 |  5922 | `				default:` |
|       2 |  5923 | `					break;` |
|       - |  5924 | `				}` |
|   63409 |  5925 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5926 | `			}` |
|   38746 |  5927 | `		}else{` |
|       - |  5928 | `			/* No type is associated with this parameter which mean` |
|       - |  5929 | `			 * that this function is not condidate for overloading.` |
|       - |  5930 | `			 */` |
|   61495 |  5931 | `			SyBlobRelease(&sSig);` |
|       - |  5932 | `		}` |
|       - |  5933 | `		/* Save in the argument set */` |
|  138977 |  5934 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5935 | `	}` |
|   92591 |  5936 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5937 | `		/* Save function signature */` |
|   49357 |  5938 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   24676 |  5939 | `	}` |
|   92591 |  5940 | `	return SXRET_OK;` |
|   46305 |  5941 |  |
|       - |  5942 | `/*` |
|       - |  5943 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5944 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5945 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5946 | ` */` |
|  219962 |  5947 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5948 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5949 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5950 | `	)` |
|       5 |  5951 |  |
|       - |  5952 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5953 | `	GenBlock *pBlock;` |
|       - |  5954 | `	sxu32 nGotoOfft;` |
|       - |  5955 | `	sxi32 rc;` |
|       - |  5956 | `	/* Attach the new function */` |
|  219967 |  5957 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  219967 |  5958 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5959 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5960 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5961 | `		return SXERR_ABORT;` |
|       - |  5962 | `	}` |
|  219967 |  5963 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5964 | `	/* Swap bytecode containers */` |
|  219967 |  5965 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  219967 |  5966 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5967 | `	/* Emit constructor property promotion prologue:` |
|       - |  5968 | `	 *   $this->NAME = $NAME;` |
|       - |  5969 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5970 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5971 | `	{` |
|  219967 |  5972 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5973 | `		sxu32 i;` |
|  330717 |  5974 | `		for( i = 0; i < nArg; i++ ){` |
|  110755 |  5975 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5976 | `			char *zSrc;` |
|       - |  5977 | `			sxu32 nSrc,nName;` |
|       - |  5978 | `			SySet sToken;` |
|       - |  5979 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5980 | `			sxi32 rcPromote;` |
|  110755 |  5981 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  110701 |  5982 | `				continue;` |
|       - |  5983 | `			}` |
|       - |  5984 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5985 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5986 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5987 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5988 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  5989 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  5990 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  5991 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  5992 | `			if( zSrc == 0 ){` |
|     ! 0 |  5993 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5994 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5995 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5996 | `				return SXERR_ABORT;` |
|       - |  5997 | `			}` |
|       - |  5998 | `			{` |
|      59 |  5999 | `				char *z = zSrc;` |
|      59 |  6000 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  6001 | `				z += sizeof("$this->")-1;` |
|      59 |  6002 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6003 | `				z += nName;` |
|      59 |  6004 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  6005 | `				z += sizeof(" = $")-1;` |
|      59 |  6006 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  6007 | `				z += nName;` |
|      59 |  6008 | `				*z = 0;` |
|       - |  6009 | `			}` |
|      59 |  6010 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  6011 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6012 | `			pTmpIn = pGen->pIn;` |
|      59 |  6013 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6014 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6015 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6016 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6017 | `			pGen->pIn = pTmpIn;` |
|      59 |  6018 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6019 | `			SySetRelease(&sToken);` |
|      59 |  6020 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6021 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6022 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6023 | `				return SXERR_ABORT;` |
|       - |  6024 | `			}` |
|       - |  6025 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6026 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6027 | `		}` |
|       - |  6028 | `	}` |
|       - |  6029 | `	/* Compile the body */` |
|  219967 |  6030 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6031 | `	/* Fix exception jumps now the destination is resolved */` |
|  219967 |  6032 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6033 | `	/* Emit the final return if not yet done */` |
|  219967 |  6034 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6035 | `	/* Fix gotos jumps now the destination is resolved */` |
|  219967 |  6036 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6037 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6038 | `	}` |
|  219967 |  6039 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6040 | `	/* Restore the default container */` |
|  219967 |  6041 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6042 | `	/* Leave function block */` |
|  219967 |  6043 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  219967 |  6044 | `	if( rc == SXERR_ABORT ){` |
|       - |  6045 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6046 | `		return SXERR_ABORT;` |
|       - |  6047 | `	}` |
|       - |  6048 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6049 | `	{` |
|  219967 |  6050 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6051 | `		sxu32 i;` |
| 4294443 |  6052 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4074571 |  6053 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      95 |  6054 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      95 |  6055 | `				break;` |
|       - |  6056 | `			}` |
| 2037243 |  6057 | `		}` |
|       - |  6058 | `	}` |
|       - |  6059 | `	/* All done, function body compiled */` |
|  219967 |  6060 | `	return SXRET_OK;` |
|  109986 |  6061 |  |
|       - |  6062 | `/*` |
|       - |  6063 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6064 | ` * According to the PHP language reference manual.` |
|       - |  6065 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6066 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6067 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6068 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6069 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6070 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6071 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6072 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6073 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6074 | ` *` |
|       - |  6075 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6076 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6077 | ` * on these extension.` |
|       - |  6078 | ` */` |
|       - |  6079 | `/*` |
|       - |  6080 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6081 | ` */` |
|     482 |  6082 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6083 |  |
|       - |  6084 | `	sxu32 i;` |
|    1335 |  6085 | `	for( i = 0; i < n; i++ ){` |
|    1147 |  6086 | `		int a = zA[i], b = zB[i];` |
|    1147 |  6087 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1147 |  6088 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1147 |  6089 | `		if( a != b ) return a - b;` |
|     429 |  6090 | `	}` |
|     193 |  6091 | `	return 0;` |
|     246 |  6092 |  |
|       - |  6093 | `/*` |
|       - |  6094 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6095 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6096 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6097 | ` */` |
|       - |  6098 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6099 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6100 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6101 |  |
|       - |  6102 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6103 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6104 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6105 |  |
|       - |  6106 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6107 | `struct PhlTypeAtom {` |
|       - |  6108 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6109 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6110 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6111 | `	sxu32 nCanon;` |
|       - |  6112 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6113 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6114 | `};` |
|       - |  6115 |  |
|       - |  6116 | `/*` |
|       - |  6117 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6118 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6119 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6120 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6121 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6122 | ` * already be consumed by the caller.` |
|       - |  6123 | ` */` |
|   78364 |  6124 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6125 |  |
|   78369 |  6126 | `	SyToken *pIn = pGen->pIn;` |
|   78369 |  6127 | `	SyZero(pOut, sizeof(*pOut));` |
|   78369 |  6128 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   78369 |  6129 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6130 | `		return SXERR_SYNTAX;` |
|       - |  6131 | `	}` |
|       - |  6132 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   78369 |  6133 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6134 | `		pIn++;` |
|       8 |  6135 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6136 | `			return SXERR_SYNTAX;` |
|       - |  6137 | `		}` |
|       3 |  6138 | `	}` |
|   78369 |  6139 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6140 | `		return SXERR_SYNTAX;` |
|       - |  6141 | `	}` |
|   78369 |  6142 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   63935 |  6143 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   63935 |  6144 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6145 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   63921 |  6146 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      69 |  6147 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   63875 |  6148 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   17925 |  6149 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   54883 |  6150 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   45861 |  6151 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   22995 |  6152 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6153 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      53 |  6154 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6155 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      25 |  6156 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       7 |  6157 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      11 |  6158 | `			pOut->nType = SXU32_HIGH;` |
|      11 |  6159 | `			pOut->sClass = pIn->sData;` |
|       7 |  6160 | `		}else{` |
|       3 |  6161 | `			return SXERR_SYNTAX;` |
|       - |  6162 | `		}` |
|   63933 |  6163 | `		pIn++;` |
|   31969 |  6164 | `	}else{` |
|       - |  6165 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6166 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   14439 |  6167 | `		SyString *pT = &pIn->sData;` |
|   14439 |  6168 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6169 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6170 | `			pIn++;` |
|   14425 |  6171 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6172 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6173 | `			pIn++;` |
|   14335 |  6174 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6175 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6176 | `			pIn++;` |
|       2 |  6177 | `		}else{` |
|       - |  6178 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14257 |  6179 | `			SyToken *pFirst = pIn;` |
|   14257 |  6180 | `			SyToken *pLast = pIn;` |
|   14257 |  6181 | `			pOut->nType = SXU32_HIGH;` |
|   14257 |  6182 | `			pOut->sClass = pIn->sData;` |
|   14257 |  6183 | `			pIn++;` |
|   21381 |  6184 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14260 |  6185 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6186 | `				pLast = &pIn[1];` |
|       3 |  6187 | `				pIn += 2;` |
|       1 |  6188 | `			}` |
|   14257 |  6189 | `			if( pLast != pFirst ){` |
|       3 |  6190 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6191 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6192 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6193 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6194 | `			}` |
|       - |  6195 | `		}` |
|       - |  6196 | `	}` |
|   78367 |  6197 | `	pGen->pIn = pIn;` |
|   78367 |  6198 | `	return SXRET_OK;` |
|   39187 |  6199 |  |
|       - |  6200 |  |
|       - |  6201 | `/*` |
|       - |  6202 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6203 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6204 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6205 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6206 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6207 | ` */` |
|   78210 |  6208 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6209 |  |
|       - |  6210 | `	int i;` |
|   78215 |  6211 | `	int nNonNull = 0;` |
|   78215 |  6212 | `	int bAnyIntersection = 0;` |
|       - |  6213 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   78215 |  6214 | `	sxu32 nMaxGroup = 0;` |
| 2580935 |  6215 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  156559 |  6216 | `	for( i = 0; i < nAtoms; i++ ){` |
|   78349 |  6217 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   78321 |  6218 | `			nNonNull++;` |
|   78321 |  6219 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   78321 |  6220 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   78321 |  6221 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   39158 |  6222 | `			}` |
|   39158 |  6223 | `		}` |
|   39177 |  6224 | `	}` |
|  156525 |  6225 | `	for( i = 0; i < nAtoms; i++ ){` |
|   78331 |  6226 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      19 |  6227 | `			bAnyIntersection = 1;` |
|      19 |  6228 | `			break;` |
|       - |  6229 | `		}` |
|   39160 |  6230 | `	}` |
|   78215 |  6231 | `	if( bAnyIntersection ){` |
|       - |  6232 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6233 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6234 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      19 |  6235 | `		sxu32 g, nGroups = 0;` |
|      19 |  6236 | `		int bFirstGroup = 1;` |
|      39 |  6237 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      39 |  6238 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      23 |  6239 | `			int bFirstMember = 1;` |
|       - |  6240 | `			int bWrap;` |
|      23 |  6241 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6242 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6243 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6244 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6245 | `			 * parens, matching PHP's canonical text. */` |
|      31 |  6246 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      23 |  6247 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      23 |  6248 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      71 |  6249 | `			for( i = 0; i < nAtoms; i++ ){` |
|      51 |  6250 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      39 |  6251 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      39 |  6252 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      37 |  6253 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      20 |  6254 | `				}else{` |
|       3 |  6255 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6256 | `				}` |
|      39 |  6257 | `				bFirstMember = 0;` |
|      21 |  6258 | `			}` |
|      23 |  6259 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      23 |  6260 | `			bFirstGroup = 0;` |
|      13 |  6261 | `		}` |
|      19 |  6262 | `		if( bNullable ){` |
|     ! 0 |  6263 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6264 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6265 | `		}` |
|      57 |  6266 | `		return;` |
|       - |  6267 | `	}` |
|   78199 |  6268 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6269 | `		/* Shorthand: ?T */` |
|      81 |  6270 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6271 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6272 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6273 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6274 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6275 | `			}else{` |
|      63 |  6276 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6277 | `			}` |
|      81 |  6278 | `			return;` |
|     ! 0 |  6279 | `		}` |
|     ! 0 |  6280 | `	}` |
|       - |  6281 | `	{` |
|   78123 |  6282 | `		int bFirst = 1;` |
|       - |  6283 | `		/* 1) Classes in declaration order */` |
|  156343 |  6284 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78225 |  6285 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14213 |  6286 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14213 |  6287 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14213 |  6288 | `				bFirst = 0;` |
|    7104 |  6289 | `			}` |
|   39115 |  6290 | `		}` |
|       - |  6291 | `		/* 2) Built-ins in canonical order */` |
|       - |  6292 | `		{` |
|       - |  6293 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6294 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6295 | `			int k;` |
|  546831 |  6296 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  874079 |  6297 | `				for( i = 0; i < nAtoms; i++ ){` |
|  469217 |  6298 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   63851 |  6299 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   63851 |  6300 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   63851 |  6301 | `						bFirst = 0;` |
|   63851 |  6302 | `						break;` |
|       - |  6303 | `					}` |
|  202688 |  6304 | `				}` |
|  234359 |  6305 | `			}` |
|       - |  6306 | `		}` |
|       - |  6307 | `		/* 3) null suffix */` |
|   78123 |  6308 | `		if( bNullable ){` |
|      20 |  6309 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6310 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6311 | `		}` |
|       - |  6312 | `	}` |
|   39110 |  6313 |  |
|       - |  6314 |  |
|       - |  6315 | `/*` |
|       - |  6316 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6317 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6318 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6319 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6320 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6321 | ` * whether it was parenthesized.` |
|       - |  6322 | ` *` |
|       - |  6323 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6324 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6325 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6326 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6327 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6328 | ` */` |
|   78346 |  6329 | `static sxi32 GenStateParsePart(` |
|       - |  6330 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6331 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6332 |  |
|       - |  6333 | `	sxi32 rc;` |
|   78351 |  6334 | `	int nMembers = 0;` |
|   78351 |  6335 | `	int bParen = 0;` |
|   78351 |  6336 | `	*pnMembers = 0;` |
|   78351 |  6337 | `	*pbParen = 0;` |
|   78351 |  6338 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6339 | `		bParen = 1;` |
|       6 |  6340 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6341 | `	}` |
|   39173 |  6342 | `	for(;;){` |
|   78369 |  6343 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6344 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6345 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6346 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6347 | `		}` |
|   78369 |  6348 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   78369 |  6349 | `		if( rc != SXRET_OK ){` |
|       3 |  6350 | `			return rc;` |
|       - |  6351 | `		}` |
|   78367 |  6352 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   78367 |  6353 | `		(*pnAtoms)++;` |
|   78367 |  6354 | `		nMembers++;` |
|       - |  6355 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   78367 |  6356 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6357 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6358 | `			if( pNext < pGen->pEnd` |
|      24 |  6359 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6360 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6361 | `				continue;` |
|       - |  6362 | `			}` |
|       1 |  6363 | `		}` |
|   78349 |  6364 | `		break;` |
|     ! 0 |  6365 | `	}` |
|   78349 |  6366 | `	if( bParen ){` |
|       6 |  6367 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6368 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6369 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6370 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6371 | `		}` |
|       6 |  6372 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6373 | `		if( nMembers < 2 ){` |
|     ! 0 |  6374 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6375 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6376 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6377 | `		}` |
|       2 |  6378 | `	}` |
|   78349 |  6379 | `	*pnMembers = nMembers;` |
|   78349 |  6380 | `	*pbParen = bParen;` |
|   78349 |  6381 | `	return SXRET_OK;` |
|   39178 |  6382 |  |
|       - |  6383 |  |
|       - |  6384 | `/*` |
|       - |  6385 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6386 | ` *` |
|       - |  6387 | ` * Outputs:` |
|       - |  6388 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6389 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6390 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6391 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6392 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6393 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6394 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6395 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6396 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6397 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6398 | ` *` |
|       - |  6399 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6400 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6401 | ` */` |
|   78222 |  6402 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6403 | `	ph7_gen_state *pGen,` |
|       - |  6404 | `	sxu32 *pnType,` |
|       - |  6405 | `	SyString *pClass,` |
|       - |  6406 | `	SySet *pAlts,` |
|       - |  6407 | `	sxi32 *piTypeFlags,` |
|       - |  6408 | `	SyString *pTypeText,` |
|       - |  6409 | `	int iNullableFlag,` |
|       - |  6410 | `	int iUnionFlag,` |
|       - |  6411 | `	int bAllowVoid,` |
|       - |  6412 | `	sxu32 nLine` |
|       5 |  6413 | `){` |
|       - |  6414 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   78227 |  6415 | `	int nAtoms = 0;` |
|   78227 |  6416 | `	int bShortNullable = 0;` |
|   78227 |  6417 | `	int bExplicitNull = 0;` |
|       - |  6418 | `	sxi32 rc;` |
|   78227 |  6419 | `	*pnType = 0;` |
|   78227 |  6420 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   78227 |  6421 | `	*piTypeFlags = 0;` |
|   78227 |  6422 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6423 |  |
|   78227 |  6424 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6425 | `		return SXRET_OK;` |
|       - |  6426 | `	}` |
|       - |  6427 | ``	/* Optional `?` shorthand prefix */`` |
|   78222 |  6428 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6429 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6430 | `		bShortNullable = 1;` |
|      71 |  6431 | `		pGen->pIn++;` |
|      71 |  6432 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6433 | `			return SXERR_SYNTAX;` |
|       - |  6434 | `		}` |
|      33 |  6435 | `	}` |
|       - |  6436 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6437 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6438 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6439 | `	{` |
|       - |  6440 | `		int nMembers, bParen;` |
|   78227 |  6441 | `		sxu32 iGroup = 0;` |
|   78227 |  6442 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   78227 |  6443 | `		if( rc != SXRET_OK ){` |
|       4 |  6444 | `			return rc;` |
|       - |  6445 | `		}` |
|       - |  6446 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6447 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6448 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6449 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6450 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  117518 |  6451 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   78413 |  6452 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     131 |  6453 | `			if( bShortNullable ){` |
|       - |  6454 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6455 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6456 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6457 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6458 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6459 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6460 | `			}` |
|     129 |  6461 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6462 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6463 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6464 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6465 | `			}` |
|     129 |  6466 | ``			pGen->pIn++; /* skip `\|` */`` |
|     129 |  6467 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     129 |  6468 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6469 | `				return rc;` |
|       - |  6470 | `			}` |
|       5 |  6471 | `		}` |
|   78223 |  6472 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6473 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6474 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6475 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6476 | `		}` |
|       - |  6477 | `	}` |
|       - |  6478 | `	/* Validation pass.` |
|       - |  6479 | `	 *` |
|       - |  6480 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6481 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6482 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6483 | `	 */` |
|       - |  6484 | `	{` |
|       - |  6485 | `		int i, j;` |
|   78223 |  6486 | `		int bHasNonNull = 0;` |
|   78223 |  6487 | `		int bAnyIntersection = 0;` |
|       - |  6488 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6489 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6490 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2581199 |  6491 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  156583 |  6492 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78365 |  6493 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   39185 |  6494 | `		}` |
|  156545 |  6495 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78345 |  6496 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   39166 |  6497 | `		}` |
|       - |  6498 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6499 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   78223 |  6500 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6501 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6502 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6503 | `			return SXERR_SYNTAX;` |
|       - |  6504 | `		}` |
|  156573 |  6505 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6506 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6507 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6508 | ``			 * `true`/`false` in an intersection). */`` |
|   78363 |  6509 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6510 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6511 | `				if( bClassLike ){` |
|      35 |  6512 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6513 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6514 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6515 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      35 |  6516 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6517 | `						bClassLike = 0;` |
|     ! 0 |  6518 | `					}` |
|      16 |  6519 | `				}` |
|      38 |  6520 | `				if( !bClassLike ){` |
|       - |  6521 | `					const char *zName; sxu32 nName;` |
|       3 |  6522 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6523 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6524 | `					}else{` |
|       3 |  6525 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6526 | `					}` |
|       4 |  6527 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6528 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6529 | `						(int)nName, zName);` |
|       3 |  6530 | `					return SXERR_SYNTAX;` |
|       - |  6531 | `				}` |
|      16 |  6532 | `			}` |
|   78361 |  6533 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6534 | `				if( nAtoms > 1 ){` |
|       3 |  6535 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6536 | `						"Void can only be used as a standalone type");` |
|       3 |  6537 | `					return SXERR_SYNTAX;` |
|       - |  6538 | `				}` |
|     155 |  6539 | `				if( !bAllowVoid ){` |
|     ! 0 |  6540 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6541 | `						"void cannot be used here");` |
|     ! 0 |  6542 | `					return SXERR_SYNTAX;` |
|       - |  6543 | `				}` |
|     155 |  6544 | `				if( bShortNullable ){` |
|     ! 0 |  6545 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6546 | `						"Void type cannot be nullable");` |
|     ! 0 |  6547 | `					return SXERR_SYNTAX;` |
|       - |  6548 | `				}` |
|      75 |  6549 | `			}` |
|   78359 |  6550 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6551 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6552 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6553 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6554 | `				 * does not return), and folding them would mislead any` |
|       - |  6555 | `				 * future return-enforcement work. */` |
|       3 |  6556 | `				if( nAtoms > 1 ){` |
|       3 |  6557 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6558 | `						"never can only be used as a standalone type");` |
|       3 |  6559 | `					return SXERR_SYNTAX;` |
|       - |  6560 | `				}` |
|     ! 0 |  6561 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6562 | `					"never type is not yet implemented");` |
|     ! 0 |  6563 | `				return SXERR_SYNTAX;` |
|       - |  6564 | `			}` |
|   78357 |  6565 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6566 | `				bExplicitNull = 1;` |
|      18 |  6567 | `			}else{` |
|   78329 |  6568 | `				bHasNonNull = 1;` |
|       - |  6569 | `			}` |
|       - |  6570 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6571 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6572 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6573 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6574 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   78537 |  6575 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6576 | `				int bDup = 0;` |
|     187 |  6577 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6578 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6579 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6580 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6581 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      40 |  6582 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6583 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      37 |  6584 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6585 | `								aAtoms[j].sClass.zString,` |
|      32 |  6586 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6587 | `							bDup = 1;` |
|     ! 0 |  6588 | `						}` |
|      21 |  6589 | `					}else{` |
|       3 |  6590 | `						bDup = 1;` |
|       - |  6591 | `					}` |
|      18 |  6592 | `				}` |
|     179 |  6593 | `				if( bDup ){` |
|       - |  6594 | `					const char *zName;` |
|       - |  6595 | `					sxu32 nName;` |
|       3 |  6596 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6597 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6598 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6599 | `					}else{` |
|       3 |  6600 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6601 | `						nName = aAtoms[i].nCanon;` |
|       - |  6602 | `					}` |
|       4 |  6603 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6604 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6605 | `					return SXERR_SYNTAX;` |
|       - |  6606 | `				}` |
|      91 |  6607 | `			}` |
|   39180 |  6608 | `		}` |
|   78215 |  6609 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6610 | `			if( bShortNullable ){` |
|       - |  6611 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6612 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6613 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6614 | `				return SXERR_SYNTAX;` |
|       - |  6615 | `			}` |
|       - |  6616 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6617 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6618 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6619 | `			 * atom, so set it here. */` |
|       7 |  6620 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6621 | `		}` |
|       - |  6622 | `	}` |
|       - |  6623 | `	/* Compute nullability flag */` |
|   78215 |  6624 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6625 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6626 | `	}` |
|       - |  6627 | `	/* Build canonical type text */` |
|   78215 |  6628 | `	if( pTypeText ){` |
|       - |  6629 | `		SyBlob sBlob;` |
|   78215 |  6630 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  117288 |  6631 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   39105 |  6632 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   78215 |  6633 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  117095 |  6634 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   78060 |  6635 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   78065 |  6636 | `			if( zDup ){` |
|   78065 |  6637 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   39030 |  6638 | `			}` |
|   39030 |  6639 | `		}` |
|   78215 |  6640 | `		SyBlobRelease(&sBlob);` |
|   39105 |  6641 | `	}` |
|       - |  6642 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6643 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6644 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6645 | `	{` |
|   78215 |  6646 | `		int nNonNull = 0;` |
|   78215 |  6647 | `		int iNonNullIdx = -1;` |
|       - |  6648 | `		int i;` |
|  156559 |  6649 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78349 |  6650 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   78321 |  6651 | `				nNonNull++;` |
|   78321 |  6652 | `				iNonNullIdx = i;` |
|   39158 |  6653 | `			}` |
|   39177 |  6654 | `		}` |
|   78215 |  6655 | `		if( nNonNull <= 1 ){` |
|       - |  6656 | `			/* Fast path: store as single type. */` |
|   78123 |  6657 | `			if( iNonNullIdx >= 0 ){` |
|   78117 |  6658 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   78117 |  6659 | `				if( pA->nType == SXU32_HIGH ){` |
|   21284 |  6660 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7093 |  6661 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14191 |  6662 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14191 |  6663 | `					*pnType = SXU32_HIGH;` |
|   14191 |  6664 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   71024 |  6665 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6666 | `					*pnType = MEMOBJ_VOID;` |
|      80 |  6667 | `				}else{` |
|       - |  6668 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6669 | `					 * pass above rejects it as not-yet-implemented. */` |
|   63781 |  6670 | `					*pnType = pA->nType;` |
|       - |  6671 | `				}` |
|   39056 |  6672 | `			}` |
|   39064 |  6673 | `		}else{` |
|       - |  6674 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6675 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6676 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6677 | `				ph7_type_alt sAlt;` |
|     219 |  6678 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6679 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6680 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6681 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6682 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6683 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6684 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6685 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6686 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6687 | `				}else{` |
|     135 |  6688 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6689 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6690 | `				}` |
|     209 |  6691 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6692 | `			}` |
|       - |  6693 | `		}` |
|       - |  6694 | `	}` |
|   78215 |  6695 | `	return SXRET_OK;` |
|   39116 |  6696 |  |
|       - |  6697 |  |
|       - |  6698 | `/*` |
|       - |  6699 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6700 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6701 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6702 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6703 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6704 | `` *          and union types `: T\|U`.`` |
|       - |  6705 | ` */` |
|  311458 |  6706 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6707 |  |
|  311463 |  6708 | `	sxi32 iFlags = 0;` |
|       - |  6709 | `	sxi32 rc;` |
|       - |  6710 | `	sxu32 nLine;` |
|  311463 |  6711 | `	pFunc->nReturnType = 0;` |
|  311463 |  6712 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  311463 |  6713 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  311463 |  6714 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  310991 |  6715 | `		return SXRET_OK;` |
|       - |  6716 | `	}` |
|     477 |  6717 | `	pGen->pIn++; /* Skip ':' */` |
|     477 |  6718 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6719 | `		return SXRET_OK;` |
|       - |  6720 | `	}` |
|     477 |  6721 | `	nLine = pGen->pIn->nLine;` |
|     477 |  6722 | `	rc = GenStateParseUnionTypeDecl(` |
|     236 |  6723 | `		pGen,` |
|     236 |  6724 | `		&pFunc->nReturnType,` |
|     236 |  6725 | `		&pFunc->sReturnClass,` |
|     236 |  6726 | `		&pFunc->aReturnUnion,` |
|       - |  6727 | `		&iFlags,` |
|     236 |  6728 | `		&pFunc->sReturnTypeName,` |
|       - |  6729 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6730 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6731 | `		/* iUnionFlag */ 0,` |
|       - |  6732 | `		/* bAllowVoid */ 1,` |
|     236 |  6733 | `		nLine);` |
|     477 |  6734 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6735 | `		return SXERR_ABORT;` |
|       - |  6736 | `	}` |
|     477 |  6737 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6738 | `		/* Error already reported */` |
|     ! 0 |  6739 | `		return SXERR_SYNTAX;` |
|       - |  6740 | `	}` |
|     477 |  6741 | `	if( rc == SXERR_SYNTAX ){` |
|       6 |  6742 | `		if( pGen->pIn < pGen->pEnd ){` |
|       8 |  6743 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6744 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6745 | `				&pGen->pIn->sData);` |
|       4 |  6746 | `		}else{` |
|     ! 0 |  6747 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6748 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6749 | `		}` |
|       6 |  6750 | `		return SXERR_SYNTAX;` |
|       - |  6751 | `	}` |
|     473 |  6752 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     473 |  6753 | `	return SXRET_OK;` |
|  155734 |  6754 |  |
|       - |  6755 |  |
|   46866 |  6756 | `static sxi32 GenStateCompileFunc(` |
|       - |  6757 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6758 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6759 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6760 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6761 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6762 | `	)` |
|       5 |  6763 |  |
|       - |  6764 | `	ph7_vm_func *pFunc;` |
|       - |  6765 | `	SyToken *pEnd;` |
|       - |  6766 | `	sxu32 nLine;` |
|       - |  6767 | `	char *zName;` |
|       - |  6768 | `	sxi32 rc;` |
|       - |  6769 | `	/* Extract line number */` |
|   46871 |  6770 | `	nLine = pGen->pIn->nLine;` |
|       - |  6771 | `	/* Jump the left parenthesis '(' */` |
|   46871 |  6772 | `	pGen->pIn++;` |
|       - |  6773 | `	/* Delimit the function signature */` |
|   46871 |  6774 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   46871 |  6775 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6776 | `		/* Syntax error */` |
|       9 |  6777 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6778 | `		if( rc == SXERR_ABORT ){` |
|       - |  6779 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6780 | `			return SXERR_ABORT;` |
|       - |  6781 | `		}` |
|       9 |  6782 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6783 | `		return SXRET_OK;` |
|       - |  6784 | `	}` |
|       - |  6785 | `	/* Create the function state */` |
|   46865 |  6786 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   46865 |  6787 | `	if( pFunc == 0 ){` |
|     ! 0 |  6788 | `		goto OutOfMem;` |
|       - |  6789 | `	}` |
|       - |  6790 | `	/* Build the function name, prepending namespace if active */` |
|   46872 |  6791 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6792 | `		SyBlob sFQN;` |
|       - |  6793 | `		sxu32 nLen;` |
|      16 |  6794 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6795 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6796 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6797 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6798 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6799 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6800 | `		SyBlobRelease(&sFQN);` |
|      16 |  6801 | `		if( zName == 0 ){` |
|     ! 0 |  6802 | `			goto OutOfMem;` |
|       - |  6803 | `		}` |
|      16 |  6804 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6805 | `	}else{` |
|   46851 |  6806 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   46851 |  6807 | `		if( zName == 0 ){` |
|     ! 0 |  6808 | `			goto OutOfMem;` |
|       - |  6809 | `		}` |
|   46851 |  6810 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6811 | `	}` |
|   46865 |  6812 | `	if( pGen->pIn < pEnd ){` |
|       - |  6813 | `		/* Collect function arguments */` |
|   32385 |  6814 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   32385 |  6815 | `		if( rc == SXERR_ABORT ){` |
|       - |  6816 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6817 | `			return SXERR_ABORT;` |
|       - |  6818 | `		}` |
|   16190 |  6819 | `	}` |
|       - |  6820 | `	/* Point past ')' and parse optional return type ': type' */` |
|   46865 |  6821 | `	pGen->pIn = &pEnd[1];` |
|       - |  6822 | `	{` |
|   46865 |  6823 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   46865 |  6824 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6825 | `			return SXERR_ABORT;` |
|   46865 |  6826 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       6 |  6827 | `			return SXERR_SYNTAX;` |
|       - |  6828 | `		}` |
|       - |  6829 | `	}` |
|   46861 |  6830 | `	if( bHandleClosure ){` |
|       - |  6831 | `		ph7_vm_func_closure_env sEnv;` |
|     261 |  6832 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     256 |  6833 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     141 |  6834 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      21 |  6835 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6836 | `				/* Closure,record environment variable */` |
|      21 |  6837 | `				pGen->pIn++;` |
|      21 |  6838 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6839 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6840 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6841 | `						return SXERR_ABORT;` |
|       - |  6842 | `					}` |
|     ! 0 |  6843 | `				}` |
|      21 |  6844 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6845 | `				/* Compile until we hit the first closing parenthesis */` |
|      41 |  6846 | `				while( pGen->pIn < pGen->pEnd ){` |
|      41 |  6847 | `					int iFlagsLocal = 0;` |
|      41 |  6848 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      21 |  6849 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      21 |  6850 | `						break;` |
|       - |  6851 | `					}` |
|      25 |  6852 | `					nLineLocal = pGen->pIn->nLine;` |
|      25 |  6853 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6854 | `						/* Pass by reference,record that */` |
|     ! 0 |  6855 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6856 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6857 | `							);` |
|     ! 0 |  6858 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6859 | `						pGen->pIn++;` |
|     ! 0 |  6860 | `					}` |
|      20 |  6861 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      25 |  6862 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6863 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6864 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6865 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6866 | `								return SXERR_ABORT;` |
|       - |  6867 | `							}` |
|       - |  6868 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6869 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6870 | `								pGen->pIn++;` |
|     ! 0 |  6871 | `							}` |
|     ! 0 |  6872 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6873 | `								pGen->pIn++;` |
|     ! 0 |  6874 | `							}` |
|     ! 0 |  6875 | `							break;` |
|       - |  6876 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6877 | `					}else{` |
|       - |  6878 | `						SyString *pNameLocal;` |
|       - |  6879 | `						char *zDup;` |
|       - |  6880 | `						/* Duplicate variable name */` |
|      25 |  6881 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      25 |  6882 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      25 |  6883 | `						if( zDup ){` |
|       - |  6884 | `							/* Zero the structure */` |
|      25 |  6885 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      25 |  6886 | `							sEnv.iFlags = iFlagsLocal;` |
|      25 |  6887 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      25 |  6888 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      25 |  6889 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6890 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6891 | `									got_this = 1;` |
|     ! 0 |  6892 | `							}` |
|       - |  6893 | `							/* Save imported variable */` |
|      25 |  6894 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      15 |  6895 | `						}else{` |
|     ! 0 |  6896 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6897 | `							 return SXERR_ABORT;` |
|       - |  6898 | `						}` |
|       - |  6899 | `					}` |
|      25 |  6900 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      31 |  6901 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6902 | `						/* Ignore trailing commas */` |
|       7 |  6903 | `						pGen->pIn++;` |
|       1 |  6904 | `					}` |
|       5 |  6905 | `				}` |
|      21 |  6906 | `				if( !got_this ){` |
|       - |  6907 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6908 | `					 * available to the closure environment.` |
|       - |  6909 | `					 */` |
|      21 |  6910 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      21 |  6911 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      21 |  6912 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      21 |  6913 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      21 |  6914 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6915 | `				}` |
|      21 |  6916 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6917 | `					/* Mark as closure */` |
|      21 |  6918 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6919 | `				}` |
|       8 |  6920 | `		}` |
|     128 |  6921 | `	}` |
|       - |  6922 | `	/* Compile the body */` |
|   46861 |  6923 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   46861 |  6924 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6925 | `		return SXERR_ABORT;` |
|       - |  6926 | `	}` |
|   46861 |  6927 | `	if( ppFunc ){` |
|     261 |  6928 | `		*ppFunc = pFunc;` |
|     128 |  6929 | `	}` |
|   46861 |  6930 | `	rc = SXRET_OK;` |
|   46861 |  6931 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6932 | `		/* Finally register the function */` |
|   46845 |  6933 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   23420 |  6934 | `	}` |
|   46861 |  6935 | `	if( rc == SXRET_OK ){` |
|   46861 |  6936 | `		return SXRET_OK;` |
|       - |  6937 | `	}` |
|       - |  6938 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6939 | `OutOfMem:` |
|       - |  6940 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6941 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6942 | `	 */` |
|     ! 0 |  6943 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6944 | `	return SXERR_ABORT;` |
|   23438 |  6945 |  |
|       - |  6946 | `/*` |
|       - |  6947 | ` * Compile a standard PHP function.` |
|       - |  6948 | ` *  Refer to the block-comment above for more information.` |
|       - |  6949 | ` */` |
|   46618 |  6950 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6951 |  |
|       - |  6952 | `	SyString *pName;` |
|       - |  6953 | `	sxi32 iFlags;` |
|       - |  6954 | `	sxu32 nLine;` |
|       - |  6955 | `	sxi32 rc;` |
|       - |  6956 |  |
|   46623 |  6957 | `	nLine = pGen->pIn->nLine;` |
|   46623 |  6958 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   46623 |  6959 | `	iFlags = 0;` |
|   46623 |  6960 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6961 | `		/* Return by reference,remember that */` |
|       7 |  6962 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6963 | `		/* Jump the '&' token */` |
|       7 |  6964 | `		pGen->pIn++;` |
|       3 |  6965 | `	}` |
|   46623 |  6966 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6967 | `		/* Invalid function name */` |
|       8 |  6968 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  6969 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6970 | `			return SXERR_ABORT;` |
|       - |  6971 | `		}` |
|       - |  6972 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  6973 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  6974 | `			pGen->pIn++;` |
|       2 |  6975 | `		}` |
|       8 |  6976 | `		return SXRET_OK;` |
|       - |  6977 | `	}` |
|   46617 |  6978 | `	pName = &pGen->pIn->sData;` |
|   46617 |  6979 | `	nLine = pGen->pIn->nLine;` |
|       - |  6980 | `	/* Jump the function name */` |
|   46617 |  6981 | `	pGen->pIn++;` |
|   46617 |  6982 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6983 | `		/* Syntax error */` |
|       3 |  6984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6985 | `		if( rc == SXERR_ABORT ){` |
|       - |  6986 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6987 | `			return SXERR_ABORT;` |
|       - |  6988 | `		}` |
|       - |  6989 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6990 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6991 | `			pGen->pIn++;` |
|     ! 0 |  6992 | `		}` |
|       3 |  6993 | `		return SXRET_OK;` |
|       - |  6994 | `	}` |
|       - |  6995 | `	/* Compile function body */` |
|   46615 |  6996 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   46615 |  6997 | `	return rc;` |
|   23314 |  6998 |  |
|       - |  6999 | `/*` |
|       - |  7000 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  7001 | ` * According to the PHP language reference manual` |
|       - |  7002 | ` *  Visibility:` |
|       - |  7003 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  7004 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  7005 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  7006 | ` *  Members declared protected can be accessed only within the class` |
|       - |  7007 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  7008 | ` *  may only be accessed by the class that defines the member.` |
|       - |  7009 | ` */` |
|  331934 |  7010 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  7011 |  |
|  331939 |  7012 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   10647 |  7013 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  321297 |  7014 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   45713 |  7015 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7016 | `	}` |
|       - |  7017 | `	/* Assume public by default */` |
|  275589 |  7018 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  165972 |  7019 |  |
|       - |  7020 | `/*` |
|       - |  7021 | ` * Compile a class constant.` |
|       - |  7022 | ` * According to the PHP language reference manual` |
|       - |  7023 | ` *  Class Constants` |
|       - |  7024 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7025 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7026 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7027 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7028 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7029 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7030 | ` * Symisc eXtension.` |
|       - |  7031 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7032 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7033 | ` *  Example:` |
|       - |  7034 | ` *   class Test{` |
|       - |  7035 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7036 | ` *   };` |
|       - |  7037 | ` *   var_dump(TEST::MyConst);` |
|       - |  7038 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7039 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7040 | ` */` |
|       - |  7041 | `/*` |
|       - |  7042 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7043 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7044 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7045 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7046 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7047 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7048 | ` */` |
|      78 |  7049 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7050 |  |
|       - |  7051 | `	SyToken *p0, *p1;` |
|      83 |  7052 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7053 | `		return 0;` |
|       - |  7054 | `	}` |
|      83 |  7055 | `	p0 = pGen->pIn;` |
|       - |  7056 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      83 |  7057 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7058 | `		return 1;` |
|       - |  7059 | `	}` |
|      83 |  7060 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7061 | `		return 1;` |
|       - |  7062 | `	}` |
|       - |  7063 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7064 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7065 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      79 |  7066 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      79 |  7067 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      79 |  7068 | `		if( p1 ){` |
|      79 |  7069 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  7070 | `				return 1;` |
|       - |  7071 | `			}` |
|      59 |  7072 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7073 | `				return 1;` |
|       - |  7074 | `			}` |
|      25 |  7075 | `		}` |
|      25 |  7076 | `	}` |
|      55 |  7077 | `	return 0;` |
|      44 |  7078 |  |
|       - |  7079 | `/*` |
|       - |  7080 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7081 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7082 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7083 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7084 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7085 | ` * share the same backing.` |
|       - |  7086 | ` */` |
|     200 |  7087 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7088 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7089 |  |
|     205 |  7090 | `	pAttr->nType = nType;` |
|     205 |  7091 | `	pAttr->sClass = *pClass;` |
|     205 |  7092 | `	pAttr->sTypeName = *pTypeName;` |
|     205 |  7093 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7094 | `		sxu32 i;` |
|      66 |  7095 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7096 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7097 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7098 | `		}` |
|      10 |  7099 | `	}` |
|     205 |  7100 |  |
|      78 |  7101 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7102 |  |
|      83 |  7103 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7104 | `	SySet *pInstrContainer;` |
|       - |  7105 | `	ph7_class_attr *pCons;` |
|       - |  7106 | `	SyString *pName;` |
|       - |  7107 | `	sxi32 rc;` |
|      83 |  7108 | `	sxu32 nType = 0;` |
|       - |  7109 | `	SyString sTypeClass;` |
|       - |  7110 | `	SyString sTypeText;` |
|       - |  7111 | `	SySet aUnionAlts;` |
|      83 |  7112 | `	sxi32 iTypeFlags = 0;` |
|      83 |  7113 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      83 |  7114 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      83 |  7115 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7116 | `	/* Extract visibility level */` |
|      83 |  7117 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7118 | `	/* Mark as constant */` |
|      83 |  7119 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      83 |  7120 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7121 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7122 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      97 |  7123 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  7124 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  7125 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7126 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7127 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7128 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7129 | `		 * and success paths release. */` |
|      32 |  7130 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7131 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7132 | `			goto Synchronize;` |
|      32 |  7133 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7134 | `			return SXERR_ABORT;` |
|      32 |  7135 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7137 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7138 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7139 | `				return SXERR_ABORT;` |
|       - |  7140 | `			}` |
|     ! 0 |  7141 | `			goto Synchronize;` |
|       - |  7142 | `		}` |
|      32 |  7143 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  7144 | `	}` |
|      39 |  7145 | `loop:` |
|      85 |  7146 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7147 | `		/* Invalid constant name */` |
|     ! 0 |  7148 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7149 | `		if( rc == SXERR_ABORT ){` |
|       - |  7150 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7151 | `			return SXERR_ABORT;` |
|       - |  7152 | `		}` |
|     ! 0 |  7153 | `		goto Synchronize;` |
|       - |  7154 | `	}` |
|       - |  7155 | `	/* Peek constant name */` |
|      85 |  7156 | `	pName = &pGen->pIn->sData;` |
|       - |  7157 | `	/* Make sure the constant name isn't reserved */` |
|      85 |  7158 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7159 | `		/* Reserved constant name */` |
|     ! 0 |  7160 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7161 | `		if( rc == SXERR_ABORT ){` |
|       - |  7162 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7163 | `			return SXERR_ABORT;` |
|       - |  7164 | `		}` |
|     ! 0 |  7165 | `		goto Synchronize;` |
|       - |  7166 | `	}` |
|       - |  7167 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      85 |  7168 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  7169 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  7170 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  7171 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  7172 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7173 | `			return SXERR_ABORT;` |
|      32 |  7174 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7175 | `			goto Synchronize;` |
|       - |  7176 | `		}` |
|      13 |  7177 | `	}` |
|       - |  7178 | `	/* Advance the stream cursor */` |
|      83 |  7179 | `	pGen->pIn++;` |
|      83 |  7180 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7181 | `		/* Invalid declaration */` |
|     ! 0 |  7182 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7183 | `		if( rc == SXERR_ABORT ){` |
|       - |  7184 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7185 | `			return SXERR_ABORT;` |
|       - |  7186 | `		}` |
|     ! 0 |  7187 | `		goto Synchronize;` |
|       - |  7188 | `	}` |
|      83 |  7189 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7190 | `	/* Allocate a new class attribute */` |
|      83 |  7191 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      83 |  7192 | `	if( pCons == 0 ){` |
|     ! 0 |  7193 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7194 | `		return SXERR_ABORT;` |
|       - |  7195 | `	}` |
|      83 |  7196 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7197 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  7198 | `	}` |
|       - |  7199 | `	/* Swap bytecode container */` |
|      83 |  7200 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      83 |  7201 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7202 | `	/* Compile constant value.` |
|       - |  7203 | `	 */` |
|      83 |  7204 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      83 |  7205 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7206 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7207 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7208 | `			return SXERR_ABORT;` |
|       - |  7209 | `		}` |
|       1 |  7210 | `	}` |
|       - |  7211 | `	/* Emit the done instruction */` |
|      83 |  7212 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      83 |  7213 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      83 |  7214 | `	if( rc == SXERR_ABORT ){` |
|       - |  7215 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7216 | `		return SXERR_ABORT;` |
|       - |  7217 | `	}` |
|       - |  7218 | `	/* All done,install the constant */` |
|      83 |  7219 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      83 |  7220 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7221 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7222 | `		return SXERR_ABORT;` |
|       - |  7223 | `	}` |
|      83 |  7224 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7225 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7226 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7227 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7228 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7229 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7230 | `				pTok--;` |
|     ! 0 |  7231 | `			}` |
|     ! 0 |  7232 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7233 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7234 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7236 | `				return SXERR_ABORT;` |
|       - |  7237 | `			}` |
|     ! 0 |  7238 | `		}else{` |
|       3 |  7239 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7240 | `				goto loop;` |
|       - |  7241 | `			}` |
|       - |  7242 | `		}` |
|     ! 0 |  7243 | `	}` |
|      81 |  7244 | `	SySetRelease(&aUnionAlts);` |
|      81 |  7245 | `	return SXRET_OK;` |
|       1 |  7246 | `Synchronize:` |
|       3 |  7247 | `	SySetRelease(&aUnionAlts);` |
|       - |  7248 | `	/* Synchronize with the first semi-colon */` |
|       9 |  7249 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7250 | `		pGen->pIn++;` |
|       1 |  7251 | `	}` |
|       3 |  7252 | `	return SXERR_CORRUPT;` |
|      44 |  7253 |  |
|       - |  7254 | `/*` |
|       - |  7255 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7256 | ` * According to the PHP language reference manual` |
|       - |  7257 | ` *  Properties` |
|       - |  7258 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7259 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7260 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7261 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7262 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7263 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7264 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7265 | ` * Symisc eXtension.` |
|       - |  7266 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7267 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7268 | ` *  Example:` |
|       - |  7269 | ` *   class Test{` |
|       - |  7270 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7271 | ` *   };` |
|       - |  7272 | ` *   var_dump(TEST::myVar);` |
|       - |  7273 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7274 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7275 | ` */` |
|       - |  7276 | `/*` |
|       - |  7277 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7278 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7279 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7280 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7281 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7282 | ` */` |
|  173186 |  7283 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7284 |  |
|  173191 |  7285 | `	SyToken *p = pStart;` |
|  173191 |  7286 | `	int bFirst = 1;` |
|  173191 |  7287 | `	if( p >= pEnd ) return 0;` |
|       - |  7288 | ``	/* Optional nullable `?` shorthand. */`` |
|  173191 |  7289 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7290 | `		p++;` |
|      18 |  7291 | `		if( p >= pEnd ) return 0;` |
|       8 |  7292 | `	}` |
|       - |  7293 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7294 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7295 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7296 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   86593 |  7297 | `	for(;;){` |
|  173209 |  7298 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7299 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7300 | `			p++;` |
|       9 |  7301 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7302 | `			if( p >= pEnd ) return 0;` |
|       3 |  7303 | `			p++; /* skip ')' */` |
|       2 |  7304 | `		}else{` |
|       - |  7305 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7306 | ``			 * then any `&`-joined intersection members. */`` |
|  173207 |  7307 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  173207 |  7308 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7309 | `				return 0;` |
|       - |  7310 | `			}` |
|       - |  7311 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7312 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7313 | `			 * may still appear at the initial dispatch site). */` |
|  173207 |  7314 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  173167 |  7315 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  173239 |  7316 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3732 |  7317 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  173013 |  7318 | `					return 0;` |
|       - |  7319 | `				}` |
|      77 |  7320 | `			}` |
|     199 |  7321 | `			p++;` |
|     201 |  7322 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7323 | `				p += 2;` |
|       1 |  7324 | `			}` |
|     294 |  7325 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     202 |  7326 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7327 | `				p++; /* skip '&' */` |
|       3 |  7328 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7329 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7330 | `				p++;` |
|       3 |  7331 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7332 | `					p += 2;` |
|     ! 0 |  7333 | `				}` |
|       1 |  7334 | `			}` |
|       - |  7335 | `		}` |
|     201 |  7336 | `		bFirst = 0;` |
|     196 |  7337 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7338 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7339 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7340 | `			continue;` |
|       - |  7341 | `		}` |
|     183 |  7342 | `		break;` |
|     ! 0 |  7343 | `	}` |
|     183 |  7344 | `	if( p >= pEnd ) return 0;` |
|     183 |  7345 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   86598 |  7346 |  |
|       - |  7347 |  |
|       - |  7348 | `/*` |
|       - |  7349 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7350 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7351 | ` * if not). Recognized forms:` |
|       - |  7352 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7353 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7354 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7355 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7356 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7357 | ` * on unrecoverable error.` |
|       - |  7358 | ` *` |
|       - |  7359 | ` * When a type is parsed:` |
|       - |  7360 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7361 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7362 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7363 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7364 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7365 | ` */` |
|     178 |  7366 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7367 | `	ph7_gen_state *pGen,` |
|       - |  7368 | `	sxu32 *pnType,` |
|       - |  7369 | `	SyString *pClass,` |
|       - |  7370 | `	sxi32 *piTypeFlags,` |
|       - |  7371 | `	SyString *pTypeText,` |
|       - |  7372 | `	SySet *pAlts` |
|       5 |  7373 | `){` |
|     183 |  7374 | `	sxi32 iFlags = 0;` |
|       - |  7375 | `	sxi32 rc;` |
|     183 |  7376 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7377 | `		return SXRET_OK;` |
|       - |  7378 | `	}` |
|       - |  7379 | `	/* If the first token is '$', there's no type */` |
|     183 |  7380 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7381 | `		return SXRET_OK;` |
|       - |  7382 | `	}` |
|     183 |  7383 | `	rc = GenStateParseUnionTypeDecl(` |
|      89 |  7384 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7385 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7386 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7387 | `		/* bAllowVoid */ 0,` |
|     178 |  7388 | `		pGen->pIn->nLine);` |
|     183 |  7389 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7390 | `		return rc;` |
|       - |  7391 | `	}` |
|       - |  7392 | `	/* Verify next token is '$' (start of property name) */` |
|     183 |  7393 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7394 | `		return SXERR_SYNTAX;` |
|       - |  7395 | `	}` |
|     183 |  7396 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     183 |  7397 | `	return SXRET_OK;` |
|      94 |  7398 |  |
|       - |  7399 |  |
|       - |  7400 | `/*` |
|       - |  7401 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7402 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7403 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7404 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7405 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7406 | ` * by the type parser itself before reaching here.` |
|       - |  7407 | ` *` |
|       - |  7408 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7409 | ` * use in the error message.` |
|       - |  7410 | ` */` |
|     320 |  7411 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7412 | `	sxu32 nType,` |
|       - |  7413 | `	const SyString *pClass,` |
|       - |  7414 | `	const char **pzName,` |
|       - |  7415 | `	sxu32 *pnName)` |
|       5 |  7416 |  |
|       - |  7417 | `	const char *z;` |
|       - |  7418 | `	sxu32 n;` |
|     325 |  7419 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     277 |  7420 | `		return 0;` |
|       - |  7421 | `	}` |
|      53 |  7422 | `	z = pClass->zString;` |
|      53 |  7423 | `	n = pClass->nByte;` |
|      53 |  7424 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7425 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7426 | `	}` |
|       - |  7427 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7428 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7429 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      46 |  7430 | `	return 0;` |
|     165 |  7431 |  |
|       - |  7432 |  |
|       - |  7433 | `/*` |
|       - |  7434 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7435 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7436 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7437 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7438 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7439 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7440 | ` *` |
|       - |  7441 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7442 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7443 | ` */` |
|     262 |  7444 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7445 | `	ph7_gen_state *pGen,` |
|       - |  7446 | `	ph7_class *pClass,` |
|       - |  7447 | `	const SyString *pMemberName,` |
|       - |  7448 | `	sxu32 nType,` |
|       - |  7449 | `	const SyString *pTypeClass,` |
|       - |  7450 | `	const SyString *pTypeText,` |
|       - |  7451 | `	SySet *pUnionAlts,` |
|       - |  7452 | `	const char *zErrFmt,` |
|       - |  7453 | `	sxu32 nLine)` |
|       5 |  7454 |  |
|     267 |  7455 | `	const char *zBad = 0;` |
|     267 |  7456 | `	sxu32 nBad = 0;` |
|       - |  7457 | `	SyString sFallback;` |
|       - |  7458 | `	const SyString *pBad;` |
|       - |  7459 | `	sxi32 rc;` |
|     267 |  7460 | `	int bDisallowed = 0;` |
|     267 |  7461 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7462 | `		bDisallowed = 1;` |
|     265 |  7463 | `	}else if( pUnionAlts ){` |
|       - |  7464 | `		sxu32 i;` |
|      88 |  7465 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7466 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7467 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7468 | `				bDisallowed = 1;` |
|       3 |  7469 | `				break;` |
|       - |  7470 | `			}` |
|      32 |  7471 | `		}` |
|      14 |  7472 | `	}` |
|     267 |  7473 | `	if( !bDisallowed ){` |
|     261 |  7474 | `		return SXRET_OK;` |
|       - |  7475 | `	}` |
|       - |  7476 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7477 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7478 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7479 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7480 | `		pBad = pTypeText;` |
|       5 |  7481 | `	}else{` |
|     ! 0 |  7482 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7483 | `		pBad = &sFallback;` |
|       - |  7484 | `	}` |
|      11 |  7485 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7486 | `		zErrFmt,` |
|       3 |  7487 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7488 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7489 | `		return SXERR_ABORT;` |
|       - |  7490 | `	}` |
|       8 |  7491 | `	return SXERR_SYNTAX;` |
|     136 |  7492 |  |
|       - |  7493 | `/*` |
|       - |  7494 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7495 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7496 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7497 | ` * than promoted to a lexer keyword.` |
|       - |  7498 | ` */` |
| 1565692 |  7499 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7500 |  |
| 1598914 |  7501 | `	return (pTok->nType & PH7_TK_ID)` |
|  816063 |  7502 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1598909 |  7503 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7504 |  |
|   67372 |  7505 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7506 |  |
|   67377 |  7507 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7508 | `	ph7_class_attr *pAttr;` |
|       - |  7509 | `	SyString *pName;` |
|       - |  7510 | `	sxi32 rc;` |
|   67377 |  7511 | `	sxu32 nType = 0;` |
|       - |  7512 | `	SyString sTypeClass;` |
|       - |  7513 | `	SyString sTypeText;` |
|       - |  7514 | `	SySet aUnionAlts;` |
|   67377 |  7515 | `	sxi32 iTypeFlags = 0;` |
|   67377 |  7516 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   67377 |  7517 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   67377 |  7518 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7519 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7520 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7521 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   67377 |  7522 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7523 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7524 | `	}` |
|       - |  7525 | `	/* Extract visibility level */` |
|   67377 |  7526 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7527 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   67466 |  7528 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     183 |  7529 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     183 |  7530 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7531 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7532 | `			goto Synchronize;` |
|     183 |  7533 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7534 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7535 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7536 | `				&pGen->pIn->sData);` |
|     ! 0 |  7537 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7538 | `				return SXERR_ABORT;` |
|       - |  7539 | `			}` |
|     ! 0 |  7540 | `			goto Synchronize;` |
|     183 |  7541 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7542 | `			return SXERR_ABORT;` |
|       - |  7543 | `		}` |
|      89 |  7544 | `	}` |
|     ! 0 |  7545 | `loop:` |
|   67381 |  7546 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7547 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7548 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7549 | `			return SXERR_ABORT;` |
|       - |  7550 | `		}` |
|     ! 0 |  7551 | `		goto Synchronize;` |
|       - |  7552 | `	}` |
|   67381 |  7553 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   67381 |  7554 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7555 | `		/* Invalid attribute name */` |
|     ! 0 |  7556 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7557 | `		if( rc == SXERR_ABORT ){` |
|       - |  7558 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7559 | `			return SXERR_ABORT;` |
|       - |  7560 | `		}` |
|     ! 0 |  7561 | `		goto Synchronize;` |
|       - |  7562 | `	}` |
|       - |  7563 | `	/* Peek attribute name */` |
|   67381 |  7564 | `	pName = &pGen->pIn->sData;` |
|       - |  7565 | `	/* Advance the stream cursor */` |
|   67381 |  7566 | `	pGen->pIn++;` |
|   67381 |  7567 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7568 | `		/* Invalid declaration */` |
|       3 |  7569 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7570 | `		if( rc == SXERR_ABORT ){` |
|       - |  7571 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7572 | `			return SXERR_ABORT;` |
|       - |  7573 | `		}` |
|       3 |  7574 | `		goto Synchronize;` |
|       - |  7575 | `	}` |
|       - |  7576 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7577 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   67379 |  7578 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7579 | `		const char *zRoErr = 0;` |
|      39 |  7580 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7581 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7582 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7583 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7584 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7585 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7586 | `		}` |
|      39 |  7587 | `		if( zRoErr ){` |
|      13 |  7588 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7589 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7590 | `				return SXERR_ABORT;` |
|       - |  7591 | `			}` |
|      13 |  7592 | `			goto Synchronize;` |
|       - |  7593 | `		}` |
|      12 |  7594 | `	}` |
|       - |  7595 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7596 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7597 | `	 * by the type parser. */` |
|   67369 |  7598 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     269 |  7599 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7600 | `			&sTypeText,` |
|     176 |  7601 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      88 |  7602 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     181 |  7603 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7604 | `			return SXERR_ABORT;` |
|     181 |  7605 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7606 | `			goto Synchronize;` |
|       - |  7607 | `		}` |
|      88 |  7608 | `	}` |
|       - |  7609 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   67369 |  7610 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7611 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7612 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7613 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7614 | `			return SXERR_ABORT;` |
|       - |  7615 | `		}` |
|       3 |  7616 | `		goto Synchronize;` |
|       - |  7617 | `	}` |
|       - |  7618 | `	/* Allocate a new class attribute */` |
|   67367 |  7619 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   67367 |  7620 | `	if( pAttr == 0 ){` |
|     ! 0 |  7621 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7622 | `		return SXERR_ABORT;` |
|       - |  7623 | `	}` |
|   67367 |  7624 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     179 |  7625 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      87 |  7626 | `	}` |
|   67367 |  7627 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7628 | `		SySet *pInstrContainer;` |
|   21539 |  7629 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7630 | `		/* Swap bytecode container */` |
|   21539 |  7631 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   21539 |  7632 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7633 | `		/* Compile attribute value.` |
|       - |  7634 | `		 */` |
|   21539 |  7635 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   21539 |  7636 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7637 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7638 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7639 | `				return SXERR_ABORT;` |
|       - |  7640 | `			}` |
|     ! 0 |  7641 | `		}` |
|       - |  7642 | `		/* Emit the done instruction */` |
|   21539 |  7643 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   21539 |  7644 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10767 |  7645 | `	}` |
|       - |  7646 | `	/* All done,install the attribute */` |
|   67367 |  7647 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   67367 |  7648 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7649 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7650 | `		return SXERR_ABORT;` |
|       - |  7651 | `	}` |
|   67367 |  7652 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7653 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7654 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7655 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7656 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7657 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7658 | `				pTok--;` |
|     ! 0 |  7659 | `			}` |
|     ! 0 |  7660 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7661 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7662 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7663 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7664 | `				return SXERR_ABORT;` |
|       - |  7665 | `			}` |
|     ! 0 |  7666 | `		}else{` |
|       5 |  7667 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7668 | `				goto loop;` |
|       - |  7669 | `			}` |
|       - |  7670 | `		}` |
|     ! 0 |  7671 | `	}` |
|   67363 |  7672 | `	SySetRelease(&aUnionAlts);` |
|   67363 |  7673 | `	return SXRET_OK;` |
|       7 |  7674 | `Synchronize:` |
|       - |  7675 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7676 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7677 | `		pGen->pIn++;` |
|       2 |  7678 | `	}` |
|      17 |  7679 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7680 | `	return SXERR_CORRUPT;` |
|   33691 |  7681 |  |
|       - |  7682 | `/*` |
|       - |  7683 | ` * Compile a class method.` |
|       - |  7684 | ` *` |
|       - |  7685 | ` * Refer to the official documentation for more information` |
|       - |  7686 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7687 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7688 | ` * overloading and many more.` |
|       - |  7689 | ` */` |
|  264484 |  7690 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7691 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7692 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7693 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7694 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7695 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7696 | `	)` |
|       5 |  7697 |  |
|  264489 |  7698 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7699 | `	ph7_class_method *pMeth;` |
|       - |  7700 | `	sxi32 iFuncFlags;` |
|       - |  7701 | `	SyString *pName;` |
|       - |  7702 | `	SyToken *pEnd;` |
|       - |  7703 | `	sxi32 rc;` |
|       - |  7704 | `	/* Extract visibility level */` |
|  264489 |  7705 | `	iProtection = GetProtectionLevel(iProtection);` |
|  264489 |  7706 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  264489 |  7707 | `	iFuncFlags = 0;` |
|  264489 |  7708 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7709 | `		/* Invalid method name */` |
|     ! 0 |  7710 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7711 | `		if( rc == SXERR_ABORT ){` |
|       - |  7712 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7713 | `			return SXERR_ABORT;` |
|       - |  7714 | `		}` |
|     ! 0 |  7715 | `		goto Synchronize;` |
|       - |  7716 | `	}` |
|  264489 |  7717 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7718 | `		/* Return by reference,remember that */` |
|     ! 0 |  7719 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7720 | `		/* Jump the '&' token */` |
|     ! 0 |  7721 | `		pGen->pIn++;` |
|     ! 0 |  7722 | `	}` |
|  264489 |  7723 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7724 | `		/* Invalid method name */` |
|     ! 0 |  7725 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7726 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7727 | `			return SXERR_ABORT;` |
|       - |  7728 | `		}` |
|     ! 0 |  7729 | `		goto Synchronize;` |
|       - |  7730 | `	}` |
|       - |  7731 | `	/* Peek method name */` |
|  264489 |  7732 | `	pName = &pGen->pIn->sData;` |
|  264489 |  7733 | `	nLine = pGen->pIn->nLine;` |
|       - |  7734 | `	/* Jump the method name */` |
|  264489 |  7735 | `	pGen->pIn++;` |
|  264489 |  7736 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7737 | `		/* Abstract method */` |
|   91371 |  7738 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7739 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7740 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7741 | `				&pClass->sName,pName);` |
|     ! 0 |  7742 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7743 | `				return SXERR_ABORT;` |
|       - |  7744 | `			}` |
|     ! 0 |  7745 | `		}` |
|       - |  7746 | `		/* Assemble method signature only */` |
|   91371 |  7747 | `		doBody = FALSE;` |
|   45683 |  7748 | `	}` |
|  264489 |  7749 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7750 | `		/* Syntax error */` |
|     ! 0 |  7751 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7752 | `		if( rc == SXERR_ABORT ){` |
|       - |  7753 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7754 | `			return SXERR_ABORT;` |
|       - |  7755 | `		}` |
|     ! 0 |  7756 | `		goto Synchronize;` |
|       - |  7757 | `	}` |
|       - |  7758 | `	/* Allocate a new class_method instance */` |
|  264489 |  7759 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  264489 |  7760 | `	if( pMeth == 0 ){` |
|     ! 0 |  7761 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7762 | `		return SXERR_ABORT;` |
|       - |  7763 | `	}` |
|       - |  7764 | `	/* Jump the left parenthesis '(' */` |
|  264489 |  7765 | `	pGen->pIn++;` |
|  264489 |  7766 | `	pEnd = 0; /* cc warning */` |
|       - |  7767 | `	/* Delimit the method signature */` |
|  264489 |  7768 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  264489 |  7769 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7770 | `		/* Syntax error */` |
|       3 |  7771 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7772 | `		if( rc == SXERR_ABORT ){` |
|       - |  7773 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7774 | `			return SXERR_ABORT;` |
|       - |  7775 | `		}` |
|       3 |  7776 | `		goto Synchronize;` |
|       - |  7777 | `	}` |
|       - |  7778 | `	{` |
|  264487 |  7779 | `		int bIsCtor = 0;` |
|  264487 |  7780 | `		int bAbstractCtor = 0;` |
|  386107 |  7781 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  156948 |  7782 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  253871 |  7783 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   21237 |  7784 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7785 | `				bAbstractCtor = 1;` |
|       2 |  7786 | `			}else{` |
|   21235 |  7787 | `				bIsCtor = 1;` |
|       - |  7788 | `			}` |
|   10616 |  7789 | `		}` |
|  264487 |  7790 | `		if( pGen->pIn < pEnd ){` |
|       - |  7791 | `			/* Collect method arguments */` |
|   60139 |  7792 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   60139 |  7793 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7794 | `				return SXERR_ABORT;` |
|       - |  7795 | `			}` |
|   30067 |  7796 | `		}` |
|       - |  7797 | `	}` |
|       - |  7798 | `	/* Point past ')' and parse optional return type ': type' */` |
|  264487 |  7799 | `	pGen->pIn = &pEnd[1];` |
|       - |  7800 | `	{` |
|  264487 |  7801 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  264487 |  7802 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7803 | `			return SXERR_ABORT;` |
|  264487 |  7804 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7805 | `			goto Synchronize;` |
|       - |  7806 | `		}` |
|       - |  7807 | `	}` |
|       - |  7808 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7809 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7810 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7811 | `	{` |
|  264487 |  7812 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7813 | `		sxu32 i;` |
|  359841 |  7814 | `		for( i = 0; i < nArg; i++ ){` |
|   95369 |  7815 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7816 | `			ph7_class_attr *pAttr;` |
|   95369 |  7817 | `			sxi32 iAttrFlags = 0;` |
|       - |  7818 | `			int bArgTyped;` |
|   95369 |  7819 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   95305 |  7820 | `				continue;` |
|       - |  7821 | `			}` |
|       - |  7822 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  7823 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  7824 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  7825 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  7826 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  7827 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7828 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7829 | `					"Cannot declare variadic promoted property");` |
|       3 |  7830 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7831 | `					return SXERR_ABORT;` |
|       - |  7832 | `				}` |
|       3 |  7833 | `				goto Synchronize;` |
|       - |  7834 | `			}` |
|       - |  7835 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7836 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7837 | `			 * appear as an alternative of a union type. */` |
|      67 |  7838 | `			if( bArgTyped ){` |
|      92 |  7839 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  7840 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  7841 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  7842 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  7843 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7844 | `					return SXERR_ABORT;` |
|      63 |  7845 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7846 | `					goto Synchronize;` |
|       - |  7847 | `				}` |
|      27 |  7848 | `			}` |
|       - |  7849 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  7850 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7851 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7852 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7853 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7854 | `					return SXERR_ABORT;` |
|       - |  7855 | `				}` |
|       3 |  7856 | `				goto Synchronize;` |
|       - |  7857 | `			}` |
|      61 |  7858 | `			if( bArgTyped ){` |
|      57 |  7859 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  7860 | `			}` |
|      61 |  7861 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7862 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7863 | `			}` |
|      61 |  7864 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  7865 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  7866 | `			}` |
|      61 |  7867 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7868 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7869 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7870 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7871 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7872 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7873 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7874 | `						return SXERR_ABORT;` |
|       - |  7875 | `					}` |
|       3 |  7876 | `					goto Synchronize;` |
|       - |  7877 | `				}` |
|      22 |  7878 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7879 | `			}` |
|      59 |  7880 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  7881 | `			if( pAttr == 0 ){` |
|     ! 0 |  7882 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7883 | `				return SXERR_ABORT;` |
|       - |  7884 | `			}` |
|      59 |  7885 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  7886 | `				pAttr->nType = pArg->nType;` |
|      57 |  7887 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  7888 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  7889 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7890 | `					sxu32 k;` |
|      20 |  7891 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  7892 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  7893 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  7894 | `					}` |
|       3 |  7895 | `				}` |
|      26 |  7896 | `			}` |
|      59 |  7897 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  7898 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7899 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7900 | `				return SXERR_ABORT;` |
|       - |  7901 | `			}` |
|      32 |  7902 | `		}` |
|       - |  7903 | `	}` |
|  264477 |  7904 | `	if( doBody ){` |
|       - |  7905 | `		/* Compile method body */` |
|  173111 |  7906 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  173111 |  7907 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7908 | `			return SXERR_ABORT;` |
|       - |  7909 | `		}` |
|   86558 |  7910 | `	}else{` |
|       - |  7911 | `		/* Only method signature is allowed */` |
|   91371 |  7912 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7913 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7914 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7915 | `				if( rc == SXERR_ABORT ){` |
|       - |  7916 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7917 | `					return SXERR_ABORT;` |
|       - |  7918 | `				}` |
|     ! 0 |  7919 | `				return SXERR_CORRUPT;` |
|       - |  7920 | `			}` |
|       - |  7921 | `	}` |
|       - |  7922 | `	/* All done,install the method */` |
|  264477 |  7923 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  264477 |  7924 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7925 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7926 | `		return SXERR_ABORT;` |
|       - |  7927 | `	}` |
|  264477 |  7928 | `	return SXRET_OK;` |
|       6 |  7929 | `Synchronize:` |
|       - |  7930 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7931 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7932 | `		pGen->pIn++;` |
|       4 |  7933 | `	}` |
|      16 |  7934 | `	return SXERR_CORRUPT;` |
|  132247 |  7935 |  |
|       - |  7936 | `/*` |
|       - |  7937 | ` * Compile an object interface.` |
|       - |  7938 | ` *  According to the PHP language reference manual` |
|       - |  7939 | ` *   Object Interfaces:` |
|       - |  7940 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7941 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7942 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7943 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7944 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7945 | ` */` |
|   38704 |  7946 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7947 |  |
|   38709 |  7948 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7949 | `	ph7_class *pClass,*pBase;` |
|       - |  7950 | `	SyToken *pEnd,*pTmp;` |
|       - |  7951 | `	SyString *pName;` |
|       - |  7952 | `	sxi32 nKwrd;` |
|       - |  7953 | `	sxi32 rc;` |
|       - |  7954 | `	/* Jump the 'interface' keyword */` |
|   38709 |  7955 | `	pGen->pIn++;` |
|       - |  7956 | `	/* Extract interface name */` |
|   38709 |  7957 | `	pName = &pGen->pIn->sData;` |
|       - |  7958 | `	/* Advance the stream cursor */` |
|   38709 |  7959 | `	pGen->pIn++;` |
|       - |  7960 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7961 | `		SyBlob sFQN;` |
|       - |  7962 | `		SyString sFQNStr;` |
|   38709 |  7963 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   38709 |  7964 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   38709 |  7965 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   38709 |  7966 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   38709 |  7967 | `		SyBlobRelease(&sFQN);` |
|       - |  7968 | `	}` |
|   38709 |  7969 | `	if( pClass == 0 ){` |
|     ! 0 |  7970 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7971 | `		return SXERR_ABORT;` |
|       - |  7972 | `	}` |
|       - |  7973 | `	/* Mark as an interface */` |
|   38709 |  7974 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7975 | `	/* Assume no base class is given */` |
|   38709 |  7976 | `	pBase = 0;` |
|   38709 |  7977 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10549 |  7978 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10549 |  7979 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7980 | `			SyBlob sResolved;` |
|       - |  7981 | `			SyString sBaseName;` |
|       - |  7982 | `			sxu32 nRefLine;` |
|       - |  7983 | `			/* Extract base interface */` |
|   10549 |  7984 | `			pGen->pIn++;` |
|   10549 |  7985 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10549 |  7986 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10549 |  7987 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7988 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7989 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7990 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7991 | `					pName);` |
|     ! 0 |  7992 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7993 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7994 | `					return SXERR_ABORT;` |
|       - |  7995 | `				}` |
|     ! 0 |  7996 | `				return SXRET_OK;` |
|       - |  7997 | `			}` |
|   15821 |  7998 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10544 |  7999 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10549 |  8000 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8001 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8002 | `			/* Only interfaces is allowed */` |
|   10549 |  8003 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8004 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8005 | `			}` |
|   10549 |  8006 | `			if( pBase == 0 ){` |
|     ! 0 |  8007 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8008 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  8009 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8010 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8011 | `					return SXERR_ABORT;` |
|       - |  8012 | `				}` |
|     ! 0 |  8013 | `			}` |
|   10549 |  8014 | `			SyBlobRelease(&sResolved);` |
|    5272 |  8015 | `		}` |
|    5272 |  8016 | `	}` |
|   38709 |  8017 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8018 | `		/* Syntax error */` |
|     ! 0 |  8019 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8020 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8021 | `		if( rc == SXERR_ABORT ){` |
|       - |  8022 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8023 | `			return SXERR_ABORT;` |
|       - |  8024 | `		}` |
|     ! 0 |  8025 | `		return SXRET_OK;` |
|       - |  8026 | `	}` |
|   38709 |  8027 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   38709 |  8028 | `	pEnd = 0; /* cc warning */` |
|       - |  8029 | `	/* Delimit the interface body */` |
|   38709 |  8030 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   38709 |  8031 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8032 | `		/* Syntax error */` |
|     ! 0 |  8033 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8034 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8035 | `		if( rc == SXERR_ABORT ){` |
|       - |  8036 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8037 | `			return SXERR_ABORT;` |
|       - |  8038 | `		}` |
|     ! 0 |  8039 | `		return SXRET_OK;` |
|       - |  8040 | `	}` |
|       - |  8041 | `	/* Swap token stream */` |
|   38709 |  8042 | `	pTmp = pGen->pEnd;` |
|   38709 |  8043 | `	pGen->pEnd = pEnd;` |
|       - |  8044 | `	/* Start the parse process` |
|       - |  8045 | `	 * Note (According to the PHP reference manual):` |
|       - |  8046 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8047 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8048 | `	 */` |
|   65031 |  8049 | `	for(;;){` |
|       - |  8050 | `		/* Jump leading/trailing semi-colons */` |
|  221425 |  8051 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   91363 |  8052 | `			pGen->pIn++;` |
|       5 |  8053 | `		}` |
|  130067 |  8054 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8055 | `			/* End of interface body */` |
|   38707 |  8056 | `			break;` |
|       - |  8057 | `		}` |
|   91365 |  8058 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8059 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8060 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8061 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8062 | `			if( rc == SXERR_ABORT ){` |
|       - |  8063 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8064 | `				return SXERR_ABORT;` |
|       - |  8065 | `			}` |
|     ! 0 |  8066 | `			goto done;` |
|       - |  8067 | `		}` |
|       - |  8068 | `		/* Extract the current keyword */` |
|   91365 |  8069 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   91365 |  8070 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8071 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8072 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8073 | `			const char *zKind = "member";` |
|       3 |  8074 | `			SyString *pMemberName = 0;` |
|       3 |  8075 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8076 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8077 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8078 | `					zKind = "constant";` |
|       3 |  8079 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8080 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8081 | `					}` |
|       1 |  8082 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8083 | `					zKind = "method";` |
|     ! 0 |  8084 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8085 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8086 | `					}` |
|     ! 0 |  8087 | `				}` |
|       1 |  8088 | `			}` |
|       3 |  8089 | `			if( pMemberName ){` |
|       4 |  8090 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8091 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8092 | `			}else{` |
|     ! 0 |  8093 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8094 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8095 | `			}` |
|       3 |  8096 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8097 | `				return SXERR_ABORT;` |
|       - |  8098 | `			}` |
|       3 |  8099 | `			goto done;` |
|       - |  8100 | `		}` |
|   91363 |  8101 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8102 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8103 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8104 | `			if( rc == SXERR_ABORT ){` |
|       - |  8105 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8106 | `				return SXERR_ABORT;` |
|       - |  8107 | `			}` |
|     ! 0 |  8108 | `			goto done;` |
|       - |  8109 | `		}` |
|   91363 |  8110 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8111 | `			/* Advance the stream cursor */` |
|   91353 |  8112 | `			pGen->pIn++;` |
|   91353 |  8113 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8114 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8115 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8116 | `				if( rc == SXERR_ABORT ){` |
|       - |  8117 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8118 | `					return SXERR_ABORT;` |
|       - |  8119 | `				}` |
|     ! 0 |  8120 | `				goto done;` |
|       - |  8121 | `			}` |
|   91353 |  8122 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   91353 |  8123 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8124 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8125 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8126 | `				if( rc == SXERR_ABORT ){` |
|       - |  8127 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8128 | `					return SXERR_ABORT;` |
|       - |  8129 | `				}` |
|     ! 0 |  8130 | `				goto done;` |
|       - |  8131 | `			}` |
|   45674 |  8132 | `		}` |
|   91363 |  8133 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8134 | `			/* Parse constant */` |
|       7 |  8135 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  8136 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8137 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8138 | `					return SXERR_ABORT;` |
|       - |  8139 | `				}` |
|     ! 0 |  8140 | `				goto done;` |
|       - |  8141 | `			}` |
|       4 |  8142 | `		}else{` |
|   91357 |  8143 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   91357 |  8144 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8145 | `				/* Static method,record that */` |
|   10541 |  8146 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8147 | `				/* Advance the stream cursor */` |
|   10541 |  8148 | `				pGen->pIn++;` |
|   10536 |  8149 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10541 |  8150 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8151 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8152 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8153 | `						if( rc == SXERR_ABORT ){` |
|       - |  8154 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8155 | `							return SXERR_ABORT;` |
|       - |  8156 | `						}` |
|     ! 0 |  8157 | `						goto done;` |
|       - |  8158 | `				}` |
|    5268 |  8159 | `			}` |
|       - |  8160 | `			/* Process method signature (no body for interface methods) */` |
|   91357 |  8161 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   91357 |  8162 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8163 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8164 | `					return SXERR_ABORT;` |
|       - |  8165 | `				}` |
|     ! 0 |  8166 | `				goto done;` |
|       - |  8167 | `			}` |
|       - |  8168 | `		}` |
|       5 |  8169 | `	}` |
|       - |  8170 | `	/* Install the interface */` |
|   38707 |  8171 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   38707 |  8172 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8173 | `		/* Inherit from the base interface */` |
|   10549 |  8174 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5272 |  8175 | `	}` |
|   38707 |  8176 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8177 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8178 | `		return SXERR_ABORT;` |
|       - |  8179 | `	}` |
|   19351 |  8180 | `done:` |
|       - |  8181 | `	/* Point beyond the interface body */` |
|   38709 |  8182 | `	pGen->pIn  = &pEnd[1];` |
|   38709 |  8183 | `	pGen->pEnd = pTmp;` |
|   38709 |  8184 | `	return PH7_OK;` |
|   19357 |  8185 |  |
|       - |  8186 | `/*` |
|       - |  8187 | ` * Compile a user-defined class.` |
|       - |  8188 | ` * According to the PHP language reference manual` |
|       - |  8189 | ` *  class` |
|       - |  8190 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8191 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8192 | ` *  of the properties and methods belonging to the class.` |
|       - |  8193 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8194 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8195 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8196 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8197 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8198 | ` *  (called "methods").` |
|       - |  8199 | ` */` |
|       - |  8200 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8201 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8202 | `struct TraitUseEntry {` |
|       - |  8203 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8204 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8205 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8206 | `};` |
|       - |  8207 | `/*` |
|       - |  8208 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8209 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8210 | ` */` |
|   95974 |  8211 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8212 |  |
|       - |  8213 | `	ph7_class **apIface;` |
|       - |  8214 | `	sxu32 nIface,i;` |
|       - |  8215 | `	sxi32 rc;` |
|   95979 |  8216 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8217 | `		return SXRET_OK;` |
|       - |  8218 | `	}` |
|   95979 |  8219 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   95979 |  8220 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  191041 |  8221 | `	for(i = 0; i < nIface; i++){` |
|   95067 |  8222 | `		ph7_class *pIface = apIface[i];` |
|       - |  8223 | `		SyHashEntry *pEntry;` |
|   95067 |  8224 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  253553 |  8225 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  158491 |  8226 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8227 | `			ph7_class_method *pImplMeth;` |
|  158491 |  8228 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8229 | `			/* Find the implementing method in the class */` |
|  158491 |  8230 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  158491 |  8231 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8232 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8233 | `			}` |
|       - |  8234 | `			/* Check visibility: interface methods must be implemented as public */` |
|  158477 |  8235 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8236 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8237 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8238 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8239 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8240 | `					return SXERR_ABORT;` |
|       - |  8241 | `				}` |
|       1 |  8242 | `			}` |
|       - |  8243 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8244 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8245 | `			 */` |
|       - |  8246 | `			{` |
|  158477 |  8247 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  158477 |  8248 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  158477 |  8249 | `				int sigError = 0;` |
|  158477 |  8250 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8251 | `					sigError = 1;` |
|  158476 |  8252 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8253 | `					/* Extra parameters must all have default values */` |
|       6 |  8254 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8255 | `					sxu32 k;` |
|       8 |  8256 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8257 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8258 | `							sigError = 1;` |
|       3 |  8259 | `							break;` |
|       - |  8260 | `						}` |
|       2 |  8261 | `					}` |
|       2 |  8262 | `				}` |
|  158477 |  8263 | `				if( sigError ){` |
|       - |  8264 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8265 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8266 | `					sxu32 j;` |
|       6 |  8267 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8268 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8269 | `					/* Build implementing method signature */` |
|       6 |  8270 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8271 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8272 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8273 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8274 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8275 | `					}` |
|       - |  8276 | `					/* Build interface method signature */` |
|       6 |  8277 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8278 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8279 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8280 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8281 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8282 | `					}` |
|       8 |  8283 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8284 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8285 | `						&pClass->sName,pMName,` |
|       4 |  8286 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8287 | `						&pIface->sName,pMName,` |
|       4 |  8288 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8289 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8290 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8291 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8292 | `						return SXERR_ABORT;` |
|       - |  8293 | `					}` |
|       2 |  8294 | `				}` |
|       - |  8295 | `			}` |
|       5 |  8296 | `		}` |
|   47536 |  8297 | `	}` |
|   95979 |  8298 | `	return SXRET_OK;` |
|   47992 |  8299 |  |
|       - |  8300 | `/*` |
|       - |  8301 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8302 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8303 | ` */` |
|   95974 |  8304 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8305 |  |
|       - |  8306 | `	ph7_class_method *pMeth;` |
|       - |  8307 | `	SyHashEntry *pEntry;` |
|       - |  8308 | `	sxu32 nAbstract;` |
|       - |  8309 | `	SyBlob sMsg;` |
|       - |  8310 | `	sxi32 rc;` |
|       - |  8311 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   95979 |  8312 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      29 |  8313 | `		return SXRET_OK;` |
|       - |  8314 | `	}` |
|       - |  8315 | `	/* Count abstract methods */` |
|   95955 |  8316 | `	nAbstract = 0;` |
|   95955 |  8317 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  929899 |  8318 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  833949 |  8319 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  833949 |  8320 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8321 | `			nAbstract++;` |
|       8 |  8322 | `		}` |
|       5 |  8323 | `	}` |
|   95955 |  8324 | `	if( nAbstract == 0 ){` |
|   95941 |  8325 | `		return SXRET_OK;` |
|       - |  8326 | `	}` |
|       - |  8327 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8328 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8329 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8330 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8331 | `		&pClass->sName,nAbstract,` |
|       7 |  8332 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8333 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8334 | `	/* Second pass: list methods with origins */` |
|       - |  8335 | `	{` |
|      18 |  8336 | `		sxu32 nListed = 0;` |
|      18 |  8337 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8338 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8339 | `			ph7_class *pOrigin = 0;` |
|       - |  8340 | `			SyString *pMName;` |
|      22 |  8341 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8342 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8343 | `				continue;` |
|       - |  8344 | `			}` |
|      20 |  8345 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8346 | `			if( nListed > 0 ){` |
|       3 |  8347 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8348 | `			}` |
|       - |  8349 | `			/* Find the origin of this abstract method.` |
|       - |  8350 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8351 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8352 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8353 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8354 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8355 | `			 * class's namespace.` |
|       - |  8356 | `			 */` |
|       - |  8357 | `			{` |
|       - |  8358 | `				ph7_class **apIface;` |
|       - |  8359 | `				ph7_class **apTrait;` |
|       - |  8360 | `				ph7_class *pWalk;` |
|       - |  8361 | `				sxu32 i;` |
|       - |  8362 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8363 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8364 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8365 | `				 */` |
|      20 |  8366 | `				if( pClass->pBase ){` |
|      11 |  8367 | `					pWalk = pClass->pBase;` |
|      19 |  8368 | `					while( pWalk ){` |
|       - |  8369 | `						ph7_class_method *pParentMeth;` |
|      13 |  8370 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8371 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8372 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8373 | `							 * in this class's ancestor chain.` |
|       - |  8374 | `							 */` |
|      13 |  8375 | `							int fromIface = 0;` |
|      13 |  8376 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8377 | `							while( pAnc ){` |
|       - |  8378 | `								ph7_class **apPI;` |
|       - |  8379 | `								sxu32 j;` |
|      15 |  8380 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8381 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8382 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8383 | `										fromIface = 1;` |
|      10 |  8384 | `										break;` |
|       - |  8385 | `									}` |
|     ! 0 |  8386 | `								}` |
|      15 |  8387 | `								if( fromIface ) break;` |
|       6 |  8388 | `								pAnc = pAnc->pBase;` |
|       2 |  8389 | `							}` |
|      13 |  8390 | `							if( !fromIface ){` |
|       3 |  8391 | `								pOrigin = pWalk;` |
|       3 |  8392 | `								break;` |
|       - |  8393 | `							}` |
|       4 |  8394 | `						}` |
|      10 |  8395 | `						pWalk = pWalk->pBase;` |
|       2 |  8396 | `					}` |
|       4 |  8397 | `				}` |
|       - |  8398 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8399 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8400 | `				 */` |
|      20 |  8401 | `				if( !pOrigin ){` |
|      18 |  8402 | `					pWalk = pClass;` |
|      40 |  8403 | `					while( pWalk && !pOrigin ){` |
|      26 |  8404 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8405 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8406 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8407 | `							ph7_class *pDeepest = 0;` |
|      28 |  8408 | `							while( pIface ){` |
|      16 |  8409 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8410 | `									pDeepest = pIface;` |
|       6 |  8411 | `								}` |
|      16 |  8412 | `								pIface = pIface->pBase;` |
|       4 |  8413 | `							}` |
|      16 |  8414 | `							if( pDeepest ){` |
|      16 |  8415 | `								pOrigin = pDeepest;` |
|      16 |  8416 | `								break;` |
|       - |  8417 | `							}` |
|     ! 0 |  8418 | `						}` |
|      26 |  8419 | `						pWalk = pWalk->pBase;` |
|       4 |  8420 | `					}` |
|       7 |  8421 | `				}` |
|       - |  8422 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8423 | `				if( !pOrigin ){` |
|       3 |  8424 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8425 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8426 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8427 | `							pOrigin = pClass;` |
|       3 |  8428 | `							break;` |
|       - |  8429 | `						}` |
|     ! 0 |  8430 | `					}` |
|       1 |  8431 | `				}` |
|       - |  8432 | `			}` |
|      20 |  8433 | `			if( pOrigin ){` |
|      20 |  8434 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8435 | `			}else{` |
|       - |  8436 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8437 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8438 | `			}` |
|      20 |  8439 | `			nListed++;` |
|       4 |  8440 | `		}` |
|       - |  8441 | `	}` |
|      18 |  8442 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8443 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8444 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8445 | `	SyBlobRelease(&sMsg);` |
|      18 |  8446 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8447 | `		return SXERR_ABORT;` |
|       - |  8448 | `	}` |
|      18 |  8449 | `	return SXRET_OK;` |
|   47992 |  8450 |  |
|       - |  8451 | `/*` |
|       - |  8452 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8453 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8454 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8455 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8456 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8457 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8458 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8459 | ` */` |
|   95676 |  8460 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8461 |  |
|   95681 |  8462 | `	int isAbsolute = 0;` |
|   95681 |  8463 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8464 | `	SyBlob sName;` |
|   95681 |  8465 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      89 |  8466 | `		isAbsolute = 1;` |
|      89 |  8467 | `		pGen->pIn++;` |
|      42 |  8468 | `	}` |
|   95681 |  8469 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8470 | `		pGen->pIn = pStart;` |
|       8 |  8471 | `		return SXERR_INVALID;` |
|       - |  8472 | `	}` |
|   95675 |  8473 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   95675 |  8474 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   95675 |  8475 | `	pGen->pIn++;` |
|  143523 |  8476 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   47858 |  8477 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8478 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8479 | `		pGen->pIn++;` |
|      13 |  8480 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8481 | `		pGen->pIn++;` |
|       1 |  8482 | `	}` |
|   95675 |  8483 | `	if( isAbsolute ){` |
|      87 |  8484 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      46 |  8485 | `	}else{` |
|       - |  8486 | `		SyString sRaw;` |
|   95593 |  8487 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   95593 |  8488 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8489 | `	}` |
|   95675 |  8490 | `	SyBlobRelease(&sName);` |
|   95675 |  8491 | `	return SXRET_OK;` |
|   47843 |  8492 |  |
|       - |  8493 | `/*` |
|       - |  8494 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8495 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8496 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8497 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8498 | ` * either direction cannot run unbounded.` |
|       - |  8499 | ` */` |
|       - |  8500 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10700 |  8501 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8502 |  |
|       - |  8503 | `	ph7_class **apParent;` |
|       - |  8504 | `	sxu32 n;` |
|   17925 |  8505 | `	while( pInterface ){` |
|   14259 |  8506 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8507 | `			return FALSE;` |
|       - |  8508 | `		}` |
|   17784 |  8509 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7050 |  8510 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7039 |  8511 | `			return TRUE;` |
|       - |  8512 | `		}` |
|    7225 |  8513 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7225 |  8514 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8515 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8516 | `				return TRUE;` |
|       - |  8517 | `			}` |
|     ! 0 |  8518 | `		}` |
|    7225 |  8519 | `		pInterface = pInterface->pBase;` |
|    7225 |  8520 | `		iDepth++;` |
|       5 |  8521 | `	}` |
|    3671 |  8522 | `	return FALSE;` |
|    5355 |  8523 |  |
|   10700 |  8524 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8525 |  |
|   10705 |  8526 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8527 |  |
|       - |  8528 | `/*` |
|       - |  8529 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8530 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8531 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8532 | ` */` |
|    7034 |  8533 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8534 |  |
|    7043 |  8535 | `	while( pBase ){` |
|      10 |  8536 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8537 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8538 | `			return TRUE;` |
|       - |  8539 | `		}` |
|      10 |  8540 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8541 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8542 | `			return TRUE;` |
|       - |  8543 | `		}` |
|       5 |  8544 | `		pBase = pBase->pBase;` |
|       1 |  8545 | `	}` |
|    7035 |  8546 | `	return FALSE;` |
|    3522 |  8547 |  |
|       - |  8548 | `/*` |
|       - |  8549 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8550 | ` *` |
|       - |  8551 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8552 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8553 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8554 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8555 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8556 | ` * implements, body, install) is shared by both paths.` |
|       - |  8557 | ` */` |
|   96004 |  8558 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8559 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8560 |  |
|   96009 |  8561 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8562 | `	ph7_class *pClass,*pBase;` |
|       - |  8563 | `	SyToken *pEnd,*pTmp;` |
|       - |  8564 | `	sxi32 iProtection;` |
|       - |  8565 | `	SySet aInterfaces;` |
|       - |  8566 | `	SySet aUseEntries;` |
|       - |  8567 | `	sxi32 iAttrflags;` |
|       - |  8568 | `	SyString *pName;` |
|       - |  8569 | `	sxi32 nKwrd;` |
|       - |  8570 | `	sxi32 rc;` |
|       - |  8571 | `	/* Jump the 'class' keyword */` |
|   96009 |  8572 | `	pGen->pIn++;` |
|   96009 |  8573 | `	if( pAnonName ){` |
|       - |  8574 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8575 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8576 | `		 * then use the synthesized name. */` |
|      29 |  8577 | `		*ppArgStart = *ppArgEnd = 0;` |
|      29 |  8578 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8579 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8580 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8581 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8582 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8583 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8584 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8585 | `		}` |
|      29 |  8586 | `		pName = pAnonName;` |
|      29 |  8587 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      16 |  8588 | `	}else{` |
|   95983 |  8589 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8590 | `			/* Syntax error */` |
|     ! 0 |  8591 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8592 | `			if( rc == SXERR_ABORT ){` |
|       - |  8593 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8594 | `				return SXERR_ABORT;` |
|       - |  8595 | `			}` |
|       - |  8596 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8597 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8598 | `				pGen->pIn++;` |
|     ! 0 |  8599 | `			}` |
|     ! 0 |  8600 | `			return SXRET_OK;` |
|       - |  8601 | `		}` |
|       - |  8602 | `		/* Extract class name */` |
|   95983 |  8603 | `		pName = &pGen->pIn->sData;` |
|       - |  8604 | `		/* Advance the stream cursor */` |
|   95983 |  8605 | `		pGen->pIn++;` |
|       - |  8606 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8607 | `			SyBlob sFQN;` |
|       - |  8608 | `			SyString sFQNStr;` |
|   95983 |  8609 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   95983 |  8610 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   95983 |  8611 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   95983 |  8612 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   95983 |  8613 | `			SyBlobRelease(&sFQN);` |
|       - |  8614 | `		}` |
|       - |  8615 | `	}` |
|   96009 |  8616 | `	if( pClass == 0 ){` |
|     ! 0 |  8617 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8618 | `		return SXERR_ABORT;` |
|       - |  8619 | `	}` |
|       - |  8620 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   96009 |  8621 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   96009 |  8622 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8623 | `	/* Assume a standalone class */` |
|   96009 |  8624 | `	pBase = 0;` |
|   96009 |  8625 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   84605 |  8626 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   84605 |  8627 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8628 | `			SyBlob sResolved;` |
|       - |  8629 | `			SyString sBaseName;` |
|       - |  8630 | `			sxu32 nRefLine;` |
|   73923 |  8631 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   73923 |  8632 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   73923 |  8633 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   73923 |  8634 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8635 | `				SyBlobRelease(&sResolved);` |
|       4 |  8636 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8637 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8638 | `					pName);` |
|       3 |  8639 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8640 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8641 | `					return SXERR_ABORT;` |
|       - |  8642 | `				}` |
|       3 |  8643 | `				return SXRET_OK;` |
|       - |  8644 | `			}` |
|  110879 |  8645 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   73916 |  8646 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   73921 |  8647 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8648 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8649 | `			/* Interfaces are not allowed */` |
|   73921 |  8650 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8651 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8652 | `			}` |
|   73921 |  8653 | `			if( pBase == 0 ){` |
|     ! 0 |  8654 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8655 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8656 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8657 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8658 | `					return SXERR_ABORT;` |
|       - |  8659 | `				}` |
|     ! 0 |  8660 | `			}else{` |
|   73921 |  8661 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8662 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8663 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8664 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8665 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8666 | `						return SXERR_ABORT;` |
|       - |  8667 | `					}` |
|     ! 0 |  8668 | `				}` |
|       - |  8669 | `			}` |
|   73921 |  8670 | `			SyBlobRelease(&sResolved);` |
|   36958 |  8671 | `		}` |
|   84603 |  8672 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8673 | `			ph7_class *pInterface;` |
|       - |  8674 | `			/* Interface implementation */` |
|   10695 |  8675 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5355 |  8676 | `			for(;;){` |
|       - |  8677 | `				SyBlob sResolved;` |
|       - |  8678 | `				SyString sIntName;` |
|       - |  8679 | `				sxu32 nRefLine;` |
|   10705 |  8680 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10705 |  8681 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10705 |  8682 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8683 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8684 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8685 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8686 | `						pName);` |
|     ! 0 |  8687 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8688 | `						return SXERR_ABORT;` |
|       - |  8689 | `					}` |
|     ! 0 |  8690 | `					break;` |
|       - |  8691 | `				}` |
|   21405 |  8692 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10700 |  8693 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10705 |  8694 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8695 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8696 | `				/* Only interfaces are allowed */` |
|   10705 |  8697 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8698 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8699 | `				}` |
|   10705 |  8700 | `				if( pInterface == 0 ){` |
|     ! 0 |  8701 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8702 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8703 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8704 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8705 | `						return SXERR_ABORT;` |
|       - |  8706 | `					}` |
|     ! 0 |  8707 | `				}else{` |
|       - |  8708 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8709 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8710 | `					 * unless they already extend Exception or Error.` |
|       - |  8711 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8712 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8713 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10705 |  8714 | `					SyString *pFqn = &pClass->sName;` |
|   10705 |  8715 | `					int bIsExceptionOrError =` |
|    8865 |  8716 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   17809 |  8717 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    8951 |  8718 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3526 |  8719 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   17732 |  8720 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10554 |  8721 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3515 |  8722 | `						!bIsExceptionOrError ){` |
|      12 |  8723 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8724 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8725 | `							&pClass->sName);` |
|       9 |  8726 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8727 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8728 | `							return SXERR_ABORT;` |
|       - |  8729 | `						}` |
|       - |  8730 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8731 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8732 | `					}else{` |
|   10699 |  8733 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8734 | `					}` |
|       - |  8735 | `				}` |
|   10705 |  8736 | `				SyBlobRelease(&sResolved);` |
|   10705 |  8737 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5350 |  8738 | `					break;` |
|       - |  8739 | `				}` |
|      13 |  8740 | `				pGen->pIn++;/* Jump the comma */` |
|       3 |  8741 | `			}` |
|    5345 |  8742 | `		}` |
|   42299 |  8743 | `	}` |
|   96007 |  8744 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8745 | `		/* Syntax error */` |
|     ! 0 |  8746 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8747 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8748 | `		if( rc == SXERR_ABORT ){` |
|       - |  8749 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8750 | `			return SXERR_ABORT;` |
|       - |  8751 | `		}` |
|     ! 0 |  8752 | `		return SXRET_OK;` |
|       - |  8753 | `	}` |
|   96007 |  8754 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   96007 |  8755 | `	pEnd = 0; /* cc warning */` |
|       - |  8756 | `	/* Delimit the class body */` |
|   96007 |  8757 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   96007 |  8758 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8759 | `		/* Syntax error */` |
|     ! 0 |  8760 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8761 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8762 | `		if( rc == SXERR_ABORT ){` |
|       - |  8763 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8764 | `			return SXERR_ABORT;` |
|       - |  8765 | `		}` |
|     ! 0 |  8766 | `		return SXRET_OK;` |
|       - |  8767 | `	}` |
|       - |  8768 | `	/* Swap token stream */` |
|   96007 |  8769 | `	pTmp = pGen->pEnd;` |
|   96007 |  8770 | `	pGen->pEnd = pEnd;` |
|       - |  8771 | `	/* Set the inherited flags */` |
|   96007 |  8772 | `	pClass->iFlags = iFlags;` |
|       - |  8773 | `	/* Start the parse process */` |
|  134564 |  8774 | `	for(;;){` |
|       - |  8775 | `		/* Jump leading/trailing semi-colons */` |
|  403987 |  8776 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   67465 |  8777 | `			pGen->pIn++;` |
|       5 |  8778 | `		}` |
|  336527 |  8779 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8780 | `			/* End of class body */` |
|   95979 |  8781 | `			break;` |
|       - |  8782 | `		}` |
|  240548 |  8783 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  120279 |  8784 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8785 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8786 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8787 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8788 | `			if( rc == SXERR_ABORT ){` |
|       - |  8789 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8790 | `				return SXERR_ABORT;` |
|       - |  8791 | `			}` |
|     ! 0 |  8792 | `			goto done;` |
|       - |  8793 | `		}` |
|       - |  8794 | `		/* Assume public visibility */` |
|  240553 |  8795 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  240553 |  8796 | `		iAttrflags = 0;` |
|       - |  8797 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8798 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8799 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8800 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  240553 |  8801 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8802 | `			int bMod = 0;` |
|     ! 0 |  8803 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8804 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8805 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8806 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8807 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8808 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8809 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8810 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8811 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8812 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8813 | `			}` |
|     ! 0 |  8814 | `			if( !bMod ){` |
|     ! 0 |  8815 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8816 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8817 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8818 | `						return SXERR_ABORT;` |
|       - |  8819 | `					}` |
|     ! 0 |  8820 | `					goto done;` |
|       - |  8821 | `				}` |
|     ! 0 |  8822 | `				continue;` |
|       - |  8823 | `			}` |
|     ! 0 |  8824 | `		}` |
|  240553 |  8825 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8826 | `			/* Extract the current keyword */` |
|  240553 |  8827 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  240553 |  8828 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8829 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8830 | `				TraitUseEntry sUse;` |
|      53 |  8831 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      53 |  8832 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      53 |  8833 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      32 |  8834 | `				for(;;){` |
|       - |  8835 | `					ph7_class *pTrait;` |
|       - |  8836 | `					SyString *pTraitName;` |
|      61 |  8837 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8838 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8839 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8840 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8841 | `							return SXERR_ABORT;` |
|       - |  8842 | `						}` |
|     ! 0 |  8843 | `						break;` |
|       - |  8844 | `					}` |
|      61 |  8845 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8846 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8847 | `						SyBlob sResolved;` |
|      61 |  8848 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      61 |  8849 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     117 |  8850 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      56 |  8851 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      61 |  8852 | `						SyBlobRelease(&sResolved);` |
|       - |  8853 | `					}` |
|       - |  8854 | `					/* Only traits are allowed */` |
|      61 |  8855 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8856 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8857 | `					}` |
|      61 |  8858 | `					if( pTrait == 0 ){` |
|     ! 0 |  8859 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8860 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8861 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8862 | `							return SXERR_ABORT;` |
|       - |  8863 | `						}` |
|     ! 0 |  8864 | `					}else{` |
|      61 |  8865 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8866 | `					}` |
|      61 |  8867 | `					pGen->pIn++; /* Advance past trait name */` |
|      61 |  8868 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      29 |  8869 | `						break;` |
|       - |  8870 | `					}` |
|      10 |  8871 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8872 | `				}` |
|       - |  8873 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      53 |  8874 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8875 | `					SyToken *pBlock;` |
|      13 |  8876 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  8877 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  8878 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  8879 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  8880 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  8881 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  8882 | `					}else{` |
|     ! 0 |  8883 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8884 | `					}` |
|       5 |  8885 | `				}` |
|      53 |  8886 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8887 | `				/* The semicolon will be consumed by the outer loop */` |
|      53 |  8888 | `				continue;` |
|       - |  8889 | `			}` |
|  240505 |  8890 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  236747 |  8891 | `				iProtection = nKwrd;` |
|  236747 |  8892 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8893 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  236747 |  8894 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8895 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8896 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8897 | `				}` |
|  236742 |  8898 | `				if( pGen->pIn >= pGen->pEnd` |
|  236747 |  8899 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8900 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8901 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8902 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8903 | `					if( rc == SXERR_ABORT ){` |
|       - |  8904 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8905 | `						return SXERR_ABORT;` |
|       - |  8906 | `					}` |
|     ! 0 |  8907 | `					goto done;` |
|       - |  8908 | `				}` |
|  236747 |  8909 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8910 | `					/* Attribute declaration (untyped) */` |
|   67175 |  8911 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   67175 |  8912 | `					if( rc != SXRET_OK ){` |
|       9 |  8913 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8914 | `							return SXERR_ABORT;` |
|       - |  8915 | `						}` |
|       9 |  8916 | `						goto done;` |
|       - |  8917 | `					}` |
|   67169 |  8918 | `					continue;` |
|       - |  8919 | `				}` |
|  169577 |  8920 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8921 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     167 |  8922 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     167 |  8923 | `					if( rc != SXRET_OK ){` |
|       8 |  8924 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8925 | `							return SXERR_ABORT;` |
|       - |  8926 | `						}` |
|       8 |  8927 | `						goto done;` |
|       - |  8928 | `					}` |
|     161 |  8929 | `					continue;` |
|       - |  8930 | `				}` |
|       - |  8931 | `				/* Extract the keyword */` |
|  169415 |  8932 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   84705 |  8933 | `			}` |
|  173173 |  8934 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8935 | `				/* Process constant declaration */` |
|      67 |  8936 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      67 |  8937 | `				if( rc != SXRET_OK ){` |
|       3 |  8938 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8939 | `						return SXERR_ABORT;` |
|       - |  8940 | `					}` |
|       3 |  8941 | `					goto done;` |
|       - |  8942 | `				}` |
|      35 |  8943 | `			}else{` |
|  173111 |  8944 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8945 | `					/* Static method or attribute,record that */` |
|    3565 |  8946 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3565 |  8947 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3565 |  8948 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8949 | `						/* Extract the keyword */` |
|    3557 |  8950 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3557 |  8951 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8952 | `							iProtection = nKwrd;` |
|     ! 0 |  8953 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8954 | `						}` |
|    1776 |  8955 | `					}` |
|       - |  8956 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8957 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8958 | `					 * than a generic "expecting method" parse error. */` |
|    3565 |  8959 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8960 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8961 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8962 | `					}` |
|    3560 |  8963 | `					if( pGen->pIn >= pGen->pEnd` |
|    3565 |  8964 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8965 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8966 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8967 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8968 | `						if( rc == SXERR_ABORT ){` |
|       - |  8969 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8970 | `							return SXERR_ABORT;` |
|       - |  8971 | `						}` |
|     ! 0 |  8972 | `						goto done;` |
|       - |  8973 | `					}` |
|    3565 |  8974 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8975 | `						/* Attribute declaration */` |
|       8 |  8976 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  8977 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8978 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8979 | `								return SXERR_ABORT;` |
|       - |  8980 | `							}` |
|     ! 0 |  8981 | `							goto done;` |
|       - |  8982 | `						}` |
|       8 |  8983 | `						continue;` |
|       - |  8984 | `					}` |
|    3559 |  8985 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8986 | `						/* Typed static attribute declaration */` |
|      15 |  8987 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8988 | `						if( rc != SXRET_OK ){` |
|       3 |  8989 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8990 | `								return SXERR_ABORT;` |
|       - |  8991 | `							}` |
|       3 |  8992 | `							goto done;` |
|       - |  8993 | `						}` |
|      13 |  8994 | `						continue;` |
|       - |  8995 | `					}` |
|       - |  8996 | `					/* Extract the keyword */` |
|    3547 |  8997 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  171322 |  8998 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8999 | `					/* Abstract method,record that */` |
|      12 |  9000 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  9001 | `					/* Mark the whole class as abstract */` |
|      12 |  9002 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  9003 | `					/* Advance the stream cursor */` |
|      12 |  9004 | `					pGen->pIn++;` |
|      12 |  9005 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  9006 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  9007 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  9008 | `							iProtection = nKwrd;` |
|      10 |  9009 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  9010 | `						}` |
|       5 |  9011 | `					}` |
|      12 |  9012 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  9013 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9014 | `							/* Static method */` |
|     ! 0 |  9015 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9016 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9017 | `					}` |
|      12 |  9018 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  9019 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9020 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9021 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9022 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9023 | `							if( rc == SXERR_ABORT ){` |
|       - |  9024 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9025 | `								return SXERR_ABORT;` |
|       - |  9026 | `							}` |
|     ! 0 |  9027 | `							goto done;` |
|       - |  9028 | `					}` |
|      12 |  9029 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  169546 |  9030 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9031 | `					/* final method ,record that */` |
|      17 |  9032 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9033 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9034 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9035 | `						/* Extract the keyword */` |
|      17 |  9036 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9037 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  9038 | `							iProtection = nKwrd;` |
|       9 |  9039 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9040 | `						}` |
|       7 |  9041 | `					}` |
|      17 |  9042 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9043 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9044 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9045 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9046 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9047 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9048 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9049 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9050 | `									return SXERR_ABORT;` |
|       - |  9051 | `								}` |
|     ! 0 |  9052 | `								goto done;` |
|       - |  9053 | `							}` |
|      12 |  9054 | `							continue;` |
|       - |  9055 | `					}` |
|       6 |  9056 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9057 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9058 | `							/* Static method */` |
|     ! 0 |  9059 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9060 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9061 | `					}` |
|       6 |  9062 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9063 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9064 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9065 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9066 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9067 | `							if( rc == SXERR_ABORT ){` |
|       - |  9068 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9069 | `								return SXERR_ABORT;` |
|       - |  9070 | `							}` |
|     ! 0 |  9071 | `							goto done;` |
|       - |  9072 | `					}` |
|       6 |  9073 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9074 | `				}` |
|  173083 |  9075 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9076 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9077 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9078 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9079 | `						if( rc == SXERR_ABORT ){` |
|       - |  9080 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9081 | `							return SXERR_ABORT;` |
|       - |  9082 | `						}` |
|     ! 0 |  9083 | `						goto done;` |
|       - |  9084 | `				}` |
|  173083 |  9085 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9086 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9087 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9088 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9089 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9090 | `						if( rc == SXERR_ABORT ){` |
|       - |  9091 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9092 | `							return SXERR_ABORT;` |
|       - |  9093 | `						}` |
|     ! 0 |  9094 | `						goto done;` |
|       - |  9095 | `					}` |
|       - |  9096 | `					/* Attribute declaration */` |
|       7 |  9097 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9098 | `				}else{` |
|       - |  9099 | `					/* Process method declaration */` |
|  173077 |  9100 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9101 | `				}` |
|  173083 |  9102 | `				if( rc != SXRET_OK ){` |
|      16 |  9103 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9104 | `						return SXERR_ABORT;` |
|       - |  9105 | `					}` |
|      16 |  9106 | `					goto done;` |
|       - |  9107 | `				}` |
|       - |  9108 | `			}` |
|   86568 |  9109 | `		}else{` |
|       - |  9110 | `			/* Attribute declaration */` |
|     ! 0 |  9111 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9112 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9113 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9114 | `					return SXERR_ABORT;` |
|       - |  9115 | `				}` |
|     ! 0 |  9116 | `				goto done;` |
|       - |  9117 | `			}` |
|       - |  9118 | `		}` |
|       5 |  9119 | `	}` |
|       - |  9120 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9121 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9122 | `	 */` |
|       - |  9123 | `	{` |
|       - |  9124 | `		TraitUseEntry *apUse;` |
|       - |  9125 | `		sxu32 nU;` |
|   95979 |  9126 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   96027 |  9127 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      53 |  9128 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      53 |  9129 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      53 |  9130 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      53 |  9131 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9132 | `			sxu32 nT;` |
|      53 |  9133 | `			if( !hasResolution ){` |
|       - |  9134 | `				/* No conflict resolution block: use standard trait application */` |
|      87 |  9135 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      49 |  9136 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      49 |  9137 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9138 | `						break;` |
|       - |  9139 | `					}` |
|      27 |  9140 | `				}` |
|      24 |  9141 | `			}else{` |
|       - |  9142 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9143 | `				 * then use the block to resolve method conflicts.` |
|       - |  9144 | `				 */` |
|       - |  9145 | `				SyToken *pR;` |
|      25 |  9146 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9147 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9148 | `					ph7_class_attr *pAR;` |
|       - |  9149 | `					SyHashEntry *pER;` |
|       - |  9150 | `					SyString *pNR;` |
|      15 |  9151 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9152 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9153 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9154 | `						pNR = &pAR->sName;` |
|     ! 0 |  9155 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9156 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9157 | `						}` |
|     ! 0 |  9158 | `					}` |
|      15 |  9159 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9160 | `				}` |
|       - |  9161 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9162 | `				pR = pUse->pResolvStart;` |
|      27 |  9163 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9164 | `					SyString sTrait,sMethod;` |
|       - |  9165 | `					ph7_class *pSrcTrait;` |
|       - |  9166 | `					ph7_class_method *pMeth;` |
|       - |  9167 | `					sxi32 nRKwrd;` |
|      41 |  9168 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9169 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9170 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9171 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9172 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9173 | `					sMethod = pR->sData;` |
|      17 |  9174 | `					pR++;` |
|      17 |  9175 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9176 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9177 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9178 | `							sTrait = sMethod;` |
|       7 |  9179 | `							pR++;` |
|       7 |  9180 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9181 | `							sMethod = pR->sData;` |
|       7 |  9182 | `							pR++;` |
|       3 |  9183 | `						}` |
|       3 |  9184 | `					}` |
|      17 |  9185 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9186 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9187 | `						continue;` |
|       - |  9188 | `					}` |
|      17 |  9189 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9190 | `					pR++;` |
|      17 |  9191 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9192 | `						pSrcTrait = 0;` |
|       7 |  9193 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9194 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9195 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9196 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9197 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9198 | `								break;` |
|       - |  9199 | `							}` |
|       2 |  9200 | `						}` |
|       5 |  9201 | `						if( pSrcTrait ){` |
|       5 |  9202 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9203 | `							if( pMeth ){` |
|       5 |  9204 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9205 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9206 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9207 | `								}` |
|       2 |  9208 | `							}` |
|       2 |  9209 | `						}` |
|       2 |  9210 | `					}` |
|      35 |  9211 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9212 | `				}` |
|       - |  9213 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9214 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9215 | `					ph7_class_method *pMR;` |
|       - |  9216 | `					SyHashEntry *pER;` |
|       - |  9217 | `					SyString *pNR;` |
|      15 |  9218 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9219 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9220 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9221 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9222 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9223 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9224 | `						}` |
|       3 |  9225 | `					}` |
|       9 |  9226 | `				}` |
|       - |  9227 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9228 | `				pR = pUse->pResolvStart;` |
|      27 |  9229 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9230 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9231 | `					ph7_class *pSrcTrait;` |
|       - |  9232 | `					ph7_class_method *pMeth;` |
|      27 |  9233 | `					int hasQual = 0;` |
|       - |  9234 | `					sxi32 nRKwrd;` |
|      41 |  9235 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9236 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9237 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9238 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9239 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9240 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9241 | `					sMethod = pR->sData;` |
|      17 |  9242 | `					pR++;` |
|      17 |  9243 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9244 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9245 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9246 | `							sTrait = sMethod;` |
|       7 |  9247 | `							hasQual = 1;` |
|       7 |  9248 | `							pR++;` |
|       7 |  9249 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9250 | `							sMethod = pR->sData;` |
|       7 |  9251 | `							pR++;` |
|       3 |  9252 | `						}` |
|       3 |  9253 | `					}` |
|      17 |  9254 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9255 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9256 | `						continue;` |
|       - |  9257 | `					}` |
|      17 |  9258 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9259 | `					pR++;` |
|      17 |  9260 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9261 | `						sxi32 iNewVis = -1;` |
|      13 |  9262 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9263 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9264 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9265 | `								iNewVis = nAK;` |
|       7 |  9266 | `								pR++;` |
|       3 |  9267 | `							}` |
|       3 |  9268 | `						}` |
|      13 |  9269 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9270 | `							sAlias = pR->sData;` |
|      11 |  9271 | `							pR++;` |
|       4 |  9272 | `						}` |
|      13 |  9273 | `						pMeth = 0;` |
|      13 |  9274 | `						if( hasQual ){` |
|       3 |  9275 | `							pSrcTrait = 0;` |
|       5 |  9276 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9277 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9278 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9279 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9280 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9281 | `									break;` |
|       - |  9282 | `								}` |
|       2 |  9283 | `							}` |
|       3 |  9284 | `							if( pSrcTrait ){` |
|       3 |  9285 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9286 | `							}` |
|       2 |  9287 | `						}else{` |
|      10 |  9288 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9289 | `						}` |
|      13 |  9290 | `						if( pMeth ){` |
|      13 |  9291 | `							if( sAlias.nByte > 0 ){` |
|       - |  9292 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9293 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9294 | `								 */` |
|       - |  9295 | `								ph7_class_method *pAlias;` |
|       - |  9296 | `								char *zAliasDup;` |
|      11 |  9297 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9298 | `								if( pAlias ){` |
|      11 |  9299 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9300 | `									if( iNewVis >= 0 ){` |
|       5 |  9301 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9302 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9303 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9304 | `									}` |
|      11 |  9305 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9306 | `									if( zAliasDup ){` |
|      11 |  9307 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9308 | `									}` |
|       7 |  9309 | `								}` |
|       7 |  9310 | `							}else if( iNewVis >= 0 ){` |
|       - |  9311 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9312 | `								ph7_class_method *pCopy;` |
|       3 |  9313 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9314 | `								if( pCopy ){` |
|       3 |  9315 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9316 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9317 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9318 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9319 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9320 | `									/* Replace the method in the class hash */` |
|       3 |  9321 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9322 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9323 | `								}` |
|       1 |  9324 | `							}` |
|       5 |  9325 | `						}` |
|       5 |  9326 | `						SXUNUSED(hasQual);` |
|       5 |  9327 | `					}` |
|      21 |  9328 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9329 | `				}` |
|       - |  9330 | `			}` |
|      53 |  9331 | `			SySetRelease(&pUse->aTraits);` |
|      29 |  9332 | `		}` |
|       - |  9333 | `	}` |
|       - |  9334 | `	/* Install the class */` |
|   95979 |  9335 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   95979 |  9336 | `	if( rc == SXRET_OK ){` |
|       - |  9337 | `		ph7_class **apInterface;` |
|       - |  9338 | `		sxu32 n;` |
|   95979 |  9339 | `		if( pBase ){` |
|       - |  9340 | `			/* Inherit from base class and mark as a subclass */` |
|   73921 |  9341 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   36958 |  9342 | `		}` |
|   95979 |  9343 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  106673 |  9344 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9345 | `			/* Implements one or more interface */` |
|   10699 |  9346 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10699 |  9347 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9348 | `				break;` |
|       - |  9349 | `			}` |
|    5352 |  9350 | `		}` |
|       - |  9351 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9352 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  143961 |  9353 | `		if( rc == SXRET_OK` |
|   95974 |  9354 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   95979 |  9355 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   84375 |  9356 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9357 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   84375 |  9358 | `			if( pStringable ){` |
|   84375 |  9359 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   84375 |  9360 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9361 | `				sxu32 i;` |
|   84375 |  9362 | `				int bAlready = 0;` |
|   91403 |  9363 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7035 |  9364 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9365 | `						bAlready = 1;` |
|       3 |  9366 | `						break;` |
|       - |  9367 | `					}` |
|    3519 |  9368 | `				}` |
|   84375 |  9369 | `				if( !bAlready ){` |
|   84373 |  9370 | `					PH7_ClassImplement(pClass,pStringable);` |
|   42184 |  9371 | `				}` |
|   42185 |  9372 | `			}` |
|   42185 |  9373 | `		}` |
|       - |  9374 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   95979 |  9375 | `		if( rc == SXRET_OK ){` |
|   95979 |  9376 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   95979 |  9377 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9378 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9379 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9380 | `				return SXERR_ABORT;` |
|       - |  9381 | `			}` |
|   47987 |  9382 | `		}` |
|       - |  9383 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   95979 |  9384 | `		if( rc == SXRET_OK ){` |
|   95979 |  9385 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   95979 |  9386 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9387 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9388 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9389 | `				return SXERR_ABORT;` |
|       - |  9390 | `			}` |
|   47987 |  9391 | `		}` |
|   47987 |  9392 | `	}` |
|   95979 |  9393 | `	SySetRelease(&aUseEntries);` |
|   95979 |  9394 | `	SySetRelease(&aInterfaces);` |
|   95979 |  9395 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9396 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9397 | `		return SXERR_ABORT;` |
|       - |  9398 | `	}` |
|   47987 |  9399 | `done:` |
|       - |  9400 | `	/* Point beyond the class body */` |
|   96007 |  9401 | `	pGen->pIn = &pEnd[1];` |
|   96007 |  9402 | `	pGen->pEnd = pTmp;` |
|   96007 |  9403 | `	return PH7_OK;` |
|   48007 |  9404 |  |
|       - |  9405 | `/* Compile a named class declaration (the common case). */` |
|   95978 |  9406 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9407 |  |
|   95983 |  9408 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9409 |  |
|       - |  9410 | `/*` |
|       - |  9411 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9412 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9413 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9414 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9415 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9416 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9417 | ` */` |
|      26 |  9418 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  9419 |  |
|       - |  9420 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9421 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9422 | `	SyString sName;` |
|       - |  9423 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9424 | `	ph7_value *pObj;` |
|      29 |  9425 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9426 | `	sxu32 nIdx,nLen;` |
|       - |  9427 | `	sxi32 nArg,rc;` |
|      13 |  9428 | `	SXUNUSED(iCompileFlag);` |
|       - |  9429 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      29 |  9430 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      29 |  9431 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9432 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9433 | `	}` |
|      29 |  9434 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9435 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9436 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9437 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      29 |  9438 | `	pArgStart = pArgEnd = 0;` |
|      29 |  9439 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      29 |  9440 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9441 | `		return rc;` |
|       - |  9442 | `	}` |
|       - |  9443 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9444 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      29 |  9445 | `	nArg = 0;` |
|      29 |  9446 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9447 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9448 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9449 | `		SyToken *pArgNext;` |
|       7 |  9450 | `		pGen->pIn = pArgStart;` |
|       7 |  9451 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9452 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9453 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9454 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9455 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9456 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9457 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9458 | `					return SXERR_ABORT;` |
|       - |  9459 | `				}` |
|       7 |  9460 | `				nArg++;` |
|       3 |  9461 | `			}` |
|       7 |  9462 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9463 | `		}` |
|       7 |  9464 | `		pGen->pIn = pSavedIn;` |
|       7 |  9465 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9466 | `	}` |
|       - |  9467 | `	/* Load the synthesized class name */` |
|      29 |  9468 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  9469 | `	if( pObj == 0 ){` |
|     ! 0 |  9470 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9471 | `		return SXERR_ABORT;` |
|       - |  9472 | `	}` |
|      29 |  9473 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      29 |  9474 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9475 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      29 |  9476 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      29 |  9477 | `	return SXRET_OK;` |
|      16 |  9478 |  |
|       - |  9479 | `/*` |
|       - |  9480 | ` * Compile a user-defined abstract class.` |
|       - |  9481 | ` *  According to the PHP language reference manual` |
|       - |  9482 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9483 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9484 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9485 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9486 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9487 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9488 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9489 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9490 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9491 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9492 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9493 | ` *   could differ.` |
|       - |  9494 | ` */` |
|       - |  9495 | `/*` |
|       - |  9496 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9497 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9498 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9499 | ` */` |
|  945854 |  9500 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9501 |  |
|  945859 |  9502 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  626993 |  9503 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  626993 |  9504 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  626975 |  9505 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  313462 |  9506 | `	}` |
|  945795 |  9507 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  945735 |  9508 | `	return FALSE;` |
|  472932 |  9509 |  |
|       - |  9510 | `/*` |
|       - |  9511 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9512 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9513 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9514 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9515 | ` */` |
|  945730 |  9516 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9517 |  |
|  945735 |  9518 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  945735 |  9519 | `	sxi32 iFlags = 0,iFlag;` |
|  945859 |  9520 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     129 |  9521 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9522 | `			pDup = pIn;` |
|       2 |  9523 | `		}` |
|     129 |  9524 | `		iFlags \|= iFlag;` |
|     129 |  9525 | `		pIn++;` |
|       5 |  9526 | `	}` |
|  945735 |  9527 | `	*ppIn = pIn;` |
|  945735 |  9528 | `	if( ppDup ){ *ppDup = pDup; }` |
|  945735 |  9529 | `	return iFlags;` |
|       5 |  9530 |  |
|       - |  9531 | `/*` |
|       - |  9532 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9533 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9534 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9535 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9536 | `` * `readonly`) to their existing handlers.`` |
|       - |  9537 | ` */` |
|  945678 |  9538 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9539 |  |
|  945683 |  9540 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  472898 |  9541 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  945706 |  9542 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9543 |  |
|       - |  9544 | `/*` |
|       - |  9545 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9546 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9547 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9548 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9549 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9550 | ` */` |
|      52 |  9551 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9552 |  |
|       - |  9553 | `	SyToken *pDup;` |
|      57 |  9554 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9555 | `	sxi32 rc;` |
|      57 |  9556 | `	if( pDup ){` |
|       4 |  9557 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9558 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9559 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9560 | `			return SXERR_ABORT;` |
|       - |  9561 | `		}` |
|       1 |  9562 | `	}` |
|      78 |  9563 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      31 |  9564 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9566 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9567 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9568 | `			return SXERR_ABORT;` |
|       - |  9569 | `		}` |
|       1 |  9570 | `	}` |
|      57 |  9571 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      31 |  9572 |  |
|       - |  9573 | `/*` |
|       - |  9574 | ` * Compile a user-defined trait.` |
|       - |  9575 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9576 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9577 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9578 | ` */` |
|      60 |  9579 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9580 |  |
|      65 |  9581 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9582 | `	ph7_class *pClass;` |
|       - |  9583 | `	SyToken *pEnd,*pTmp;` |
|       - |  9584 | `	sxi32 iProtection;` |
|       - |  9585 | `	sxi32 iAttrflags;` |
|       - |  9586 | `	SyString *pName;` |
|       - |  9587 | `	sxi32 nKwrd;` |
|       - |  9588 | `	sxi32 rc;` |
|       - |  9589 | `	/* Jump the 'trait' keyword */` |
|      65 |  9590 | `	pGen->pIn++;` |
|      65 |  9591 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9592 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9593 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9594 | `			return SXERR_ABORT;` |
|       - |  9595 | `		}` |
|     ! 0 |  9596 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9597 | `			pGen->pIn++;` |
|     ! 0 |  9598 | `		}` |
|     ! 0 |  9599 | `		return SXRET_OK;` |
|       - |  9600 | `	}` |
|       - |  9601 | `	/* Extract trait name */` |
|      65 |  9602 | `	pName = &pGen->pIn->sData;` |
|      65 |  9603 | `	pGen->pIn++;` |
|       - |  9604 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9605 | `		SyBlob sFQN;` |
|       - |  9606 | `		SyString sFQNStr;` |
|      65 |  9607 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      65 |  9608 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      65 |  9609 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      65 |  9610 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      65 |  9611 | `		SyBlobRelease(&sFQN);` |
|       - |  9612 | `	}` |
|      65 |  9613 | `	if( pClass == 0 ){` |
|     ! 0 |  9614 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9615 | `		return SXERR_ABORT;` |
|       - |  9616 | `	}` |
|       - |  9617 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      65 |  9618 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9619 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9620 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9621 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9622 | `			return SXERR_ABORT;` |
|       - |  9623 | `		}` |
|     ! 0 |  9624 | `		return SXRET_OK;` |
|       - |  9625 | `	}` |
|      65 |  9626 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      65 |  9627 | `	pEnd = 0;` |
|      65 |  9628 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      65 |  9629 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9630 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9631 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9632 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9633 | `			return SXERR_ABORT;` |
|       - |  9634 | `		}` |
|     ! 0 |  9635 | `		return SXRET_OK;` |
|       - |  9636 | `	}` |
|       - |  9637 | `	/* Swap token stream */` |
|      65 |  9638 | `	pTmp = pGen->pEnd;` |
|      65 |  9639 | `	pGen->pEnd = pEnd;` |
|       - |  9640 | `	/* Mark as trait */` |
|      65 |  9641 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9642 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      60 |  9643 | `	for(;;){` |
|     169 |  9644 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9645 | `			pGen->pIn++;` |
|       4 |  9646 | `		}` |
|     145 |  9647 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      65 |  9648 | `			break;` |
|       - |  9649 | `		}` |
|      85 |  9650 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9651 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9652 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9653 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9654 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9655 | `				return SXERR_ABORT;` |
|       - |  9656 | `			}` |
|     ! 0 |  9657 | `			goto done;` |
|       - |  9658 | `		}` |
|      85 |  9659 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      85 |  9660 | `		iAttrflags = 0;` |
|      85 |  9661 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      85 |  9662 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  9663 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9664 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9665 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9666 | `				for(;;){` |
|       - |  9667 | `					ph7_class *pUsedTrait;` |
|       - |  9668 | `					SyString *pUsedName;` |
|       5 |  9669 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9670 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9671 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9672 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9673 | `							return SXERR_ABORT;` |
|       - |  9674 | `						}` |
|     ! 0 |  9675 | `						break;` |
|       - |  9676 | `					}` |
|       5 |  9677 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9678 | `					{` |
|       - |  9679 | `						SyBlob sResolved;` |
|       5 |  9680 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9681 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9682 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9683 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9684 | `						SyBlobRelease(&sResolved);` |
|       - |  9685 | `					}` |
|       5 |  9686 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9687 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9688 | `					}` |
|       5 |  9689 | `					if( pUsedTrait == 0 ){` |
|       4 |  9690 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9691 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9692 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9693 | `							return SXERR_ABORT;` |
|       - |  9694 | `						}` |
|       2 |  9695 | `					}else{` |
|       3 |  9696 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9697 | `					}` |
|       5 |  9698 | `					pGen->pIn++;` |
|       5 |  9699 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9700 | `						break;` |
|       - |  9701 | `					}` |
|     ! 0 |  9702 | `					pGen->pIn++;` |
|     ! 0 |  9703 | `				}` |
|       5 |  9704 | `				continue;` |
|       - |  9705 | `			}` |
|      81 |  9706 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9707 | `				iProtection = nKwrd;` |
|      73 |  9708 | `				pGen->pIn++;` |
|      68 |  9709 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9710 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9711 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9712 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9713 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9714 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9715 | `						return SXERR_ABORT;` |
|       - |  9716 | `					}` |
|     ! 0 |  9717 | `					goto done;` |
|       - |  9718 | `				}` |
|      73 |  9719 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9720 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9721 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9722 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9723 | `							return SXERR_ABORT;` |
|       - |  9724 | `						}` |
|     ! 0 |  9725 | `						goto done;` |
|       - |  9726 | `					}` |
|      12 |  9727 | `					continue;` |
|       - |  9728 | `				}` |
|      63 |  9729 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9730 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9731 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9732 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9733 | `							return SXERR_ABORT;` |
|       - |  9734 | `						}` |
|     ! 0 |  9735 | `						goto done;` |
|       - |  9736 | `					}` |
|       5 |  9737 | `					continue;` |
|       - |  9738 | `				}` |
|      58 |  9739 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9740 | `			}` |
|      66 |  9741 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9742 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9743 | `					"Traits cannot have constants");` |
|     ! 0 |  9744 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9745 | `					return SXERR_ABORT;` |
|       - |  9746 | `				}` |
|     ! 0 |  9747 | `				goto done;` |
|     ! 0 |  9748 | `			}else{` |
|      66 |  9749 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9750 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9751 | `					pGen->pIn++;` |
|       5 |  9752 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9753 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9754 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9755 | `							iProtection = nKwrd;` |
|     ! 0 |  9756 | `							pGen->pIn++;` |
|     ! 0 |  9757 | `						}` |
|       1 |  9758 | `					}` |
|       4 |  9759 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9760 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9761 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9762 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9763 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9764 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9765 | `							return SXERR_ABORT;` |
|       - |  9766 | `						}` |
|     ! 0 |  9767 | `						goto done;` |
|       - |  9768 | `					}` |
|       5 |  9769 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9770 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9771 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9772 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9773 | `								return SXERR_ABORT;` |
|       - |  9774 | `							}` |
|     ! 0 |  9775 | `							goto done;` |
|       - |  9776 | `						}` |
|       3 |  9777 | `						continue;` |
|       - |  9778 | `					}` |
|       3 |  9779 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9780 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9781 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9782 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9783 | `								return SXERR_ABORT;` |
|       - |  9784 | `							}` |
|     ! 0 |  9785 | `							goto done;` |
|       - |  9786 | `						}` |
|     ! 0 |  9787 | `						continue;` |
|       - |  9788 | `					}` |
|       3 |  9789 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      63 |  9790 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9791 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9792 | `					pGen->pIn++;` |
|       6 |  9793 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9794 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9795 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9796 | `							iProtection = nKwrd;` |
|       6 |  9797 | `							pGen->pIn++;` |
|       2 |  9798 | `						}` |
|       2 |  9799 | `					}` |
|       6 |  9800 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9801 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9802 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9803 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9804 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9805 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9806 | `							return SXERR_ABORT;` |
|       - |  9807 | `						}` |
|     ! 0 |  9808 | `						goto done;` |
|       - |  9809 | `					}` |
|       6 |  9810 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9811 | `				}` |
|      64 |  9812 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9813 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9814 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9815 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9816 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9817 | `						return SXERR_ABORT;` |
|       - |  9818 | `					}` |
|     ! 0 |  9819 | `					goto done;` |
|       - |  9820 | `				}` |
|      64 |  9821 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9822 | `					pGen->pIn++;` |
|     ! 0 |  9823 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9824 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9825 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9826 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9827 | `							return SXERR_ABORT;` |
|       - |  9828 | `						}` |
|     ! 0 |  9829 | `						goto done;` |
|       - |  9830 | `					}` |
|     ! 0 |  9831 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9832 | `				}else{` |
|      64 |  9833 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9834 | `				}` |
|      64 |  9835 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9836 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9837 | `						return SXERR_ABORT;` |
|       - |  9838 | `					}` |
|     ! 0 |  9839 | `					goto done;` |
|       - |  9840 | `				}` |
|       - |  9841 | `			}` |
|      34 |  9842 | `		}else{` |
|     ! 0 |  9843 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9844 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9845 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9846 | `					return SXERR_ABORT;` |
|       - |  9847 | `				}` |
|     ! 0 |  9848 | `				goto done;` |
|       - |  9849 | `			}` |
|       - |  9850 | `		}` |
|       4 |  9851 | `	}` |
|       - |  9852 | `	/* Install the trait */` |
|      65 |  9853 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      65 |  9854 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9855 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9856 | `		return SXERR_ABORT;` |
|       - |  9857 | `	}` |
|      30 |  9858 | `done:` |
|       - |  9859 | `	/* Point beyond the trait body */` |
|      65 |  9860 | `	pGen->pIn = &pEnd[1];` |
|      65 |  9861 | `	pGen->pEnd = pTmp;` |
|      65 |  9862 | `	return PH7_OK;` |
|      35 |  9863 |  |
|       - |  9864 | `/*` |
|       - |  9865 | ` * Compile a user-defined class.` |
|       - |  9866 | ` *  According to the PHP language reference manual` |
|       - |  9867 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9868 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9869 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9870 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9871 | ` *   and functions (called "methods").` |
|       - |  9872 | ` */` |
|   95926 |  9873 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9874 |  |
|       - |  9875 | `	sxi32 rc;` |
|   95931 |  9876 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   95931 |  9877 | `	return rc;` |
|       5 |  9878 |  |
|       - |  9879 | `/*` |
|       - |  9880 | ` * Exception handling.` |
|       - |  9881 | ` *  According to the PHP language reference manual` |
|       - |  9882 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9883 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9884 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9885 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9886 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9887 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9888 | ` *    (or re-thrown) within a catch block.` |
|       - |  9889 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9890 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9891 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9892 | ` *    been defined with set_exception_handler().` |
|       - |  9893 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9894 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9895 | ` */` |
|       - |  9896 | `/*` |
|       - |  9897 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9898 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9899 | ` * indicates failure.` |
|       - |  9900 | ` */` |
|   10772 |  9901 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9902 |  |
|   10777 |  9903 | `	sxi32 rc = SXRET_OK;` |
|   10777 |  9904 | `	if( pRoot->pOp ){` |
|   10769 |  9905 | `		switch( pRoot->pOp->iOp ){` |
|    5382 |  9906 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9907 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9908 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9909 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9910 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9911 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   10769 |  9912 | `			break;` |
|     ! 0 |  9913 | `		default:` |
|       - |  9914 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9915 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9916 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9917 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9918 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9919 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9920 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9921 | `			}` |
|     ! 0 |  9922 | `			break;` |
|       - |  9923 | `		}` |
|    5395 |  9924 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9925 | `		/* Unexpected expression */` |
|     ! 0 |  9926 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9927 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9928 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9929 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9930 | `		}` |
|     ! 0 |  9931 | `	}` |
|   10777 |  9932 | `	return rc;` |
|       5 |  9933 |  |
|       - |  9934 | `/*` |
|       - |  9935 | ` * Compile a 'throw' statement.` |
|       - |  9936 | ` * throw: This is how you trigger an exception.` |
|       - |  9937 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9938 | ` */` |
|   10736 |  9939 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9940 |  |
|   10741 |  9941 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9942 | `	GenBlock *pBlock;` |
|       - |  9943 | `	sxu32 nIdx;` |
|       - |  9944 | `	sxi32 rc;` |
|   10741 |  9945 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9946 | `	/* Compile the expression */` |
|   10741 |  9947 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   10741 |  9948 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9949 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9950 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9951 | `			return SXERR_ABORT;` |
|       - |  9952 | `		}` |
|     ! 0 |  9953 | `		return SXRET_OK;` |
|       - |  9954 | `	}` |
|   10741 |  9955 | `	pBlock = pGen->pCurrent;` |
|       - |  9956 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   49573 |  9957 | `	while(pBlock->pParent){` |
|   49569 |  9958 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   10737 |  9959 | `			break;` |
|       - |  9960 | `		}` |
|       - |  9961 | `		/* Point to the parent block */` |
|   38837 |  9962 | `		pBlock = pBlock->pParent;` |
|       5 |  9963 | `	}` |
|       - |  9964 | `	/* Emit the throw instruction */` |
|   10741 |  9965 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9966 | `	/* Emit the jump */` |
|   10741 |  9967 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   10741 |  9968 | `	return SXRET_OK;` |
|    5373 |  9969 |  |
|       - |  9970 | `/*` |
|       - |  9971 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9972 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9973 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9974 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9975 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9976 | ` */` |
|      36 |  9977 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9978 |  |
|      38 |  9979 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9980 | `	GenBlock *pBlock;` |
|       - |  9981 | `	sxu32 nIdx;` |
|       - |  9982 | `	sxi32 rc;` |
|      18 |  9983 | `	(void)iCompileFlag;` |
|      38 |  9984 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9985 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9986 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9987 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9988 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9989 | `			return SXERR_ABORT;` |
|       - |  9990 | `		}` |
|     ! 0 |  9991 | `		return SXRET_OK;` |
|       - |  9992 | `	}` |
|      38 |  9993 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9994 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9995 | `		return SXERR_ABORT;` |
|       - |  9996 | `	}` |
|      38 |  9997 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9998 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9999 | `			"throw: Expecting an exception class instance");` |
|     ! 0 | 10000 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10001 | `			return SXERR_ABORT;` |
|       - | 10002 | `		}` |
|     ! 0 | 10003 | `		return SXRET_OK;` |
|       - | 10004 | `	}` |
|       - | 10005 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 | 10006 | `	pBlock = pGen->pCurrent;` |
|      60 | 10007 | `	while( pBlock->pParent ){` |
|      49 | 10008 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 | 10009 | `			break;` |
|       - | 10010 | `		}` |
|      23 | 10011 | `		pBlock = pBlock->pParent;` |
|       1 | 10012 | `	}` |
|      38 | 10013 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10014 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10015 | `	return SXRET_OK;` |
|      20 | 10016 |  |
|       - | 10017 | `/*` |
|       - | 10018 | ` * Compile a 'catch' block.` |
|       - | 10019 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10020 | ` * an object containing the exception information.` |
|       - | 10021 | ` */` |
|     488 | 10022 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10023 |  |
|     493 | 10024 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10025 | `	ph7_exception_block sCatch;` |
|       - | 10026 | `	SySet *pInstrContainer;` |
|       - | 10027 | `	SyString sClassName;` |
|       - | 10028 | `	GenBlock *pCatch;` |
|       - | 10029 | `	SyToken *pToken;` |
|       - | 10030 | `	SyString *pName;` |
|       - | 10031 | `	char *zDup;` |
|       - | 10032 | `	sxi32 rc;` |
|     493 | 10033 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10034 | `	/* Zero the structure */` |
|     493 | 10035 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10036 | `	/* Initialize fields */` |
|     493 | 10037 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     493 | 10038 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     493 | 10039 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10040 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10041 | `			pToken = pGen->pIn;` |
|     ! 0 | 10042 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10043 | `				pToken--;` |
|     ! 0 | 10044 | `			}` |
|     ! 0 | 10045 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10046 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10047 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10048 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10049 | `				return SXERR_ABORT;` |
|       - | 10050 | `			}` |
|     ! 0 | 10051 | `			return SXERR_INVALID;` |
|       - | 10052 | `	}` |
|       - | 10053 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     493 | 10054 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     257 | 10055 | `	for(;;){` |
|       - | 10056 | `		SyBlob sResolved;` |
|     519 | 10057 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     519 | 10058 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10059 | `			SyBlobRelease(&sResolved);` |
|       6 | 10060 | `			pToken = pGen->pIn;` |
|       6 | 10061 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10062 | `				pToken--;` |
|     ! 0 | 10063 | `			}` |
|       8 | 10064 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10065 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10066 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10067 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10068 | `				return SXERR_ABORT;` |
|       - | 10069 | `			}` |
|       6 | 10070 | `			return SXERR_INVALID;` |
|       - | 10071 | `		}` |
|       - | 10072 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10073 | `		 * transient SyBlob allocation. */` |
|     770 | 10074 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     510 | 10075 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     515 | 10076 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     515 | 10077 | `		SyBlobRelease(&sResolved);` |
|     515 | 10078 | `		if( zDup == 0 ){` |
|     ! 0 | 10079 | `			goto Mem;` |
|       - | 10080 | `		}` |
|     515 | 10081 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     515 | 10082 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10083 | `			goto Mem;` |
|       - | 10084 | `		}` |
|       - | 10085 | `		/* Check for '\|' (multi-catch separator) */` |
|     523 | 10086 | `		if( pGen->pIn < pGen->pEnd &&` |
|     510 | 10087 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      31 | 10088 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 | 10089 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 | 10090 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 | 10091 | `			continue;` |
|       - | 10092 | `		}` |
|     489 | 10093 | `		break;` |
|     ! 0 | 10094 | `	}` |
|     726 | 10095 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     489 | 10096 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10097 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10098 | `			pToken = pGen->pIn;` |
|     ! 0 | 10099 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10100 | `				pToken--;` |
|     ! 0 | 10101 | `			}` |
|     ! 0 | 10102 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10103 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10104 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10105 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10106 | `				return SXERR_ABORT;` |
|       - | 10107 | `			}` |
|     ! 0 | 10108 | `			return SXERR_INVALID;` |
|       - | 10109 | `	}` |
|     489 | 10110 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10111 | `	/* Duplicate instance name */` |
|     489 | 10112 | `	pName = &pGen->pIn->sData;` |
|     489 | 10113 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     489 | 10114 | `	if( zDup == 0 ){` |
|     ! 0 | 10115 | `		goto Mem;` |
|       - | 10116 | `	}` |
|     489 | 10117 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     489 | 10118 | `	pGen->pIn++;` |
|     489 | 10119 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10120 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10121 | `		pToken = pGen->pIn;` |
|     ! 0 | 10122 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10123 | `			pToken--;` |
|     ! 0 | 10124 | `		}` |
|     ! 0 | 10125 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10126 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10127 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10128 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10129 | `			return SXERR_ABORT;` |
|       - | 10130 | `		}` |
|     ! 0 | 10131 | `		return SXERR_INVALID;` |
|       - | 10132 | `	}` |
|       - | 10133 | `	/* Compile the block */` |
|     489 | 10134 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10135 | `	/* Create the catch block */` |
|     489 | 10136 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     489 | 10137 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10138 | `		return SXERR_ABORT;` |
|       - | 10139 | `	}` |
|       - | 10140 | `	/* Swap bytecode container */` |
|     489 | 10141 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     489 | 10142 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10143 | `	/* Compile the block */` |
|     489 | 10144 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10145 | `	/* Fix forward jumps now the destination is resolved  */` |
|     489 | 10146 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10147 | `	/* Emit the DONE instruction */` |
|     489 | 10148 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10149 | `	/* Leave the block */` |
|     489 | 10150 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10151 | `	/* Restore the default container */` |
|     489 | 10152 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10153 | `	/* Install the catch block */` |
|     489 | 10154 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     489 | 10155 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10156 | `		goto Mem;` |
|       - | 10157 | `	}` |
|     489 | 10158 | `	return SXRET_OK;` |
|     ! 0 | 10159 | `Mem:` |
|     ! 0 | 10160 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10161 | `	return SXERR_ABORT;` |
|     249 | 10162 |  |
|       - | 10163 | `/*` |
|       - | 10164 | ` * Compile a 'try' block.` |
|       - | 10165 | ` * A function using an exception should be in a "try" block.` |
|       - | 10166 | ` * If the exception does not trigger, the code will continue` |
|       - | 10167 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10168 | ` * is "thrown".` |
|       - | 10169 | ` */` |
|     506 | 10170 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10171 |  |
|       - | 10172 | `	ph7_exception *pException;` |
|     511 | 10173 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10174 | `	GenBlock *pTry;` |
|       - | 10175 | `	sxu32 nJmpIdx;` |
|       - | 10176 | `	sxi32 rc;` |
|       - | 10177 | `	/* Create the exception container */` |
|     511 | 10178 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     511 | 10179 | `	if( pException == 0 ){` |
|     ! 0 | 10180 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10181 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10182 | `		return SXERR_ABORT;` |
|       - | 10183 | `	}` |
|       - | 10184 | `	/* Zero the structure */` |
|     511 | 10185 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10186 | `	/* Initialize fields */` |
|     511 | 10187 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     511 | 10188 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     511 | 10189 | `	pException->iHasFinally = 0;` |
|     511 | 10190 | `	pException->iFinallyDone = 0;` |
|     511 | 10191 | `	pException->pVm = pGen->pVm;` |
|       - | 10192 | `	/* Create the try block */` |
|     511 | 10193 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     511 | 10194 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10195 | `		return SXERR_ABORT;` |
|       - | 10196 | `	}` |
|       - | 10197 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     511 | 10198 | `	pTry->pUserData = pException;` |
|       - | 10199 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     511 | 10200 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10201 | `	/* Fix the jump later when the destination is resolved */` |
|     511 | 10202 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     511 | 10203 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10204 | `	/* Compile the block */` |
|     511 | 10205 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     511 | 10206 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10207 | `		return SXERR_ABORT;` |
|       - | 10208 | `	}` |
|       - | 10209 | `	/* Fix forward jumps now the destination is resolved */` |
|     511 | 10210 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10211 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     511 | 10212 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10213 | `	/* Leave the block */` |
|     511 | 10214 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10215 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     511 | 10216 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     504 | 10217 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10218 | `		/* Compile one or more catch blocks */` |
|     484 | 10219 | `		for(;;){` |
|     968 | 10220 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     766 | 10221 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     245 | 10222 | `					break;` |
|       - | 10223 | `			}` |
|     493 | 10224 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     493 | 10225 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10226 | `				return SXERR_ABORT;` |
|       - | 10227 | `			}` |
|       5 | 10228 | `		}` |
|     240 | 10229 | `	}` |
|       - | 10230 | `	/* Compile optional finally block */` |
|     511 | 10231 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     254 | 10232 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10233 | `		SySet *pInstrContainer;` |
|       - | 10234 | `		GenBlock *pFinBlock;` |
|      63 | 10235 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10236 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      63 | 10237 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      63 | 10238 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10239 | `			return SXERR_ABORT;` |
|       - | 10240 | `		}` |
|       - | 10241 | `		/* Swap bytecode container */` |
|      63 | 10242 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      63 | 10243 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10244 | `		/* Compile the finally body */` |
|      63 | 10245 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      63 | 10246 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10247 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10248 | `			return SXERR_ABORT;` |
|       - | 10249 | `		}` |
|       - | 10250 | `		/* Fix forward jumps now the destination is resolved */` |
|      63 | 10251 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10252 | `		/* Emit DONE to terminate the finally block */` |
|      63 | 10253 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10254 | `		/* Leave the block */` |
|      63 | 10255 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10256 | `		/* Restore the default container */` |
|      63 | 10257 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      63 | 10258 | `		pException->iHasFinally = 1;` |
|      29 | 10259 | `	}` |
|       - | 10260 | `	/* Must have at least one catch or finally */` |
|     511 | 10261 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 10262 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10263 | `			"Cannot use try without catch or finally");` |
|       8 | 10264 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10265 | `			return SXERR_ABORT;` |
|       - | 10266 | `		}` |
|       3 | 10267 | `	}` |
|     511 | 10268 | `	return SXRET_OK;` |
|     258 | 10269 |  |
|       - | 10270 | `/*` |
|       - | 10271 | ` * Compile a switch block.` |
|       - | 10272 | ` *  (See block-comment below for more information)` |
|       - | 10273 | ` */` |
|     112 | 10274 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10275 |  |
|     117 | 10276 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10277 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10278 | `		/* Unexpected token */` |
|     ! 0 | 10279 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10280 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10281 | `			return SXERR_ABORT;` |
|       - | 10282 | `		}` |
|     ! 0 | 10283 | `		pGen->pIn++;` |
|     ! 0 | 10284 | `	}` |
|     117 | 10285 | `	pGen->pIn++;` |
|       - | 10286 | `	/* First instruction to execute in this block. */` |
|     117 | 10287 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10288 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10289 | `	 * or the '}' token */` |
|     206 | 10290 | `	for(;;){` |
|     417 | 10291 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10292 | `			/* No more input to process */` |
|     ! 0 | 10293 | `			break;` |
|       - | 10294 | `		}` |
|     417 | 10295 | `		rc = SXRET_OK;` |
|     417 | 10296 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10297 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10298 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10299 | `					/* Unexpected token */` |
|     ! 0 | 10300 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10301 | `						&pGen->pIn->sData);` |
|     ! 0 | 10302 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10303 | `						return SXERR_ABORT;` |
|       - | 10304 | `					}` |
|       - | 10305 | `					/* FALL THROUGH */` |
|     ! 0 | 10306 | `				}` |
|      31 | 10307 | `				rc = SXERR_EOF;` |
|      31 | 10308 | `				break;` |
|       - | 10309 | `			}` |
|      32 | 10310 | `		}else{` |
|       - | 10311 | `			sxi32 nKwrd;` |
|       - | 10312 | `			/* Extract the keyword */` |
|     337 | 10313 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10314 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10315 | `				break;` |
|       - | 10316 | `			}` |
|     253 | 10317 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10318 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10319 | `					/* Unexpected token */` |
|     ! 0 | 10320 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10321 | `						&pGen->pIn->sData);` |
|     ! 0 | 10322 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10323 | `						return SXERR_ABORT;` |
|       - | 10324 | `					}` |
|       - | 10325 | `					/* FALL THROUGH */` |
|     ! 0 | 10326 | `				}` |
|       - | 10327 | `				/* Block compiled */` |
|       3 | 10328 | `				break;` |
|       - | 10329 | `			}` |
|       - | 10330 | `		}` |
|       - | 10331 | `		/* Compile block */` |
|     305 | 10332 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10333 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10334 | `			return SXERR_ABORT;` |
|       - | 10335 | `		}` |
|       5 | 10336 | `	}` |
|     117 | 10337 | `	return rc;` |
|      61 | 10338 |  |
|       - | 10339 | `/*` |
|       - | 10340 | ` * Compile a case eXpression.` |
|       - | 10341 | ` *  (See block-comment below for more information)` |
|       - | 10342 | ` */` |
|      92 | 10343 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10344 |  |
|       - | 10345 | `	SySet *pInstrContainer;` |
|       - | 10346 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10347 | `	sxi32 iNest = 0;` |
|       - | 10348 | `	sxi32 rc;` |
|       - | 10349 | `	/* Delimit the expression */` |
|      97 | 10350 | `	pEnd = pGen->pIn;` |
|     197 | 10351 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10352 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10353 | `			/* Increment nesting level */` |
|       3 | 10354 | `			iNest++;` |
|     196 | 10355 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10356 | `			/* Decrement nesting level */` |
|       3 | 10357 | `			iNest--;` |
|     194 | 10358 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10359 | `			break;` |
|       - | 10360 | `		}` |
|     105 | 10361 | `		pEnd++;` |
|       5 | 10362 | `	}` |
|      97 | 10363 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10364 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10365 | `		if( rc == SXERR_ABORT ){` |
|       - | 10366 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10367 | `			return SXERR_ABORT;` |
|       - | 10368 | `		}` |
|     ! 0 | 10369 | `	}` |
|       - | 10370 | `	/* Swap token stream */` |
|      97 | 10371 | `	pTmp = pGen->pEnd;` |
|      97 | 10372 | `	pGen->pEnd = pEnd;` |
|      97 | 10373 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10374 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10375 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10376 | `	/* Emit the done instruction */` |
|      97 | 10377 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10378 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10379 | `	/* Update token stream */` |
|      97 | 10380 | `	pGen->pIn  = pEnd;` |
|      97 | 10381 | `	pGen->pEnd = pTmp;` |
|      97 | 10382 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10383 | `		return SXERR_ABORT;` |
|       - | 10384 | `	}` |
|      97 | 10385 | `	return SXRET_OK;` |
|      51 | 10386 |  |
|       - | 10387 | `/*` |
|       - | 10388 | ` * Compile the smart switch statement.` |
|       - | 10389 | ` * According to the PHP language reference manual` |
|       - | 10390 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10391 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10392 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10393 | ` *  This is exactly what the switch statement is for.` |
|       - | 10394 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10395 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10396 | ` *  of the outer loop, use continue 2.` |
|       - | 10397 | ` *  Note that switch/case does loose comparision.` |
|       - | 10398 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10399 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10400 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10401 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10402 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10403 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10404 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10405 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10406 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10407 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10408 | ` *  list for the next case.` |
|       - | 10409 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10410 | ` *  or floating-point numbers and strings.` |
|       - | 10411 | ` */` |
|      28 | 10412 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10413 |  |
|       - | 10414 | `	GenBlock *pSwitchBlock;` |
|       - | 10415 | `	SyToken *pTmp,*pEnd;` |
|       - | 10416 | `	ph7_switch *pSwitch;` |
|       - | 10417 | `	sxu32 nToken;` |
|       - | 10418 | `	sxu32 nLine;` |
|       - | 10419 | `	sxi32 rc;` |
|      33 | 10420 | `	nLine = pGen->pIn->nLine;` |
|       - | 10421 | `	/* Jump the 'switch' keyword */` |
|      33 | 10422 | `	pGen->pIn++;` |
|      33 | 10423 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10424 | `		/* Syntax error */` |
|     ! 0 | 10425 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10426 | `		if( rc == SXERR_ABORT ){` |
|       - | 10427 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10428 | `			return SXERR_ABORT;` |
|       - | 10429 | `		}` |
|     ! 0 | 10430 | `		goto Synchronize;` |
|       - | 10431 | `	}` |
|       - | 10432 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10433 | `	pGen->pIn++;` |
|      33 | 10434 | `	pEnd = 0; /* cc warning */` |
|       - | 10435 | `	/* Create the loop block */` |
|      47 | 10436 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10437 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10438 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10439 | `		return SXERR_ABORT;` |
|       - | 10440 | `	}` |
|       - | 10441 | `	/* Delimit the condition */` |
|      33 | 10442 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10443 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10444 | `		/* Empty expression */` |
|     ! 0 | 10445 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10446 | `		if( rc == SXERR_ABORT ){` |
|       - | 10447 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10448 | `			return SXERR_ABORT;` |
|       - | 10449 | `		}` |
|     ! 0 | 10450 | `	}` |
|       - | 10451 | `	/* Swap token streams */` |
|      33 | 10452 | `	pTmp = pGen->pEnd;` |
|      33 | 10453 | `	pGen->pEnd = pEnd;` |
|       - | 10454 | `	/* Compile the expression */` |
|      33 | 10455 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10456 | `	if( rc == SXERR_ABORT ){` |
|       - | 10457 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10458 | `		return SXERR_ABORT;` |
|       - | 10459 | `	}` |
|       - | 10460 | `	/* Update token stream */` |
|      33 | 10461 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10462 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10463 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10464 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10465 | `			return SXERR_ABORT;` |
|       - | 10466 | `		}` |
|     ! 0 | 10467 | `		pGen->pIn++;` |
|     ! 0 | 10468 | `	}` |
|      33 | 10469 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10470 | `	pGen->pEnd = pTmp;` |
|      33 | 10471 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10472 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10473 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10474 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10475 | `				pTmp--;` |
|     ! 0 | 10476 | `			}` |
|       - | 10477 | `			/* Unexpected token */` |
|     ! 0 | 10478 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10479 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10480 | `				return SXERR_ABORT;` |
|       - | 10481 | `			}` |
|     ! 0 | 10482 | `			goto Synchronize;` |
|       - | 10483 | `	}` |
|       - | 10484 | `	/* Set the delimiter token */` |
|      33 | 10485 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10486 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10487 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10488 | `	}else{` |
|      31 | 10489 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10490 | `	}` |
|      33 | 10491 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10492 | `	/* Create the switch blocks container */` |
|      33 | 10493 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10494 | `	if( pSwitch == 0 ){` |
|       - | 10495 | `		/* Abort compilation */` |
|     ! 0 | 10496 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10497 | `		return SXERR_ABORT;` |
|       - | 10498 | `	}` |
|       - | 10499 | `	/* Zero the structure */` |
|      33 | 10500 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10501 | `	/* Initialize fields */` |
|      33 | 10502 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10503 | `	/* Emit the switch instruction */` |
|      33 | 10504 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10505 | `	/* Compile case blocks */` |
|     100 | 10506 | `	for(;;){` |
|       - | 10507 | `		sxu32 nKwrd;` |
|     119 | 10508 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10509 | `			/* No more input to process */` |
|     ! 0 | 10510 | `			break;` |
|       - | 10511 | `		}` |
|     119 | 10512 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10513 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10514 | `				/* Unexpected token */` |
|     ! 0 | 10515 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10516 | `					&pGen->pIn->sData);` |
|     ! 0 | 10517 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10518 | `					return SXERR_ABORT;` |
|       - | 10519 | `				}` |
|       - | 10520 | `				/* FALL THROUGH */` |
|     ! 0 | 10521 | `			}` |
|       - | 10522 | `			/* Block compiled */` |
|     ! 0 | 10523 | `			break;` |
|       - | 10524 | `		}` |
|       - | 10525 | `		/* Extract the keyword */` |
|     119 | 10526 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10527 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10528 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10529 | `				/* Unexpected token */` |
|     ! 0 | 10530 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10531 | `					&pGen->pIn->sData);` |
|     ! 0 | 10532 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10533 | `					return SXERR_ABORT;` |
|       - | 10534 | `				}` |
|       - | 10535 | `				/* FALL THROUGH */` |
|     ! 0 | 10536 | `			}` |
|       - | 10537 | `			/* Block compiled */` |
|       3 | 10538 | `			break;` |
|       - | 10539 | `		}` |
|     117 | 10540 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10541 | `			/*` |
|       - | 10542 | `			 * Accroding to the PHP language reference manual` |
|       - | 10543 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10544 | `			 *  that wasn't matched by the other cases.` |
|       - | 10545 | `			 */` |
|      25 | 10546 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10547 | `				/* Default case already compiled */` |
|     ! 0 | 10548 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10549 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10550 | `					return SXERR_ABORT;` |
|       - | 10551 | `				}` |
|     ! 0 | 10552 | `			}` |
|      25 | 10553 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10554 | `			/* Compile the default block */` |
|      25 | 10555 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10556 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10557 | `				return SXERR_ABORT;` |
|      25 | 10558 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10559 | `				break;` |
|       1 | 10560 | `			}` |
|      98 | 10561 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10562 | `			ph7_case_expr sCase;` |
|       - | 10563 | `			/* Standard case block */` |
|      97 | 10564 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10565 | `			/* initialize the structure */` |
|      97 | 10566 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10567 | `			/* Compile the case expression */` |
|      97 | 10568 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10569 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10570 | `				return SXERR_ABORT;` |
|       - | 10571 | `			}` |
|       - | 10572 | `			/* Compile the case block */` |
|      97 | 10573 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10574 | `			/* Insert in the switch container */` |
|      97 | 10575 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10576 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10577 | `				return SXERR_ABORT;` |
|      97 | 10578 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10579 | `				break;` |
|       - | 10580 | `			}` |
|      47 | 10581 | `		}else{` |
|       - | 10582 | `			/* Unexpected token */` |
|     ! 0 | 10583 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10584 | `				&pGen->pIn->sData);` |
|     ! 0 | 10585 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10586 | `				return SXERR_ABORT;` |
|       - | 10587 | `			}` |
|     ! 0 | 10588 | `			break;` |
|       - | 10589 | `		}` |
|       5 | 10590 | `	}` |
|       - | 10591 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10592 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10593 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10594 | `	/* Release the loop block */` |
|      33 | 10595 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10596 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10597 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10598 | `		pGen->pIn++;` |
|      14 | 10599 | `	}` |
|       - | 10600 | `	/* Statement successfully compiled */` |
|      33 | 10601 | `	return SXRET_OK;` |
|     ! 0 | 10602 | `Synchronize:` |
|       - | 10603 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10604 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10605 | `		pGen->pIn++;` |
|     ! 0 | 10606 | `	}` |
|     ! 0 | 10607 | `	return SXRET_OK;` |
|      19 | 10608 |  |
|       - | 10609 | `/*` |
|       - | 10610 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10611 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10612 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10613 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10614 | ` */` |
|       - | 10615 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10616 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10617 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10618 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10619 |  |
|       - | 10620 | `/*` |
|       - | 10621 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10622 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10623 | ` * patched entries from the pending set.` |
|       - | 10624 | ` */` |
| 2562810 | 10625 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10626 |  |
| 2562815 | 10627 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10628 | `	sxu32 nTarget;` |
|       - | 10629 | `	sxu32 *aIdx;` |
|       - | 10630 | `	sxu32 i;` |
| 2562815 | 10631 | `	if( nCur <= nBaseline ){` |
| 2562721 | 10632 | `		return;` |
|       - | 10633 | `	}` |
|      97 | 10634 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      97 | 10635 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     199 | 10636 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     105 | 10637 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     105 | 10638 | `		if( pInstr ){` |
|     105 | 10639 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 10640 | `		}` |
|      54 | 10641 | `	}` |
|      97 | 10642 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1281410 | 10643 |  |
|       - | 10644 |  |
|       - | 10645 | `/*` |
|       - | 10646 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10647 | ` *` |
|       - | 10648 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10649 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10650 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10651 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10652 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10653 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10654 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10655 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10656 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10657 | ` * creates it" behaviour).` |
|       - | 10658 | ` *` |
|       - | 10659 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10660 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10661 | ` */` |
|  416764 | 10662 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10663 |  |
|       - | 10664 | `	static const struct {` |
|       - | 10665 | `		const char *zName;` |
|       - | 10666 | `		sxu32 nByte;` |
|       - | 10667 | `		sxu32 mask;` |
|       - | 10668 | `	} aByRef[] = {` |
|       - | 10669 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10670 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10671 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10672 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10673 | `	};` |
|       - | 10674 | `	sxu32 i;` |
|  416769 | 10675 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1317 | 10676 | `		return 0;` |
|       - | 10677 | `	}` |
| 2077045 | 10678 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1661656 | 10679 | `		if( pName->nByte == aByRef[i].nByte` |
|  852793 | 10680 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      73 | 10681 | `			return aByRef[i].mask;` |
|       - | 10682 | `		}` |
|  830799 | 10683 | `	}` |
|  415389 | 10684 | `	return 0;` |
|  208387 | 10685 |  |
|       - | 10686 | `/*` |
|       - | 10687 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10688 | ` *` |
|       - | 10689 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10690 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10691 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10692 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10693 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10694 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10695 | ` */` |
|  416764 | 10696 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10697 |  |
|       - | 10698 | `	SyToken *p, *pEnd;` |
|  416769 | 10699 | `	pOut->zString = 0;` |
|  416769 | 10700 | `	pOut->nByte = 0;` |
|  416769 | 10701 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10702 | `		return;` |
|       - | 10703 | `	}` |
|  416769 | 10704 | `	p = pLeft->pStart;` |
|  416769 | 10705 | `	pEnd = pLeft->pEnd;` |
|       - | 10706 | `	/* Optional single leading namespace separator (absolute path). */` |
|  416769 | 10707 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      26 | 10708 | `		p++;` |
|      11 | 10709 | `	}` |
|  416769 | 10710 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1289 | 10711 | `		return;` |
|       - | 10712 | `	}` |
|       - | 10713 | `	/* Must be a single component: nothing follows the name token. */` |
|  415485 | 10714 | `	if( p + 1 != pEnd ){` |
|      33 | 10715 | `		return;` |
|       - | 10716 | `	}` |
|  415457 | 10717 | `	*pOut = p->sData;` |
|  208387 | 10718 |  |
|       - | 10719 | `/*` |
|       - | 10720 | ` * Generate bytecode for a given expression tree.` |
|       - | 10721 | ` * If something goes wrong while generating bytecode` |
|       - | 10722 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10723 | ` * this function takes care of generating the appropriate` |
|       - | 10724 | ` * error message.` |
|       - | 10725 | ` */` |
| 3454692 | 10726 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10727 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10728 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10729 | `	sxi32 iFlags /* Control flags */` |
|       - | 10730 | `	)` |
|       5 | 10731 |  |
|       - | 10732 | `	VmInstr *pInstr;` |
|       - | 10733 | `	sxu32 nJmpIdx;` |
| 3454697 | 10734 | `	sxi32 iP1 = 0;` |
| 3454697 | 10735 | `	sxu32 iP2 = 0;` |
| 3454697 | 10736 | `	void *p3  = 0;` |
|       - | 10737 | `	sxi32 iVmOp;` |
|       - | 10738 | `	sxi32 rc;` |
| 3454697 | 10739 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3454697 | 10740 | `	sxu32 nRhsNsBase = 0;` |
| 3454697 | 10741 | `	if( pNode->xCode ){` |
|       - | 10742 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10743 | `		/* Compile node */` |
| 2139829 | 10744 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2139829 | 10745 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2139829 | 10746 | `		RE_SWAP_DELIMITER(pGen);` |
| 2139829 | 10747 | `		return rc;` |
|       - | 10748 | `	}` |
| 1314873 | 10749 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10750 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10751 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10752 | `		return SXERR_ABORT;` |
|       - | 10753 | `	}` |
| 1314873 | 10754 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1314873 | 10755 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10756 | `		sxu32 nJmp = 0;` |
|       - | 10757 | `		sxu32 nNcNsBase;` |
|       - | 10758 | `		VmInstr *pInstrFix;` |
|       - | 10759 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10760 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10761 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10762 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10763 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10764 | `		if( pNode->pRight ){` |
|      59 | 10765 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10766 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10767 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10768 | `				return rc;` |
|       - | 10769 | `			}` |
|      59 | 10770 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10771 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10772 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10773 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10774 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10775 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10776 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10777 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10778 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10779 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10780 | `				pInstrFix->iP2 = 3;` |
|      13 | 10781 | `			}` |
|      28 | 10782 | `		}` |
|       - | 10783 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10784 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10785 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10786 | `		if( pNode->pLeft ){` |
|      59 | 10787 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10788 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10789 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10790 | `				return rc;` |
|       - | 10791 | `			}` |
|      59 | 10792 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10793 | `		}` |
|       - | 10794 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10795 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10796 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10797 | `		if( nJmp > 0 ){` |
|      59 | 10798 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10799 | `			if( pInstrFix ){` |
|      59 | 10800 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10801 | `			}` |
|      28 | 10802 | `		}` |
|      59 | 10803 | `		return SXRET_OK;` |
|       - | 10804 | `	}` |
| 1314817 | 10805 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10806 | `		sxu32 nJz,nJmp;` |
|       - | 10807 | `		sxu32 nTernaryNsBase;` |
|       - | 10808 | `		/* Ternary operator require special handling */` |
|       - | 10809 | `		/* Phase#1: Compile the condition */` |
|    2651 | 10810 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 10811 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2651 | 10812 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10813 | `			return rc;` |
|       - | 10814 | `		}` |
|       - | 10815 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10816 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10817 | `		 * condition expression, not leak past the ternary. */` |
|    2651 | 10818 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2651 | 10819 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2651 | 10820 | `		if( pNode->pLeft ){` |
|       - | 10821 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10822 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2583 | 10823 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10824 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2583 | 10825 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2583 | 10826 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2583 | 10827 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10828 | `				return rc;` |
|       - | 10829 | `			}` |
|    2583 | 10830 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1294 | 10831 | `		}else{` |
|       - | 10832 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10833 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10834 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10836 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10837 | `		}` |
|       - | 10838 | `		/* Phase#4: Emit the unconditional jump */` |
|    2651 | 10839 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10840 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2651 | 10841 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2651 | 10842 | `		if( pInstr ){` |
|    2651 | 10843 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 10844 | `		}` |
|    2651 | 10845 | `		if( !pNode->pLeft ){` |
|       - | 10846 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10847 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10848 | `		}` |
|       - | 10849 | `		/* Phase#6: Compile the 'else' expression */` |
|    2651 | 10850 | `		if( pNode->pRight ){` |
|    2651 | 10851 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 10852 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2651 | 10853 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10854 | `				return rc;` |
|       - | 10855 | `			}` |
|    2651 | 10856 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1323 | 10857 | `		}` |
|    2651 | 10858 | `		if( nJmp > 0 ){` |
|       - | 10859 | `			/* Phase#7: Fix the unconditional jump */` |
|    2651 | 10860 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2651 | 10861 | `			if( pInstr ){` |
|    2651 | 10862 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 10863 | `			}` |
|    1323 | 10864 | `		}` |
|       - | 10865 | `		/* All done */` |
|    2651 | 10866 | `		return SXRET_OK;` |
|       - | 10867 | `	}` |
| 1312171 | 10868 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10869 | `	/* Generate code for the left tree */` |
| 1312171 | 10870 | `	if( pNode->pLeft ){` |
| 1312133 | 10871 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1312133 | 10872 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10873 | `			ph7_expr_node **apNode;` |
|  416889 | 10874 | `			int hasSpread = 0;` |
|  416889 | 10875 | `			int hasNamed = 0;` |
|  416889 | 10876 | `			int bAnySpread = 0;` |
|  416889 | 10877 | `			sxu32 byRefMask = 0;` |
|       - | 10878 | `			sxi32 nArgs;` |
|       - | 10879 | `			sxi32 n;` |
|       - | 10880 | `			/* Recurse and generate bytecodes for function arguments */` |
|  416889 | 10881 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  416889 | 10882 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10883 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10884 | `			{` |
|  416889 | 10885 | `				int seenNamed = 0;` |
|  825489 | 10886 | `				for( n = 0; n < nArgs; ++n ){` |
|  408607 | 10887 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10888 | `						seenNamed = 1;` |
|     188 | 10889 | `						hasNamed = 1;` |
|  408515 | 10890 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10891 | `						bAnySpread = 1;` |
|  408413 | 10892 | `					}else if( seenNamed ){` |
|       3 | 10893 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10894 | `							"Cannot use positional argument after named argument");` |
|       3 | 10895 | `						return SXERR_SYNTAX;` |
|       - | 10896 | `					}` |
|  204305 | 10897 | `				}` |
|       - | 10898 | `			}` |
|       - | 10899 | `			/* Read-only load */` |
|  416887 | 10900 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10901 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10902 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10903 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10904 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  416887 | 10905 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  416887 | 10906 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  416882 | 10907 | `				if( pCallName->nByte == 5` |
|  228752 | 10908 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   21375 | 10909 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  406202 | 10910 | `				}else if( pCallName->nByte == 5` |
|  207382 | 10911 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10912 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10913 | `				}` |
|       - | 10914 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10915 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10916 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10917 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10918 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10919 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  416887 | 10920 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10921 | `					SyString sBuiltin;` |
|  416769 | 10922 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  416769 | 10923 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  208382 | 10924 | `				}` |
|  208441 | 10925 | `			}` |
|  825485 | 10926 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  408603 | 10927 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  408603 | 10928 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10929 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10930 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 10931 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 10932 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 10933 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 10934 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  408603 | 10935 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      53 | 10936 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      53 | 10937 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      24 | 10938 | `				}` |
|  408603 | 10939 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  408603 | 10940 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10941 | `					return rc;` |
|       - | 10942 | `				}` |
|       - | 10943 | `				/* Each argument is an independent nullsafe scope. */` |
|  408603 | 10944 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  408603 | 10945 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10946 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10947 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10948 | `					hasSpread = 1;` |
|      10 | 10949 | `				}` |
|  204304 | 10950 | `			}` |
|       - | 10951 | `			/* Total number of given arguments */` |
|  416887 | 10952 | `			iP1 = nArgs;` |
|  416887 | 10953 | `			iP2 = hasSpread;` |
|       - | 10954 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10955 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  416887 | 10956 | `			if( hasNamed ){` |
|     101 | 10957 | `				sxu32 nStrBytes = 0;` |
|       - | 10958 | `				char *zBuf;` |
|     297 | 10959 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10960 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10961 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10962 | `					}` |
|     101 | 10963 | `				}` |
|       - | 10964 | `				{` |
|     101 | 10965 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10966 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10967 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10968 | `				if( pMap ){` |
|     101 | 10969 | `					SyZero(pMap, mapSize);` |
|     101 | 10970 | `					pMap->bHasNamed = 1;` |
|     101 | 10971 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10972 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10973 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10974 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10975 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10976 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10977 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10978 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10979 | `							zBuf += nb;` |
|      91 | 10980 | `						}` |
|       - | 10981 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10982 | `					}` |
|     101 | 10983 | `					p3 = (void *)pMap;` |
|      49 | 10984 | `				}` |
|       - | 10985 | `				}` |
|      49 | 10986 | `			}` |
|       - | 10987 | `			/* Remove stale flags now */` |
|  416887 | 10988 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  208441 | 10989 | `		}` |
| 1312131 | 10990 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1312131 | 10991 | `		if( rc != SXRET_OK ){` |
|      34 | 10992 | `			return rc;` |
|       - | 10993 | `		}` |
| 1312101 | 10994 | `		if( !bIsChainOp ){` |
|       - | 10995 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10996 | `			 * target the end of that LHS chain, which is right here. */` |
|  612541 | 10997 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  306268 | 10998 | `		}` |
| 1312101 | 10999 | `		if( iVmOp == PH7_OP_CALL ){` |
|  416887 | 11000 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  416887 | 11001 | `			if( pInstr ){` |
|  416887 | 11002 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  415579 | 11003 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 11004 | `					sxu32 nQual;` |
|  415579 | 11005 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11006 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 11007 | `					 * so the later NEW handler (if any) can see it. */` |
|  415579 | 11008 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 11009 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 11010 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 11011 | `					 * imports — class imports must NOT affect function` |
|       - | 11012 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11013 | `					 * before NEW; we store the original literal index in the` |
|       - | 11014 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11015 | `					 * the unqualified name and re-qualify with class imports. */` |
|  415579 | 11016 | `					if( bAbsolute ){` |
|      26 | 11017 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      15 | 11018 | `					}else{` |
|  415557 | 11019 | `						int fromImport = 0;` |
|  415557 | 11020 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  415557 | 11021 | `						pInstr->iP2 = (sxi32)nQual;` |
|  415557 | 11022 | `						if( nQual != nOrig ){` |
|       - | 11023 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11024 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11025 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11026 | `							if( !fromImport ){` |
|       - | 11027 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11028 | `								if( p3 == 0 ){` |
|      67 | 11029 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11030 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11031 | `									if( pMap ){` |
|      67 | 11032 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11033 | `										p3 = (void *)pMap;` |
|      31 | 11034 | `									}` |
|      31 | 11035 | `								}` |
|      67 | 11036 | `								if( p3 ){` |
|      67 | 11037 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11038 | `								}` |
|      31 | 11039 | `							}` |
|      36 | 11040 | `						}` |
|       5 | 11041 | `					}` |
|  209100 | 11042 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11043 | `					/* Method call,flag that */` |
|    1033 | 11044 | `					pInstr->iP2 = 1;` |
|     514 | 11045 | `				}` |
|  208446 | 11046 | `			}` |
| 1103660 | 11047 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11048 | `			ph7_expr_node **apNode;` |
|       - | 11049 | `			sxi32 n;` |
|   90387 | 11050 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11051 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11052 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 11053 | `			/* Recurse and generate bytecodes for array index */` |
|   90387 | 11054 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  163115 | 11055 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   72733 | 11056 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   72733 | 11057 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   72733 | 11058 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11059 | `					return rc;` |
|       - | 11060 | `				}` |
|       - | 11061 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   72733 | 11062 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   36369 | 11063 | `			}` |
|   90387 | 11064 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   72733 | 11065 | `				iP1 = 1; /* Node have an index associated with it */` |
|   36364 | 11066 | `			}` |
|   90387 | 11067 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11068 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11069 | `				iP2 = 4;` |
|   90268 | 11070 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11071 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11072 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 11073 | `				iP2 = 5;` |
|   90124 | 11074 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11075 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11076 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11077 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11078 | `				iP2 = 6;` |
|   90087 | 11079 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11080 | `				/* Create an empty entry when the desired index is not found */` |
|   35609 | 11081 | `				iP2 = 1;` |
|   17807 | 11082 | `			}` |
|  850028 | 11083 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11084 | `			/* POP the left node */` |
|      32 | 11085 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11086 | `		}` |
|  656048 | 11087 | `	}` |
| 1312139 | 11088 | `	rc = SXRET_OK;` |
| 1312139 | 11089 | `	nJmpIdx = 0;` |
|       - | 11090 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11091 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11092 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1312139 | 11093 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     331 | 11094 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     331 | 11095 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     331 | 11096 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     331 | 11097 | `			int isSpecial = 0;` |
|     331 | 11098 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     243 | 11099 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     243 | 11100 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     253 | 11101 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     221 | 11102 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     112 | 11103 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 11104 | `					isSpecial = 1;` |
|      44 | 11105 | `				}` |
|     141 | 11106 | `			}` |
|     375 | 11107 | `			pInstr->iP1 = 0;` |
|     375 | 11108 | `			if( !isSpecial ){` |
|     199 | 11109 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      97 | 11110 | `			}` |
|       - | 11111 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11112 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     287 | 11113 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     199 | 11114 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     199 | 11115 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      44 | 11116 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 11117 | `					return SXRET_OK;` |
|       - | 11118 | `				}` |
|      76 | 11119 | `			}` |
|     120 | 11120 | `		}` |
|     196 | 11121 | `	}` |
|       - | 11122 | `	/* Generate code for the right tree */` |
| 1312061 | 11123 | `	if( pNode->pRight ){` |
|  724219 | 11124 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11125 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11031 | 11126 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  718706 | 11127 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11128 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3695 | 11129 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  711348 | 11130 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11131 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11132 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  709492 | 11134 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11135 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11136 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11137 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11138 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11139 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11140 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     105 | 11141 | `			sxu32 nNsJmp = 0;` |
|     105 | 11142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     105 | 11143 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  709328 | 11144 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  294011 | 11145 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  147003 | 11146 | `		}` |
|  724219 | 11147 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  724219 | 11148 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  724219 | 11149 | `		if( !bIsChainOp ){` |
|       - | 11150 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11151 | `			 * operator instruction is emitted. */` |
|  531965 | 11152 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  265980 | 11153 | `		}` |
|  724219 | 11154 | `		if( iVmOp == PH7_OP_STORE ){` |
|  290241 | 11155 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  290210 | 11156 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11157 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11158 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11159 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11160 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11161 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11162 | `				 */` |
|      74 | 11163 | `				iVmOp = 0;` |
|  290206 | 11164 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  290171 | 11165 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11166 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   81225 | 11167 | `					iP2 = 1;` |
|   40615 | 11168 | `				}else{` |
|  208951 | 11169 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11170 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   35537 | 11171 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   35537 | 11172 | `						iP1 = pInstr->iP1;` |
|   17771 | 11173 | `					}else{` |
|  173419 | 11174 | `						p3 = pInstr->p3;` |
|       - | 11175 | `					}` |
|       - | 11176 | `					/* POP the last dynamic load instruction */` |
|  208951 | 11177 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11178 | `				}` |
|  145088 | 11179 | `			}` |
|  579101 | 11180 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11181 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11182 | `			if( pInstr ){` |
|      54 | 11183 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11184 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11185 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11186 | `					 */` |
|      17 | 11187 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11188 | `					iP1 = pInstr->iP1;` |
|      17 | 11189 | `					iP2 = pInstr->iP2;` |
|      17 | 11190 | `					p3  = pInstr->p3;` |
|       9 | 11191 | `				}else{` |
|      38 | 11192 | `					p3 = pInstr->p3;` |
|       - | 11193 | `				}` |
|      26 | 11194 | `			}` |
|      26 | 11195 | `		}` |
|  362107 | 11196 | `	}` |
| 1312056 | 11197 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    9538 | 11198 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11199 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11200 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      29 | 11201 | `		iVmOp = 0;` |
|      13 | 11202 | `	}` |
| 1312061 | 11203 | `	if( iVmOp > 0 ){` |
| 1311811 | 11204 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14445 | 11205 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11206 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10561 | 11207 | `				iP1 = 1;` |
|    5283 | 11208 | `			}` |
| 1304591 | 11209 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11210 | `			/* Namespace-qualify the class name for NEW */ {` |
|   18887 | 11211 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   18887 | 11212 | `				VmInstr *pCallInstr = 0;` |
|   18887 | 11213 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   18755 | 11214 | `					pCallInstr = pPeek;` |
|   18755 | 11215 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    9375 | 11216 | `				}` |
|   18887 | 11217 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   18885 | 11218 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11219 | `					sxu32 nLitForClass;` |
|       - | 11220 | `					/* If the CALL handler already qualified the name using` |
|       - | 11221 | `					 * function imports, recover the original unqualified` |
|       - | 11222 | `					 * literal so we can re-qualify with class imports. */` |
|   18885 | 11223 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11224 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11225 | `					}else{` |
|   18853 | 11226 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11227 | `					}` |
|   18885 | 11228 | `					pPeek->iP1 = 0;` |
|   18885 | 11229 | `					if( !bAbsolute ){` |
|   18867 | 11230 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9436 | 11231 | `					}else{` |
|      22 | 11232 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11233 | `					}` |
|    9440 | 11234 | `				}` |
|       - | 11235 | `			}` |
|   18887 | 11236 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   18887 | 11237 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11238 | `				VmInstr *pPrev;` |
|   18755 | 11239 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   18755 | 11240 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11241 | `					/* Pop the call instruction, preserve named-arg map */` |
|   18755 | 11242 | `					iP1 = pInstr->iP1;` |
|   18755 | 11243 | `					if( pInstr->p3 ){` |
|      43 | 11244 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11245 | `					}` |
|   18755 | 11246 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    9375 | 11247 | `				}` |
|    9380 | 11248 | `			}` |
| 1287930 | 11249 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11250 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11251 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     169 | 11252 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     169 | 11253 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     169 | 11254 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     169 | 11255 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     169 | 11256 | `				int isSpecialIs = 0;` |
|     169 | 11257 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     165 | 11258 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     165 | 11259 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     165 | 11260 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     160 | 11261 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      81 | 11262 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11263 | `						isSpecialIs = 1;` |
|       5 | 11264 | `					}` |
|      81 | 11265 | `				}` |
|     171 | 11266 | `				pInstr->iP1 = 0;` |
|     171 | 11267 | `				if( !isSpecialIs && !bAbsolute ){` |
|     149 | 11268 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      72 | 11269 | `				}` |
|      86 | 11270 | `			}` |
| 1278410 | 11271 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11272 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11273 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11274 | `			 * should not trigger constant lookup. */` |
|  192259 | 11275 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  192259 | 11276 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  192217 | 11277 | `				pInstr->iP1 = 0;` |
|   96106 | 11278 | `			}` |
|  192259 | 11279 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11280 | `				/* Static member access,remember that */` |
|     253 | 11281 | `				iP1 = 1;` |
|     253 | 11282 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     253 | 11283 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 11284 | `					p3 = pInstr->p3;` |
|      38 | 11285 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 11286 | `				}` |
|     124 | 11287 | `			}` |
|   96127 | 11288 | `		}` |
|       - | 11289 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11290 | `		 * This is the primary emit path for user-visible calls. */` |
| 1311809 | 11291 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  435769 | 11292 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  217882 | 11293 | `		}` |
|       - | 11294 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1311809 | 11295 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  655902 | 11296 | `	}` |
| 1312059 | 11297 | `	if( nJmpIdx > 0 ){` |
|       - | 11298 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   14845 | 11299 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   14845 | 11300 | `		if( pInstr ){` |
|   14845 | 11301 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7420 | 11302 | `		}` |
|    7420 | 11303 | `	}` |
| 1312059 | 11304 | `	return rc;` |
| 1727332 | 11305 |  |
|       - | 11306 | `/*` |
|       - | 11307 | ` * Compile a PHP expression.` |
|       - | 11308 | ` * According to the PHP language reference manual:` |
|       - | 11309 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11310 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11311 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11312 | ` *  is "anything that has a value".` |
|       - | 11313 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11314 | ` * function takes care of generating the appropriate error` |
|       - | 11315 | ` * message.` |
|       - | 11316 | ` */` |
|  929198 | 11317 | `static sxi32 PH7_CompileExpr(` |
|       - | 11318 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11319 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11320 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11321 | `	)` |
|       5 | 11322 |  |
|       - | 11323 | `	ph7_expr_node *pRoot;` |
|       - | 11324 | `	SySet sExprNode;` |
|       - | 11325 | `	SyToken *pEnd;` |
|       - | 11326 | `	sxi32 nExpr;` |
|       - | 11327 | `	sxi32 iNest;` |
|       - | 11328 | `	sxi32 rc;` |
|       - | 11329 | `	sxu32 nNullsafeBase;` |
|       - | 11330 | `	/* Initialize worker variables */` |
|  929203 | 11331 | `	nExpr = 0;` |
|  929203 | 11332 | `	pRoot = 0;` |
|       - | 11333 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11334 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  929203 | 11335 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  929203 | 11336 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  929203 | 11337 | `	SySetAlloc(&sExprNode,0x10);` |
|  929203 | 11338 | `	rc = SXRET_OK;` |
|       - | 11339 | `	/* Delimit the expression */` |
|  929203 | 11340 | `	pEnd = pGen->pIn;` |
|  929203 | 11341 | `	iNest = 0;` |
| 6217659 | 11342 | `	while( pEnd < pGen->pEnd ){` |
| 5901253 | 11343 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11344 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     475 | 11345 | `			iNest++;` |
| 5901018 | 11346 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     483 | 11347 | `			iNest--;` |
| 5900544 | 11348 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  613133 | 11349 | `			if( iNest <= 0 ){` |
|  612797 | 11350 | `				break;` |
|       - | 11351 | `			}` |
|     168 | 11352 | `		}` |
| 5288461 | 11353 | `		pEnd++;` |
|       5 | 11354 | `	}` |
|  929203 | 11355 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   21617 | 11356 | `		SyToken *pEnd2 = pGen->pIn;` |
|   21617 | 11357 | `		iNest = 0;` |
|       - | 11358 | `		/* Stop at the first comma */` |
|   43523 | 11359 | `		while( pEnd2 < pEnd ){` |
|   21917 | 11360 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      67 | 11361 | `				iNest++;` |
|   21886 | 11362 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      67 | 11363 | `				iNest--;` |
|   21824 | 11364 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11365 | `				if( iNest <= 0 ){` |
|       7 | 11366 | `					break;` |
|       - | 11367 | `				}` |
|      23 | 11368 | `			}` |
|   21911 | 11369 | `			pEnd2++;` |
|       5 | 11370 | `		}` |
|   21617 | 11371 | `		if( pEnd2 <pEnd ){` |
|       7 | 11372 | `			pEnd = pEnd2;` |
|       3 | 11373 | `		}` |
|   10806 | 11374 | `	}` |
|  929203 | 11375 | `	if( pEnd > pGen->pIn ){` |
|  929193 | 11376 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11377 | `		/* Swap delimiter */` |
|  929193 | 11378 | `		pGen->pEnd = pEnd;` |
|       - | 11379 | `		/* Try to get an expression tree */` |
|  929193 | 11380 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  929193 | 11381 | `		if( rc == SXRET_OK && pRoot ){` |
|  929011 | 11382 | `			rc = SXRET_OK;` |
|  929011 | 11383 | `			if( xTreeValidator ){` |
|       - | 11384 | `				/* Call the upper layer validator callback */` |
|   25389 | 11385 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   12692 | 11386 | `			}` |
|  929011 | 11387 | `			if( rc != SXERR_ABORT ){` |
|       - | 11388 | `				/* Generate code for the given tree */` |
|  929011 | 11389 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11390 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11391 | `				 * expression so they short-circuit to its end. */` |
|  929011 | 11392 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  464503 | 11393 | `			}` |
|  929011 | 11394 | `			nExpr = 1;` |
|  464503 | 11395 | `		}` |
|       - | 11396 | `		/* Release the whole tree */` |
|  929193 | 11397 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11398 | `		/* Synchronize token stream */` |
|  929193 | 11399 | `		pGen->pEnd = pTmp;` |
|  929193 | 11400 | `		pGen->pIn  = pEnd;` |
|  929193 | 11401 | `		if( rc == SXERR_ABORT ){` |
|      13 | 11402 | `			SySetRelease(&sExprNode);` |
|      13 | 11403 | `			return SXERR_ABORT;` |
|       - | 11404 | `		}` |
|  464589 | 11405 | `	}` |
|  929193 | 11406 | `	SySetRelease(&sExprNode);` |
|  929193 | 11407 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  464604 | 11408 |  |
|       - | 11409 | `/*` |
|       - | 11410 | ` * Return a pointer to the node construct handler associated` |
|       - | 11411 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11412 | ` */` |
|  235206 | 11413 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11414 |  |
|  235211 | 11415 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11416 | `		/* Numeric literal: Either real or integer */` |
|  123883 | 11417 | `		return PH7_CompileNumLiteral;` |
|  111333 | 11418 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11419 | `		/* Double quoted string */` |
|   22601 | 11420 | `		return PH7_CompileString;` |
|   88737 | 11421 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11422 | `		/* Single quoted string */` |
|   88621 | 11423 | `		return PH7_CompileSimpleString;` |
|     121 | 11424 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11425 | `		/* Heredoc */` |
|      68 | 11426 | `		return PH7_CompileHereDoc;` |
|      56 | 11427 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11428 | `		/* Nowdoc */` |
|      50 | 11429 | `		return PH7_CompileNowDoc;` |
|       9 | 11430 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11431 | `		/* Backtick quoted string */` |
|       6 | 11432 | `		return PH7_CompileBacktic;` |
|       - | 11433 | `	}` |
|       3 | 11434 | `	return 0;` |
|  117608 | 11435 |  |
|       - | 11436 | `/*` |
|       - | 11437 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11438 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11439 | ` * in write context" parse error.` |
|       - | 11440 | ` */` |
|    6822 | 11441 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11442 |  |
|       - | 11443 | `	sxi32 rc;` |
|    6827 | 11444 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6825 | 11445 | `		return SXRET_OK;` |
|       - | 11446 | `	}` |
|       5 | 11447 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11448 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11449 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11450 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3416 | 11451 |  |
|       - | 11452 | `/*` |
|       - | 11453 | ` * Compile an unset() statement.` |
|       - | 11454 | ` * unset($var, $arr[$key], ...);` |
|       - | 11455 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11456 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11457 | ` * parent array before extracting the element to unset.` |
|       - | 11458 | ` */` |
|    2946 | 11459 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11460 |  |
|    2951 | 11461 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2951 | 11462 | `	sxu32 nIdx = 0;` |
|       - | 11463 | `	SyString sName;` |
|       - | 11464 | `	sxi32 rc;` |
|       - | 11465 | `	/* Jump the 'unset' keyword */` |
|    2951 | 11466 | `	pGen->pIn++;` |
|       - | 11467 | `	/* Save delimiter */` |
|    2951 | 11468 | `	pTmp = pGen->pEnd;` |
|       - | 11469 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2951 | 11470 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2951 | 11471 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11472 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11473 | `		SyToken *pClose;` |
|    2951 | 11474 | `		pGen->pIn++;   /* Skip '(' */` |
|    2951 | 11475 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2951 | 11476 | `		pEnd = pClose; /* Stop at ')' */` |
|    1473 | 11477 | `	}` |
|    2951 | 11478 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11479 | `	/* Resolve the 'unset' builtin name once */` |
|    2951 | 11480 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     363 | 11481 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     363 | 11482 | `		if( pObj == 0 ){` |
|     ! 0 | 11483 | `			return SXERR_ABORT;` |
|       - | 11484 | `		}` |
|     363 | 11485 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     363 | 11486 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     179 | 11487 | `	}` |
|       - | 11488 | `	/* Compile each comma-separated argument */` |
|    9775 | 11489 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6829 | 11490 | `		if( pGen->pIn < pNext ){` |
|    6829 | 11491 | `			pGen->pEnd = pNext;` |
|    6829 | 11492 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11493 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11494 | `				GenStateUnsetValidator);` |
|    6829 | 11495 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11496 | `				return SXERR_ABORT;` |
|       - | 11497 | `			}` |
|    6829 | 11498 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11499 | `				/* Emit call for this single argument */` |
|    6827 | 11500 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6827 | 11501 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6827 | 11502 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3411 | 11503 | `			}` |
|    3412 | 11504 | `		}` |
|       - | 11505 | `		/* Jump trailing commas */` |
|   10709 | 11506 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3885 | 11507 | `			pNext++;` |
|       5 | 11508 | `		}` |
|    6829 | 11509 | `		pGen->pIn = pNext;` |
|       5 | 11510 | `	}` |
|       - | 11511 | `	/* Skip past the closing ')' if present */` |
|    2951 | 11512 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2951 | 11513 | `		pGen->pIn++;` |
|    1473 | 11514 | `	}` |
|       - | 11515 | `	/* Restore token stream */` |
|    2951 | 11516 | `	pGen->pEnd = pTmp;` |
|    2951 | 11517 | `	return SXRET_OK;` |
|    1478 | 11518 |  |
|       - | 11519 | `/*` |
|       - | 11520 | ` * PHP Language construct table.` |
|       - | 11521 | ` */` |
|       - | 11522 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11523 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11524 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11525 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11526 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11527 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11528 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11529 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11530 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11531 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11532 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11533 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11534 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11535 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11536 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11537 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11538 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11539 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11540 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11541 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11542 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11543 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11544 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11545 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11546 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11547 | `};` |
|       - | 11548 | `/*` |
|       - | 11549 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11550 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11551 | ` */` |
|  626824 | 11552 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11553 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11554 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11555 | `	)` |
|       5 | 11556 |  |
|  626829 | 11557 | `	sxu32 n = 0;` |
| 3238212 | 11558 | `	for(;;){` |
| 6476429 | 11559 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  134963 | 11560 | `			break;` |
|       - | 11561 | `		}` |
| 6341471 | 11562 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  491871 | 11563 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11564 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11565 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11566 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11567 | `					return 0;` |
|       - | 11568 | `				}` |
|     ! 0 | 11569 | `			}` |
|  491866 | 11570 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11571 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11572 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11573 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11574 | `				return 0;` |
|       - | 11575 | `			}` |
|       - | 11576 | `			/* Return a pointer to the handler.` |
|       - | 11577 | `			*/` |
|  491871 | 11578 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11579 | `		}` |
| 5849605 | 11580 | `		n++;` |
|       5 | 11581 | `	}` |
|  134963 | 11582 | `	if( pLookahed ){` |
|  134963 | 11583 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   38709 | 11584 | `			return PH7_CompileClassInterface;` |
|   96259 | 11585 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   95931 | 11586 | `			return PH7_CompileClass;` |
|     333 | 11587 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      65 | 11588 | `			return PH7_CompileTrait;` |
|       - | 11589 | `		}` |
|       - | 11590 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11591 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11592 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11593 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     134 | 11594 | `	}` |
|       - | 11595 | `	/* Not a language construct */` |
|     273 | 11596 | `	return 0;` |
|  313417 | 11597 |  |
|       - | 11598 | `/*` |
|       - | 11599 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11600 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11601 | ` */` |
|     268 | 11602 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11603 |  |
|       - | 11604 | `	int rc;` |
|     273 | 11605 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     273 | 11606 | `	if( rc == FALSE ){` |
|     158 | 11607 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     157 | 11608 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11609 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11610 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11611 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11612 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11613 | `			*/` |
|       - | 11614 | `			){` |
|     155 | 11615 | `				rc = TRUE;` |
|      75 | 11616 | `		}` |
|      79 | 11617 | `	}` |
|     273 | 11618 | `	return rc;` |
|       5 | 11619 |  |
|       - | 11620 | `/*` |
|       - | 11621 | ` * Compile a PHP chunk.` |
|       - | 11622 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11623 | ` * takes care of generating the appropriate error message.` |
|       - | 11624 | ` */` |
|  750956 | 11625 | `static sxi32 GenStateCompileChunk(` |
|       - | 11626 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11627 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11628 | `	)` |
|       5 | 11629 |  |
|       - | 11630 | `	ProcLangConstruct xCons;` |
|       - | 11631 | `	sxi32 rc;` |
|  750961 | 11632 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  584340 | 11633 | `	for(;;){` |
|  959823 | 11634 | `		int bStmtIsDeclare = 0;` |
|  959823 | 11635 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11636 | `			/* No more input to process */` |
|   14129 | 11637 | `			break;` |
|       - | 11638 | `		}` |
|       - | 11639 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11640 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  945699 | 11641 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  626855 | 11642 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  626855 | 11643 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11644 | `				bStmtIsDeclare = 1;` |
|      20 | 11645 | `			}` |
|  313425 | 11646 | `		}` |
|  945699 | 11647 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11648 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11649 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  208837 | 11650 | `			pGen->bStrictTypesLocked = 1;` |
|  104416 | 11651 | `		}` |
|  945699 | 11652 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11653 | `			/* Compile block */` |
|      21 | 11654 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 11655 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11656 | `				break;` |
|       - | 11657 | `			}` |
|      13 | 11658 | `		}else{` |
|  945683 | 11659 | `			xCons = 0;` |
|  945683 | 11660 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11661 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11662 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11663 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      57 | 11664 | `				xCons = PH7_CompileClassModifiers;` |
|  945657 | 11665 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  626829 | 11666 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11667 | `				/* Try to extract a language construct handler */` |
|  626829 | 11668 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  626829 | 11669 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11670 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11671 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11672 | `						&pGen->pIn->sData);` |
|       9 | 11673 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11674 | `						break;` |
|       - | 11675 | `					}` |
|       - | 11676 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11677 | `					 * this erroneous statement.` |
|       - | 11678 | `					 */` |
|       9 | 11679 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11680 | `				}` |
|  632219 | 11681 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   52245 | 11682 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11683 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11684 | `				xCons = PH7_CompileLabel;` |
|      56 | 11685 | `			}` |
|  945683 | 11686 | `			if( xCons == 0 ){` |
|       - | 11687 | `				/* Assume an expression an try to compile it */` |
|  318955 | 11688 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  318955 | 11689 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11690 | `					/* Pop l-value */` |
|  318805 | 11691 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  159400 | 11692 | `				}` |
|  159480 | 11693 | `			}else{` |
|       - | 11694 | `				/* Go compile the sucker */` |
|  626733 | 11695 | `				rc = xCons(&(*pGen));` |
|       - | 11696 | `			}` |
|  945683 | 11697 | `			if( rc == SXERR_ABORT ){` |
|       - | 11698 | `				/* Request to abort compilation */` |
|      13 | 11699 | `				break;` |
|       - | 11700 | `			}` |
|       - | 11701 | `		}` |
|       - | 11702 | `		/* Ignore trailing semi-colons ';' */` |
| 1529807 | 11703 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  584123 | 11704 | `			pGen->pIn++;` |
|       5 | 11705 | `		}` |
|  945689 | 11706 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11707 | `			/* Compile a single statement and return */` |
|  736827 | 11708 | `			break;` |
|       - | 11709 | `		}` |
|       - | 11710 | `		/* LOOP ONE */` |
|       - | 11711 | `		/* LOOP TWO */` |
|       - | 11712 | `		/* LOOP THREE */` |
|       - | 11713 | `		/* LOOP FOUR */` |
|       5 | 11714 | `	}` |
|       - | 11715 | `	/* Return compilation status */` |
|  750961 | 11716 | `	return rc;` |
|       5 | 11717 |  |
|       - | 11718 | `/*` |
|       - | 11719 | ` * Compile a Raw PHP chunk.` |
|       - | 11720 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11721 | ` * takes care of generating the appropriate error message.` |
|       - | 11722 | ` */` |
|   14136 | 11723 | `static sxi32 PH7_CompilePHP(` |
|       - | 11724 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11725 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11726 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11727 | `	)` |
|       5 | 11728 |  |
|   14141 | 11729 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11730 | `	sxi32 rc;` |
|       - | 11731 | `	/* Reset the token set */` |
|   14141 | 11732 | `	SySetReset(&(*pTokenSet));` |
|       - | 11733 | `	/* Mark as the default token set */` |
|   14141 | 11734 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11735 | `	/* Advance the stream cursor */` |
|   14141 | 11736 | `	pGen->pRawIn++;` |
|       - | 11737 | `	/* Tokenize the PHP chunk first */` |
|   14141 | 11738 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11739 | `	/* Point to the head and tail of the token stream. */` |
|   14141 | 11740 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14141 | 11741 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14141 | 11742 | `	if( is_expr ){` |
|     ! 0 | 11743 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11744 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11745 | `			/* A simple expression,compile it */` |
|     ! 0 | 11746 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11747 | `		}` |
|       - | 11748 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11749 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11750 | `		return SXRET_OK;` |
|       - | 11751 | `	}` |
|   14141 | 11752 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11753 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11754 | `		/*` |
|       - | 11755 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11756 | `		 * According to the PHP reference manual:` |
|       - | 11757 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11758 | `		 *  immediately follow` |
|       - | 11759 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11760 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11761 | `		 * Symisc extension:` |
|       - | 11762 | `		 *   This short syntax works with all PHP opening` |
|       - | 11763 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11764 | `		 *   only short tag.` |
|       - | 11765 | `		 */` |
|       - | 11766 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11767 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11768 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11769 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11770 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11771 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11772 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11773 | `		}` |
|       3 | 11774 | `		return SXRET_OK;` |
|       - | 11775 | `	}` |
|       - | 11776 | `	/* Compile the PHP chunk */` |
|   14139 | 11777 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11778 | `	/* Fix exceptions jumps */` |
|   14139 | 11779 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11780 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14139 | 11781 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11782 | `		rc = SXERR_ABORT;` |
|       1 | 11783 | `	}` |
|       - | 11784 | `	/* Reset container */` |
|   14139 | 11785 | `	SySetReset(&pGen->aGoto);` |
|   14139 | 11786 | `	SySetReset(&pGen->aLabel);` |
|   14139 | 11787 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11788 | `	/* Compilation result */` |
|   14139 | 11789 | `	return rc;` |
|    7073 | 11790 |  |
|       - | 11791 | `/*` |
|       - | 11792 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11793 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11794 | ` * This is the only compile interface exported from this file.` |
|       - | 11795 | ` */` |
|   17034 | 11796 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11797 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11798 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11799 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11800 | `	)` |
|       5 | 11801 |  |
|       - | 11802 | `	SySet aPhpToken,aRawToken;` |
|       - | 11803 | `	ph7_gen_state *pCodeGen;` |
|       - | 11804 | `	ph7_value *pRawObj;` |
|       - | 11805 | `	sxu32 nObjIdx;` |
|       - | 11806 | `	sxi32 nRawObj;` |
|       - | 11807 | `	int is_expr;` |
|       - | 11808 | `	sxi8 bSavedStrict;` |
|       - | 11809 | `	sxi8 bSavedStrictLocked;` |
|       - | 11810 | `	sxi32 rc;` |
|   17039 | 11811 | `	if( pScript->nByte < 1 ){` |
|       - | 11812 | `		/* Nothing to compile */` |
|     ! 0 | 11813 | `		return PH7_OK;` |
|       - | 11814 | `	}` |
|       - | 11815 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11816 | `	 * file's flags so include/require restore them on return. */` |
|   17039 | 11817 | `	pCodeGen = &pVm->sCodeGen;` |
|   17039 | 11818 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17039 | 11819 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17039 | 11820 | `	pCodeGen->bStrictTypes = 0;` |
|   17039 | 11821 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11822 | `	/* Initialize the tokens containers */` |
|   17039 | 11823 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17039 | 11824 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17039 | 11825 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17039 | 11826 | `	is_expr = 0;` |
|   17039 | 11827 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11828 | `		SyToken sTmp;` |
|       - | 11829 | `		/* PHP only: -*/` |
|    3585 | 11830 | `		sTmp.nLine = 1;` |
|    3585 | 11831 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3585 | 11832 | `		sTmp.pUserData = 0;` |
|    3585 | 11833 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3585 | 11834 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3585 | 11835 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11836 | `			/* A simple PHP expression */` |
|     ! 0 | 11837 | `			is_expr = 1;` |
|     ! 0 | 11838 | `		}` |
|    1795 | 11839 | `	}else{` |
|       - | 11840 | `		/* Tokenize raw text */` |
|   13459 | 11841 | `		SySetAlloc(&aRawToken,32);` |
|   13459 | 11842 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11843 | `	}` |
|       - | 11844 | `	/* Process high-level tokens */` |
|   17039 | 11845 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17039 | 11846 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17039 | 11847 | `	rc = PH7_OK;` |
|   17039 | 11848 | `	if( is_expr ){` |
|       - | 11849 | `		/* Compile the expression */` |
|     ! 0 | 11850 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11851 | `		goto cleanup;` |
|       - | 11852 | `	}` |
|   17039 | 11853 | `	nObjIdx = 0;` |
|       - | 11854 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11855 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11856 | `	 * preventing namespace bleeding across include()d files. */` |
|   17039 | 11857 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11858 | `	/* Start the compilation process */` |
|   15250 | 11859 | `	for(;;){` |
|   44629 | 11860 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17027 | 11861 | `			break; /* No more tokens to process */` |
|       - | 11862 | `		}` |
|   27607 | 11863 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11864 | `			/* Compile the PHP chunk */` |
|   14141 | 11865 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14141 | 11866 | `			if( rc == SXERR_ABORT ){` |
|      15 | 11867 | `				break;` |
|       - | 11868 | `			}` |
|   14129 | 11869 | `			continue;` |
|       - | 11870 | `		}` |
|       - | 11871 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13471 | 11872 | `		nRawObj = 0;` |
|   26979 | 11873 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11874 | `			/* Consume the raw chunk without any processing */` |
|   13513 | 11875 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13513 | 11876 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11877 | `				rc = SXERR_MEM;` |
|     ! 0 | 11878 | `				break;` |
|       - | 11879 | `			}` |
|       - | 11880 | `			/* Mark as constant and emit the load constant instruction */` |
|   13513 | 11881 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13513 | 11882 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13513 | 11883 | `			++nRawObj;` |
|   13513 | 11884 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11885 | `		}` |
|   13471 | 11886 | `		if( nRawObj > 0 ){` |
|       - | 11887 | `			/* Emit the consume instruction */` |
|   13471 | 11888 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6733 | 11889 | `		}` |
|    8522 | 11890 | `	}` |
|    8517 | 11891 | `cleanup:` |
|   17039 | 11892 | `	SySetRelease(&aRawToken);` |
|   17039 | 11893 | `	SySetRelease(&aPhpToken);` |
|       - | 11894 | `	/* Restore outer file's strict_types scope */` |
|   17039 | 11895 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17039 | 11896 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17039 | 11897 | `	return rc;` |
|    8522 | 11898 |  |
|       - | 11899 | `/*` |
|       - | 11900 | ` * Utility routines.Initialize the code generator.` |
|       - | 11901 | ` */` |
|    3512 | 11902 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11903 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11904 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11905 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11906 | `	)` |
|       5 | 11907 |  |
|    3517 | 11908 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11909 | `	/* Zero the structure */` |
|    3517 | 11910 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11911 | `	/* Initial state */` |
|    3517 | 11912 | `	pGen->pVm  = &(*pVm);` |
|    3517 | 11913 | `	pGen->xErr = xErr;` |
|    3517 | 11914 | `	pGen->pErrData = pErrData;` |
|    3517 | 11915 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3517 | 11916 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3517 | 11917 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3517 | 11918 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3517 | 11919 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11920 | `	/* Error log buffer */` |
|    3517 | 11921 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11922 | `	/* General purpose working buffer */` |
|    3517 | 11923 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11924 | `	/* Namespace state */` |
|    3517 | 11925 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3517 | 11926 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3517 | 11927 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3517 | 11928 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11929 | `	/* Create the global scope */` |
|    3517 | 11930 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11931 | `	/* Point to the global scope */` |
|    3517 | 11932 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3517 | 11933 | `	return SXRET_OK;` |
|       5 | 11934 |  |
|       - | 11935 | `/*` |
|       - | 11936 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11937 | ` */` |
|   20200 | 11938 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11939 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11940 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11941 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11942 | `	)` |
|       5 | 11943 |  |
|   20205 | 11944 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11945 | `	GenBlock *pBlock,*pParent;` |
|       - | 11946 | `	/* Reset state */` |
|   20205 | 11947 | `	SySetReset(&pGen->aLabel);` |
|   20205 | 11948 | `	SySetReset(&pGen->aGoto);` |
|   20205 | 11949 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20205 | 11950 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20205 | 11951 | `	SyBlobRelease(&pGen->sWorker);` |
|   20205 | 11952 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20205 | 11953 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20205 | 11954 | `	SyHashRelease(&pGen->hUseImports);` |
|   20205 | 11955 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20205 | 11956 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20205 | 11957 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20205 | 11958 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20205 | 11959 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11960 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11961 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11962 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11963 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11964 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11965 | `	 * number of unique names, which is acceptable. */` |
|       - | 11966 | `	/* Point to the global scope */` |
|   20205 | 11967 | `	pBlock = pGen->pCurrent;` |
|   20205 | 11968 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11969 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11970 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11971 | `		pBlock = pParent;` |
|     ! 0 | 11972 | `	}` |
|   20205 | 11973 | `	pGen->xErr = xErr;` |
|   20205 | 11974 | `	pGen->pErrData = pErrData;` |
|   20205 | 11975 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20205 | 11976 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20205 | 11977 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20205 | 11978 | `	pGen->nErr = 0;` |
|   20205 | 11979 | `	return SXRET_OK;` |
|       5 | 11980 |  |
|       - | 11981 | `/*` |
|       - | 11982 | ` * Generate a compile-time error message.` |
|       - | 11983 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11984 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11985 | ` * abort compilation immediately.` |
|       - | 11986 | ` */` |
|     610 | 11987 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11988 |  |
|     615 | 11989 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     615 | 11990 | `	const char *zErr = "Error";` |
|       - | 11991 | `	SyString *pFile;` |
|       - | 11992 | `	va_list ap;` |
|       - | 11993 | `	sxi32 rc;` |
|       - | 11994 | `	/* Reset the working buffer */` |
|     615 | 11995 | `	SyBlobReset(pWorker);` |
|       - | 11996 | `	/* Peek the processed file path if available */` |
|     615 | 11997 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     615 | 11998 | `	if( nErrType == E_ERROR ){` |
|       - | 11999 | `		/* Increment the error counter */` |
|     507 | 12000 | `		pGen->nErr++;` |
|     507 | 12001 | `		if( pGen->nErr > 15 ){` |
|       - | 12002 | `			/* Error count limit reached */` |
|       5 | 12003 | `			if( pGen->xErr ){` |
|       5 | 12004 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 12005 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 12006 | `				if( pFile ){` |
|       5 | 12007 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 12008 | `				}` |
|       5 | 12009 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 12010 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 12011 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12012 | `				}` |
|       2 | 12013 | `			}` |
|       - | 12014 | `			/* Abort immediately */` |
|       5 | 12015 | `			return SXERR_ABORT;` |
|       - | 12016 | `		}` |
|     249 | 12017 | `	}` |
|     611 | 12018 | `	if( pGen->xErr == 0 ){` |
|       - | 12019 | `		/* No available error consumer,return immediately */` |
|       3 | 12020 | `		return SXRET_OK;` |
|       - | 12021 | `	}` |
|     608 | 12022 | `	switch(nErrType){` |
|     500 | 12023 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12024 | `	case E_WARNING: zErr = "Warning";     break;` |
|      78 | 12025 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12026 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12027 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12028 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12029 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12030 | `	default:` |
|     ! 0 | 12031 | `		break;` |
|       - | 12032 | `	}` |
|     608 | 12033 | `	rc = SXRET_OK;` |
|       - | 12034 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     608 | 12035 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     608 | 12036 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     608 | 12037 | `	va_start(ap,zFormat);` |
|     608 | 12038 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     608 | 12039 | `	va_end(ap);` |
|     608 | 12040 | `	if( pFile ){` |
|     608 | 12041 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     302 | 12042 | `	}` |
|       - | 12043 | `	/* Append a new line */` |
|     608 | 12044 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     608 | 12045 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12046 | `		/* Consume the generated error message */` |
|     608 | 12047 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     302 | 12048 | `	}` |
|     608 | 12049 | `	return rc;` |
|     310 | 12050 |  |
|       - | 12051 |  |
