# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5536/6895 lines (80.29%)

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
|      97 |   116 | `			aLabel[n].bRef = TRUE;` |
|      97 |   117 | `			if( ppOut ){` |
|      97 |   118 | `				*ppOut = &aLabel[n];` |
|      46 |   119 | `			}` |
|      97 |   120 | `			return SXRET_OK;` |
|       - |   121 | `		}` |
|      92 |   122 | `	}` |
|       - |   123 | `	/* No such destination */` |
|      59 |   124 | `	return SXERR_NOTFOUND;` |
|      79 |   125 |  |
|       - |   126 | `/*` |
|       - |   127 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|       - |   128 | ` * compiled blocks.` |
|       - |   129 | ` * Return a pointer to that block on success. NULL otherwise.` |
|       - |   130 | ` */` |
|    3728 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3733 |   133 | `	GenBlock *pBlock = pCurrent;` |
|   10601 |   134 | `	for(;;){` |
|   21207 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3625 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3625 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3599 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   17613 |   143 | `		pBlock = pBlock->pParent;` |
|   17613 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1869 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  814358 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  814363 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  814363 |   165 | `	pBlock->pUserData   = pUserData;` |
|  814363 |   166 | `	pBlock->pGen        = pGen;` |
|  814363 |   167 | `	pBlock->iFlags      = iType;` |
|  814363 |   168 | `	pBlock->pParent     = 0;` |
|  814363 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  814363 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  814363 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  810908 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  810913 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  810913 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  810913 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  810913 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  810913 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  810913 |   203 | `	pGen->pCurrent = pBlock;` |
|  810913 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  393873 |   206 | `		*ppBlock = pBlock;` |
|  196934 |   207 | `	}` |
|  810913 |   208 | `	return SXRET_OK;` |
|  405459 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  810900 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  810905 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  810905 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  810905 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  810900 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  810905 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  810905 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  810905 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  810905 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  810900 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  810905 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  810905 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  810905 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  810905 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  810905 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  810905 |   247 | `	return SXRET_OK;` |
|  405455 |   248 |  |
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
|  230330 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  230335 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  230335 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  230335 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  230335 |   268 | `	return rc;` |
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
|  566536 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  566541 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
| 1020427 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  453891 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  181025 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  272871 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   42543 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  230333 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  230333 |   301 | `		if( pInstr ){` |
|  230333 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  230333 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  230333 |   305 | `			aFix[n].nJumpType = -1;` |
|  115164 |   306 | `		}` |
|  115169 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  566541 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  230032 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  230037 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  230183 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|     153 |   329 | `		pJump = &aJumps[n];` |
|       - |   330 | `		/* Extract the target label */` |
|     153 |   331 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|     153 |   332 | `		if( rc != SXRET_OK ){` |
|       - |   333 | `			/* No such label */` |
|      59 |   334 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' was referenced but not defined",&pJump->sLabel);` |
|      59 |   335 | `			if( rc == SXERR_ABORT ){` |
|       3 |   336 | `				return SXERR_ABORT;` |
|       - |   337 | `			}` |
|      57 |   338 | `			continue;` |
|       - |   339 | `		}` |
|       - |   340 | `		/* Make sure the target label is reachable */` |
|      97 |   341 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|      11 |   342 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"Label '%z' is unreachable",&pJump->sLabel);` |
|      11 |   343 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |   344 | `				return SXERR_ABORT;` |
|       - |   345 | `			}` |
|       4 |   346 | `		}` |
|       - |   347 | `		/* Fix the jump now the destination is resolved */` |
|      97 |   348 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|      97 |   349 | `		if( pInstr ){` |
|      97 |   350 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      46 |   351 | `		}` |
|      51 |   352 | `	}` |
|  230035 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  230167 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  230035 |   361 | `	return SXRET_OK;` |
|  115021 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  727858 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  727863 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  727863 |   370 | `	if( pEntry == 0 ){` |
|  316765 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  411103 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  411103 |   374 | `	return SXRET_OK;` |
|  363934 |   375 |  |
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
|  316760 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  316765 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  316765 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  158380 |   390 | `	}` |
|  316765 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  121120 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  121125 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  121125 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  121125 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  121125 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  121125 |   411 | `	return pObj;` |
|   60565 |   412 |  |
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
|  435068 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  435073 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  217539 |   439 |  |
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
|  121784 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  121789 |   501 | `	const char *z = pRaw->zString;` |
|  121789 |   502 | `	sxu32 n = pRaw->nByte;` |
|  121789 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  121789 |   505 | `	if( n < 2 ) return 0;` |
|   10149 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|   10114 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   36813 |   511 | `	for( i = 0; i < n; ++i ){` |
|   26683 |   512 | `		if( z[i] != '_' ) continue;` |
|     546 |   513 | `		if( i > 0 && i + 1 < n` |
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
|   10135 |   529 | `	return 0;` |
|   60897 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  121784 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  121789 |   541 | `	const char *zBad = 0;` |
|  121789 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  121789 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  121775 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   60897 |   555 |  |
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
|  121770 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  121775 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  121775 |   581 | `	*pzAlloc = 0;` |
|  258013 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  136495 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   68124 |   584 | `	}` |
|  121775 |   585 | `	if( !hasUnderscore ){` |
|  121523 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  121523 |   587 | `		return SXRET_OK;` |
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
|   60890 |   604 |  |
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
|  121756 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  121761 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  121761 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  121761 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   60878 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  121761 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  121761 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  182624 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   60873 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  121751 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  121751 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  121125 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  121125 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  121125 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  121125 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   60565 |   649 | `	}else{` |
|       - |   650 | `		/* Real number */` |
|       - |   651 | `		ph7_value *pObj;` |
|       - |   652 | `		/* Reserve a new constant */` |
|     631 |   653 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     631 |   654 | `		if( pObj == 0 ){` |
|     ! 0 |   655 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   656 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   657 | `			return SXERR_ABORT;` |
|       - |   658 | `		}` |
|     631 |   659 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     631 |   660 | `		PH7_MemObjToReal(pObj);` |
|       - |   661 | `	}` |
|  121751 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  121751 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  121751 |   666 | `	return SXRET_OK;` |
|   60883 |   667 |  |
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
|   87240 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   87245 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   87245 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   87245 |   687 | `	zIn  = pStr->zString;` |
|   87245 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   87245 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    7071 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7071 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   80179 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   31663 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   31663 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   48521 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   48521 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   48521 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   48571 |   711 | `	for(;;){` |
|   97147 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   48521 |   714 | `			break;` |
|       - |   715 | `		}` |
|   48631 |   716 | `		zCur = zIn;` |
|  765587 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  716961 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   48631 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   48607 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   24301 |   723 | `		}` |
|   48631 |   724 | `		zIn++;` |
|   48631 |   725 | `		if( zIn < zEnd ){` |
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
|   48631 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   48521 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   48521 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   48521 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   24258 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   48521 |   749 | `	return SXRET_OK;` |
|   43625 |   750 |  |
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
|      49 |   787 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      49 |   788 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   789 | `		zPrefix += 2;` |
|     ! 0 |   790 | `	}else{` |
|      49 |   791 | `		zPrefix += 1;` |
|       - |   792 | `	}` |
|       - |   793 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      49 |   794 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      49 |   795 | `	if( zBuf == 0 ){` |
|     ! 0 |   796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   797 | `		return SXERR_ABORT;` |
|       - |   798 | `	}` |
|      49 |   799 | `	zDst = zBuf;` |
|      49 |   800 | `	z = pIn->zString;` |
|      49 |   801 | `	zEnd = z + pIn->nByte;` |
|     131 |   802 | `	while( z < zEnd ){` |
|      73 |   803 | `		const char *zLine = z;` |
|       - |   804 | `		sxu32 nLine;` |
|       - |   805 | `		int bEmpty;` |
|     801 |   806 | `		while( z < zEnd && z[0] != '\n' ){` |
|     733 |   807 | `			z++;` |
|       5 |   808 | `		}` |
|      73 |   809 | `		nLine = (sxu32)(z - zLine);` |
|      73 |   810 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      73 |   811 | `		if( !bEmpty ){` |
|       - |   812 | `			sxu32 i;` |
|      69 |   813 | `			if( nLine < nIndent ){` |
|     ! 0 |   814 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   815 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   816 | `					nIndent);` |
|     ! 0 |   817 | `				return SXERR_ABORT;` |
|       - |   818 | `			}` |
|     271 |   819 | `			for( i = 0; i < nIndent; i++ ){` |
|     215 |   820 | `				if( zLine[i] != zPrefix[i] ){` |
|      12 |   821 | `					unsigned char c = (unsigned char)zLine[i];` |
|      12 |   822 | `					if( c == ' ' \|\| c == '\t' ){` |
|       6 |   823 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   824 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       4 |   825 | `					}else{` |
|       8 |   826 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   827 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   828 | `							nIndent);` |
|       - |   829 | `					}` |
|      12 |   830 | `					return SXERR_ABORT;` |
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
|    2186 |   916 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2191 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2191 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2191 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2191 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2191 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2191 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2191 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2191 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2191 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2191 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2191 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2191 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   23912 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   23917 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   23917 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   23917 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   23917 |   960 | `	(*pCount)++;` |
|   23917 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   23917 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   23917 |   964 | `	return pConstObj;` |
|   11961 |   965 |  |
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
|   22448 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   22453 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   22453 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   22453 |  1012 | `	zIn  = pStr->zString;` |
|   22453 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   22453 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     311 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     311 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   22147 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   22147 |  1024 | `	iCons = 0;` |
|   12164 |  1025 | `	for(;;){` |
|   36435 |  1026 | `		zCur = zIn;` |
|  174393 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  140149 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  140025 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2066 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1034 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  137963 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   36435 |  1036 | `		if( zIn > zCur ){` |
|   17055 |  1037 | `			if( pObj == 0 ){` |
|   16581 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   16581 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    8288 |  1042 | `			}` |
|   17055 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8525 |  1044 | `		}` |
|   36435 |  1045 | `		if( zIn >= zEnd ){` |
|   22147 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   14293 |  1048 | `		if( zIn[0] == '\\' ){` |
|   12107 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   12107 |  1051 | `			zIn++;` |
|   12107 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   12107 |  1055 | `			if( pObj == 0 ){` |
|    7341 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7341 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3668 |  1060 | `			}` |
|   12107 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   12107 |  1062 | `			switch( zIn[0] ){` |
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
|    5568 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11141 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11141 |  1086 | `				break;` |
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
|   12107 |  1154 | `			zIn += n;` |
|   12107 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2191 |  1157 | `		if( zIn[0] == '{' ){` |
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
|    2063 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|    1038 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    4139 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2063 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|    1038 |  1198 | `				for(;;){` |
|   11617 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8503 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    2081 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    2081 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    2081 |  1212 | `				if( zIn >= zEnd ){` |
|     173 |  1213 | `					break;` |
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
|    2063 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2063 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    2063 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    2061 |  1267 | `				++iCons;` |
|    1028 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2191 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   22147 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1631 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     813 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   22147 |  1278 | `	return SXRET_OK;` |
|   11229 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   22388 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   22393 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   11194 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   22393 |  1290 | `	return rc;` |
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
|   21156 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   21161 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   21161 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   21161 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   21161 |  1350 | `	return rc;` |
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
|      14 |  1364 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
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
|   23422 |  1391 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1392 |  |
|   23427 |  1393 | `	SyToken *pCur = pStart;` |
|   23427 |  1394 | `	sxi32 iNest = 0;` |
|   66285 |  1395 | `	while( pCur < pEnd ){` |
|   48181 |  1396 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5319 |  1397 | `			return pCur;` |
|       - |  1398 | `		}` |
|       - |  1399 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1400 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1401 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1402 | `		 */` |
|   42867 |  1403 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   42861 |  1464 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     324 |  1465 | `			iNest++;` |
|   42701 |  1466 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1467 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1468 | `			 * parser will shortly detect any syntax error. */` |
|     324 |  1469 | `			iNest--;` |
|     160 |  1470 | `		}` |
|   42861 |  1471 | `		pCur++;` |
|       5 |  1472 | `	}` |
|   18109 |  1473 | `	return pEnd;` |
|   11716 |  1474 |  |
|       - |  1475 | `/*` |
|       - |  1476 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1477 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1478 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1479 | ` */` |
|   30366 |  1480 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1481 |  |
|       - |  1482 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1483 | `	SyToken *pKey,*pCur;` |
|   30371 |  1484 | `	sxi32 iEmitRef = 0;` |
|   30371 |  1485 | `	sxi32 iSpread = 0;` |
|   30371 |  1486 | `	sxi32 nPair = 0;` |
|       - |  1487 | `	sxi32 rc;` |
|   30371 |  1488 | `	xValidator = 0;` |
|   24869 |  1489 | `	for(;;){` |
|       - |  1490 | `		/* Jump leading commas */` |
|   56441 |  1491 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6703 |  1492 | `			pGen->pIn++;` |
|       5 |  1493 | `		}` |
|   49743 |  1494 | `		pCur = pGen->pIn;` |
|   49743 |  1495 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1496 | `			/* No more entry to process */` |
|   30355 |  1497 | `			break;` |
|       - |  1498 | `		}` |
|   19393 |  1499 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1500 | `			continue;` |
|       - |  1501 | `		}` |
|       - |  1502 | `		/* Compile the key if available */` |
|   19393 |  1503 | `		pKey = pCur;` |
|   19393 |  1504 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   19393 |  1505 | `		rc = SXERR_EMPTY;` |
|   19393 |  1506 | `		if( pCur < pGen->pIn ){` |
|    1605 |  1507 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1508 | `				/* Missing value */` |
|      13 |  1509 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      13 |  1510 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1511 | `					return SXERR_ABORT;` |
|       - |  1512 | `				}` |
|      13 |  1513 | `				return SXRET_OK;` |
|       - |  1514 | `			}` |
|       - |  1515 | `			/* Compile the expression holding the key */` |
|    1595 |  1516 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1517 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1595 |  1518 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1519 | `				return SXERR_ABORT;` |
|       - |  1520 | `			}` |
|    1595 |  1521 | `			pCur++; /* Jump the '=>' operator */` |
|   18588 |  1522 | `		}else if( pKey == pCur ){` |
|       - |  1523 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1524 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1525 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1526 | `		}else{` |
|       - |  1527 | `			/* Reset back the cursor and point to the entry value */` |
|   17793 |  1528 | `			pCur = pKey;` |
|       - |  1529 | `		}` |
|   19383 |  1530 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1531 | `			/* No available key,load NULL */` |
|   17795 |  1532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    8895 |  1533 | `		}` |
|   19383 |  1534 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1535 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      45 |  1536 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      45 |  1537 | `			iEmitRef = 1;` |
|      45 |  1538 | `			pCur++; /* Jump the '&' token */` |
|      45 |  1539 | `			if( pCur >= pGen->pIn ){` |
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
|   19381 |  1553 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   19381 |  1554 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   19377 |  1567 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   19377 |  1568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1569 | `			return SXERR_ABORT;` |
|       - |  1570 | `		}` |
|   19377 |  1571 | `		if( iSpread ){` |
|       - |  1572 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      64 |  1573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   19346 |  1574 | `		}else if( iEmitRef ){` |
|       - |  1575 | `			/* Emit the load reference instruction */` |
|      40 |  1576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1577 | `		}` |
|   19377 |  1578 | `		xValidator = 0;` |
|   19377 |  1579 | `		iEmitRef = 0;` |
|   19377 |  1580 | `		iSpread = 0;` |
|   19377 |  1581 | `		nPair++;` |
|       5 |  1582 | `	}` |
|       - |  1583 | `	/* Emit the load map instruction */` |
|   30355 |  1584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1585 | `	/* Node successfully compiled */` |
|   30355 |  1586 | `	return SXRET_OK;` |
|   15188 |  1587 |  |
|       - |  1588 | `/*` |
|       - |  1589 | ` * Compile the 'array' language construct.` |
|       - |  1590 | ` *	 According to the PHP language reference manual` |
|       - |  1591 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1592 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1593 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1594 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1595 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1596 | ` */` |
|   29408 |  1597 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1598 |  |
|       - |  1599 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   29413 |  1600 | `	pGen->pIn += 2;` |
|   29413 |  1601 | `	pGen->pEnd--;` |
|   14704 |  1602 | `	SXUNUSED(iCompileFlag);` |
|   29413 |  1603 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1604 |  |
|       - |  1605 | `/*` |
|       - |  1606 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1607 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1608 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1609 | ` */` |
|     958 |  1610 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1611 |  |
|       - |  1612 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     963 |  1613 | `	pGen->pIn++;` |
|     963 |  1614 | `	pGen->pEnd--;` |
|     479 |  1615 | `	SXUNUSED(iCompileFlag);` |
|     963 |  1616 | `	return GenStateCompileArrayBody(pGen);` |
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
|     254 |  1952 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|     259 |  1965 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     259 |  1966 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1967 | `		pGen->pIn++;` |
|     ! 0 |  1968 | `	}` |
|       - |  1969 | `	/* Reserve a constant for the lambda */` |
|     259 |  1970 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     259 |  1971 | `	if( pObj == 0 ){` |
|     ! 0 |  1972 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1973 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1974 | `		return SXERR_ABORT;` |
|       - |  1975 | `	}` |
|       - |  1976 | `	/* Generate a unique name */` |
|     259 |  1977 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1978 | `	/* Make sure the generated name is unique */` |
|     259 |  1979 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1980 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1981 | `	}` |
|     259 |  1982 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     259 |  1983 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1984 | `	/* Compile the lambda body */` |
|     259 |  1985 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     259 |  1986 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1987 | `		return SXERR_ABORT;` |
|       - |  1988 | `	}` |
|     259 |  1989 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1990 | `		/* Emit the load closure instruction */` |
|      21 |  1991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      13 |  1992 | `	}else{` |
|       - |  1993 | `		/* Emit the load constant instruction */` |
|     243 |  1994 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1995 | `	}` |
|       - |  1996 | `	/* Node successfully compiled */` |
|     259 |  1997 | `	return SXRET_OK;` |
|     132 |  1998 |  |
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
|     118 |  2389 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     118 |  2390 | `	if( pFunc == 0 ){` |
|     ! 0 |  2391 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2392 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2393 | `		return SXERR_ABORT;` |
|       - |  2394 | `	}` |
|       - |  2395 | `	/* Generate a unique lambda name */` |
|     118 |  2396 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     220 |  2397 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     104 |  2398 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2399 | `	}` |
|     118 |  2400 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     118 |  2401 | `	if( zDup == 0 ){` |
|     ! 0 |  2402 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2403 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2404 | `		return SXERR_ABORT;` |
|       - |  2405 | `	}` |
|     118 |  2406 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2407 | `	/* Collect function arguments */` |
|     118 |  2408 | `	if( pGen->pIn < pSigEnd ){` |
|      88 |  2409 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      88 |  2410 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2411 | `			return SXERR_ABORT;` |
|       - |  2412 | `		}` |
|      43 |  2413 | `	}` |
|       - |  2414 | `	/* Point past ')' and parse optional return type */` |
|     118 |  2415 | `	pGen->pIn = &pSigEnd[1];` |
|     118 |  2416 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     118 |  2417 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2418 | `		return SXERR_ABORT;` |
|     118 |  2419 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2420 | `		return SXERR_SYNTAX;` |
|       - |  2421 | `	}` |
|       - |  2422 | `	/* Expect '=>' */` |
|     118 |  2423 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
| 1080038 |  2871 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2872 |  |
| 1080043 |  2873 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2874 | `	sxi32 iVv;` |
|       - |  2875 | `	sxi32 iP1;` |
|       - |  2876 | `	void *p3;` |
|       - |  2877 | `	sxi32 rc;` |
| 1080043 |  2878 | `	iVv = -1; /* Variable variable counter */` |
| 2160093 |  2879 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1080055 |  2880 | `		pGen->pIn++;` |
| 1080055 |  2881 | `		iVv++;` |
|       5 |  2882 | `	}` |
| 1080043 |  2883 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2884 | `		/* Invalid variable name */` |
|     ! 0 |  2885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2886 | `		if( rc == SXERR_ABORT ){` |
|       - |  2887 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2888 | `			return SXERR_ABORT;` |
|       - |  2889 | `		}` |
|     ! 0 |  2890 | `		return SXRET_OK;` |
|       - |  2891 | `	}` |
| 1080043 |  2892 | `	p3  = 0;` |
| 1080043 |  2893 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
| 1080027 |  2913 | `		char *zName = 0;` |
|       - |  2914 | `		/* Extract variable name */` |
| 1080027 |  2915 | `		pName = &pGen->pIn->sData;` |
|       - |  2916 | `		/* Advance the stream cursor */` |
| 1080027 |  2917 | `		pGen->pIn++;` |
| 1080027 |  2918 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1080027 |  2919 | `		if( pEntry == 0 ){` |
|       - |  2920 | `			/* Duplicate name */` |
|  144987 |  2921 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  144987 |  2922 | `			if( zName == 0 ){` |
|     ! 0 |  2923 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2924 | `				return SXERR_ABORT;` |
|       - |  2925 | `			}` |
|       - |  2926 | `			/* Install in the hashtable */` |
|  144987 |  2927 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   72496 |  2928 | `		}else{` |
|       - |  2929 | `			/* Name already available */` |
|  935045 |  2930 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2931 | `		}` |
| 1080027 |  2932 | `		p3 = (void *)zName;` |
|       - |  2933 | `	}` |
| 1080039 |  2934 | `	iP1 = 0;` |
| 1080039 |  2935 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  393209 |  2936 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2937 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  393191 |  2938 | `			iP1 = 1;` |
|  196593 |  2939 | `		}` |
|  196602 |  2940 | `	}` |
|       - |  2941 | `	/* Emit the load instruction */` |
| 1080039 |  2942 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1080051 |  2943 | `	while( iVv > 0 ){` |
|      13 |  2944 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2945 | `		iVv--;` |
|       1 |  2946 | `	}` |
|       - |  2947 | `	/* Node successfully compiled */` |
| 1080039 |  2948 | `	return SXRET_OK;` |
|  540024 |  2949 |  |
|       - |  2950 | `/*` |
|       - |  2951 | ` * Load a literal.` |
|       - |  2952 | ` */` |
|  760440 |  2953 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2954 |  |
|  760445 |  2955 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2956 | `	ph7_value *pObj;` |
|       - |  2957 | `	SyString *pStr;` |
|       - |  2958 | `	sxu32 nIdx;` |
|       - |  2959 | `	/* Extract token value */` |
|  760445 |  2960 | `	pStr = &pToken->sData;` |
|       - |  2961 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  760445 |  2962 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  161177 |  2963 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2964 | `			/* NULL constant are always indexed at 0 */` |
|   59377 |  2965 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   59377 |  2966 | `			return SXRET_OK;` |
|  101805 |  2967 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2968 | `			/* TRUE constant are always indexed at 1 */` |
|     669 |  2969 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     669 |  2970 | `			return SXRET_OK;` |
|       5 |  2971 | `		}` |
|  710031 |  2972 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  120380 |  2973 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2974 | `			/* FALSE constant are always indexed at 2 */` |
|   45537 |  2975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   45537 |  2976 | `			return SXRET_OK;` |
|  607762 |  2977 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  108042 |  2978 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2979 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10361 |  2980 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10361 |  2981 | `			if( pObj == 0 ){` |
|     ! 0 |  2982 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2983 | `				return SXERR_ABORT;` |
|       - |  2984 | `			}` |
|   10361 |  2985 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2986 | `			/* Emit the load constant instruction */` |
|   10361 |  2987 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10361 |  2988 | `			return SXRET_OK;` |
|  560849 |  2989 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   34928 |  2990 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  550653 |  3006 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   23834 |  3007 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  552638 |  3008 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   18542 |  3009 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  644505 |  3039 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3040 | `		ph7_value *pLitObj;` |
|       - |  3041 | `		/* Unknown literal,install it in the literal table */` |
|  267777 |  3042 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  267777 |  3043 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3044 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3045 | `			return SXERR_ABORT;` |
|       - |  3046 | `		}` |
|  267777 |  3047 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  267777 |  3048 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  133886 |  3049 | `	}` |
|       - |  3050 | `	/* Emit the load constant instruction */` |
|  644505 |  3051 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  644505 |  3052 | `	return SXRET_OK;` |
|  380225 |  3053 |  |
|       - |  3054 | `/*` |
|       - |  3055 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3056 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3057 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3058 | ` * Otherwise, load the simple literal directly.` |
|       - |  3059 | ` */` |
|  760480 |  3060 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3061 |  |
|       - |  3062 | `	sxi32 rc;` |
|  760485 |  3063 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3064 | `		return SXRET_OK;` |
|       - |  3065 | `	}` |
|       - |  3066 | `	/* Check if this is a multi-token namespace path */` |
|  760485 |  3067 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3068 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      44 |  3069 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      44 |  3070 | `		int isAbsolute = 0;` |
|      44 |  3071 | `		SyBlobReset(pWorker);` |
|       - |  3072 | `		/* Check for leading backslash (absolute path) */` |
|      44 |  3073 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      42 |  3074 | `			isAbsolute = 1;` |
|      42 |  3075 | `			pGen->pIn++; /* Skip leading backslash */` |
|      19 |  3076 | `		}` |
|       - |  3077 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      44 |  3078 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3079 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3080 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3081 | `		}` |
|       - |  3082 | `		/* Collect all path components */` |
|     140 |  3083 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     140 |  3084 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      52 |  3085 | `				SyBlobAppend(pWorker,"\\",1);` |
|      28 |  3086 | `			}else{` |
|      92 |  3087 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3088 | `			}` |
|     140 |  3089 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      44 |  3090 | `				pGen->pIn++;` |
|      44 |  3091 | `				break;` |
|       - |  3092 | `			}` |
|     100 |  3093 | `			pGen->pIn++;` |
|       4 |  3094 | `		}` |
|      44 |  3095 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3096 | `			ph7_value *pObj;` |
|       - |  3097 | `			SyString sPath;` |
|       - |  3098 | `			sxu32 nIdx;` |
|      44 |  3099 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3100 | `			/* Install in the literal table */` |
|      44 |  3101 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
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
|      64 |  3113 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      20 |  3114 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      20 |  3115 | `				nIdx,0,0);` |
|      44 |  3116 | `			return SXRET_OK;` |
|       - |  3117 | `		}` |
|     ! 0 |  3118 | `	}` |
|       - |  3119 | `	/* Single-token literal: load directly */` |
|  760445 |  3120 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  760445 |  3121 | `	return rc;` |
|  380245 |  3122 |  |
|       - |  3123 | `/*` |
|       - |  3124 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3125 | ` */` |
|  760480 |  3126 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3127 |  |
|       - |  3128 | `	sxi32 rc;` |
|  760485 |  3129 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  760485 |  3130 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3131 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3132 | `		return rc;` |
|       - |  3133 | `	}` |
|       - |  3134 | `	/* Node successfully compiled */` |
|  760485 |  3135 | `	return SXRET_OK;` |
|  380245 |  3136 |  |
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
|       5 |  3159 | `			return TRUE;` |
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
|       9 |  3214 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3215 | `		if( rc == SXERR_ABORT ){` |
|       - |  3216 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3217 | `			return SXERR_ABORT;` |
|       - |  3218 | `		}` |
|       9 |  3219 | `		goto Synchronize;` |
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
|      41 |  3270 | `		pGen->pIn++;` |
|       3 |  3271 | `	}` |
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
|    3590 |  3296 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3297 |  |
|    3595 |  3298 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   21053 |  3299 | `	while( pBlock && pBlock != pTarget ){` |
|   17463 |  3300 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   17463 |  3312 | `		pBlock = pBlock->pParent;` |
|       5 |  3313 | `	}` |
|    3595 |  3314 |  |
|    3494 |  3315 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3316 |  |
|       - |  3317 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3318 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3319 | `	sxu32 nLineLocal;` |
|       - |  3320 | `	sxi32 rc;` |
|    3499 |  3321 | `	nLineLocal = pGen->pIn->nLine;` |
|    3499 |  3322 | `	iLevel = 0;` |
|       - |  3323 | `	/* Jump the 'continue' keyword */` |
|    3499 |  3324 | `	pGen->pIn++;` |
|    3499 |  3325 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    3499 |  3351 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3499 |  3352 | `	if( pLoop == 0 ){` |
|       - |  3353 | `		/* Illegal continue */` |
|      13 |  3354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3355 | `		if( rc == SXERR_ABORT ){` |
|       - |  3356 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3357 | `			return SXERR_ABORT;` |
|       - |  3358 | `		}` |
|       8 |  3359 | `	}else{` |
|    3489 |  3360 | `		sxu32 nInstrIdx = 0;` |
|       - |  3361 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3489 |  3362 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3489 |  3363 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    3485 |  3375 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3485 |  3376 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3377 | `				JumpFixup sJumpFix;` |
|       - |  3378 | `				/* Post-continue */` |
|      14 |  3379 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3380 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3381 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3382 | `			}` |
|       - |  3383 | `		}` |
|       - |  3384 | `	}` |
|    3499 |  3385 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3386 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3387 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3388 | `	}` |
|       - |  3389 | `	/* Statement successfully compiled */` |
|    3499 |  3390 | `	return SXRET_OK;` |
|    1752 |  3391 |  |
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
|      20 |  3420 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3421 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3422 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3423 | `				return SXERR_ABORT;` |
|       - |  3424 | `			}` |
|      14 |  3425 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3426 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
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
|      25 |  3498 | `				break;` |
|       - |  3499 | `			}` |
|       - |  3500 | `			/* Point to the upper block */` |
|     113 |  3501 | `			pBlock = pBlock->pParent;` |
|       5 |  3502 | `		}` |
|     113 |  3503 | `		if( pBlock ){` |
|      25 |  3504 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3505 | `		}else{` |
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
|       6 |  3543 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3544 | `		if( rc == SXERR_ABORT ){` |
|       - |  3545 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3546 | `			return SXERR_ABORT;` |
|       - |  3547 | `		}` |
|       4 |  3548 | `	}else{` |
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
|      37 |  3565 | `				break;` |
|       - |  3566 | `			}` |
|       - |  3567 | `			/* Point to the upper block */` |
|     167 |  3568 | `			pBlock = pBlock->pParent;` |
|       5 |  3569 | `		}` |
|     153 |  3570 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3571 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3572 | `			if( rc == SXERR_ABORT ){` |
|       - |  3573 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3574 | `				return SXERR_ABORT;` |
|       - |  3575 | `			}` |
|       3 |  3576 | `		}` |
|     153 |  3577 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      31 |  3578 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      18 |  3579 | `		}else{` |
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
|  418718 |  3668 | `static sxi32 PH7_CompileBlock(` |
|       - |  3669 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3670 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3671 | `	)` |
|       5 |  3672 |  |
|       - |  3673 | `	sxi32 rc;` |
|       - |  3674 | `	sxu32 nLine;` |
|  418723 |  3675 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  417045 |  3676 | `		nLine = pGen->pIn->nLine;` |
|  417045 |  3677 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  417045 |  3678 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3679 | `			return SXERR_ABORT;` |
|       - |  3680 | `		}` |
|  417045 |  3681 | `		pGen->pIn++;` |
|       - |  3682 | `		/* Compile until we hit the closing braces '}' */` |
|  569591 |  3683 | `		for(;;){` |
| 1139187 |  3684 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
| 1139167 |  3695 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3696 | `				/* Closing braces found,break immediately*/` |
|  417025 |  3697 | `				pGen->pIn++;` |
|  417025 |  3698 | `				break;` |
|       - |  3699 | `			}` |
|       - |  3700 | `			/* Compile a single statement */` |
|  722147 |  3701 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  722147 |  3702 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3703 | `				return SXERR_ABORT;` |
|       - |  3704 | `			}` |
|       5 |  3705 | `		}` |
|  417045 |  3706 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  210203 |  3707 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|    1683 |  3751 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1683 |  3752 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3753 | `			return SXERR_ABORT;` |
|       - |  3754 | `		}` |
|       - |  3755 | `	}` |
|       - |  3756 | `	/* Jump trailing semi-colons ';' */` |
|  418723 |  3757 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3758 | `		pGen->pIn++;` |
|     ! 0 |  3759 | `	}` |
|  418723 |  3760 | `	return SXRET_OK;` |
|  209364 |  3761 |  |
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
|   13920 |  3781 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3782 |  |
|   13925 |  3783 | `	GenBlock *pWhileBlock = 0;` |
|   13925 |  3784 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3785 | `	sxu32 nFalseJump;` |
|       - |  3786 | `	sxu32 nLine;` |
|       - |  3787 | `	sxi32 rc;` |
|   13925 |  3788 | `	nLine = pGen->pIn->nLine;` |
|       - |  3789 | `	/* Jump the 'while' keyword */` |
|   13925 |  3790 | `	pGen->pIn++;` |
|   13925 |  3791 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3792 | `		/* Syntax error */` |
|     ! 0 |  3793 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3794 | `		if( rc == SXERR_ABORT ){` |
|       - |  3795 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3796 | `			return SXERR_ABORT;` |
|       - |  3797 | `		}` |
|     ! 0 |  3798 | `		goto Synchronize;` |
|       - |  3799 | `	}` |
|       - |  3800 | `	/* Jump the left parenthesis '(' */` |
|   13925 |  3801 | `	pGen->pIn++;` |
|       - |  3802 | `	/* Create the loop block */` |
|   13925 |  3803 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   13925 |  3804 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3805 | `		return SXERR_ABORT;` |
|       - |  3806 | `	}` |
|       - |  3807 | `	/* Delimit the condition */` |
|   13925 |  3808 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   13925 |  3809 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3810 | `		/* Empty expression */` |
|       3 |  3811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3812 | `		if( rc == SXERR_ABORT ){` |
|       - |  3813 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3814 | `			return SXERR_ABORT;` |
|       - |  3815 | `		}` |
|       1 |  3816 | `	}` |
|       - |  3817 | `	/* Swap token streams */` |
|   13925 |  3818 | `	pTmp = pGen->pEnd;` |
|   13925 |  3819 | `	pGen->pEnd = pEnd;` |
|       - |  3820 | `	/* Compile the expression */` |
|   13925 |  3821 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13925 |  3822 | `	if( rc == SXERR_ABORT ){` |
|       - |  3823 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3824 | `		return SXERR_ABORT;` |
|       - |  3825 | `	}` |
|       - |  3826 | `	/* Update token stream */` |
|   13925 |  3827 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3829 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3830 | `			return SXERR_ABORT;` |
|       - |  3831 | `		}` |
|     ! 0 |  3832 | `		pGen->pIn++;` |
|     ! 0 |  3833 | `	}` |
|       - |  3834 | `	/* Synchronize pointers */` |
|   13925 |  3835 | `	pGen->pIn  = &pEnd[1];` |
|   13925 |  3836 | `	pGen->pEnd = pTmp;` |
|       - |  3837 | `	/* Emit the false jump */` |
|   13925 |  3838 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3839 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   13925 |  3840 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3841 | `	/* Compile the loop body */` |
|   13925 |  3842 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   13925 |  3843 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3844 | `		return SXERR_ABORT;` |
|       - |  3845 | `	}` |
|       - |  3846 | `	/* Emit the unconditional jump to the start of the loop */` |
|   13925 |  3847 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3848 | `	/* Fix all jumps now the destination is resolved */` |
|   13925 |  3849 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3850 | `	/* Release the loop block */` |
|   13925 |  3851 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3852 | `	/* Statement successfully compiled */` |
|   13925 |  3853 | `	return SXRET_OK;` |
|     ! 0 |  3854 | `Synchronize:` |
|       - |  3855 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3856 | `	 * compiling this erroneous block.` |
|       - |  3857 | `	 */` |
|     ! 0 |  3858 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3859 | `		pGen->pIn++;` |
|     ! 0 |  3860 | `	}` |
|     ! 0 |  3861 | `	return SXRET_OK;` |
|    6965 |  3862 |  |
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
|   13920 |  4010 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  4011 |  |
|   13925 |  4012 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   13925 |  4013 | `	GenBlock *pForBlock = 0;` |
|       - |  4014 | `	sxu32 nFalseJump;` |
|       - |  4015 | `	sxu32 nLine;` |
|       - |  4016 | `	sxi32 rc;` |
|   13925 |  4017 | `	nLine = pGen->pIn->nLine;` |
|       - |  4018 | `	/* Jump the 'for' keyword */` |
|   13925 |  4019 | `	pGen->pIn++;` |
|   13925 |  4020 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4021 | `		/* Syntax error */` |
|     ! 0 |  4022 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4023 | `		if( rc == SXERR_ABORT ){` |
|       - |  4024 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4025 | `			return SXERR_ABORT;` |
|       - |  4026 | `		}` |
|     ! 0 |  4027 | `		return SXRET_OK;` |
|       - |  4028 | `	}` |
|       - |  4029 | `	/* Jump the left parenthesis '(' */` |
|   13925 |  4030 | `	pGen->pIn++;` |
|       - |  4031 | `	/* Delimit the init-expr;condition;post-expr */` |
|   13925 |  4032 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   13925 |  4033 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   13925 |  4048 | `	pTmp = pGen->pEnd;` |
|   13925 |  4049 | `	pGen->pEnd = pEnd;` |
|       - |  4050 | `	/* Compile initialization expressions if available */` |
|   13925 |  4051 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4052 | `	/* Pop operand lvalues */` |
|   13925 |  4053 | `	if( rc == SXERR_ABORT ){` |
|       - |  4054 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4055 | `		return SXERR_ABORT;` |
|   13925 |  4056 | `	}else if( rc != SXERR_EMPTY ){` |
|   13923 |  4057 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6959 |  4058 | `	}` |
|   13925 |  4059 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   13925 |  4070 | `	pGen->pIn++;` |
|       - |  4071 | `	/* Create the loop block */` |
|   13925 |  4072 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   13925 |  4073 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4074 | `		return SXERR_ABORT;` |
|       - |  4075 | `	}` |
|       - |  4076 | `	/* Deffer continue jumps */` |
|   13925 |  4077 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4078 | `	/* Compile the condition */` |
|   13925 |  4079 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13925 |  4080 | `	if( rc == SXERR_ABORT ){` |
|       - |  4081 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4082 | `		return SXERR_ABORT;` |
|   13925 |  4083 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4084 | `		/* Emit the false jump */` |
|   13923 |  4085 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4086 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   13923 |  4087 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    6959 |  4088 | `	}` |
|   13925 |  4089 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   13921 |  4100 | `	pGen->pIn++;` |
|       - |  4101 | `	/* Save the post condition stream */` |
|   13921 |  4102 | `	pPostStart = pGen->pIn;` |
|       - |  4103 | `	/* Compile the loop body */` |
|   13921 |  4104 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   13921 |  4105 | `	pGen->pEnd = pTmp;` |
|   13921 |  4106 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   13921 |  4107 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4108 | `		return SXERR_ABORT;` |
|       - |  4109 | `	}` |
|       - |  4110 | `	/* Fix post-continue jumps */` |
|   13921 |  4111 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   13921 |  4127 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4128 | `		pPostStart++;` |
|     ! 0 |  4129 | `	}` |
|   13921 |  4130 | `	if( pPostStart < pEnd ){` |
|       - |  4131 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   13921 |  4132 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   13921 |  4133 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   13921 |  4134 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4135 | `			/* Syntax error */` |
|     ! 0 |  4136 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4137 | `			if( rc == SXERR_ABORT ){` |
|       - |  4138 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4139 | `				return SXERR_ABORT;` |
|       - |  4140 | `			}` |
|     ! 0 |  4141 | `			return SXRET_OK;` |
|       - |  4142 | `		}` |
|   13921 |  4143 | `		RE_SWAP_DELIMITER(pGen);` |
|   13921 |  4144 | `		if( rc == SXERR_ABORT ){` |
|       - |  4145 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4146 | `			return SXERR_ABORT;` |
|   13921 |  4147 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4148 | `			/* Pop operand lvalue */` |
|   13921 |  4149 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6958 |  4150 | `		}` |
|    6958 |  4151 | `	}` |
|       - |  4152 | `	/* Emit the unconditional jump to the start of the loop */` |
|   13921 |  4153 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4154 | `	/* Fix all jumps now the destination is resolved */` |
|   13921 |  4155 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4156 | `	/* Release the loop block */` |
|   13921 |  4157 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4158 | `	/* Statement successfully compiled */` |
|   13921 |  4159 | `	return SXRET_OK;` |
|    6965 |  4160 |  |
|       - |  4161 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4162 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4163 | ` * are allowed.` |
|       - |  4164 | ` */` |
|    7470 |  4165 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4166 |  |
|    7475 |  4167 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7475 |  4168 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4169 | `		/* Unexpected expression */` |
|     ! 0 |  4170 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4171 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4172 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4173 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4174 | `		}` |
|     ! 0 |  4175 | `	}` |
|    7475 |  4176 | `	return rc;` |
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
|    3828 |  4204 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4205 |  |
|    3833 |  4206 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3833 |  4207 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3833 |  4208 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4209 | `	ph7_foreach_info *pInfo;` |
|       - |  4210 | `	sxu32 nFalseJump;` |
|       - |  4211 | `	VmInstr *pInstr;` |
|       - |  4212 | `	sxu32 nLine;` |
|       - |  4213 | `	sxi32 rc;` |
|    3833 |  4214 | `	nLine = pGen->pIn->nLine;` |
|       - |  4215 | `	/* Jump the 'foreach' keyword */` |
|    3833 |  4216 | `	pGen->pIn++;` |
|    3833 |  4217 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4218 | `		/* Syntax error */` |
|     ! 0 |  4219 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4220 | `		if( rc == SXERR_ABORT ){` |
|       - |  4221 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4222 | `			return SXERR_ABORT;` |
|       - |  4223 | `		}` |
|     ! 0 |  4224 | `		goto Synchronize;` |
|       - |  4225 | `	}` |
|       - |  4226 | `	/* Jump the left parenthesis '(' */` |
|    3833 |  4227 | `	pGen->pIn++;` |
|       - |  4228 | `	/* Create the loop block */` |
|    3833 |  4229 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3833 |  4230 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4231 | `		return SXERR_ABORT;` |
|       - |  4232 | `	}` |
|       - |  4233 | `	/* Delimit the expression */` |
|    3833 |  4234 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3833 |  4235 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    3833 |  4250 | `	pCur = pGen->pIn;` |
|   26299 |  4251 | `	while( pCur < pEnd ){` |
|   26299 |  4252 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3847 |  4253 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3847 |  4254 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4255 | `				/* Break with the first 'as' found */` |
|    3833 |  4256 | `				break;` |
|       - |  4257 | `			}` |
|       7 |  4258 | `		}` |
|       - |  4259 | `		/* Advance the stream cursor */` |
|   22471 |  4260 | `		pCur++;` |
|       5 |  4261 | `	}` |
|    3833 |  4262 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4263 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4264 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4265 | `		if( rc == SXERR_ABORT ){` |
|       - |  4266 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4267 | `			return SXERR_ABORT;` |
|       - |  4268 | `		}` |
|     ! 0 |  4269 | `		goto Synchronize;` |
|       - |  4270 | `	}` |
|       - |  4271 | `	/* Swap token streams */` |
|    3833 |  4272 | `	pTmp = pGen->pEnd;` |
|    3833 |  4273 | `	pGen->pEnd = pCur;` |
|    3833 |  4274 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3833 |  4275 | `	if( rc == SXERR_ABORT ){` |
|       - |  4276 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4277 | `		return SXERR_ABORT;` |
|       - |  4278 | `	}` |
|       - |  4279 | `	/* Update token stream */` |
|    3833 |  4280 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4281 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4282 | `		if( rc == SXERR_ABORT ){` |
|       - |  4283 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4284 | `			return SXERR_ABORT;` |
|       - |  4285 | `		}` |
|     ! 0 |  4286 | `		pGen->pIn++;` |
|     ! 0 |  4287 | `	}` |
|    3833 |  4288 | `	pCur++; /* Jump the 'as' keyword */` |
|    3833 |  4289 | `	pGen->pIn = pCur;` |
|    3833 |  4290 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4291 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4292 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4293 | `			return SXERR_ABORT;` |
|       - |  4294 | `		}` |
|     ! 0 |  4295 | `	}` |
|       - |  4296 | `	/* Create the foreach context */` |
|    3833 |  4297 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3833 |  4298 | `	if( pInfo == 0 ){` |
|     ! 0 |  4299 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4300 | `		return SXERR_ABORT;` |
|       - |  4301 | `	}` |
|       - |  4302 | `	/* Zero the structure */` |
|    3833 |  4303 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4304 | `	/* Initialize structure fields */` |
|    3833 |  4305 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4306 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4307 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4308 | `	 * '=>'. */` |
|    3833 |  4309 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3833 |  4310 | `	if( pCur < pEnd ){` |
|       - |  4311 | `		/* Compile the expression holding the key name */` |
|    3659 |  4312 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4313 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4314 | `			if( rc == SXERR_ABORT ){` |
|       - |  4315 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4316 | `				return SXERR_ABORT;` |
|       - |  4317 | `			}` |
|     ! 0 |  4318 | `		}else{` |
|    3659 |  4319 | `			pGen->pEnd = pCur;` |
|    3659 |  4320 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3659 |  4321 | `			if( rc == SXERR_ABORT ){` |
|       - |  4322 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4323 | `				return SXERR_ABORT;` |
|       - |  4324 | `			}` |
|    3659 |  4325 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3659 |  4326 | `			if( pInstr->p3 ){` |
|       - |  4327 | `				/* Record key name */` |
|    3659 |  4328 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1827 |  4329 | `			}` |
|    3659 |  4330 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4331 | `		}` |
|    3659 |  4332 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1827 |  4333 | `	}` |
|    3833 |  4334 | `	pGen->pEnd = pEnd;` |
|    3833 |  4335 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4336 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4337 | `		if( rc == SXERR_ABORT ){` |
|       - |  4338 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4339 | `			return SXERR_ABORT;` |
|       - |  4340 | `		}` |
|     ! 0 |  4341 | `		goto Synchronize;` |
|       - |  4342 | `	}` |
|    3833 |  4343 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4344 | `		pGen->pIn++;` |
|       - |  4345 | `		/* Pass by reference  */` |
|      11 |  4346 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4347 | `	}` |
|       - |  4348 | `	/* Check if the value target is list() */` |
|    3833 |  4349 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    3828 |  4390 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|    3821 |  4423 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3821 |  4424 | `		if( rc == SXERR_ABORT ){` |
|       - |  4425 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4426 | `			return SXERR_ABORT;` |
|       - |  4427 | `		}` |
|    3821 |  4428 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3821 |  4429 | `		if( pInstr->p3 ){` |
|       - |  4430 | `			/* Record value name */` |
|    3821 |  4431 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1908 |  4432 | `		}` |
|       - |  4433 | `	}` |
|       - |  4434 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3831 |  4435 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4436 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3831 |  4437 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4438 | `	/* Record the first instruction to execute */` |
|    3831 |  4439 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4440 | `	/* Emit the FOREACH_STEP instruction */` |
|    3831 |  4441 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4442 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3831 |  4443 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4444 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3831 |  4445 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    3831 |  4473 | `	pGen->pIn = &pEnd[1];` |
|    3831 |  4474 | `	pGen->pEnd = pTmp;` |
|    3831 |  4475 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3831 |  4476 | `	if( rc == SXERR_ABORT ){` |
|       - |  4477 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4478 | `		return SXERR_ABORT;` |
|       - |  4479 | `	}` |
|       - |  4480 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3831 |  4481 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4482 | `	/* Fix all jumps now the destination is resolved */` |
|    3831 |  4483 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4484 | `	/* Release the loop block */` |
|    3831 |  4485 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4486 | `	/* Statement successfully compiled */` |
|    3831 |  4487 | `	return SXRET_OK;` |
|       1 |  4488 | `Synchronize:` |
|       - |  4489 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4490 | `	 * compiling this erroneous block.` |
|       - |  4491 | `	 */` |
|       3 |  4492 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4493 | `		pGen->pIn++;` |
|     ! 0 |  4494 | `	}` |
|       3 |  4495 | `	return SXRET_OK;` |
|    1919 |  4496 |  |
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
|  144724 |  4529 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4530 |  |
|  144729 |  4531 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  144729 |  4532 | `	GenBlock *pCondBlock = 0;` |
|       - |  4533 | `	sxu32 nJumpIdx;` |
|       - |  4534 | `	sxu32 nKeyID;` |
|       - |  4535 | `	sxi32 rc;` |
|       - |  4536 | `	/* Jump the 'if' keyword */` |
|  144729 |  4537 | `	pGen->pIn++;` |
|  144729 |  4538 | `	pToken = pGen->pIn;` |
|       - |  4539 | `	/* Create the conditional block */` |
|  144729 |  4540 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  144729 |  4541 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4542 | `		return SXERR_ABORT;` |
|       - |  4543 | `	}` |
|       - |  4544 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   79319 |  4545 | `	for(;;){` |
|  158643 |  4546 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  158643 |  4559 | `		pToken++;` |
|       - |  4560 | `		/* Delimit the condition */` |
|  158643 |  4561 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  158643 |  4562 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  158643 |  4575 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4576 | `		/* Compile the condition */` |
|  158643 |  4577 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4578 | `		/* Update token stream */` |
|  158643 |  4579 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4580 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4581 | `			pGen->pIn++;` |
|     ! 0 |  4582 | `		}` |
|  158643 |  4583 | `		pGen->pIn  = &pEnd[1];` |
|  158643 |  4584 | `		pGen->pEnd = pTmp;` |
|  158643 |  4585 | `		if( rc == SXERR_ABORT ){` |
|       - |  4586 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4587 | `			return SXERR_ABORT;` |
|       - |  4588 | `		}` |
|       - |  4589 | `		/* Emit the false jump */` |
|  158643 |  4590 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4591 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  158643 |  4592 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4593 | `		/* Compile the body */` |
|  158643 |  4594 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  158643 |  4595 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4596 | `			return SXERR_ABORT;` |
|       - |  4597 | `		}` |
|  158643 |  4598 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   44226 |  4599 | `			break;` |
|       - |  4600 | `		}` |
|       - |  4601 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   70201 |  4602 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   70201 |  4603 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   45173 |  4604 | `			break;` |
|       - |  4605 | `		}` |
|       - |  4606 | `		/* Emit the unconditional jump */` |
|   25033 |  4607 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4608 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   25033 |  4609 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   25033 |  4610 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18019 |  4611 | `			pToken = &pGen->pIn[1];` |
|   18019 |  4612 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    6952 |  4613 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5562 |  4614 | `					break;` |
|       - |  4615 | `			}` |
|    6905 |  4616 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3450 |  4617 | `		}` |
|   13919 |  4618 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4619 | `		/* Synchronize cursors */` |
|   13919 |  4620 | `		pToken = pGen->pIn;` |
|       - |  4621 | `		/* Fix the false jump */` |
|   13919 |  4622 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4623 | `	} /* For(;;) */` |
|       - |  4624 | `	/* Fix the false jump */` |
|  144729 |  4625 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  144729 |  4626 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   56282 |  4627 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4628 | `			/* Compile the else block */` |
|   11119 |  4629 | `			pGen->pIn++;` |
|   11119 |  4630 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11119 |  4631 | `			if( rc == SXERR_ABORT ){` |
|       - |  4632 |  |
|     ! 0 |  4633 | `				return SXERR_ABORT;` |
|       - |  4634 | `			}` |
|    5557 |  4635 | `	}` |
|  144729 |  4636 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4637 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  144729 |  4638 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4639 | `	/* Release the conditional block */` |
|  144729 |  4640 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4641 | `	/* Statement successfully compiled */` |
|  144729 |  4642 | `	return SXRET_OK;` |
|     ! 0 |  4643 | `Synchronize:` |
|       - |  4644 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4645 | `	 */` |
|     ! 0 |  4646 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4647 | `		pGen->pIn++;` |
|     ! 0 |  4648 | `	}` |
|     ! 0 |  4649 | `	return SXRET_OK;` |
|   72367 |  4650 |  |
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
|  228886 |  4744 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4745 |  |
|  228891 |  4746 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4747 | `	sxi32 rc;` |
|       - |  4748 | `	/* Jump the 'return' keyword */` |
|  228891 |  4749 | `	pGen->pIn++;` |
|  228891 |  4750 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4751 | `		/* Compile the expression */` |
|  228863 |  4752 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  228863 |  4753 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4754 | `			return SXERR_ABORT;` |
|  228863 |  4755 | `		}else if(rc != SXERR_EMPTY ){` |
|  228863 |  4756 | `			nRet = 1;` |
|  114429 |  4757 | `		}` |
|  114429 |  4758 | `	}` |
|       - |  4759 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4760 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4761 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4762 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4763 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  228891 |  4764 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  228891 |  4765 | `	return SXRET_OK;` |
|  114448 |  4766 |  |
|       - |  4767 | `/*` |
|       - |  4768 | ` * Compile a yield expression.` |
|       - |  4769 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4770 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4771 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4772 | ` */` |
|     140 |  4773 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4774 |  |
|       - |  4775 | `	SyToken *pTmp, *pSplit;` |
|     145 |  4776 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     145 |  4777 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4778 | `	sxi32 rc;` |
|      70 |  4779 | `	(void)iCompileFlag;` |
|       - |  4780 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     145 |  4781 | `	pGen->pIn++;` |
|       - |  4782 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4783 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4784 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4785 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4786 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     140 |  4787 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|      87 |  4788 | `		&& pGen->pIn->sData.nByte == 4` |
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
|     111 |  4806 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4807 | `		/* Bare yield — no value */` |
|     ! 0 |  4808 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4809 | `		return SXRET_OK;` |
|       - |  4810 | `	}` |
|       - |  4811 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     111 |  4812 | `	pSplit = 0;` |
|       - |  4813 | `	{` |
|     111 |  4814 | `		SyToken *pCur = pGen->pIn;` |
|     111 |  4815 | `		sxi32 nNest = 0;` |
|     233 |  4816 | `		while( pCur < pGen->pEnd ){` |
|     141 |  4817 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4818 | `				nNest++;` |
|     141 |  4819 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4820 | `				nNest--;` |
|     141 |  4821 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4822 | `				pSplit = pCur;` |
|      16 |  4823 | `				break;` |
|       - |  4824 | `			}` |
|     127 |  4825 | `			pCur++;` |
|       5 |  4826 | `		}` |
|       - |  4827 | `	}` |
|     111 |  4828 | `	pTmp = pGen->pEnd;` |
|     111 |  4829 | `	if( pSplit ){` |
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
|      97 |  4842 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      97 |  4843 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      97 |  4844 | `		if( rc != SXERR_EMPTY ){` |
|      97 |  4845 | `			iP1 = 1;` |
|      46 |  4846 | `		}` |
|       - |  4847 | `	}` |
|     111 |  4848 | `	pGen->pEnd = pTmp;` |
|     111 |  4849 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     111 |  4850 | `	return SXRET_OK;` |
|      75 |  4851 |  |
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
|   14118 |  4879 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4880 |  |
|   14123 |  4881 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4882 | `	sxi32 rc;` |
|       - |  4883 | `	/* Jump the 'echo' keyword */` |
|   14123 |  4884 | `	pGen->pIn++;` |
|       - |  4885 | `	/* Compile arguments one after one */` |
|   14123 |  4886 | `	pTmp = pGen->pEnd;` |
|   30775 |  4887 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   16657 |  4888 | `		if( pGen->pIn < pNext ){` |
|   16657 |  4889 | `			pGen->pEnd = pNext;` |
|   16657 |  4890 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   16657 |  4891 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4892 | `				return SXERR_ABORT;` |
|   16657 |  4893 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4894 | `				/* Emit the consume instruction */` |
|   16633 |  4895 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8314 |  4896 | `			}` |
|    8326 |  4897 | `		}` |
|       - |  4898 | `		/* Jump trailing commas */` |
|   19191 |  4899 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2539 |  4900 | `			pNext++;` |
|       5 |  4901 | `		}` |
|   16657 |  4902 | `		pGen->pIn = pNext;` |
|       5 |  4903 | `	}` |
|       - |  4904 | `	/* Restore token stream */` |
|   14123 |  4905 | `	pGen->pEnd = pTmp;` |
|   14123 |  4906 | `	return SXRET_OK;` |
|    7064 |  4907 |  |
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
|  427208 |  5074 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
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
|  427213 |  5085 | `	if( pFromImport ){` |
|  408375 |  5086 | `		*pFromImport = 0;` |
|  204185 |  5087 | `	}` |
|  427213 |  5088 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  427213 |  5089 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5090 | `		return nOrigIdx;` |
|       - |  5091 | `	}` |
|  427213 |  5092 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  427213 |  5093 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5094 | `	/* Skip if already qualified (contains backslash) */` |
|  427213 |  5095 | `	hasNsSep = 0;` |
| 4621679 |  5096 | `	for( k = 0; k < nLit; k++ ){` |
| 4194479 |  5097 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2097238 |  5098 | `	}` |
|  427213 |  5099 | `	if( hasNsSep ){` |
|      11 |  5100 | `		return nOrigIdx;` |
|       - |  5101 | `	}` |
|       - |  5102 | `	/* Check use imports first (works even outside namespaces) */` |
|  427205 |  5103 | `	SyBlobReset(&pGen->sWorker);` |
|  427205 |  5104 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  427205 |  5105 | `	if( pImport ){` |
|      41 |  5106 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5107 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5108 | `		if( pFromImport ){` |
|      18 |  5109 | `			*pFromImport = 1;` |
|       8 |  5110 | `		}` |
|      23 |  5111 | `	}else{` |
|  427169 |  5112 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  427079 |  5113 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
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
|  213609 |  5132 |  |
|       - |  5133 | `/*` |
|       - |  5134 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5135 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5136 | ` */` |
|   93930 |  5137 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5138 |  |
|       - |  5139 | `	SyHashEntry *pImport;` |
|       - |  5140 | `	/* Check use imports first */` |
|   93935 |  5141 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   93935 |  5142 | `	if( pImport ){` |
|      15 |  5143 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      15 |  5144 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      15 |  5145 | `		return;` |
|       - |  5146 | `	}` |
|       - |  5147 | `	/* Prepend current namespace if active */` |
|   93923 |  5148 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5149 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5150 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5151 | `	}` |
|   93923 |  5152 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   46970 |  5153 |  |
|       - |  5154 | `/*` |
|       - |  5155 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5156 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5157 | ` * The caller must release pOut when done.` |
|       - |  5158 | ` */` |
|  132330 |  5159 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5160 |  |
|  132335 |  5161 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5162 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5163 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5164 | `	}` |
|  132335 |  5165 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  132335 |  5166 |  |
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
|       4 |  5204 |  |
|      18 |  5205 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      12 |  5206 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      12 |  5207 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      12 |  5208 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      12 |  5209 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      12 |  5210 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5211 | `	return "token";` |
|      11 |  5212 |  |
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
|      27 |  5246 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5247 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5248 | `			}` |
|      16 |  5249 | `		}else{` |
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
|      35 |  5589 | `		pCursor = pValTok + 1;` |
|       - |  5590 | `		/* Consume separating comma (or end). */` |
|      35 |  5591 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5592 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5593 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5594 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5595 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5596 | `				return SXRET_OK;` |
|       - |  5597 | `			}` |
|       3 |  5598 | `			pCursor++;` |
|       1 |  5599 | `		}` |
|       5 |  5600 | `	}` |
|       - |  5601 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5602 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5603 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5604 | `	return SXRET_OK;` |
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
|   65584 |  5664 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5665 |  |
|       - |  5666 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5667 | `	SySet *pInstrContainer;` |
|       - |  5668 | `	sxi32 rc;` |
|       - |  5669 | `	/* Swap token stream */` |
|   65589 |  5670 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   65589 |  5671 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   65589 |  5672 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5673 | `	/* Compile the expression holding the argument value */` |
|   65589 |  5674 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5675 | `	/* Emit the done instruction */` |
|   65589 |  5676 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   65589 |  5677 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   65589 |  5678 | `	RE_SWAP_DELIMITER(pGen);` |
|   65589 |  5679 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5680 | `		return SXERR_ABORT;` |
|       - |  5681 | `	}` |
|   65589 |  5682 | `	return SXRET_OK;` |
|   32797 |  5683 |  |
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
|   90922 |  5721 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5722 |  |
|       - |  5723 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5724 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5725 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5726 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5727 | `	sxi32 rc;` |
|       - |  5728 |  |
|   90927 |  5729 | `	pIn = pGen->pIn;` |
|   90927 |  5730 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5731 | `	/* Process arguments one after one */` |
|  113705 |  5732 | `	for(;;){` |
|  227415 |  5733 | `		if( pIn >= pEnd ){` |
|       - |  5734 | `			/* No more arguments to process */` |
|   90915 |  5735 | `			break;` |
|       - |  5736 | `		}` |
|  136505 |  5737 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  136505 |  5738 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  136505 |  5739 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  136505 |  5740 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5741 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5742 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5743 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5744 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5745 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5746 | `		{` |
|  136505 |  5747 | `			int bReadonly = 0, bVisSeen = 0;` |
|  136505 |  5748 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  136505 |  5749 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5750 | `				bReadonly = 1;` |
|       3 |  5751 | `				pIn++;` |
|       1 |  5752 | `			}` |
|  136505 |  5753 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   62301 |  5754 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   62301 |  5755 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      67 |  5756 | `					bVisSeen = 1;` |
|      67 |  5757 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      89 |  5758 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      29 |  5759 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      67 |  5760 | `					pIn++;` |
|      67 |  5761 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5762 | `						bReadonly = 1;` |
|      16 |  5763 | `						pIn++;` |
|       6 |  5764 | `					}` |
|      31 |  5765 | `				}` |
|   31148 |  5766 | `			}` |
|  136505 |  5767 | `			if( bVisSeen \|\| bReadonly ){` |
|      69 |  5768 | `				if( !bCtorCtx ){` |
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
|      65 |  5781 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      65 |  5782 | `				sArg.iPromoteVis = iVis;` |
|      65 |  5783 | `				if( bReadonly ){` |
|      18 |  5784 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5785 | `				}` |
|      30 |  5786 | `			}` |
|       - |  5787 | `		}` |
|       - |  5788 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  136496 |  5789 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  106341 |  5790 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   74453 |  5791 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   72693 |  5792 | `			sxu32 nLineLocal = pIn->nLine;` |
|   72693 |  5793 | `			sxi32 iTFlags = 0;` |
|   72693 |  5794 | `			pGen->pIn = pIn;` |
|   72693 |  5795 | `			rc = GenStateParseUnionTypeDecl(` |
|   36344 |  5796 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   36344 |  5797 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5798 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5799 | `				/* bAllowVoid */ 0,` |
|   36344 |  5800 | `						nLineLocal);` |
|   72693 |  5801 | `			pIn = pGen->pIn;` |
|   72693 |  5802 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5803 | `				return SXERR_ABORT;` |
|   72693 |  5804 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5805 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5806 | `				return SXERR_SYNTAX;` |
|   72691 |  5807 | `			}else if( rc == SXERR_SYNTAX ){` |
|       6 |  5808 | `				if( pIn < pEnd ){` |
|       8 |  5809 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5810 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5811 | `						&pIn->sData);` |
|       4 |  5812 | `				}else{` |
|     ! 0 |  5813 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5814 | `						"syntax error, unexpected end of file");` |
|       - |  5815 | `				}` |
|       6 |  5816 | `				return SXERR_SYNTAX;` |
|       - |  5817 | `			}` |
|   72687 |  5818 | `			sArg.iFlags \|= iTFlags;` |
|   36341 |  5819 | `		}` |
|  136495 |  5820 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5821 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5822 | `			return rc;` |
|       - |  5823 | `		}` |
|  136495 |  5824 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5825 | `			/* Pass by reference,record that */` |
|    3483 |  5826 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3483 |  5827 | `			pIn++;` |
|    1739 |  5828 | `		}` |
|  136495 |  5829 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5830 | `			/* Variadic parameter: ...$args */` |
|      47 |  5831 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      47 |  5832 | `			pIn++;` |
|      21 |  5833 | `		}` |
|  136495 |  5834 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5835 | `			/* Invalid argument */` |
|     ! 0 |  5836 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5837 | `			return rc;` |
|       - |  5838 | `		}` |
|  136495 |  5839 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5840 | `		/* Copy argument name */` |
|  136495 |  5841 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  136495 |  5842 | `		if( zDup == 0 ){` |
|     ! 0 |  5843 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5844 | `			return SXERR_ABORT;` |
|       - |  5845 | `		}` |
|  136495 |  5846 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  136495 |  5847 | `		pIn++;` |
|  136495 |  5848 | `		if( pIn < pEnd ){` |
|   76667 |  5849 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5850 | `				SyToken *pDefend;` |
|   65591 |  5851 | `				sxi32 iNest = 0;` |
|   65591 |  5852 | `				pIn++; /* Jump the equal sign */` |
|   65591 |  5853 | `				pDefend = pIn;` |
|       - |  5854 | `				/* Process the default value associated with this argument */` |
|  138075 |  5855 | `				while( pDefend < pEnd ){` |
|  106997 |  5856 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   34513 |  5857 | `						break;` |
|       - |  5858 | `					}` |
|   72489 |  5859 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5860 | `						/* Increment nesting level */` |
|    3455 |  5861 | `						iNest++;` |
|   70764 |  5862 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5863 | `						/* Decrement nesting level */` |
|    3455 |  5864 | `						iNest--;` |
|    1725 |  5865 | `					}` |
|   72489 |  5866 | `					pDefend++;` |
|       5 |  5867 | `				}` |
|   65591 |  5868 | `				if( pIn >= pDefend ){` |
|       3 |  5869 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5870 | `					return rc;` |
|       - |  5871 | `				}` |
|       - |  5872 | `				/* Process default value */` |
|   65589 |  5873 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   65589 |  5874 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5875 | `					return rc;` |
|       - |  5876 | `				}` |
|       - |  5877 | `				/* Point beyond the default value */` |
|   65589 |  5878 | `				pIn = pDefend;` |
|   32792 |  5879 | `			}` |
|   76665 |  5880 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5881 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5882 | `				return rc;` |
|       - |  5883 | `			}` |
|   76665 |  5884 | `			pIn++; /* Jump the trailing comma */` |
|   38330 |  5885 | `		}` |
|       - |  5886 | `		/* Append argument signature */` |
|  136493 |  5887 | `		if( sArg.nType > 0 ){` |
|   72643 |  5888 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5889 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   10377 |  5890 | `				int marker = 'o';` |
|   10377 |  5891 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   10377 |  5892 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    5191 |  5893 | `			}else{` |
|       - |  5894 | `				int c;` |
|   62271 |  5895 | `				c = 'n'; /* cc warning */` |
|       - |  5896 | `				/* Type leading character */` |
|   62271 |  5897 | `				switch(sArg.nType){` |
|       3 |  5898 | `				case MEMOBJ_HASHMAP:` |
|       - |  5899 | `					/* Hashmap aka 'array' */` |
|       7 |  5900 | `					c = 'h';` |
|       7 |  5901 | `					break;` |
|    8671 |  5902 | `				case MEMOBJ_INT:` |
|       - |  5903 | `					/* Integer */` |
|   17347 |  5904 | `					c = 'i';` |
|   17347 |  5905 | `					break;` |
|       1 |  5906 | `				case MEMOBJ_BOOL:` |
|       - |  5907 | `					/* Bool */` |
|       3 |  5908 | `					c = 'b';` |
|       3 |  5909 | `					break;` |
|       2 |  5910 | `				case MEMOBJ_REAL:` |
|       - |  5911 | `					/* Float */` |
|       5 |  5912 | `					c = 'f';` |
|       5 |  5913 | `					break;` |
|   22448 |  5914 | `				case MEMOBJ_STRING:` |
|       - |  5915 | `					/* String */` |
|   44901 |  5916 | `					c = 's';` |
|   44901 |  5917 | `					break;` |
|       7 |  5918 | `				case MEMOBJ_OBJ:` |
|       - |  5919 | `					/* Object */` |
|      16 |  5920 | `					c = 'o';` |
|      14 |  5921 | `					break;` |
|       1 |  5922 | `				default:` |
|       2 |  5923 | `					break;` |
|       - |  5924 | `				}` |
|   62271 |  5925 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5926 | `			}` |
|   36324 |  5927 | `		}else{` |
|       - |  5928 | `			/* No type is associated with this parameter which mean` |
|       - |  5929 | `			 * that this function is not condidate for overloading.` |
|       - |  5930 | `			 */` |
|   63855 |  5931 | `			SyBlobRelease(&sSig);` |
|       - |  5932 | `		}` |
|       - |  5933 | `		/* Save in the argument set */` |
|  136493 |  5934 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5935 | `	}` |
|   90915 |  5936 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5937 | `		/* Save function signature */` |
|   45011 |  5938 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   22503 |  5939 | `	}` |
|   90915 |  5940 | `	return SXRET_OK;` |
|   45466 |  5941 |  |
|       - |  5942 | `/*` |
|       - |  5943 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5944 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5945 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5946 | ` */` |
|  215994 |  5947 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5948 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5949 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5950 | `	)` |
|       5 |  5951 |  |
|       - |  5952 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5953 | `	GenBlock *pBlock;` |
|       - |  5954 | `	sxu32 nGotoOfft;` |
|       - |  5955 | `	sxi32 rc;` |
|       - |  5956 | `	/* Attach the new function */` |
|  215999 |  5957 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  215999 |  5958 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5959 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5960 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5961 | `		return SXERR_ABORT;` |
|       - |  5962 | `	}` |
|  215999 |  5963 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5964 | `	/* Swap bytecode containers */` |
|  215999 |  5965 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  215999 |  5966 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5967 | `	/* Emit constructor property promotion prologue:` |
|       - |  5968 | `	 *   $this->NAME = $NAME;` |
|       - |  5969 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5970 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5971 | `	{` |
|  215999 |  5972 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5973 | `		sxu32 i;` |
|  324761 |  5974 | `		for( i = 0; i < nArg; i++ ){` |
|  108767 |  5975 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5976 | `			char *zSrc;` |
|       - |  5977 | `			sxu32 nSrc,nName;` |
|       - |  5978 | `			SySet sToken;` |
|       - |  5979 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5980 | `			sxi32 rcPromote;` |
|  108767 |  5981 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  108717 |  5982 | `				continue;` |
|       - |  5983 | `			}` |
|       - |  5984 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5985 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5986 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5987 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5988 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      55 |  5989 | `			nName = SyStringLength(&pArg->sName);` |
|      55 |  5990 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      55 |  5991 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      55 |  5992 | `			if( zSrc == 0 ){` |
|     ! 0 |  5993 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5994 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5995 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5996 | `				return SXERR_ABORT;` |
|       - |  5997 | `			}` |
|       - |  5998 | `			{` |
|      55 |  5999 | `				char *z = zSrc;` |
|      55 |  6000 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      55 |  6001 | `				z += sizeof("$this->")-1;` |
|      55 |  6002 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      55 |  6003 | `				z += nName;` |
|      55 |  6004 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      55 |  6005 | `				z += sizeof(" = $")-1;` |
|      55 |  6006 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      55 |  6007 | `				z += nName;` |
|      55 |  6008 | `				*z = 0;` |
|       - |  6009 | `			}` |
|      55 |  6010 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      55 |  6011 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      55 |  6012 | `			pTmpIn = pGen->pIn;` |
|      55 |  6013 | `			pTmpEnd = pGen->pEnd;` |
|      55 |  6014 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      55 |  6015 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      55 |  6016 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      55 |  6017 | `			pGen->pIn = pTmpIn;` |
|      55 |  6018 | `			pGen->pEnd = pTmpEnd;` |
|      55 |  6019 | `			SySetRelease(&sToken);` |
|      55 |  6020 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6021 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6022 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6023 | `				return SXERR_ABORT;` |
|       - |  6024 | `			}` |
|       - |  6025 | `			/* Discard the assignment result — this is a statement expression. */` |
|      55 |  6026 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      30 |  6027 | `		}` |
|       - |  6028 | `	}` |
|       - |  6029 | `	/* Compile the body */` |
|  215999 |  6030 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6031 | `	/* Fix exception jumps now the destination is resolved */` |
|  215999 |  6032 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6033 | `	/* Emit the final return if not yet done */` |
|  215999 |  6034 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6035 | `	/* Fix gotos jumps now the destination is resolved */` |
|  215999 |  6036 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6037 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6038 | `	}` |
|  215999 |  6039 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6040 | `	/* Restore the default container */` |
|  215999 |  6041 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6042 | `	/* Leave function block */` |
|  215999 |  6043 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  215999 |  6044 | `	if( rc == SXERR_ABORT ){` |
|       - |  6045 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6046 | `		return SXERR_ABORT;` |
|       - |  6047 | `	}` |
|       - |  6048 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6049 | `	{` |
|  215999 |  6050 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6051 | `		sxu32 i;` |
| 4218459 |  6052 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4002545 |  6053 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      85 |  6054 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      85 |  6055 | `				break;` |
|       - |  6056 | `			}` |
| 2001235 |  6057 | `		}` |
|       - |  6058 | `	}` |
|       - |  6059 | `	/* All done, function body compiled */` |
|  215999 |  6060 | `	return SXRET_OK;` |
|  108002 |  6061 |  |
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
|     382 |  6082 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6083 |  |
|       - |  6084 | `	sxu32 i;` |
|    1059 |  6085 | `	for( i = 0; i < n; i++ ){` |
|     907 |  6086 | `		int a = zA[i], b = zB[i];` |
|     907 |  6087 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     907 |  6088 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     907 |  6089 | `		if( a != b ) return a - b;` |
|     341 |  6090 | `	}` |
|     157 |  6091 | `	return 0;` |
|     196 |  6092 |  |
|       - |  6093 | `/*` |
|       - |  6094 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6095 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6096 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6097 | ` */` |
|       - |  6098 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6099 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6100 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6101 |  |
|       - |  6102 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  6103 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  6104 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  6105 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  6106 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  6107 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  6108 |  |
|       - |  6109 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6110 | `struct PhlTypeAtom {` |
|       - |  6111 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6112 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6113 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6114 | `	sxu32 nCanon;` |
|       - |  6115 | `};` |
|       - |  6116 |  |
|       - |  6117 | `/*` |
|       - |  6118 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6119 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6120 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6121 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6122 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6123 | ` * already be consumed by the caller.` |
|       - |  6124 | ` */` |
|   73376 |  6125 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6126 |  |
|   73381 |  6127 | `	SyToken *pIn = pGen->pIn;` |
|   73381 |  6128 | `	SyZero(pOut, sizeof(*pOut));` |
|   73381 |  6129 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   73381 |  6130 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6131 | `		return SXERR_SYNTAX;` |
|       - |  6132 | `	}` |
|       - |  6133 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   73381 |  6134 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6135 | `		pIn++;` |
|       8 |  6136 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6137 | `			return SXERR_SYNTAX;` |
|       - |  6138 | `		}` |
|       3 |  6139 | `	}` |
|   73381 |  6140 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6141 | `		return SXERR_SYNTAX;` |
|       - |  6142 | `	}` |
|   73381 |  6143 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   62749 |  6144 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   62749 |  6145 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6146 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   62735 |  6147 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      69 |  6148 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   62689 |  6149 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   17569 |  6150 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   53875 |  6151 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   45035 |  6152 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   22578 |  6153 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      32 |  6154 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      48 |  6155 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6156 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      21 |  6157 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  6158 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  6159 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  6160 | `			pOut->sClass = pIn->sData;` |
|       4 |  6161 | `		}else{` |
|       3 |  6162 | `			return SXERR_SYNTAX;` |
|       - |  6163 | `		}` |
|   62747 |  6164 | `		pIn++;` |
|   31376 |  6165 | `	}else{` |
|       - |  6166 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6167 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   10637 |  6168 | `		SyString *pT = &pIn->sData;` |
|   10637 |  6169 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      18 |  6170 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      18 |  6171 | `			pIn++;` |
|   10629 |  6172 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     133 |  6173 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     133 |  6174 | `			pIn++;` |
|   10557 |  6175 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6176 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6177 | `			pIn++;` |
|       2 |  6178 | `		}else{` |
|       - |  6179 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   10491 |  6180 | `			SyToken *pFirst = pIn;` |
|   10491 |  6181 | `			SyToken *pLast = pIn;` |
|   10491 |  6182 | `			pOut->nType = SXU32_HIGH;` |
|   10491 |  6183 | `			pOut->sClass = pIn->sData;` |
|   10491 |  6184 | `			pIn++;` |
|   15732 |  6185 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   10494 |  6186 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6187 | `				pLast = &pIn[1];` |
|       3 |  6188 | `				pIn += 2;` |
|       1 |  6189 | `			}` |
|   10491 |  6190 | `			if( pLast != pFirst ){` |
|       3 |  6191 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6192 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6193 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6194 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6195 | `			}` |
|       - |  6196 | `		}` |
|       - |  6197 | `	}` |
|   73379 |  6198 | `	pGen->pIn = pIn;` |
|   73379 |  6199 | `	return SXRET_OK;` |
|   36693 |  6200 |  |
|       - |  6201 |  |
|       - |  6202 | `/*` |
|       - |  6203 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6204 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6205 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6206 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6207 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6208 | ` */` |
|   73272 |  6209 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6210 |  |
|       - |  6211 | `	int i;` |
|   73277 |  6212 | `	int nNonNull = 0;` |
|  146637 |  6213 | `	for( i = 0; i < nAtoms; i++ ){` |
|   73365 |  6214 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   73349 |  6215 | `			nNonNull++;` |
|   36672 |  6216 | `		}` |
|   36685 |  6217 | `	}` |
|   73277 |  6218 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6219 | `		/* Shorthand: ?T */` |
|      65 |  6220 | `		for( i = 0; i < nAtoms; i++ ){` |
|      65 |  6221 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      65 |  6222 | `			SyBlobAppend(pBlob, "?", 1);` |
|      65 |  6223 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      15 |  6224 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       9 |  6225 | `			}else{` |
|      53 |  6226 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6227 | `			}` |
|      65 |  6228 | `			return;` |
|     ! 0 |  6229 | `		}` |
|     ! 0 |  6230 | `	}` |
|       - |  6231 | `	{` |
|   73215 |  6232 | `		int bFirst = 1;` |
|       - |  6233 | `		/* 1) Classes in declaration order */` |
|  146507 |  6234 | `		for( i = 0; i < nAtoms; i++ ){` |
|   73297 |  6235 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   10483 |  6236 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   10483 |  6237 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   10483 |  6238 | `				bFirst = 0;` |
|    5239 |  6239 | `			}` |
|   36651 |  6240 | `		}` |
|       - |  6241 | `		/* 2) Built-ins in canonical order */` |
|       - |  6242 | `		{` |
|       - |  6243 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6244 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6245 | `			int k;` |
|  512475 |  6246 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  816243 |  6247 | `				for( i = 0; i < nAtoms; i++ ){` |
|  439661 |  6248 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   62683 |  6249 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   62683 |  6250 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   62683 |  6251 | `						bFirst = 0;` |
|   62683 |  6252 | `						break;` |
|       - |  6253 | `					}` |
|  188494 |  6254 | `				}` |
|  219635 |  6255 | `			}` |
|       - |  6256 | `		}` |
|       - |  6257 | `		/* 3) null suffix */` |
|   73215 |  6258 | `		if( bNullable ){` |
|      12 |  6259 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      12 |  6260 | `			SyBlobAppend(pBlob, "null", 4);` |
|       5 |  6261 | `		}` |
|       - |  6262 | `	}` |
|   36641 |  6263 |  |
|       - |  6264 |  |
|       - |  6265 | `/*` |
|       - |  6266 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6267 | ` *` |
|       - |  6268 | ` * Outputs:` |
|       - |  6269 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6270 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6271 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6272 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6273 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6274 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6275 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6276 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6277 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6278 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6279 | ` *` |
|       - |  6280 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6281 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6282 | ` */` |
|   73282 |  6283 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6284 | `	ph7_gen_state *pGen,` |
|       - |  6285 | `	sxu32 *pnType,` |
|       - |  6286 | `	SyString *pClass,` |
|       - |  6287 | `	SySet *pAlts,` |
|       - |  6288 | `	sxi32 *piTypeFlags,` |
|       - |  6289 | `	SyString *pTypeText,` |
|       - |  6290 | `	int iNullableFlag,` |
|       - |  6291 | `	int iUnionFlag,` |
|       - |  6292 | `	int bAllowVoid,` |
|       - |  6293 | `	sxu32 nLine` |
|       5 |  6294 | `){` |
|       - |  6295 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   73287 |  6296 | `	int nAtoms = 0;` |
|   73287 |  6297 | `	int bShortNullable = 0;` |
|   73287 |  6298 | `	int bExplicitNull = 0;` |
|       - |  6299 | `	sxi32 rc;` |
|   73287 |  6300 | `	*pnType = 0;` |
|   73287 |  6301 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   73287 |  6302 | `	*piTypeFlags = 0;` |
|   73287 |  6303 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6304 |  |
|   73287 |  6305 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6306 | `		return SXRET_OK;` |
|       - |  6307 | `	}` |
|       - |  6308 | ``	/* Optional `?` shorthand prefix */`` |
|   73282 |  6309 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      63 |  6310 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      62 |  6311 | `		bShortNullable = 1;` |
|      62 |  6312 | `		pGen->pIn++;` |
|      62 |  6313 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6314 | `			return SXERR_SYNTAX;` |
|       - |  6315 | `		}` |
|      29 |  6316 | `	}` |
|       - |  6317 | `	/* First atom is mandatory */` |
|   73287 |  6318 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   73287 |  6319 | `	if( rc != SXRET_OK ){` |
|       3 |  6320 | `		return rc;` |
|       - |  6321 | `	}` |
|   73285 |  6322 | `	nAtoms = 1;` |
|       - |  6323 | ``	/* Subsequent atoms separated by `\|` */`` |
|  110063 |  6324 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   73428 |  6325 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     101 |  6326 | `		if( bShortNullable ){` |
|       - |  6327 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6328 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6329 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6330 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6331 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6332 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6333 | `		}` |
|      99 |  6334 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6335 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6336 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6337 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6338 | `		}` |
|      99 |  6339 | ``		pGen->pIn++; /* skip `\|` */`` |
|      99 |  6340 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      99 |  6341 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6342 | `			return rc;` |
|       - |  6343 | `		}` |
|      99 |  6344 | `		nAtoms++;` |
|       5 |  6345 | `	}` |
|       - |  6346 | `	/* Validation pass.` |
|       - |  6347 | `	 *` |
|       - |  6348 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6349 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6350 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6351 | `	 */` |
|       - |  6352 | `	{` |
|       - |  6353 | `		int i, j;` |
|   73283 |  6354 | `		int bHasNonNull = 0;` |
|  146649 |  6355 | `		for( i = 0; i < nAtoms; i++ ){` |
|   73377 |  6356 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     133 |  6357 | `				if( nAtoms > 1 ){` |
|       3 |  6358 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6359 | `						"Void can only be used as a standalone type");` |
|       3 |  6360 | `					return SXERR_SYNTAX;` |
|       - |  6361 | `				}` |
|     131 |  6362 | `				if( !bAllowVoid ){` |
|     ! 0 |  6363 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6364 | `						"void cannot be used here");` |
|     ! 0 |  6365 | `					return SXERR_SYNTAX;` |
|       - |  6366 | `				}` |
|     131 |  6367 | `				if( bShortNullable ){` |
|     ! 0 |  6368 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6369 | `						"Void type cannot be nullable");` |
|     ! 0 |  6370 | `					return SXERR_SYNTAX;` |
|       - |  6371 | `				}` |
|      63 |  6372 | `			}` |
|   73375 |  6373 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6374 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6375 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6376 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6377 | `				 * does not return), and folding them would mislead any` |
|       - |  6378 | `				 * future return-enforcement work. */` |
|       3 |  6379 | `				if( nAtoms > 1 ){` |
|       3 |  6380 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6381 | `						"never can only be used as a standalone type");` |
|       3 |  6382 | `					return SXERR_SYNTAX;` |
|       - |  6383 | `				}` |
|     ! 0 |  6384 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6385 | `					"never type is not yet implemented");` |
|     ! 0 |  6386 | `				return SXERR_SYNTAX;` |
|       - |  6387 | `			}` |
|   73373 |  6388 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      18 |  6389 | `				bExplicitNull = 1;` |
|      10 |  6390 | `			}else{` |
|   73357 |  6391 | `				bHasNonNull = 1;` |
|       - |  6392 | `			}` |
|       - |  6393 | `			/* Duplicate detection */` |
|   73497 |  6394 | `			for( j = 0; j < i; j++ ){` |
|     131 |  6395 | `				int bDup = 0;` |
|     131 |  6396 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      17 |  6397 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6398 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      15 |  6399 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6400 | `								aAtoms[j].sClass.zString,` |
|      12 |  6401 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6402 | `							bDup = 1;` |
|     ! 0 |  6403 | `						}` |
|       9 |  6404 | `					}else{` |
|       3 |  6405 | `						bDup = 1;` |
|       - |  6406 | `					}` |
|       7 |  6407 | `				}` |
|     131 |  6408 | `				if( bDup ){` |
|       - |  6409 | `					const char *zName;` |
|       - |  6410 | `					sxu32 nName;` |
|       3 |  6411 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6412 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6413 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6414 | `					}else{` |
|       3 |  6415 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6416 | `						nName = aAtoms[i].nCanon;` |
|       - |  6417 | `					}` |
|       4 |  6418 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6419 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6420 | `					return SXERR_SYNTAX;` |
|       - |  6421 | `				}` |
|      67 |  6422 | `			}` |
|   36688 |  6423 | `		}` |
|   73277 |  6424 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6425 | `			if( bShortNullable ){` |
|       - |  6426 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6427 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6428 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6429 | `				return SXERR_SYNTAX;` |
|       - |  6430 | `			}` |
|       - |  6431 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6432 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6433 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6434 | `			 * atom, so set it here. */` |
|       7 |  6435 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6436 | `		}` |
|       - |  6437 | `	}` |
|       - |  6438 | `	/* Compute nullability flag */` |
|   73277 |  6439 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      76 |  6440 | `		*piTypeFlags \|= iNullableFlag;` |
|      36 |  6441 | `	}` |
|       - |  6442 | `	/* Build canonical type text */` |
|   73277 |  6443 | `	if( pTypeText ){` |
|       - |  6444 | `		SyBlob sBlob;` |
|   73277 |  6445 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  109885 |  6446 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   36636 |  6447 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   73277 |  6448 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  109724 |  6449 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   73146 |  6450 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   73151 |  6451 | `			if( zDup ){` |
|   73151 |  6452 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   36573 |  6453 | `			}` |
|   36573 |  6454 | `		}` |
|   73277 |  6455 | `		SyBlobRelease(&sBlob);` |
|   36636 |  6456 | `	}` |
|       - |  6457 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6458 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6459 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6460 | `	{` |
|   73277 |  6461 | `		int nNonNull = 0;` |
|   73277 |  6462 | `		int iNonNullIdx = -1;` |
|       - |  6463 | `		int i;` |
|  146637 |  6464 | `		for( i = 0; i < nAtoms; i++ ){` |
|   73365 |  6465 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   73349 |  6466 | `				nNonNull++;` |
|   73349 |  6467 | `				iNonNullIdx = i;` |
|   36672 |  6468 | `			}` |
|   36685 |  6469 | `		}` |
|   73277 |  6470 | `		if( nNonNull <= 1 ){` |
|       - |  6471 | `			/* Fast path: store as single type. */` |
|   73215 |  6472 | `			if( iNonNullIdx >= 0 ){` |
|   73209 |  6473 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   73209 |  6474 | `				if( pA->nType == SXU32_HIGH ){` |
|   15698 |  6475 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    5231 |  6476 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   10467 |  6477 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   10467 |  6478 | `					*pnType = SXU32_HIGH;` |
|   10467 |  6479 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   67978 |  6480 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     131 |  6481 | `					*pnType = MEMOBJ_VOID;` |
|      68 |  6482 | `				}else{` |
|       - |  6483 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6484 | `					 * pass above rejects it as not-yet-implemented. */` |
|   62621 |  6485 | `					*pnType = pA->nType;` |
|       - |  6486 | `				}` |
|   36602 |  6487 | `			}` |
|   36610 |  6488 | `		}else{` |
|       - |  6489 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      67 |  6490 | `			*piTypeFlags \|= iUnionFlag;` |
|     211 |  6491 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6492 | `				ph7_type_alt sAlt;` |
|     149 |  6493 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     145 |  6494 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     145 |  6495 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      46 |  6496 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      14 |  6497 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      32 |  6498 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      32 |  6499 | `					sAlt.nType = SXU32_HIGH;` |
|      32 |  6500 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      18 |  6501 | `				}else{` |
|     117 |  6502 | `					sAlt.nType = aAtoms[i].nType;` |
|     117 |  6503 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6504 | `				}` |
|     145 |  6505 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      75 |  6506 | `			}` |
|       - |  6507 | `		}` |
|       - |  6508 | `	}` |
|   73277 |  6509 | `	return SXRET_OK;` |
|   36646 |  6510 |  |
|       - |  6511 |  |
|       - |  6512 | `/*` |
|       - |  6513 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6514 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6515 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6516 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6517 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6518 | `` *          and union types `: T\|U`.`` |
|       - |  6519 | ` */` |
|  305878 |  6520 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6521 |  |
|  305883 |  6522 | `	sxi32 iFlags = 0;` |
|       - |  6523 | `	sxi32 rc;` |
|       - |  6524 | `	sxu32 nLine;` |
|  305883 |  6525 | `	pFunc->nReturnType = 0;` |
|  305883 |  6526 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  305883 |  6527 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  305883 |  6528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  305489 |  6529 | `		return SXRET_OK;` |
|       - |  6530 | `	}` |
|     399 |  6531 | `	pGen->pIn++; /* Skip ':' */` |
|     399 |  6532 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6533 | `		return SXRET_OK;` |
|       - |  6534 | `	}` |
|     399 |  6535 | `	nLine = pGen->pIn->nLine;` |
|     399 |  6536 | `	rc = GenStateParseUnionTypeDecl(` |
|     197 |  6537 | `		pGen,` |
|     197 |  6538 | `		&pFunc->nReturnType,` |
|     197 |  6539 | `		&pFunc->sReturnClass,` |
|     197 |  6540 | `		&pFunc->aReturnUnion,` |
|       - |  6541 | `		&iFlags,` |
|     197 |  6542 | `		&pFunc->sReturnTypeName,` |
|       - |  6543 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6544 | `		/* iUnionFlag */ 0,` |
|       - |  6545 | `		/* bAllowVoid */ 1,` |
|     197 |  6546 | `		nLine);` |
|     197 |  6547 | `	(void)iFlags;` |
|     399 |  6548 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6549 | `		return SXERR_ABORT;` |
|       - |  6550 | `	}` |
|     399 |  6551 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6552 | `		/* Error already reported */` |
|     ! 0 |  6553 | `		return SXERR_SYNTAX;` |
|       - |  6554 | `	}` |
|     399 |  6555 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6556 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6557 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6558 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6559 | `				&pGen->pIn->sData);` |
|       3 |  6560 | `		}else{` |
|     ! 0 |  6561 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6562 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6563 | `		}` |
|       5 |  6564 | `		return SXERR_SYNTAX;` |
|       - |  6565 | `	}` |
|     395 |  6566 | `	return SXRET_OK;` |
|  152944 |  6567 |  |
|       - |  6568 |  |
|   46016 |  6569 | `static sxi32 GenStateCompileFunc(` |
|       - |  6570 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6571 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6572 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6573 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6574 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6575 | `	)` |
|       5 |  6576 |  |
|       - |  6577 | `	ph7_vm_func *pFunc;` |
|       - |  6578 | `	SyToken *pEnd;` |
|       - |  6579 | `	sxu32 nLine;` |
|       - |  6580 | `	char *zName;` |
|       - |  6581 | `	sxi32 rc;` |
|       - |  6582 | `	/* Extract line number */` |
|   46021 |  6583 | `	nLine = pGen->pIn->nLine;` |
|       - |  6584 | `	/* Jump the left parenthesis '(' */` |
|   46021 |  6585 | `	pGen->pIn++;` |
|       - |  6586 | `	/* Delimit the function signature */` |
|   46021 |  6587 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   46021 |  6588 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6589 | `		/* Syntax error */` |
|       9 |  6590 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       9 |  6591 | `		if( rc == SXERR_ABORT ){` |
|       - |  6592 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6593 | `			return SXERR_ABORT;` |
|       - |  6594 | `		}` |
|       9 |  6595 | `		pGen->pIn = pGen->pEnd;` |
|       9 |  6596 | `		return SXRET_OK;` |
|       - |  6597 | `	}` |
|       - |  6598 | `	/* Create the function state */` |
|   46015 |  6599 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   46015 |  6600 | `	if( pFunc == 0 ){` |
|     ! 0 |  6601 | `		goto OutOfMem;` |
|       - |  6602 | `	}` |
|       - |  6603 | `	/* Build the function name, prepending namespace if active */` |
|   46022 |  6604 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6605 | `		SyBlob sFQN;` |
|       - |  6606 | `		sxu32 nLen;` |
|      16 |  6607 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6608 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6609 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6610 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6611 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6612 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6613 | `		SyBlobRelease(&sFQN);` |
|      16 |  6614 | `		if( zName == 0 ){` |
|     ! 0 |  6615 | `			goto OutOfMem;` |
|       - |  6616 | `		}` |
|      16 |  6617 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6618 | `	}else{` |
|   46001 |  6619 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   46001 |  6620 | `		if( zName == 0 ){` |
|     ! 0 |  6621 | `			goto OutOfMem;` |
|       - |  6622 | `		}` |
|   46001 |  6623 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6624 | `	}` |
|   46015 |  6625 | `	if( pGen->pIn < pEnd ){` |
|       - |  6626 | `		/* Collect function arguments */` |
|   31819 |  6627 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   31819 |  6628 | `		if( rc == SXERR_ABORT ){` |
|       - |  6629 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6630 | `			return SXERR_ABORT;` |
|       - |  6631 | `		}` |
|   15907 |  6632 | `	}` |
|       - |  6633 | `	/* Point past ')' and parse optional return type ': type' */` |
|   46015 |  6634 | `	pGen->pIn = &pEnd[1];` |
|       - |  6635 | `	{` |
|   46015 |  6636 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   46015 |  6637 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6638 | `			return SXERR_ABORT;` |
|   46015 |  6639 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6640 | `			return SXERR_SYNTAX;` |
|       - |  6641 | `		}` |
|       - |  6642 | `	}` |
|   46011 |  6643 | `	if( bHandleClosure ){` |
|       - |  6644 | `		ph7_vm_func_closure_env sEnv;` |
|     259 |  6645 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     254 |  6646 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     140 |  6647 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      21 |  6648 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6649 | `				/* Closure,record environment variable */` |
|      21 |  6650 | `				pGen->pIn++;` |
|      21 |  6651 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6652 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6653 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6654 | `						return SXERR_ABORT;` |
|       - |  6655 | `					}` |
|     ! 0 |  6656 | `				}` |
|      21 |  6657 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6658 | `				/* Compile until we hit the first closing parenthesis */` |
|      41 |  6659 | `				while( pGen->pIn < pGen->pEnd ){` |
|      41 |  6660 | `					int iFlagsLocal = 0;` |
|      41 |  6661 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      21 |  6662 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      21 |  6663 | `						break;` |
|       - |  6664 | `					}` |
|      25 |  6665 | `					nLineLocal = pGen->pIn->nLine;` |
|      25 |  6666 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6667 | `						/* Pass by reference,record that */` |
|     ! 0 |  6668 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6669 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6670 | `							);` |
|     ! 0 |  6671 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6672 | `						pGen->pIn++;` |
|     ! 0 |  6673 | `					}` |
|      20 |  6674 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      25 |  6675 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6676 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6677 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6678 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6679 | `								return SXERR_ABORT;` |
|       - |  6680 | `							}` |
|       - |  6681 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6682 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6683 | `								pGen->pIn++;` |
|     ! 0 |  6684 | `							}` |
|     ! 0 |  6685 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6686 | `								pGen->pIn++;` |
|     ! 0 |  6687 | `							}` |
|     ! 0 |  6688 | `							break;` |
|       - |  6689 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6690 | `					}else{` |
|       - |  6691 | `						SyString *pNameLocal;` |
|       - |  6692 | `						char *zDup;` |
|       - |  6693 | `						/* Duplicate variable name */` |
|      25 |  6694 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      25 |  6695 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      25 |  6696 | `						if( zDup ){` |
|       - |  6697 | `							/* Zero the structure */` |
|      25 |  6698 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      25 |  6699 | `							sEnv.iFlags = iFlagsLocal;` |
|      25 |  6700 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      25 |  6701 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      25 |  6702 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6703 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6704 | `									got_this = 1;` |
|     ! 0 |  6705 | `							}` |
|       - |  6706 | `							/* Save imported variable */` |
|      25 |  6707 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      15 |  6708 | `						}else{` |
|     ! 0 |  6709 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6710 | `							 return SXERR_ABORT;` |
|       - |  6711 | `						}` |
|       - |  6712 | `					}` |
|      25 |  6713 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      31 |  6714 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6715 | `						/* Ignore trailing commas */` |
|       7 |  6716 | `						pGen->pIn++;` |
|       1 |  6717 | `					}` |
|       5 |  6718 | `				}` |
|      21 |  6719 | `				if( !got_this ){` |
|       - |  6720 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6721 | `					 * available to the closure environment.` |
|       - |  6722 | `					 */` |
|      21 |  6723 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      21 |  6724 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      21 |  6725 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      21 |  6726 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      21 |  6727 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6728 | `				}` |
|      21 |  6729 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6730 | `					/* Mark as closure */` |
|      21 |  6731 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6732 | `				}` |
|       8 |  6733 | `		}` |
|     127 |  6734 | `	}` |
|       - |  6735 | `	/* Compile the body */` |
|   46011 |  6736 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   46011 |  6737 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6738 | `		return SXERR_ABORT;` |
|       - |  6739 | `	}` |
|   46011 |  6740 | `	if( ppFunc ){` |
|     259 |  6741 | `		*ppFunc = pFunc;` |
|     127 |  6742 | `	}` |
|   46011 |  6743 | `	rc = SXRET_OK;` |
|   46011 |  6744 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6745 | `		/* Finally register the function */` |
|   45995 |  6746 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   22995 |  6747 | `	}` |
|   46011 |  6748 | `	if( rc == SXRET_OK ){` |
|   46011 |  6749 | `		return SXRET_OK;` |
|       - |  6750 | `	}` |
|       - |  6751 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6752 | `OutOfMem:` |
|       - |  6753 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6754 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6755 | `	 */` |
|     ! 0 |  6756 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6757 | `	return SXERR_ABORT;` |
|   23013 |  6758 |  |
|       - |  6759 | `/*` |
|       - |  6760 | ` * Compile a standard PHP function.` |
|       - |  6761 | ` *  Refer to the block-comment above for more information.` |
|       - |  6762 | ` */` |
|   45770 |  6763 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6764 |  |
|       - |  6765 | `	SyString *pName;` |
|       - |  6766 | `	sxi32 iFlags;` |
|       - |  6767 | `	sxu32 nLine;` |
|       - |  6768 | `	sxi32 rc;` |
|       - |  6769 |  |
|   45775 |  6770 | `	nLine = pGen->pIn->nLine;` |
|   45775 |  6771 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   45775 |  6772 | `	iFlags = 0;` |
|   45775 |  6773 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6774 | `		/* Return by reference,remember that */` |
|       7 |  6775 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6776 | `		/* Jump the '&' token */` |
|       7 |  6777 | `		pGen->pIn++;` |
|       3 |  6778 | `	}` |
|   45775 |  6779 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6780 | `		/* Invalid function name */` |
|       8 |  6781 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       8 |  6782 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6783 | `			return SXERR_ABORT;` |
|       - |  6784 | `		}` |
|       - |  6785 | `		/* Sychronize with the next semi-colon or braces*/` |
|      22 |  6786 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      16 |  6787 | `			pGen->pIn++;` |
|       2 |  6788 | `		}` |
|       8 |  6789 | `		return SXRET_OK;` |
|       - |  6790 | `	}` |
|   45769 |  6791 | `	pName = &pGen->pIn->sData;` |
|   45769 |  6792 | `	nLine = pGen->pIn->nLine;` |
|       - |  6793 | `	/* Jump the function name */` |
|   45769 |  6794 | `	pGen->pIn++;` |
|   45769 |  6795 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6796 | `		/* Syntax error */` |
|       3 |  6797 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6798 | `		if( rc == SXERR_ABORT ){` |
|       - |  6799 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6800 | `			return SXERR_ABORT;` |
|       - |  6801 | `		}` |
|       - |  6802 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6803 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6804 | `			pGen->pIn++;` |
|     ! 0 |  6805 | `		}` |
|       3 |  6806 | `		return SXRET_OK;` |
|       - |  6807 | `	}` |
|       - |  6808 | `	/* Compile function body */` |
|   45767 |  6809 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   45767 |  6810 | `	return rc;` |
|   22890 |  6811 |  |
|       - |  6812 | `/*` |
|       - |  6813 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6814 | ` * According to the PHP language reference manual` |
|       - |  6815 | ` *  Visibility:` |
|       - |  6816 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6817 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6818 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6819 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6820 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6821 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6822 | ` */` |
|  326020 |  6823 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  6824 |  |
|  326025 |  6825 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   10461 |  6826 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  315569 |  6827 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   44907 |  6828 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6829 | `	}` |
|       - |  6830 | `	/* Assume public by default */` |
|  270667 |  6831 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  163015 |  6832 |  |
|       - |  6833 | `/*` |
|       - |  6834 | ` * Compile a class constant.` |
|       - |  6835 | ` * According to the PHP language reference manual` |
|       - |  6836 | ` *  Class Constants` |
|       - |  6837 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6838 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6839 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6840 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6841 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6842 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6843 | ` * Symisc eXtension.` |
|       - |  6844 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6845 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6846 | ` *  Example:` |
|       - |  6847 | ` *   class Test{` |
|       - |  6848 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6849 | ` *   };` |
|       - |  6850 | ` *   var_dump(TEST::MyConst);` |
|       - |  6851 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6852 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6853 | ` */` |
|       - |  6854 | `/*` |
|       - |  6855 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  6856 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  6857 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  6858 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  6859 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  6860 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  6861 | ` */` |
|      78 |  6862 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  6863 |  |
|       - |  6864 | `	SyToken *p0, *p1;` |
|      83 |  6865 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6866 | `		return 0;` |
|       - |  6867 | `	}` |
|      83 |  6868 | `	p0 = pGen->pIn;` |
|       - |  6869 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      83 |  6870 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  6871 | `		return 1;` |
|       - |  6872 | `	}` |
|      83 |  6873 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  6874 | `		return 1;` |
|       - |  6875 | `	}` |
|       - |  6876 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  6877 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  6878 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      79 |  6879 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      79 |  6880 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      79 |  6881 | `		if( p1 ){` |
|      79 |  6882 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  6883 | `				return 1;` |
|       - |  6884 | `			}` |
|      59 |  6885 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  6886 | `				return 1;` |
|       - |  6887 | `			}` |
|      25 |  6888 | `		}` |
|      25 |  6889 | `	}` |
|      55 |  6890 | `	return 0;` |
|      44 |  6891 |  |
|       - |  6892 | `/*` |
|       - |  6893 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  6894 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  6895 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  6896 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  6897 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  6898 | ` * share the same backing.` |
|       - |  6899 | ` */` |
|     194 |  6900 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  6901 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  6902 |  |
|     199 |  6903 | `	pAttr->nType = nType;` |
|     199 |  6904 | `	pAttr->sClass = *pClass;` |
|     199 |  6905 | `	pAttr->sTypeName = *pTypeName;` |
|     199 |  6906 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6907 | `		sxu32 i;` |
|      46 |  6908 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      32 |  6909 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      32 |  6910 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      18 |  6911 | `		}` |
|       7 |  6912 | `	}` |
|     199 |  6913 |  |
|      78 |  6914 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  6915 |  |
|      83 |  6916 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6917 | `	SySet *pInstrContainer;` |
|       - |  6918 | `	ph7_class_attr *pCons;` |
|       - |  6919 | `	SyString *pName;` |
|       - |  6920 | `	sxi32 rc;` |
|      83 |  6921 | `	sxu32 nType = 0;` |
|       - |  6922 | `	SyString sTypeClass;` |
|       - |  6923 | `	SyString sTypeText;` |
|       - |  6924 | `	SySet aUnionAlts;` |
|      83 |  6925 | `	sxi32 iTypeFlags = 0;` |
|      83 |  6926 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      83 |  6927 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      83 |  6928 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6929 | `	/* Extract visibility level */` |
|      83 |  6930 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6931 | `	/* Mark as constant */` |
|      83 |  6932 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      83 |  6933 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  6934 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  6935 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      97 |  6936 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  6937 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  6938 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  6939 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  6940 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  6941 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  6942 | `		 * and success paths release. */` |
|      32 |  6943 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6944 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6945 | `			goto Synchronize;` |
|      32 |  6946 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6947 | `			return SXERR_ABORT;` |
|      32 |  6948 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  6949 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  6950 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  6951 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6952 | `				return SXERR_ABORT;` |
|       - |  6953 | `			}` |
|     ! 0 |  6954 | `			goto Synchronize;` |
|       - |  6955 | `		}` |
|      32 |  6956 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  6957 | `	}` |
|      39 |  6958 | `loop:` |
|      85 |  6959 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6960 | `		/* Invalid constant name */` |
|     ! 0 |  6961 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6962 | `		if( rc == SXERR_ABORT ){` |
|       - |  6963 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6964 | `			return SXERR_ABORT;` |
|       - |  6965 | `		}` |
|     ! 0 |  6966 | `		goto Synchronize;` |
|       - |  6967 | `	}` |
|       - |  6968 | `	/* Peek constant name */` |
|      85 |  6969 | `	pName = &pGen->pIn->sData;` |
|       - |  6970 | `	/* Make sure the constant name isn't reserved */` |
|      85 |  6971 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6972 | `		/* Reserved constant name */` |
|     ! 0 |  6973 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6974 | `		if( rc == SXERR_ABORT ){` |
|       - |  6975 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6976 | `			return SXERR_ABORT;` |
|       - |  6977 | `		}` |
|     ! 0 |  6978 | `		goto Synchronize;` |
|       - |  6979 | `	}` |
|       - |  6980 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      85 |  6981 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  6982 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  6983 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  6984 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  6985 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6986 | `			return SXERR_ABORT;` |
|      32 |  6987 | `		}else if( rc != SXRET_OK ){` |
|       3 |  6988 | `			goto Synchronize;` |
|       - |  6989 | `		}` |
|      13 |  6990 | `	}` |
|       - |  6991 | `	/* Advance the stream cursor */` |
|      83 |  6992 | `	pGen->pIn++;` |
|      83 |  6993 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6994 | `		/* Invalid declaration */` |
|     ! 0 |  6995 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6996 | `		if( rc == SXERR_ABORT ){` |
|       - |  6997 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6998 | `			return SXERR_ABORT;` |
|       - |  6999 | `		}` |
|     ! 0 |  7000 | `		goto Synchronize;` |
|       - |  7001 | `	}` |
|      83 |  7002 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7003 | `	/* Allocate a new class attribute */` |
|      83 |  7004 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      83 |  7005 | `	if( pCons == 0 ){` |
|     ! 0 |  7006 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7007 | `		return SXERR_ABORT;` |
|       - |  7008 | `	}` |
|      83 |  7009 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7010 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  7011 | `	}` |
|       - |  7012 | `	/* Swap bytecode container */` |
|      83 |  7013 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      83 |  7014 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7015 | `	/* Compile constant value.` |
|       - |  7016 | `	 */` |
|      83 |  7017 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      83 |  7018 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7019 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7020 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7021 | `			return SXERR_ABORT;` |
|       - |  7022 | `		}` |
|       1 |  7023 | `	}` |
|       - |  7024 | `	/* Emit the done instruction */` |
|      83 |  7025 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      83 |  7026 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      83 |  7027 | `	if( rc == SXERR_ABORT ){` |
|       - |  7028 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7029 | `		return SXERR_ABORT;` |
|       - |  7030 | `	}` |
|       - |  7031 | `	/* All done,install the constant */` |
|      83 |  7032 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      83 |  7033 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7034 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7035 | `		return SXERR_ABORT;` |
|       - |  7036 | `	}` |
|      83 |  7037 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7038 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7039 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7040 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7041 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7042 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7043 | `				pTok--;` |
|     ! 0 |  7044 | `			}` |
|     ! 0 |  7045 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7046 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7047 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7048 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7049 | `				return SXERR_ABORT;` |
|       - |  7050 | `			}` |
|     ! 0 |  7051 | `		}else{` |
|       3 |  7052 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7053 | `				goto loop;` |
|       - |  7054 | `			}` |
|       - |  7055 | `		}` |
|     ! 0 |  7056 | `	}` |
|      81 |  7057 | `	SySetRelease(&aUnionAlts);` |
|      81 |  7058 | `	return SXRET_OK;` |
|       1 |  7059 | `Synchronize:` |
|       3 |  7060 | `	SySetRelease(&aUnionAlts);` |
|       - |  7061 | `	/* Synchronize with the first semi-colon */` |
|       9 |  7062 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7063 | `		pGen->pIn++;` |
|       1 |  7064 | `	}` |
|       3 |  7065 | `	return SXERR_CORRUPT;` |
|      44 |  7066 |  |
|       - |  7067 | `/*` |
|       - |  7068 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7069 | ` * According to the PHP language reference manual` |
|       - |  7070 | ` *  Properties` |
|       - |  7071 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7072 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7073 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7074 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7075 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7076 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7077 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7078 | ` * Symisc eXtension.` |
|       - |  7079 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7080 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7081 | ` *  Example:` |
|       - |  7082 | ` *   class Test{` |
|       - |  7083 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7084 | ` *   };` |
|       - |  7085 | ` *   var_dump(TEST::myVar);` |
|       - |  7086 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7087 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7088 | ` */` |
|       - |  7089 | `/*` |
|       - |  7090 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7091 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7092 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7093 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7094 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7095 | ` */` |
|  170060 |  7096 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7097 |  |
|  170065 |  7098 | `	SyToken *p = pStart;` |
|  170065 |  7099 | `	if( p >= pEnd ) return 0;` |
|  170065 |  7100 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7101 | `		p++;` |
|      18 |  7102 | `		if( p >= pEnd ) return 0;` |
|       8 |  7103 | `	}` |
|  170065 |  7104 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  7105 | `		p++;` |
|       3 |  7106 | `		if( p >= pEnd ) return 0;` |
|       1 |  7107 | `	}` |
|  170065 |  7108 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7109 | `		return 0;` |
|       - |  7110 | `	}` |
|       - |  7111 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  7112 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  7113 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  7114 | `	 * dispatch site. */` |
|  170065 |  7115 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  170043 |  7116 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  170038 |  7117 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3664 |  7118 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  169893 |  7119 | `			return 0;` |
|       - |  7120 | `		}` |
|      75 |  7121 | `	}` |
|     177 |  7122 | `	p++;` |
|       - |  7123 | `	/* Consume optional namespace path */` |
|     179 |  7124 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7125 | `		p += 2;` |
|       1 |  7126 | `	}` |
|       - |  7127 | ``	/* Consume any `\| Type` union alternatives */`` |
|     276 |  7128 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|     109 |  7129 | `		&& p->sData.zString[0] == '\|' ){` |
|      16 |  7130 | `		p++;` |
|      16 |  7131 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      16 |  7132 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      16 |  7133 | `		p++;` |
|      16 |  7134 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7135 | `			p += 2;` |
|     ! 0 |  7136 | `		}` |
|       4 |  7137 | `	}` |
|     177 |  7138 | `	if( p >= pEnd ) return 0;` |
|     177 |  7139 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   85035 |  7140 |  |
|       - |  7141 |  |
|       - |  7142 | `/*` |
|       - |  7143 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7144 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7145 | ` * if not). Recognized forms:` |
|       - |  7146 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7147 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7148 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7149 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7150 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7151 | ` * on unrecoverable error.` |
|       - |  7152 | ` *` |
|       - |  7153 | ` * When a type is parsed:` |
|       - |  7154 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7155 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7156 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7157 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7158 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7159 | ` */` |
|     172 |  7160 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7161 | `	ph7_gen_state *pGen,` |
|       - |  7162 | `	sxu32 *pnType,` |
|       - |  7163 | `	SyString *pClass,` |
|       - |  7164 | `	sxi32 *piTypeFlags,` |
|       - |  7165 | `	SyString *pTypeText,` |
|       - |  7166 | `	SySet *pAlts` |
|       5 |  7167 | `){` |
|     177 |  7168 | `	sxi32 iFlags = 0;` |
|       - |  7169 | `	sxi32 rc;` |
|     177 |  7170 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7171 | `		return SXRET_OK;` |
|       - |  7172 | `	}` |
|       - |  7173 | `	/* If the first token is '$', there's no type */` |
|     177 |  7174 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7175 | `		return SXRET_OK;` |
|       - |  7176 | `	}` |
|     177 |  7177 | `	rc = GenStateParseUnionTypeDecl(` |
|      86 |  7178 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7179 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7180 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7181 | `		/* bAllowVoid */ 0,` |
|     172 |  7182 | `		pGen->pIn->nLine);` |
|     177 |  7183 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7184 | `		return rc;` |
|       - |  7185 | `	}` |
|       - |  7186 | `	/* Verify next token is '$' (start of property name) */` |
|     177 |  7187 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7188 | `		return SXERR_SYNTAX;` |
|       - |  7189 | `	}` |
|     177 |  7190 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     177 |  7191 | `	return SXRET_OK;` |
|      91 |  7192 |  |
|       - |  7193 |  |
|       - |  7194 | `/*` |
|       - |  7195 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7196 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7197 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7198 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7199 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7200 | ` * by the type parser itself before reaching here.` |
|       - |  7201 | ` *` |
|       - |  7202 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7203 | ` * use in the error message.` |
|       - |  7204 | ` */` |
|     288 |  7205 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7206 | `	sxu32 nType,` |
|       - |  7207 | `	const SyString *pClass,` |
|       - |  7208 | `	const char **pzName,` |
|       - |  7209 | `	sxu32 *pnName)` |
|       5 |  7210 |  |
|       - |  7211 | `	const char *z;` |
|       - |  7212 | `	sxu32 n;` |
|     293 |  7213 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     257 |  7214 | `		return 0;` |
|       - |  7215 | `	}` |
|      41 |  7216 | `	z = pClass->zString;` |
|      41 |  7217 | `	n = pClass->nByte;` |
|      41 |  7218 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7219 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7220 | `	}` |
|       - |  7221 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7222 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7223 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      33 |  7224 | `	return 0;` |
|     149 |  7225 |  |
|       - |  7226 |  |
|       - |  7227 | `/*` |
|       - |  7228 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7229 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7230 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7231 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7232 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7233 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7234 | ` *` |
|       - |  7235 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7236 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7237 | ` */` |
|     252 |  7238 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7239 | `	ph7_gen_state *pGen,` |
|       - |  7240 | `	ph7_class *pClass,` |
|       - |  7241 | `	const SyString *pMemberName,` |
|       - |  7242 | `	sxu32 nType,` |
|       - |  7243 | `	const SyString *pTypeClass,` |
|       - |  7244 | `	const SyString *pTypeText,` |
|       - |  7245 | `	SySet *pUnionAlts,` |
|       - |  7246 | `	const char *zErrFmt,` |
|       - |  7247 | `	sxu32 nLine)` |
|       5 |  7248 |  |
|     257 |  7249 | `	const char *zBad = 0;` |
|     257 |  7250 | `	sxu32 nBad = 0;` |
|       - |  7251 | `	SyString sFallback;` |
|       - |  7252 | `	const SyString *pBad;` |
|       - |  7253 | `	sxi32 rc;` |
|     257 |  7254 | `	int bDisallowed = 0;` |
|     257 |  7255 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7256 | `		bDisallowed = 1;` |
|     255 |  7257 | `	}else if( pUnionAlts ){` |
|       - |  7258 | `		sxu32 i;` |
|      56 |  7259 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      40 |  7260 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      40 |  7261 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7262 | `				bDisallowed = 1;` |
|       3 |  7263 | `				break;` |
|       - |  7264 | `			}` |
|      21 |  7265 | `		}` |
|       9 |  7266 | `	}` |
|     257 |  7267 | `	if( !bDisallowed ){` |
|     251 |  7268 | `		return SXRET_OK;` |
|       - |  7269 | `	}` |
|       - |  7270 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7271 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7272 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7273 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7274 | `		pBad = pTypeText;` |
|       5 |  7275 | `	}else{` |
|     ! 0 |  7276 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7277 | `		pBad = &sFallback;` |
|       - |  7278 | `	}` |
|      11 |  7279 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7280 | `		zErrFmt,` |
|       3 |  7281 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7282 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7283 | `		return SXERR_ABORT;` |
|       - |  7284 | `	}` |
|       8 |  7285 | `	return SXERR_SYNTAX;` |
|     131 |  7286 |  |
|       - |  7287 | `/*` |
|       - |  7288 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7289 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7290 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7291 | ` * than promoted to a lexer keyword.` |
|       - |  7292 | ` */` |
| 1538162 |  7293 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7294 |  |
| 1569083 |  7295 | `	return (pTok->nType & PH7_TK_ID)` |
|  799997 |  7296 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1569078 |  7297 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7298 |  |
|   66188 |  7299 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7300 |  |
|   66193 |  7301 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7302 | `	ph7_class_attr *pAttr;` |
|       - |  7303 | `	SyString *pName;` |
|       - |  7304 | `	sxi32 rc;` |
|   66193 |  7305 | `	sxu32 nType = 0;` |
|       - |  7306 | `	SyString sTypeClass;` |
|       - |  7307 | `	SyString sTypeText;` |
|       - |  7308 | `	SySet aUnionAlts;` |
|   66193 |  7309 | `	sxi32 iTypeFlags = 0;` |
|   66193 |  7310 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   66193 |  7311 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   66193 |  7312 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7313 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7314 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7315 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   66193 |  7316 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7317 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7318 | `	}` |
|       - |  7319 | `	/* Extract visibility level */` |
|   66193 |  7320 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7321 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   66279 |  7322 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     177 |  7323 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     177 |  7324 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7325 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7326 | `			goto Synchronize;` |
|     177 |  7327 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7328 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7329 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7330 | `				&pGen->pIn->sData);` |
|     ! 0 |  7331 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7332 | `				return SXERR_ABORT;` |
|       - |  7333 | `			}` |
|     ! 0 |  7334 | `			goto Synchronize;` |
|     177 |  7335 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7336 | `			return SXERR_ABORT;` |
|       - |  7337 | `		}` |
|      86 |  7338 | `	}` |
|     ! 0 |  7339 | `loop:` |
|   66197 |  7340 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7341 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7342 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7343 | `			return SXERR_ABORT;` |
|       - |  7344 | `		}` |
|     ! 0 |  7345 | `		goto Synchronize;` |
|       - |  7346 | `	}` |
|   66197 |  7347 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   66197 |  7348 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7349 | `		/* Invalid attribute name */` |
|     ! 0 |  7350 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7351 | `		if( rc == SXERR_ABORT ){` |
|       - |  7352 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7353 | `			return SXERR_ABORT;` |
|       - |  7354 | `		}` |
|     ! 0 |  7355 | `		goto Synchronize;` |
|       - |  7356 | `	}` |
|       - |  7357 | `	/* Peek attribute name */` |
|   66197 |  7358 | `	pName = &pGen->pIn->sData;` |
|       - |  7359 | `	/* Advance the stream cursor */` |
|   66197 |  7360 | `	pGen->pIn++;` |
|   66197 |  7361 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7362 | `		/* Invalid declaration */` |
|       3 |  7363 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7364 | `		if( rc == SXERR_ABORT ){` |
|       - |  7365 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7366 | `			return SXERR_ABORT;` |
|       - |  7367 | `		}` |
|       3 |  7368 | `		goto Synchronize;` |
|       - |  7369 | `	}` |
|       - |  7370 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7371 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   66195 |  7372 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7373 | `		const char *zRoErr = 0;` |
|      39 |  7374 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7375 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7376 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7377 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7378 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7379 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7380 | `		}` |
|      39 |  7381 | `		if( zRoErr ){` |
|      13 |  7382 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7383 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7384 | `				return SXERR_ABORT;` |
|       - |  7385 | `			}` |
|      13 |  7386 | `			goto Synchronize;` |
|       - |  7387 | `		}` |
|      12 |  7388 | `	}` |
|       - |  7389 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7390 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7391 | `	 * by the type parser. */` |
|   66185 |  7392 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     260 |  7393 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7394 | `			&sTypeText,` |
|     170 |  7395 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      85 |  7396 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     175 |  7397 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7398 | `			return SXERR_ABORT;` |
|     175 |  7399 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7400 | `			goto Synchronize;` |
|       - |  7401 | `		}` |
|      85 |  7402 | `	}` |
|       - |  7403 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   66185 |  7404 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7405 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7406 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7407 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7408 | `			return SXERR_ABORT;` |
|       - |  7409 | `		}` |
|       3 |  7410 | `		goto Synchronize;` |
|       - |  7411 | `	}` |
|       - |  7412 | `	/* Allocate a new class attribute */` |
|   66183 |  7413 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   66183 |  7414 | `	if( pAttr == 0 ){` |
|     ! 0 |  7415 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7416 | `		return SXERR_ABORT;` |
|       - |  7417 | `	}` |
|   66183 |  7418 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     173 |  7419 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      84 |  7420 | `	}` |
|   66183 |  7421 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7422 | `		SySet *pInstrContainer;` |
|   21165 |  7423 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7424 | `		/* Swap bytecode container */` |
|   21165 |  7425 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   21165 |  7426 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7427 | `		/* Compile attribute value.` |
|       - |  7428 | `		 */` |
|   21165 |  7429 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   21165 |  7430 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7431 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7432 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7433 | `				return SXERR_ABORT;` |
|       - |  7434 | `			}` |
|     ! 0 |  7435 | `		}` |
|       - |  7436 | `		/* Emit the done instruction */` |
|   21165 |  7437 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   21165 |  7438 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10580 |  7439 | `	}` |
|       - |  7440 | `	/* All done,install the attribute */` |
|   66183 |  7441 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   66183 |  7442 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7443 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7444 | `		return SXERR_ABORT;` |
|       - |  7445 | `	}` |
|   66183 |  7446 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7447 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7448 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7449 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7450 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7451 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7452 | `				pTok--;` |
|     ! 0 |  7453 | `			}` |
|     ! 0 |  7454 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7455 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7456 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7457 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7458 | `				return SXERR_ABORT;` |
|       - |  7459 | `			}` |
|     ! 0 |  7460 | `		}else{` |
|       5 |  7461 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7462 | `				goto loop;` |
|       - |  7463 | `			}` |
|       - |  7464 | `		}` |
|     ! 0 |  7465 | `	}` |
|   66179 |  7466 | `	SySetRelease(&aUnionAlts);` |
|   66179 |  7467 | `	return SXRET_OK;` |
|       7 |  7468 | `Synchronize:` |
|       - |  7469 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7470 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7471 | `		pGen->pIn++;` |
|       2 |  7472 | `	}` |
|      17 |  7473 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7474 | `	return SXERR_CORRUPT;` |
|   33099 |  7475 |  |
|       - |  7476 | `/*` |
|       - |  7477 | ` * Compile a class method.` |
|       - |  7478 | ` *` |
|       - |  7479 | ` * Refer to the official documentation for more information` |
|       - |  7480 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7481 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7482 | ` * overloading and many more.` |
|       - |  7483 | ` */` |
|  259754 |  7484 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7485 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7486 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7487 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7488 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7489 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7490 | `	)` |
|       5 |  7491 |  |
|  259759 |  7492 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7493 | `	ph7_class_method *pMeth;` |
|       - |  7494 | `	sxi32 iFuncFlags;` |
|       - |  7495 | `	SyString *pName;` |
|       - |  7496 | `	SyToken *pEnd;` |
|       - |  7497 | `	sxi32 rc;` |
|       - |  7498 | `	/* Extract visibility level */` |
|  259759 |  7499 | `	iProtection = GetProtectionLevel(iProtection);` |
|  259759 |  7500 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  259759 |  7501 | `	iFuncFlags = 0;` |
|  259759 |  7502 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7503 | `		/* Invalid method name */` |
|     ! 0 |  7504 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7505 | `		if( rc == SXERR_ABORT ){` |
|       - |  7506 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7507 | `			return SXERR_ABORT;` |
|       - |  7508 | `		}` |
|     ! 0 |  7509 | `		goto Synchronize;` |
|       - |  7510 | `	}` |
|  259759 |  7511 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7512 | `		/* Return by reference,remember that */` |
|     ! 0 |  7513 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7514 | `		/* Jump the '&' token */` |
|     ! 0 |  7515 | `		pGen->pIn++;` |
|     ! 0 |  7516 | `	}` |
|  259759 |  7517 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7518 | `		/* Invalid method name */` |
|     ! 0 |  7519 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7520 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7521 | `			return SXERR_ABORT;` |
|       - |  7522 | `		}` |
|     ! 0 |  7523 | `		goto Synchronize;` |
|       - |  7524 | `	}` |
|       - |  7525 | `	/* Peek method name */` |
|  259759 |  7526 | `	pName = &pGen->pIn->sData;` |
|  259759 |  7527 | `	nLine = pGen->pIn->nLine;` |
|       - |  7528 | `	/* Jump the method name */` |
|  259759 |  7529 | `	pGen->pIn++;` |
|  259759 |  7530 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7531 | `		/* Abstract method */` |
|   89759 |  7532 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7533 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7534 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7535 | `				&pClass->sName,pName);` |
|     ! 0 |  7536 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7537 | `				return SXERR_ABORT;` |
|       - |  7538 | `			}` |
|     ! 0 |  7539 | `		}` |
|       - |  7540 | `		/* Assemble method signature only */` |
|   89759 |  7541 | `		doBody = FALSE;` |
|   44877 |  7542 | `	}` |
|  259759 |  7543 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7544 | `		/* Syntax error */` |
|     ! 0 |  7545 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7546 | `		if( rc == SXERR_ABORT ){` |
|       - |  7547 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7548 | `			return SXERR_ABORT;` |
|       - |  7549 | `		}` |
|     ! 0 |  7550 | `		goto Synchronize;` |
|       - |  7551 | `	}` |
|       - |  7552 | `	/* Allocate a new class_method instance */` |
|  259759 |  7553 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  259759 |  7554 | `	if( pMeth == 0 ){` |
|     ! 0 |  7555 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7556 | `		return SXERR_ABORT;` |
|       - |  7557 | `	}` |
|       - |  7558 | `	/* Jump the left parenthesis '(' */` |
|  259759 |  7559 | `	pGen->pIn++;` |
|  259759 |  7560 | `	pEnd = 0; /* cc warning */` |
|       - |  7561 | `	/* Delimit the method signature */` |
|  259759 |  7562 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  259759 |  7563 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7564 | `		/* Syntax error */` |
|       3 |  7565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7566 | `		if( rc == SXERR_ABORT ){` |
|       - |  7567 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7568 | `			return SXERR_ABORT;` |
|       - |  7569 | `		}` |
|       3 |  7570 | `		goto Synchronize;` |
|       - |  7571 | `	}` |
|       - |  7572 | `	{` |
|  259757 |  7573 | `		int bIsCtor = 0;` |
|  259757 |  7574 | `		int bAbstractCtor = 0;` |
|  259752 |  7575 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  154141 |  7576 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  249331 |  7577 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   20857 |  7578 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7579 | `				bAbstractCtor = 1;` |
|       2 |  7580 | `			}else{` |
|   20855 |  7581 | `				bIsCtor = 1;` |
|       - |  7582 | `			}` |
|   10426 |  7583 | `		}` |
|  259757 |  7584 | `		if( pGen->pIn < pEnd ){` |
|       - |  7585 | `			/* Collect method arguments */` |
|   59027 |  7586 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   59027 |  7587 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7588 | `				return SXERR_ABORT;` |
|       - |  7589 | `			}` |
|   29511 |  7590 | `		}` |
|       - |  7591 | `	}` |
|       - |  7592 | `	/* Point past ')' and parse optional return type ': type' */` |
|  259757 |  7593 | `	pGen->pIn = &pEnd[1];` |
|       - |  7594 | `	{` |
|  259757 |  7595 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  259757 |  7596 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7597 | `			return SXERR_ABORT;` |
|  259757 |  7598 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7599 | `			goto Synchronize;` |
|       - |  7600 | `		}` |
|       - |  7601 | `	}` |
|       - |  7602 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7603 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7604 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7605 | `	{` |
|  259757 |  7606 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7607 | `		sxu32 i;` |
|  353377 |  7608 | `		for( i = 0; i < nArg; i++ ){` |
|   93635 |  7609 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7610 | `			ph7_class_attr *pAttr;` |
|   93635 |  7611 | `			sxi32 iAttrFlags = 0;` |
|   93635 |  7612 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   93575 |  7613 | `				continue;` |
|       - |  7614 | `			}` |
|      65 |  7615 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7616 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7617 | `					"Cannot declare variadic promoted property");` |
|       3 |  7618 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7619 | `					return SXERR_ABORT;` |
|       - |  7620 | `				}` |
|       3 |  7621 | `				goto Synchronize;` |
|       - |  7622 | `			}` |
|       - |  7623 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7624 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7625 | `			 * appear as an alternative of a union type. */` |
|      58 |  7626 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      13 |  7627 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      86 |  7628 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      54 |  7629 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      54 |  7630 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      27 |  7631 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      59 |  7632 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7633 | `					return SXERR_ABORT;` |
|      59 |  7634 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7635 | `					goto Synchronize;` |
|       - |  7636 | `				}` |
|      25 |  7637 | `			}` |
|       - |  7638 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      59 |  7639 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7640 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7641 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7642 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7643 | `					return SXERR_ABORT;` |
|       - |  7644 | `				}` |
|       3 |  7645 | `				goto Synchronize;` |
|       - |  7646 | `			}` |
|      57 |  7647 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      51 |  7648 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      23 |  7649 | `			}` |
|      57 |  7650 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7651 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7652 | `			}` |
|      57 |  7653 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7654 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7655 | `			}` |
|      57 |  7656 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7657 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7658 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7659 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7660 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7661 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7662 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7663 | `						return SXERR_ABORT;` |
|       - |  7664 | `					}` |
|       3 |  7665 | `					goto Synchronize;` |
|       - |  7666 | `				}` |
|      22 |  7667 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7668 | `			}` |
|      55 |  7669 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      55 |  7670 | `			if( pAttr == 0 ){` |
|     ! 0 |  7671 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7672 | `				return SXERR_ABORT;` |
|       - |  7673 | `			}` |
|      55 |  7674 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      51 |  7675 | `				pAttr->nType = pArg->nType;` |
|      51 |  7676 | `				pAttr->sClass = pArg->sClass;` |
|      51 |  7677 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      51 |  7678 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7679 | `					sxu32 k;` |
|     ! 0 |  7680 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7681 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7682 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7683 | `					}` |
|     ! 0 |  7684 | `				}` |
|      23 |  7685 | `			}` |
|      55 |  7686 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      55 |  7687 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7688 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7689 | `				return SXERR_ABORT;` |
|       - |  7690 | `			}` |
|      30 |  7691 | `		}` |
|       - |  7692 | `	}` |
|  259747 |  7693 | `	if( doBody ){` |
|       - |  7694 | `		/* Compile method body */` |
|  169993 |  7695 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  169993 |  7696 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7697 | `			return SXERR_ABORT;` |
|       - |  7698 | `		}` |
|   84999 |  7699 | `	}else{` |
|       - |  7700 | `		/* Only method signature is allowed */` |
|   89759 |  7701 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7702 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7703 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7704 | `				if( rc == SXERR_ABORT ){` |
|       - |  7705 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7706 | `					return SXERR_ABORT;` |
|       - |  7707 | `				}` |
|     ! 0 |  7708 | `				return SXERR_CORRUPT;` |
|       - |  7709 | `			}` |
|       - |  7710 | `	}` |
|       - |  7711 | `	/* All done,install the method */` |
|  259747 |  7712 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  259747 |  7713 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7714 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7715 | `		return SXERR_ABORT;` |
|       - |  7716 | `	}` |
|  259747 |  7717 | `	return SXRET_OK;` |
|       6 |  7718 | `Synchronize:` |
|       - |  7719 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7720 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7721 | `		pGen->pIn++;` |
|       4 |  7722 | `	}` |
|      16 |  7723 | `	return SXERR_CORRUPT;` |
|  129882 |  7724 |  |
|       - |  7725 | `/*` |
|       - |  7726 | ` * Compile an object interface.` |
|       - |  7727 | ` *  According to the PHP language reference manual` |
|       - |  7728 | ` *   Object Interfaces:` |
|       - |  7729 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7730 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7731 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7732 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7733 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7734 | ` */` |
|   38002 |  7735 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7736 |  |
|   38007 |  7737 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7738 | `	ph7_class *pClass,*pBase;` |
|       - |  7739 | `	SyToken *pEnd,*pTmp;` |
|       - |  7740 | `	SyString *pName;` |
|       - |  7741 | `	sxi32 nKwrd;` |
|       - |  7742 | `	sxi32 rc;` |
|       - |  7743 | `	/* Jump the 'interface' keyword */` |
|   38007 |  7744 | `	pGen->pIn++;` |
|       - |  7745 | `	/* Extract interface name */` |
|   38007 |  7746 | `	pName = &pGen->pIn->sData;` |
|       - |  7747 | `	/* Advance the stream cursor */` |
|   38007 |  7748 | `	pGen->pIn++;` |
|       - |  7749 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7750 | `		SyBlob sFQN;` |
|       - |  7751 | `		SyString sFQNStr;` |
|   38007 |  7752 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   38007 |  7753 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   38007 |  7754 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   38007 |  7755 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   38007 |  7756 | `		SyBlobRelease(&sFQN);` |
|       - |  7757 | `	}` |
|   38007 |  7758 | `	if( pClass == 0 ){` |
|     ! 0 |  7759 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7760 | `		return SXERR_ABORT;` |
|       - |  7761 | `	}` |
|       - |  7762 | `	/* Mark as an interface */` |
|   38007 |  7763 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7764 | `	/* Assume no base class is given */` |
|   38007 |  7765 | `	pBase = 0;` |
|   38007 |  7766 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10363 |  7767 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10363 |  7768 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7769 | `			SyBlob sResolved;` |
|       - |  7770 | `			SyString sBaseName;` |
|       - |  7771 | `			sxu32 nRefLine;` |
|       - |  7772 | `			/* Extract base interface */` |
|   10363 |  7773 | `			pGen->pIn++;` |
|   10363 |  7774 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10363 |  7775 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10363 |  7776 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7777 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7778 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7779 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7780 | `					pName);` |
|     ! 0 |  7781 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7782 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7783 | `					return SXERR_ABORT;` |
|       - |  7784 | `				}` |
|     ! 0 |  7785 | `				return SXRET_OK;` |
|       - |  7786 | `			}` |
|   15542 |  7787 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10358 |  7788 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10363 |  7789 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7790 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7791 | `			/* Only interfaces is allowed */` |
|   10363 |  7792 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7793 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7794 | `			}` |
|   10363 |  7795 | `			if( pBase == 0 ){` |
|     ! 0 |  7796 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7797 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7798 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7799 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7800 | `					return SXERR_ABORT;` |
|       - |  7801 | `				}` |
|     ! 0 |  7802 | `			}` |
|   10363 |  7803 | `			SyBlobRelease(&sResolved);` |
|    5179 |  7804 | `		}` |
|    5179 |  7805 | `	}` |
|   38007 |  7806 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7807 | `		/* Syntax error */` |
|     ! 0 |  7808 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7809 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7810 | `		if( rc == SXERR_ABORT ){` |
|       - |  7811 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7812 | `			return SXERR_ABORT;` |
|       - |  7813 | `		}` |
|     ! 0 |  7814 | `		return SXRET_OK;` |
|       - |  7815 | `	}` |
|   38007 |  7816 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   38007 |  7817 | `	pEnd = 0; /* cc warning */` |
|       - |  7818 | `	/* Delimit the interface body */` |
|   38007 |  7819 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   38007 |  7820 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7821 | `		/* Syntax error */` |
|     ! 0 |  7822 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7823 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7824 | `		if( rc == SXERR_ABORT ){` |
|       - |  7825 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7826 | `			return SXERR_ABORT;` |
|       - |  7827 | `		}` |
|     ! 0 |  7828 | `		return SXRET_OK;` |
|       - |  7829 | `	}` |
|       - |  7830 | `	/* Swap token stream */` |
|   38007 |  7831 | `	pTmp = pGen->pEnd;` |
|   38007 |  7832 | `	pGen->pEnd = pEnd;` |
|       - |  7833 | `	/* Start the parse process` |
|       - |  7834 | `	 * Note (According to the PHP reference manual):` |
|       - |  7835 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7836 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7837 | `	 */` |
|   63874 |  7838 | `	for(;;){` |
|       - |  7839 | `		/* Jump leading/trailing semi-colons */` |
|  217499 |  7840 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   89751 |  7841 | `			pGen->pIn++;` |
|       5 |  7842 | `		}` |
|  127753 |  7843 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7844 | `			/* End of interface body */` |
|   38005 |  7845 | `			break;` |
|       - |  7846 | `		}` |
|   89753 |  7847 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7848 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7849 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7850 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7851 | `			if( rc == SXERR_ABORT ){` |
|       - |  7852 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7853 | `				return SXERR_ABORT;` |
|       - |  7854 | `			}` |
|     ! 0 |  7855 | `			goto done;` |
|       - |  7856 | `		}` |
|       - |  7857 | `		/* Extract the current keyword */` |
|   89753 |  7858 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   89753 |  7859 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7860 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7861 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7862 | `			const char *zKind = "member";` |
|       3 |  7863 | `			SyString *pMemberName = 0;` |
|       3 |  7864 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7865 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7866 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7867 | `					zKind = "constant";` |
|       3 |  7868 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7869 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7870 | `					}` |
|       1 |  7871 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7872 | `					zKind = "method";` |
|     ! 0 |  7873 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7874 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7875 | `					}` |
|     ! 0 |  7876 | `				}` |
|       1 |  7877 | `			}` |
|       3 |  7878 | `			if( pMemberName ){` |
|       4 |  7879 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7880 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7881 | `			}else{` |
|     ! 0 |  7882 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7883 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7884 | `			}` |
|       3 |  7885 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7886 | `				return SXERR_ABORT;` |
|       - |  7887 | `			}` |
|       3 |  7888 | `			goto done;` |
|       - |  7889 | `		}` |
|   89751 |  7890 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7891 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7892 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7893 | `			if( rc == SXERR_ABORT ){` |
|       - |  7894 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7895 | `				return SXERR_ABORT;` |
|       - |  7896 | `			}` |
|     ! 0 |  7897 | `			goto done;` |
|       - |  7898 | `		}` |
|   89751 |  7899 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7900 | `			/* Advance the stream cursor */` |
|   89741 |  7901 | `			pGen->pIn++;` |
|   89741 |  7902 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7903 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7904 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7905 | `				if( rc == SXERR_ABORT ){` |
|       - |  7906 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7907 | `					return SXERR_ABORT;` |
|       - |  7908 | `				}` |
|     ! 0 |  7909 | `				goto done;` |
|       - |  7910 | `			}` |
|   89741 |  7911 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   89741 |  7912 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7913 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7914 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7915 | `				if( rc == SXERR_ABORT ){` |
|       - |  7916 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7917 | `					return SXERR_ABORT;` |
|       - |  7918 | `				}` |
|     ! 0 |  7919 | `				goto done;` |
|       - |  7920 | `			}` |
|   44868 |  7921 | `		}` |
|   89751 |  7922 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7923 | `			/* Parse constant */` |
|       7 |  7924 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  7925 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7926 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7927 | `					return SXERR_ABORT;` |
|       - |  7928 | `				}` |
|     ! 0 |  7929 | `				goto done;` |
|       - |  7930 | `			}` |
|       4 |  7931 | `		}else{` |
|   89745 |  7932 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   89745 |  7933 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7934 | `				/* Static method,record that */` |
|   10355 |  7935 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7936 | `				/* Advance the stream cursor */` |
|   10355 |  7937 | `				pGen->pIn++;` |
|   10350 |  7938 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10355 |  7939 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7940 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7941 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7942 | `						if( rc == SXERR_ABORT ){` |
|       - |  7943 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7944 | `							return SXERR_ABORT;` |
|       - |  7945 | `						}` |
|     ! 0 |  7946 | `						goto done;` |
|       - |  7947 | `				}` |
|    5175 |  7948 | `			}` |
|       - |  7949 | `			/* Process method signature (no body for interface methods) */` |
|   89745 |  7950 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   89745 |  7951 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7952 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7953 | `					return SXERR_ABORT;` |
|       - |  7954 | `				}` |
|     ! 0 |  7955 | `				goto done;` |
|       - |  7956 | `			}` |
|       - |  7957 | `		}` |
|       5 |  7958 | `	}` |
|       - |  7959 | `	/* Install the interface */` |
|   38005 |  7960 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   38005 |  7961 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7962 | `		/* Inherit from the base interface */` |
|   10363 |  7963 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5179 |  7964 | `	}` |
|   38005 |  7965 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7966 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7967 | `		return SXERR_ABORT;` |
|       - |  7968 | `	}` |
|   19000 |  7969 | `done:` |
|       - |  7970 | `	/* Point beyond the interface body */` |
|   38007 |  7971 | `	pGen->pIn  = &pEnd[1];` |
|   38007 |  7972 | `	pGen->pEnd = pTmp;` |
|   38007 |  7973 | `	return PH7_OK;` |
|   19006 |  7974 |  |
|       - |  7975 | `/*` |
|       - |  7976 | ` * Compile a user-defined class.` |
|       - |  7977 | ` * According to the PHP language reference manual` |
|       - |  7978 | ` *  class` |
|       - |  7979 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7980 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7981 | ` *  of the properties and methods belonging to the class.` |
|       - |  7982 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7983 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7984 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7985 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7986 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7987 | ` *  (called "methods").` |
|       - |  7988 | ` */` |
|       - |  7989 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7990 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7991 | `struct TraitUseEntry {` |
|       - |  7992 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7993 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7994 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7995 | `};` |
|       - |  7996 | `/*` |
|       - |  7997 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7998 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7999 | ` */` |
|   94244 |  8000 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8001 |  |
|       - |  8002 | `	ph7_class **apIface;` |
|       - |  8003 | `	sxu32 nIface,i;` |
|       - |  8004 | `	sxi32 rc;` |
|   94249 |  8005 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8006 | `		return SXRET_OK;` |
|       - |  8007 | `	}` |
|   94249 |  8008 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   94249 |  8009 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  187609 |  8010 | `	for(i = 0; i < nIface; i++){` |
|   93365 |  8011 | `		ph7_class *pIface = apIface[i];` |
|       - |  8012 | `		SyHashEntry *pEntry;` |
|   93365 |  8013 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  249061 |  8014 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  155701 |  8015 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8016 | `			ph7_class_method *pImplMeth;` |
|  155701 |  8017 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8018 | `			/* Find the implementing method in the class */` |
|  155701 |  8019 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  155701 |  8020 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8021 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8022 | `			}` |
|       - |  8023 | `			/* Check visibility: interface methods must be implemented as public */` |
|  155687 |  8024 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8025 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8026 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8027 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8028 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8029 | `					return SXERR_ABORT;` |
|       - |  8030 | `				}` |
|       1 |  8031 | `			}` |
|       - |  8032 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8033 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8034 | `			 */` |
|       - |  8035 | `			{` |
|  155687 |  8036 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  155687 |  8037 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  155687 |  8038 | `				int sigError = 0;` |
|  155687 |  8039 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8040 | `					sigError = 1;` |
|  155686 |  8041 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8042 | `					/* Extra parameters must all have default values */` |
|       6 |  8043 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8044 | `					sxu32 k;` |
|       8 |  8045 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8046 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8047 | `							sigError = 1;` |
|       3 |  8048 | `							break;` |
|       - |  8049 | `						}` |
|       2 |  8050 | `					}` |
|       2 |  8051 | `				}` |
|  155687 |  8052 | `				if( sigError ){` |
|       - |  8053 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8054 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8055 | `					sxu32 j;` |
|       6 |  8056 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8057 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8058 | `					/* Build implementing method signature */` |
|       6 |  8059 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8060 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8061 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8062 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8063 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8064 | `					}` |
|       - |  8065 | `					/* Build interface method signature */` |
|       6 |  8066 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8067 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8068 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8069 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8070 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8071 | `					}` |
|       8 |  8072 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8073 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8074 | `						&pClass->sName,pMName,` |
|       4 |  8075 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8076 | `						&pIface->sName,pMName,` |
|       4 |  8077 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8078 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8079 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8080 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8081 | `						return SXERR_ABORT;` |
|       - |  8082 | `					}` |
|       2 |  8083 | `				}` |
|       - |  8084 | `			}` |
|       5 |  8085 | `		}` |
|   46685 |  8086 | `	}` |
|   94249 |  8087 | `	return SXRET_OK;` |
|   47127 |  8088 |  |
|       - |  8089 | `/*` |
|       - |  8090 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8091 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8092 | ` */` |
|   94244 |  8093 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8094 |  |
|       - |  8095 | `	ph7_class_method *pMeth;` |
|       - |  8096 | `	SyHashEntry *pEntry;` |
|       - |  8097 | `	sxu32 nAbstract;` |
|       - |  8098 | `	SyBlob sMsg;` |
|       - |  8099 | `	sxi32 rc;` |
|       - |  8100 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   94249 |  8101 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      29 |  8102 | `		return SXRET_OK;` |
|       - |  8103 | `	}` |
|       - |  8104 | `	/* Count abstract methods */` |
|   94225 |  8105 | `	nAbstract = 0;` |
|   94225 |  8106 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  913395 |  8107 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  819175 |  8108 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  819175 |  8109 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8110 | `			nAbstract++;` |
|       8 |  8111 | `		}` |
|       5 |  8112 | `	}` |
|   94225 |  8113 | `	if( nAbstract == 0 ){` |
|   94211 |  8114 | `		return SXRET_OK;` |
|       - |  8115 | `	}` |
|       - |  8116 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8117 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8118 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8119 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8120 | `		&pClass->sName,nAbstract,` |
|       7 |  8121 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8122 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8123 | `	/* Second pass: list methods with origins */` |
|       - |  8124 | `	{` |
|      18 |  8125 | `		sxu32 nListed = 0;` |
|      18 |  8126 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8127 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8128 | `			ph7_class *pOrigin = 0;` |
|       - |  8129 | `			SyString *pMName;` |
|      22 |  8130 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8131 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8132 | `				continue;` |
|       - |  8133 | `			}` |
|      20 |  8134 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8135 | `			if( nListed > 0 ){` |
|       3 |  8136 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8137 | `			}` |
|       - |  8138 | `			/* Find the origin of this abstract method.` |
|       - |  8139 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8140 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8141 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8142 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8143 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8144 | `			 * class's namespace.` |
|       - |  8145 | `			 */` |
|       - |  8146 | `			{` |
|       - |  8147 | `				ph7_class **apIface;` |
|       - |  8148 | `				ph7_class **apTrait;` |
|       - |  8149 | `				ph7_class *pWalk;` |
|       - |  8150 | `				sxu32 i;` |
|       - |  8151 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8152 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8153 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8154 | `				 */` |
|      20 |  8155 | `				if( pClass->pBase ){` |
|      11 |  8156 | `					pWalk = pClass->pBase;` |
|      19 |  8157 | `					while( pWalk ){` |
|       - |  8158 | `						ph7_class_method *pParentMeth;` |
|      13 |  8159 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8160 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8161 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8162 | `							 * in this class's ancestor chain.` |
|       - |  8163 | `							 */` |
|      13 |  8164 | `							int fromIface = 0;` |
|      13 |  8165 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8166 | `							while( pAnc ){` |
|       - |  8167 | `								ph7_class **apPI;` |
|       - |  8168 | `								sxu32 j;` |
|      15 |  8169 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8170 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8171 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8172 | `										fromIface = 1;` |
|      10 |  8173 | `										break;` |
|       - |  8174 | `									}` |
|     ! 0 |  8175 | `								}` |
|      15 |  8176 | `								if( fromIface ) break;` |
|       6 |  8177 | `								pAnc = pAnc->pBase;` |
|       2 |  8178 | `							}` |
|      13 |  8179 | `							if( !fromIface ){` |
|       3 |  8180 | `								pOrigin = pWalk;` |
|       3 |  8181 | `								break;` |
|       - |  8182 | `							}` |
|       4 |  8183 | `						}` |
|      10 |  8184 | `						pWalk = pWalk->pBase;` |
|       2 |  8185 | `					}` |
|       4 |  8186 | `				}` |
|       - |  8187 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8188 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8189 | `				 */` |
|      20 |  8190 | `				if( !pOrigin ){` |
|      18 |  8191 | `					pWalk = pClass;` |
|      40 |  8192 | `					while( pWalk && !pOrigin ){` |
|      26 |  8193 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8194 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8195 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8196 | `							ph7_class *pDeepest = 0;` |
|      28 |  8197 | `							while( pIface ){` |
|      16 |  8198 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8199 | `									pDeepest = pIface;` |
|       6 |  8200 | `								}` |
|      16 |  8201 | `								pIface = pIface->pBase;` |
|       4 |  8202 | `							}` |
|      16 |  8203 | `							if( pDeepest ){` |
|      16 |  8204 | `								pOrigin = pDeepest;` |
|      16 |  8205 | `								break;` |
|       - |  8206 | `							}` |
|     ! 0 |  8207 | `						}` |
|      26 |  8208 | `						pWalk = pWalk->pBase;` |
|       4 |  8209 | `					}` |
|       7 |  8210 | `				}` |
|       - |  8211 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8212 | `				if( !pOrigin ){` |
|       3 |  8213 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8214 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8215 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8216 | `							pOrigin = pClass;` |
|       3 |  8217 | `							break;` |
|       - |  8218 | `						}` |
|     ! 0 |  8219 | `					}` |
|       1 |  8220 | `				}` |
|       - |  8221 | `			}` |
|      20 |  8222 | `			if( pOrigin ){` |
|      20 |  8223 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8224 | `			}else{` |
|       - |  8225 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8226 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8227 | `			}` |
|      20 |  8228 | `			nListed++;` |
|       4 |  8229 | `		}` |
|       - |  8230 | `	}` |
|      18 |  8231 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8232 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8233 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8234 | `	SyBlobRelease(&sMsg);` |
|      18 |  8235 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8236 | `		return SXERR_ABORT;` |
|       - |  8237 | `	}` |
|      18 |  8238 | `	return SXRET_OK;` |
|   47127 |  8239 |  |
|       - |  8240 | `/*` |
|       - |  8241 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8242 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8243 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8244 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8245 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8246 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8247 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8248 | ` */` |
|   93936 |  8249 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8250 |  |
|   93941 |  8251 | `	int isAbsolute = 0;` |
|   93941 |  8252 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8253 | `	SyBlob sName;` |
|   93941 |  8254 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      66 |  8255 | `		isAbsolute = 1;` |
|      66 |  8256 | `		pGen->pIn++;` |
|      31 |  8257 | `	}` |
|   93941 |  8258 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8259 | `		pGen->pIn = pStart;` |
|       8 |  8260 | `		return SXERR_INVALID;` |
|       - |  8261 | `	}` |
|   93935 |  8262 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   93935 |  8263 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   93935 |  8264 | `	pGen->pIn++;` |
|  140913 |  8265 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   46988 |  8266 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8267 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8268 | `		pGen->pIn++;` |
|      13 |  8269 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8270 | `		pGen->pIn++;` |
|       1 |  8271 | `	}` |
|   93935 |  8272 | `	if( isAbsolute ){` |
|      64 |  8273 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      34 |  8274 | `	}else{` |
|       - |  8275 | `		SyString sRaw;` |
|   93875 |  8276 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   93875 |  8277 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8278 | `	}` |
|   93935 |  8279 | `	SyBlobRelease(&sName);` |
|   93935 |  8280 | `	return SXRET_OK;` |
|   46973 |  8281 |  |
|       - |  8282 | `/*` |
|       - |  8283 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8284 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8285 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8286 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8287 | ` * either direction cannot run unbounded.` |
|       - |  8288 | ` */` |
|       - |  8289 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10486 |  8290 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8291 |  |
|       - |  8292 | `	ph7_class **apParent;` |
|       - |  8293 | `	sxu32 n;` |
|   17559 |  8294 | `	while( pInterface ){` |
|   13983 |  8295 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8296 | `			return FALSE;` |
|       - |  8297 | `		}` |
|   17445 |  8298 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6924 |  8299 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6915 |  8300 | `			return TRUE;` |
|       - |  8301 | `		}` |
|    7073 |  8302 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7073 |  8303 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8304 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8305 | `				return TRUE;` |
|       - |  8306 | `			}` |
|     ! 0 |  8307 | `		}` |
|    7073 |  8308 | `		pInterface = pInterface->pBase;` |
|    7073 |  8309 | `		iDepth++;` |
|       5 |  8310 | `	}` |
|    3581 |  8311 | `	return FALSE;` |
|    5248 |  8312 |  |
|   10486 |  8313 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8314 |  |
|   10491 |  8315 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8316 |  |
|       - |  8317 | `/*` |
|       - |  8318 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8319 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8320 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8321 | ` */` |
|    6910 |  8322 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8323 |  |
|    6919 |  8324 | `	while( pBase ){` |
|      10 |  8325 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8326 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8327 | `			return TRUE;` |
|       - |  8328 | `		}` |
|      10 |  8329 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8330 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8331 | `			return TRUE;` |
|       - |  8332 | `		}` |
|       5 |  8333 | `		pBase = pBase->pBase;` |
|       1 |  8334 | `	}` |
|    6911 |  8335 | `	return FALSE;` |
|    3460 |  8336 |  |
|       - |  8337 | `/*` |
|       - |  8338 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8339 | ` *` |
|       - |  8340 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8341 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8342 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8343 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8344 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8345 | ` * implements, body, install) is shared by both paths.` |
|       - |  8346 | ` */` |
|   94274 |  8347 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8348 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8349 |  |
|   94279 |  8350 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8351 | `	ph7_class *pClass,*pBase;` |
|       - |  8352 | `	SyToken *pEnd,*pTmp;` |
|       - |  8353 | `	sxi32 iProtection;` |
|       - |  8354 | `	SySet aInterfaces;` |
|       - |  8355 | `	SySet aUseEntries;` |
|       - |  8356 | `	sxi32 iAttrflags;` |
|       - |  8357 | `	SyString *pName;` |
|       - |  8358 | `	sxi32 nKwrd;` |
|       - |  8359 | `	sxi32 rc;` |
|       - |  8360 | `	/* Jump the 'class' keyword */` |
|   94279 |  8361 | `	pGen->pIn++;` |
|   94279 |  8362 | `	if( pAnonName ){` |
|       - |  8363 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8364 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8365 | `		 * then use the synthesized name. */` |
|      21 |  8366 | `		*ppArgStart = *ppArgEnd = 0;` |
|      21 |  8367 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8368 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8369 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8370 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8371 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8372 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8373 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8374 | `		}` |
|      21 |  8375 | `		pName = pAnonName;` |
|      21 |  8376 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      11 |  8377 | `	}else{` |
|   94259 |  8378 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8379 | `			/* Syntax error */` |
|     ! 0 |  8380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8381 | `			if( rc == SXERR_ABORT ){` |
|       - |  8382 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8383 | `				return SXERR_ABORT;` |
|       - |  8384 | `			}` |
|       - |  8385 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8386 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8387 | `				pGen->pIn++;` |
|     ! 0 |  8388 | `			}` |
|     ! 0 |  8389 | `			return SXRET_OK;` |
|       - |  8390 | `		}` |
|       - |  8391 | `		/* Extract class name */` |
|   94259 |  8392 | `		pName = &pGen->pIn->sData;` |
|       - |  8393 | `		/* Advance the stream cursor */` |
|   94259 |  8394 | `		pGen->pIn++;` |
|       - |  8395 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8396 | `			SyBlob sFQN;` |
|       - |  8397 | `			SyString sFQNStr;` |
|   94259 |  8398 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   94259 |  8399 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|   94259 |  8400 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   94259 |  8401 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   94259 |  8402 | `			SyBlobRelease(&sFQN);` |
|       - |  8403 | `		}` |
|       - |  8404 | `	}` |
|   94279 |  8405 | `	if( pClass == 0 ){` |
|     ! 0 |  8406 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8407 | `		return SXERR_ABORT;` |
|       - |  8408 | `	}` |
|       - |  8409 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   94279 |  8410 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   94279 |  8411 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8412 | `	/* Assume a standalone class */` |
|   94279 |  8413 | `	pBase = 0;` |
|   94279 |  8414 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   83089 |  8415 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   83089 |  8416 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8417 | `			SyBlob sResolved;` |
|       - |  8418 | `			SyString sBaseName;` |
|       - |  8419 | `			sxu32 nRefLine;` |
|   72611 |  8420 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   72611 |  8421 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   72611 |  8422 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   72611 |  8423 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8424 | `				SyBlobRelease(&sResolved);` |
|       4 |  8425 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8426 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8427 | `					pName);` |
|       3 |  8428 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8429 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8430 | `					return SXERR_ABORT;` |
|       - |  8431 | `				}` |
|       3 |  8432 | `				return SXRET_OK;` |
|       - |  8433 | `			}` |
|  108911 |  8434 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   72604 |  8435 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   72609 |  8436 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8437 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8438 | `			/* Interfaces are not allowed */` |
|   72609 |  8439 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8440 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8441 | `			}` |
|   72609 |  8442 | `			if( pBase == 0 ){` |
|     ! 0 |  8443 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8444 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8445 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8446 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8447 | `					return SXERR_ABORT;` |
|       - |  8448 | `				}` |
|     ! 0 |  8449 | `			}else{` |
|   72609 |  8450 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8451 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8452 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8453 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8454 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8455 | `						return SXERR_ABORT;` |
|       - |  8456 | `					}` |
|     ! 0 |  8457 | `				}` |
|       - |  8458 | `			}` |
|   72609 |  8459 | `			SyBlobRelease(&sResolved);` |
|   36302 |  8460 | `		}` |
|   83087 |  8461 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8462 | `			ph7_class *pInterface;` |
|       - |  8463 | `			/* Interface implementation */` |
|   10491 |  8464 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5243 |  8465 | `			for(;;){` |
|       - |  8466 | `				SyBlob sResolved;` |
|       - |  8467 | `				SyString sIntName;` |
|       - |  8468 | `				sxu32 nRefLine;` |
|   10491 |  8469 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10491 |  8470 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10491 |  8471 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8472 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8473 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8474 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8475 | `						pName);` |
|     ! 0 |  8476 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8477 | `						return SXERR_ABORT;` |
|       - |  8478 | `					}` |
|     ! 0 |  8479 | `					break;` |
|       - |  8480 | `				}` |
|   20977 |  8481 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10486 |  8482 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10491 |  8483 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8484 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8485 | `				/* Only interfaces are allowed */` |
|   10491 |  8486 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8487 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8488 | `				}` |
|   10491 |  8489 | `				if( pInterface == 0 ){` |
|     ! 0 |  8490 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8491 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8492 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8493 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8494 | `						return SXERR_ABORT;` |
|       - |  8495 | `					}` |
|     ! 0 |  8496 | `				}else{` |
|       - |  8497 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8498 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8499 | `					 * unless they already extend Exception or Error.` |
|       - |  8500 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8501 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8502 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10491 |  8503 | `					SyString *pFqn = &pClass->sName;` |
|   10491 |  8504 | `					int bIsExceptionOrError =` |
|    8695 |  8505 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   17456 |  8506 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    8766 |  8507 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3460 |  8508 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   13941 |  8509 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10368 |  8510 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3453 |  8511 | `						!bIsExceptionOrError ){` |
|      12 |  8512 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8513 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8514 | `							&pClass->sName);` |
|       9 |  8515 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8516 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8517 | `							return SXERR_ABORT;` |
|       - |  8518 | `						}` |
|       - |  8519 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8520 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8521 | `					}else{` |
|   10485 |  8522 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8523 | `					}` |
|       - |  8524 | `				}` |
|   10491 |  8525 | `				SyBlobRelease(&sResolved);` |
|   10491 |  8526 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5248 |  8527 | `					break;` |
|       - |  8528 | `				}` |
|     ! 0 |  8529 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8530 | `			}` |
|    5243 |  8531 | `		}` |
|   41541 |  8532 | `	}` |
|   94277 |  8533 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8534 | `		/* Syntax error */` |
|     ! 0 |  8535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8536 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8537 | `		if( rc == SXERR_ABORT ){` |
|       - |  8538 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8539 | `			return SXERR_ABORT;` |
|       - |  8540 | `		}` |
|     ! 0 |  8541 | `		return SXRET_OK;` |
|       - |  8542 | `	}` |
|   94277 |  8543 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   94277 |  8544 | `	pEnd = 0; /* cc warning */` |
|       - |  8545 | `	/* Delimit the class body */` |
|   94277 |  8546 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   94277 |  8547 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8548 | `		/* Syntax error */` |
|     ! 0 |  8549 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8550 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8551 | `		if( rc == SXERR_ABORT ){` |
|       - |  8552 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8553 | `			return SXERR_ABORT;` |
|       - |  8554 | `		}` |
|     ! 0 |  8555 | `		return SXRET_OK;` |
|       - |  8556 | `	}` |
|       - |  8557 | `	/* Swap token stream */` |
|   94277 |  8558 | `	pTmp = pGen->pEnd;` |
|   94277 |  8559 | `	pGen->pEnd = pEnd;` |
|       - |  8560 | `	/* Set the inherited flags */` |
|   94277 |  8561 | `	pClass->iFlags = iFlags;` |
|       - |  8562 | `	/* Start the parse process */` |
|  132140 |  8563 | `	for(;;){` |
|       - |  8564 | `		/* Jump leading/trailing semi-colons */` |
|  396771 |  8565 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   66281 |  8566 | `			pGen->pIn++;` |
|       5 |  8567 | `		}` |
|  330495 |  8568 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8569 | `			/* End of class body */` |
|   94249 |  8570 | `			break;` |
|       - |  8571 | `		}` |
|  236246 |  8572 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  118128 |  8573 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8574 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8575 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8576 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8577 | `			if( rc == SXERR_ABORT ){` |
|       - |  8578 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8579 | `				return SXERR_ABORT;` |
|       - |  8580 | `			}` |
|     ! 0 |  8581 | `			goto done;` |
|       - |  8582 | `		}` |
|       - |  8583 | `		/* Assume public visibility */` |
|  236251 |  8584 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  236251 |  8585 | `		iAttrflags = 0;` |
|       - |  8586 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8587 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8588 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8589 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  236251 |  8590 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8591 | `			int bMod = 0;` |
|     ! 0 |  8592 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8593 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8594 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8595 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8596 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8597 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8598 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8599 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8600 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8601 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8602 | `			}` |
|     ! 0 |  8603 | `			if( !bMod ){` |
|     ! 0 |  8604 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8605 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8606 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8607 | `						return SXERR_ABORT;` |
|       - |  8608 | `					}` |
|     ! 0 |  8609 | `					goto done;` |
|       - |  8610 | `				}` |
|     ! 0 |  8611 | `				continue;` |
|       - |  8612 | `			}` |
|     ! 0 |  8613 | `		}` |
|  236251 |  8614 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8615 | `			/* Extract the current keyword */` |
|  236251 |  8616 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  236251 |  8617 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8618 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8619 | `				TraitUseEntry sUse;` |
|      53 |  8620 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      53 |  8621 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      53 |  8622 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      32 |  8623 | `				for(;;){` |
|       - |  8624 | `					ph7_class *pTrait;` |
|       - |  8625 | `					SyString *pTraitName;` |
|      61 |  8626 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8627 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8628 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8629 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8630 | `							return SXERR_ABORT;` |
|       - |  8631 | `						}` |
|     ! 0 |  8632 | `						break;` |
|       - |  8633 | `					}` |
|      61 |  8634 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8635 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8636 | `						SyBlob sResolved;` |
|      61 |  8637 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      61 |  8638 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     117 |  8639 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      56 |  8640 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      61 |  8641 | `						SyBlobRelease(&sResolved);` |
|       - |  8642 | `					}` |
|       - |  8643 | `					/* Only traits are allowed */` |
|      61 |  8644 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8645 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8646 | `					}` |
|      61 |  8647 | `					if( pTrait == 0 ){` |
|     ! 0 |  8648 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8649 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8650 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8651 | `							return SXERR_ABORT;` |
|       - |  8652 | `						}` |
|     ! 0 |  8653 | `					}else{` |
|      61 |  8654 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8655 | `					}` |
|      61 |  8656 | `					pGen->pIn++; /* Advance past trait name */` |
|      61 |  8657 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      29 |  8658 | `						break;` |
|       - |  8659 | `					}` |
|      10 |  8660 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8661 | `				}` |
|       - |  8662 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      53 |  8663 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8664 | `					SyToken *pBlock;` |
|      13 |  8665 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  8666 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  8667 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  8668 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  8669 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  8670 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  8671 | `					}else{` |
|     ! 0 |  8672 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8673 | `					}` |
|       5 |  8674 | `				}` |
|      53 |  8675 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8676 | `				/* The semicolon will be consumed by the outer loop */` |
|      53 |  8677 | `				continue;` |
|       - |  8678 | `			}` |
|  236203 |  8679 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  232507 |  8680 | `				iProtection = nKwrd;` |
|  232507 |  8681 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8682 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  232507 |  8683 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8684 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8685 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8686 | `				}` |
|  232502 |  8687 | `				if( pGen->pIn >= pGen->pEnd` |
|  232507 |  8688 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8689 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8690 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8691 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8692 | `					if( rc == SXERR_ABORT ){` |
|       - |  8693 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8694 | `						return SXERR_ABORT;` |
|       - |  8695 | `					}` |
|     ! 0 |  8696 | `					goto done;` |
|       - |  8697 | `				}` |
|  232507 |  8698 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8699 | `					/* Attribute declaration (untyped) */` |
|   65997 |  8700 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   65997 |  8701 | `					if( rc != SXRET_OK ){` |
|       9 |  8702 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8703 | `							return SXERR_ABORT;` |
|       - |  8704 | `						}` |
|       9 |  8705 | `						goto done;` |
|       - |  8706 | `					}` |
|   65991 |  8707 | `					continue;` |
|       - |  8708 | `				}` |
|  166515 |  8709 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8710 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     161 |  8711 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     161 |  8712 | `					if( rc != SXRET_OK ){` |
|       8 |  8713 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8714 | `							return SXERR_ABORT;` |
|       - |  8715 | `						}` |
|       8 |  8716 | `						goto done;` |
|       - |  8717 | `					}` |
|     155 |  8718 | `					continue;` |
|       - |  8719 | `				}` |
|       - |  8720 | `				/* Extract the keyword */` |
|  166359 |  8721 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   83177 |  8722 | `			}` |
|  170055 |  8723 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8724 | `				/* Process constant declaration */` |
|      67 |  8725 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      67 |  8726 | `				if( rc != SXRET_OK ){` |
|       3 |  8727 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8728 | `						return SXERR_ABORT;` |
|       - |  8729 | `					}` |
|       3 |  8730 | `					goto done;` |
|       - |  8731 | `				}` |
|      35 |  8732 | `			}else{` |
|  169993 |  8733 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8734 | `					/* Static method or attribute,record that */` |
|    3501 |  8735 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3501 |  8736 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3501 |  8737 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8738 | `						/* Extract the keyword */` |
|    3493 |  8739 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3493 |  8740 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8741 | `							iProtection = nKwrd;` |
|     ! 0 |  8742 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8743 | `						}` |
|    1744 |  8744 | `					}` |
|       - |  8745 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8746 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8747 | `					 * than a generic "expecting method" parse error. */` |
|    3501 |  8748 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8749 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8750 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8751 | `					}` |
|    3496 |  8752 | `					if( pGen->pIn >= pGen->pEnd` |
|    3501 |  8753 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8754 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8755 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8756 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8757 | `						if( rc == SXERR_ABORT ){` |
|       - |  8758 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8759 | `							return SXERR_ABORT;` |
|       - |  8760 | `						}` |
|     ! 0 |  8761 | `						goto done;` |
|       - |  8762 | `					}` |
|    3501 |  8763 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8764 | `						/* Attribute declaration */` |
|       8 |  8765 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  8766 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8767 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8768 | `								return SXERR_ABORT;` |
|       - |  8769 | `							}` |
|     ! 0 |  8770 | `							goto done;` |
|       - |  8771 | `						}` |
|       8 |  8772 | `						continue;` |
|       - |  8773 | `					}` |
|    3495 |  8774 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8775 | `						/* Typed static attribute declaration */` |
|      15 |  8776 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8777 | `						if( rc != SXRET_OK ){` |
|       3 |  8778 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8779 | `								return SXERR_ABORT;` |
|       - |  8780 | `							}` |
|       3 |  8781 | `							goto done;` |
|       - |  8782 | `						}` |
|      13 |  8783 | `						continue;` |
|       - |  8784 | `					}` |
|       - |  8785 | `					/* Extract the keyword */` |
|    3483 |  8786 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  168236 |  8787 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8788 | `					/* Abstract method,record that */` |
|      13 |  8789 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8790 | `					/* Mark the whole class as abstract */` |
|      13 |  8791 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8792 | `					/* Advance the stream cursor */` |
|      13 |  8793 | `					pGen->pIn++;` |
|      13 |  8794 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      13 |  8795 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      13 |  8796 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      11 |  8797 | `							iProtection = nKwrd;` |
|      11 |  8798 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8799 | `						}` |
|       5 |  8800 | `					}` |
|      13 |  8801 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8802 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8803 | `							/* Static method */` |
|     ! 0 |  8804 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8805 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8806 | `					}` |
|      13 |  8807 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8808 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8809 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8810 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8811 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8812 | `							if( rc == SXERR_ABORT ){` |
|       - |  8813 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8814 | `								return SXERR_ABORT;` |
|       - |  8815 | `							}` |
|     ! 0 |  8816 | `							goto done;` |
|       - |  8817 | `					}` |
|      13 |  8818 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  166492 |  8819 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8820 | `					/* final method ,record that */` |
|      18 |  8821 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      18 |  8822 | `					pGen->pIn++; /* Jump the final keyword */` |
|      18 |  8823 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8824 | `						/* Extract the keyword */` |
|      18 |  8825 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      18 |  8826 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       9 |  8827 | `							iProtection = nKwrd;` |
|       9 |  8828 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  8829 | `						}` |
|       7 |  8830 | `					}` |
|      18 |  8831 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  8832 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  8833 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  8834 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  8835 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  8836 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  8837 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  8838 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  8839 | `									return SXERR_ABORT;` |
|       - |  8840 | `								}` |
|     ! 0 |  8841 | `								goto done;` |
|       - |  8842 | `							}` |
|      12 |  8843 | `							continue;` |
|       - |  8844 | `					}` |
|       6 |  8845 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8846 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8847 | `							/* Static method */` |
|     ! 0 |  8848 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8849 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8850 | `					}` |
|       6 |  8851 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8852 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8853 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8854 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8855 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8856 | `							if( rc == SXERR_ABORT ){` |
|       - |  8857 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8858 | `								return SXERR_ABORT;` |
|       - |  8859 | `							}` |
|     ! 0 |  8860 | `							goto done;` |
|       - |  8861 | `					}` |
|       6 |  8862 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8863 | `				}` |
|  169965 |  8864 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8865 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8866 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8867 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8868 | `						if( rc == SXERR_ABORT ){` |
|       - |  8869 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8870 | `							return SXERR_ABORT;` |
|       - |  8871 | `						}` |
|     ! 0 |  8872 | `						goto done;` |
|       - |  8873 | `				}` |
|  169965 |  8874 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8875 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8876 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8877 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8878 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8879 | `						if( rc == SXERR_ABORT ){` |
|       - |  8880 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8881 | `							return SXERR_ABORT;` |
|       - |  8882 | `						}` |
|     ! 0 |  8883 | `						goto done;` |
|       - |  8884 | `					}` |
|       - |  8885 | `					/* Attribute declaration */` |
|       7 |  8886 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8887 | `				}else{` |
|       - |  8888 | `					/* Process method declaration */` |
|  169959 |  8889 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8890 | `				}` |
|  169965 |  8891 | `				if( rc != SXRET_OK ){` |
|      16 |  8892 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8893 | `						return SXERR_ABORT;` |
|       - |  8894 | `					}` |
|      16 |  8895 | `					goto done;` |
|       - |  8896 | `				}` |
|       - |  8897 | `			}` |
|   85009 |  8898 | `		}else{` |
|       - |  8899 | `			/* Attribute declaration */` |
|     ! 0 |  8900 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8901 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8902 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8903 | `					return SXERR_ABORT;` |
|       - |  8904 | `				}` |
|     ! 0 |  8905 | `				goto done;` |
|       - |  8906 | `			}` |
|       - |  8907 | `		}` |
|       5 |  8908 | `	}` |
|       - |  8909 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8910 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8911 | `	 */` |
|       - |  8912 | `	{` |
|       - |  8913 | `		TraitUseEntry *apUse;` |
|       - |  8914 | `		sxu32 nU;` |
|   94249 |  8915 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   94297 |  8916 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      53 |  8917 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      53 |  8918 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      53 |  8919 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      53 |  8920 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8921 | `			sxu32 nT;` |
|      53 |  8922 | `			if( !hasResolution ){` |
|       - |  8923 | `				/* No conflict resolution block: use standard trait application */` |
|      87 |  8924 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      49 |  8925 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      49 |  8926 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8927 | `						break;` |
|       - |  8928 | `					}` |
|      27 |  8929 | `				}` |
|      24 |  8930 | `			}else{` |
|       - |  8931 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8932 | `				 * then use the block to resolve method conflicts.` |
|       - |  8933 | `				 */` |
|       - |  8934 | `				SyToken *pR;` |
|      25 |  8935 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  8936 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8937 | `					ph7_class_attr *pAR;` |
|       - |  8938 | `					SyHashEntry *pER;` |
|       - |  8939 | `					SyString *pNR;` |
|      15 |  8940 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  8941 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8942 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8943 | `						pNR = &pAR->sName;` |
|     ! 0 |  8944 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8945 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8946 | `						}` |
|     ! 0 |  8947 | `					}` |
|      15 |  8948 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  8949 | `				}` |
|       - |  8950 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  8951 | `				pR = pUse->pResolvStart;` |
|      27 |  8952 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8953 | `					SyString sTrait,sMethod;` |
|       - |  8954 | `					ph7_class *pSrcTrait;` |
|       - |  8955 | `					ph7_class_method *pMeth;` |
|       - |  8956 | `					sxi32 nRKwrd;` |
|      41 |  8957 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  8958 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  8959 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  8960 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  8961 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  8962 | `					sMethod = pR->sData;` |
|      17 |  8963 | `					pR++;` |
|      17 |  8964 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8965 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8966 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8967 | `							sTrait = sMethod;` |
|       7 |  8968 | `							pR++;` |
|       7 |  8969 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8970 | `							sMethod = pR->sData;` |
|       7 |  8971 | `							pR++;` |
|       3 |  8972 | `						}` |
|       3 |  8973 | `					}` |
|      17 |  8974 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8975 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8976 | `						continue;` |
|       - |  8977 | `					}` |
|      17 |  8978 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  8979 | `					pR++;` |
|      17 |  8980 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8981 | `						pSrcTrait = 0;` |
|       7 |  8982 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8983 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8984 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8985 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8986 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8987 | `								break;` |
|       - |  8988 | `							}` |
|       2 |  8989 | `						}` |
|       5 |  8990 | `						if( pSrcTrait ){` |
|       5 |  8991 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8992 | `							if( pMeth ){` |
|       5 |  8993 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8994 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8995 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8996 | `								}` |
|       2 |  8997 | `							}` |
|       2 |  8998 | `						}` |
|       2 |  8999 | `					}` |
|      35 |  9000 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9001 | `				}` |
|       - |  9002 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9003 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9004 | `					ph7_class_method *pMR;` |
|       - |  9005 | `					SyHashEntry *pER;` |
|       - |  9006 | `					SyString *pNR;` |
|      15 |  9007 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9008 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9009 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9010 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9011 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9012 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9013 | `						}` |
|       3 |  9014 | `					}` |
|       9 |  9015 | `				}` |
|       - |  9016 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9017 | `				pR = pUse->pResolvStart;` |
|      27 |  9018 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9019 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9020 | `					ph7_class *pSrcTrait;` |
|       - |  9021 | `					ph7_class_method *pMeth;` |
|      27 |  9022 | `					int hasQual = 0;` |
|       - |  9023 | `					sxi32 nRKwrd;` |
|      41 |  9024 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9025 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9026 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9027 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9028 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9029 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9030 | `					sMethod = pR->sData;` |
|      17 |  9031 | `					pR++;` |
|      17 |  9032 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9033 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9034 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9035 | `							sTrait = sMethod;` |
|       7 |  9036 | `							hasQual = 1;` |
|       7 |  9037 | `							pR++;` |
|       7 |  9038 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9039 | `							sMethod = pR->sData;` |
|       7 |  9040 | `							pR++;` |
|       3 |  9041 | `						}` |
|       3 |  9042 | `					}` |
|      17 |  9043 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9044 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9045 | `						continue;` |
|       - |  9046 | `					}` |
|      17 |  9047 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9048 | `					pR++;` |
|      17 |  9049 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9050 | `						sxi32 iNewVis = -1;` |
|      13 |  9051 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9052 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9053 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9054 | `								iNewVis = nAK;` |
|       7 |  9055 | `								pR++;` |
|       3 |  9056 | `							}` |
|       3 |  9057 | `						}` |
|      13 |  9058 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9059 | `							sAlias = pR->sData;` |
|      11 |  9060 | `							pR++;` |
|       4 |  9061 | `						}` |
|      13 |  9062 | `						pMeth = 0;` |
|      13 |  9063 | `						if( hasQual ){` |
|       3 |  9064 | `							pSrcTrait = 0;` |
|       5 |  9065 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9066 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9067 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9068 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9069 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9070 | `									break;` |
|       - |  9071 | `								}` |
|       2 |  9072 | `							}` |
|       3 |  9073 | `							if( pSrcTrait ){` |
|       3 |  9074 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9075 | `							}` |
|       2 |  9076 | `						}else{` |
|      10 |  9077 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9078 | `						}` |
|      13 |  9079 | `						if( pMeth ){` |
|      13 |  9080 | `							if( sAlias.nByte > 0 ){` |
|       - |  9081 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9082 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9083 | `								 */` |
|       - |  9084 | `								ph7_class_method *pAlias;` |
|       - |  9085 | `								char *zAliasDup;` |
|      11 |  9086 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9087 | `								if( pAlias ){` |
|      11 |  9088 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9089 | `									if( iNewVis >= 0 ){` |
|       5 |  9090 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9091 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9092 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9093 | `									}` |
|      11 |  9094 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9095 | `									if( zAliasDup ){` |
|      11 |  9096 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9097 | `									}` |
|       7 |  9098 | `								}` |
|       7 |  9099 | `							}else if( iNewVis >= 0 ){` |
|       - |  9100 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9101 | `								ph7_class_method *pCopy;` |
|       3 |  9102 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9103 | `								if( pCopy ){` |
|       3 |  9104 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9105 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9106 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9107 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9108 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9109 | `									/* Replace the method in the class hash */` |
|       3 |  9110 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9111 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9112 | `								}` |
|       1 |  9113 | `							}` |
|       5 |  9114 | `						}` |
|       5 |  9115 | `						SXUNUSED(hasQual);` |
|       5 |  9116 | `					}` |
|      21 |  9117 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9118 | `				}` |
|       - |  9119 | `			}` |
|      53 |  9120 | `			SySetRelease(&pUse->aTraits);` |
|      29 |  9121 | `		}` |
|       - |  9122 | `	}` |
|       - |  9123 | `	/* Install the class */` |
|   94249 |  9124 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   94249 |  9125 | `	if( rc == SXRET_OK ){` |
|       - |  9126 | `		ph7_class **apInterface;` |
|       - |  9127 | `		sxu32 n;` |
|   94249 |  9128 | `		if( pBase ){` |
|       - |  9129 | `			/* Inherit from base class and mark as a subclass */` |
|   72609 |  9130 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   36302 |  9131 | `		}` |
|   94249 |  9132 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  104729 |  9133 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9134 | `			/* Implements one or more interface */` |
|   10485 |  9135 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10485 |  9136 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9137 | `				break;` |
|       - |  9138 | `			}` |
|    5245 |  9139 | `		}` |
|       - |  9140 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9141 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|   94244 |  9142 | `		if( rc == SXRET_OK` |
|   94244 |  9143 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|   94249 |  9144 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   82887 |  9145 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9146 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   82887 |  9147 | `			if( pStringable ){` |
|   82887 |  9148 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   82887 |  9149 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9150 | `				sxu32 i;` |
|   82887 |  9151 | `				int bAlready = 0;` |
|   89791 |  9152 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    6911 |  9153 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9154 | `						bAlready = 1;` |
|       3 |  9155 | `						break;` |
|       - |  9156 | `					}` |
|    3457 |  9157 | `				}` |
|   82887 |  9158 | `				if( !bAlready ){` |
|   82885 |  9159 | `					PH7_ClassImplement(pClass,pStringable);` |
|   41440 |  9160 | `				}` |
|   41441 |  9161 | `			}` |
|   41441 |  9162 | `		}` |
|       - |  9163 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   94249 |  9164 | `		if( rc == SXRET_OK ){` |
|   94249 |  9165 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   94249 |  9166 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9167 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9168 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9169 | `				return SXERR_ABORT;` |
|       - |  9170 | `			}` |
|   47122 |  9171 | `		}` |
|       - |  9172 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   94249 |  9173 | `		if( rc == SXRET_OK ){` |
|   94249 |  9174 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   94249 |  9175 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9176 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9177 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9178 | `				return SXERR_ABORT;` |
|       - |  9179 | `			}` |
|   47122 |  9180 | `		}` |
|   47122 |  9181 | `	}` |
|   94249 |  9182 | `	SySetRelease(&aUseEntries);` |
|   94249 |  9183 | `	SySetRelease(&aInterfaces);` |
|   94249 |  9184 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9185 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9186 | `		return SXERR_ABORT;` |
|       - |  9187 | `	}` |
|   47122 |  9188 | `done:` |
|       - |  9189 | `	/* Point beyond the class body */` |
|   94277 |  9190 | `	pGen->pIn = &pEnd[1];` |
|   94277 |  9191 | `	pGen->pEnd = pTmp;` |
|   94277 |  9192 | `	return PH7_OK;` |
|   47142 |  9193 |  |
|       - |  9194 | `/* Compile a named class declaration (the common case). */` |
|   94254 |  9195 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9196 |  |
|   94259 |  9197 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9198 |  |
|       - |  9199 | `/*` |
|       - |  9200 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9201 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9202 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9203 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9204 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9205 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9206 | ` */` |
|      20 |  9207 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  9208 |  |
|       - |  9209 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9210 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9211 | `	SyString sName;` |
|       - |  9212 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9213 | `	ph7_value *pObj;` |
|      21 |  9214 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9215 | `	sxu32 nIdx,nLen;` |
|       - |  9216 | `	sxi32 nArg,rc;` |
|      10 |  9217 | `	SXUNUSED(iCompileFlag);` |
|       - |  9218 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      21 |  9219 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      21 |  9220 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9221 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9222 | `	}` |
|      21 |  9223 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9224 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9225 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9226 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      21 |  9227 | `	pArgStart = pArgEnd = 0;` |
|      21 |  9228 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      21 |  9229 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9230 | `		return rc;` |
|       - |  9231 | `	}` |
|       - |  9232 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9233 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      21 |  9234 | `	nArg = 0;` |
|      21 |  9235 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9236 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9237 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9238 | `		SyToken *pArgNext;` |
|       7 |  9239 | `		pGen->pIn = pArgStart;` |
|       7 |  9240 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9241 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9242 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9243 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9244 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9245 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9246 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9247 | `					return SXERR_ABORT;` |
|       - |  9248 | `				}` |
|       7 |  9249 | `				nArg++;` |
|       3 |  9250 | `			}` |
|       7 |  9251 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9252 | `		}` |
|       7 |  9253 | `		pGen->pIn = pSavedIn;` |
|       7 |  9254 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9255 | `	}` |
|       - |  9256 | `	/* Load the synthesized class name */` |
|      21 |  9257 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      21 |  9258 | `	if( pObj == 0 ){` |
|     ! 0 |  9259 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9260 | `		return SXERR_ABORT;` |
|       - |  9261 | `	}` |
|      21 |  9262 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      21 |  9263 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9264 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      21 |  9265 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      21 |  9266 | `	return SXRET_OK;` |
|      11 |  9267 |  |
|       - |  9268 | `/*` |
|       - |  9269 | ` * Compile a user-defined abstract class.` |
|       - |  9270 | ` *  According to the PHP language reference manual` |
|       - |  9271 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9272 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9273 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9274 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9275 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9276 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9277 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9278 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9279 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9280 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9281 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9282 | ` *   could differ.` |
|       - |  9283 | ` */` |
|       - |  9284 | `/*` |
|       - |  9285 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9286 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9287 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9288 | ` */` |
|  929420 |  9289 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9290 |  |
|  929425 |  9291 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  616039 |  9292 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  616039 |  9293 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  616021 |  9294 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  307985 |  9295 | `	}` |
|  929361 |  9296 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  929301 |  9297 | `	return FALSE;` |
|  464715 |  9298 |  |
|       - |  9299 | `/*` |
|       - |  9300 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9301 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9302 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9303 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9304 | ` */` |
|  929296 |  9305 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9306 |  |
|  929301 |  9307 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  929301 |  9308 | `	sxi32 iFlags = 0,iFlag;` |
|  929425 |  9309 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|     129 |  9310 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9311 | `			pDup = pIn;` |
|       2 |  9312 | `		}` |
|     129 |  9313 | `		iFlags \|= iFlag;` |
|     129 |  9314 | `		pIn++;` |
|       5 |  9315 | `	}` |
|  929301 |  9316 | `	*ppIn = pIn;` |
|  929301 |  9317 | `	if( ppDup ){ *ppDup = pDup; }` |
|  929301 |  9318 | `	return iFlags;` |
|       5 |  9319 |  |
|       - |  9320 | `/*` |
|       - |  9321 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9322 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9323 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9324 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9325 | `` * `readonly`) to their existing handlers.`` |
|       - |  9326 | ` */` |
|  929244 |  9327 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9328 |  |
|  929249 |  9329 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  464681 |  9330 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  929272 |  9331 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9332 |  |
|       - |  9333 | `/*` |
|       - |  9334 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9335 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9336 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9337 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9338 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9339 | ` */` |
|      52 |  9340 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9341 |  |
|       - |  9342 | `	SyToken *pDup;` |
|      57 |  9343 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9344 | `	sxi32 rc;` |
|      57 |  9345 | `	if( pDup ){` |
|       4 |  9346 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9347 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9348 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9349 | `			return SXERR_ABORT;` |
|       - |  9350 | `		}` |
|       1 |  9351 | `	}` |
|      52 |  9352 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|      31 |  9353 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9355 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9356 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9357 | `			return SXERR_ABORT;` |
|       - |  9358 | `		}` |
|       1 |  9359 | `	}` |
|      57 |  9360 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|      31 |  9361 |  |
|       - |  9362 | `/*` |
|       - |  9363 | ` * Compile a user-defined trait.` |
|       - |  9364 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9365 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9366 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9367 | ` */` |
|      60 |  9368 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9369 |  |
|      65 |  9370 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9371 | `	ph7_class *pClass;` |
|       - |  9372 | `	SyToken *pEnd,*pTmp;` |
|       - |  9373 | `	sxi32 iProtection;` |
|       - |  9374 | `	sxi32 iAttrflags;` |
|       - |  9375 | `	SyString *pName;` |
|       - |  9376 | `	sxi32 nKwrd;` |
|       - |  9377 | `	sxi32 rc;` |
|       - |  9378 | `	/* Jump the 'trait' keyword */` |
|      65 |  9379 | `	pGen->pIn++;` |
|      65 |  9380 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9381 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9382 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9383 | `			return SXERR_ABORT;` |
|       - |  9384 | `		}` |
|     ! 0 |  9385 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9386 | `			pGen->pIn++;` |
|     ! 0 |  9387 | `		}` |
|     ! 0 |  9388 | `		return SXRET_OK;` |
|       - |  9389 | `	}` |
|       - |  9390 | `	/* Extract trait name */` |
|      65 |  9391 | `	pName = &pGen->pIn->sData;` |
|      65 |  9392 | `	pGen->pIn++;` |
|       - |  9393 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9394 | `		SyBlob sFQN;` |
|       - |  9395 | `		SyString sFQNStr;` |
|      65 |  9396 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      65 |  9397 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      65 |  9398 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      65 |  9399 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      65 |  9400 | `		SyBlobRelease(&sFQN);` |
|       - |  9401 | `	}` |
|      65 |  9402 | `	if( pClass == 0 ){` |
|     ! 0 |  9403 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9404 | `		return SXERR_ABORT;` |
|       - |  9405 | `	}` |
|       - |  9406 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      65 |  9407 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9408 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9409 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9410 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9411 | `			return SXERR_ABORT;` |
|       - |  9412 | `		}` |
|     ! 0 |  9413 | `		return SXRET_OK;` |
|       - |  9414 | `	}` |
|      65 |  9415 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      65 |  9416 | `	pEnd = 0;` |
|      65 |  9417 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      65 |  9418 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9419 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9420 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9421 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9422 | `			return SXERR_ABORT;` |
|       - |  9423 | `		}` |
|     ! 0 |  9424 | `		return SXRET_OK;` |
|       - |  9425 | `	}` |
|       - |  9426 | `	/* Swap token stream */` |
|      65 |  9427 | `	pTmp = pGen->pEnd;` |
|      65 |  9428 | `	pGen->pEnd = pEnd;` |
|       - |  9429 | `	/* Mark as trait */` |
|      65 |  9430 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9431 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      60 |  9432 | `	for(;;){` |
|     169 |  9433 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9434 | `			pGen->pIn++;` |
|       4 |  9435 | `		}` |
|     145 |  9436 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      65 |  9437 | `			break;` |
|       - |  9438 | `		}` |
|      85 |  9439 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9440 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9441 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9442 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9443 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9444 | `				return SXERR_ABORT;` |
|       - |  9445 | `			}` |
|     ! 0 |  9446 | `			goto done;` |
|       - |  9447 | `		}` |
|      85 |  9448 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      85 |  9449 | `		iAttrflags = 0;` |
|      85 |  9450 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      85 |  9451 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  9452 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9453 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9454 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9455 | `				for(;;){` |
|       - |  9456 | `					ph7_class *pUsedTrait;` |
|       - |  9457 | `					SyString *pUsedName;` |
|       5 |  9458 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9459 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9460 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9461 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9462 | `							return SXERR_ABORT;` |
|       - |  9463 | `						}` |
|     ! 0 |  9464 | `						break;` |
|       - |  9465 | `					}` |
|       5 |  9466 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9467 | `					{` |
|       - |  9468 | `						SyBlob sResolved;` |
|       5 |  9469 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9470 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9471 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9472 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9473 | `						SyBlobRelease(&sResolved);` |
|       - |  9474 | `					}` |
|       5 |  9475 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9476 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9477 | `					}` |
|       5 |  9478 | `					if( pUsedTrait == 0 ){` |
|       4 |  9479 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9480 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9481 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9482 | `							return SXERR_ABORT;` |
|       - |  9483 | `						}` |
|       2 |  9484 | `					}else{` |
|       3 |  9485 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9486 | `					}` |
|       5 |  9487 | `					pGen->pIn++;` |
|       5 |  9488 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9489 | `						break;` |
|       - |  9490 | `					}` |
|     ! 0 |  9491 | `					pGen->pIn++;` |
|     ! 0 |  9492 | `				}` |
|       5 |  9493 | `				continue;` |
|       - |  9494 | `			}` |
|      81 |  9495 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9496 | `				iProtection = nKwrd;` |
|      73 |  9497 | `				pGen->pIn++;` |
|      68 |  9498 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9499 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9500 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9501 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9502 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9503 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9504 | `						return SXERR_ABORT;` |
|       - |  9505 | `					}` |
|     ! 0 |  9506 | `					goto done;` |
|       - |  9507 | `				}` |
|      73 |  9508 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9509 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9510 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9511 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9512 | `							return SXERR_ABORT;` |
|       - |  9513 | `						}` |
|     ! 0 |  9514 | `						goto done;` |
|       - |  9515 | `					}` |
|      12 |  9516 | `					continue;` |
|       - |  9517 | `				}` |
|      63 |  9518 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9519 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9520 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9521 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9522 | `							return SXERR_ABORT;` |
|       - |  9523 | `						}` |
|     ! 0 |  9524 | `						goto done;` |
|       - |  9525 | `					}` |
|       5 |  9526 | `					continue;` |
|       - |  9527 | `				}` |
|      58 |  9528 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9529 | `			}` |
|      66 |  9530 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9531 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9532 | `					"Traits cannot have constants");` |
|     ! 0 |  9533 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9534 | `					return SXERR_ABORT;` |
|       - |  9535 | `				}` |
|     ! 0 |  9536 | `				goto done;` |
|     ! 0 |  9537 | `			}else{` |
|      66 |  9538 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9539 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9540 | `					pGen->pIn++;` |
|       5 |  9541 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9542 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9543 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9544 | `							iProtection = nKwrd;` |
|     ! 0 |  9545 | `							pGen->pIn++;` |
|     ! 0 |  9546 | `						}` |
|       1 |  9547 | `					}` |
|       4 |  9548 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9549 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  9550 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9551 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9552 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9553 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9554 | `							return SXERR_ABORT;` |
|       - |  9555 | `						}` |
|     ! 0 |  9556 | `						goto done;` |
|       - |  9557 | `					}` |
|       5 |  9558 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9559 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9560 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9561 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9562 | `								return SXERR_ABORT;` |
|       - |  9563 | `							}` |
|     ! 0 |  9564 | `							goto done;` |
|       - |  9565 | `						}` |
|       3 |  9566 | `						continue;` |
|       - |  9567 | `					}` |
|       3 |  9568 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9569 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9570 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9571 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9572 | `								return SXERR_ABORT;` |
|       - |  9573 | `							}` |
|     ! 0 |  9574 | `							goto done;` |
|       - |  9575 | `						}` |
|     ! 0 |  9576 | `						continue;` |
|       - |  9577 | `					}` |
|       3 |  9578 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      63 |  9579 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9580 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9581 | `					pGen->pIn++;` |
|       6 |  9582 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9583 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9584 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9585 | `							iProtection = nKwrd;` |
|       6 |  9586 | `							pGen->pIn++;` |
|       2 |  9587 | `						}` |
|       2 |  9588 | `					}` |
|       6 |  9589 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9590 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9591 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9592 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9593 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9594 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9595 | `							return SXERR_ABORT;` |
|       - |  9596 | `						}` |
|     ! 0 |  9597 | `						goto done;` |
|       - |  9598 | `					}` |
|       6 |  9599 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9600 | `				}` |
|      64 |  9601 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9602 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9603 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9604 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9605 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9606 | `						return SXERR_ABORT;` |
|       - |  9607 | `					}` |
|     ! 0 |  9608 | `					goto done;` |
|       - |  9609 | `				}` |
|      64 |  9610 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9611 | `					pGen->pIn++;` |
|     ! 0 |  9612 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9613 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9614 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9615 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9616 | `							return SXERR_ABORT;` |
|       - |  9617 | `						}` |
|     ! 0 |  9618 | `						goto done;` |
|       - |  9619 | `					}` |
|     ! 0 |  9620 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9621 | `				}else{` |
|      64 |  9622 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9623 | `				}` |
|      64 |  9624 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9625 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9626 | `						return SXERR_ABORT;` |
|       - |  9627 | `					}` |
|     ! 0 |  9628 | `					goto done;` |
|       - |  9629 | `				}` |
|       - |  9630 | `			}` |
|      34 |  9631 | `		}else{` |
|     ! 0 |  9632 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9633 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9634 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9635 | `					return SXERR_ABORT;` |
|       - |  9636 | `				}` |
|     ! 0 |  9637 | `				goto done;` |
|       - |  9638 | `			}` |
|       - |  9639 | `		}` |
|       4 |  9640 | `	}` |
|       - |  9641 | `	/* Install the trait */` |
|      65 |  9642 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      65 |  9643 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9644 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9645 | `		return SXERR_ABORT;` |
|       - |  9646 | `	}` |
|      30 |  9647 | `done:` |
|       - |  9648 | `	/* Point beyond the trait body */` |
|      65 |  9649 | `	pGen->pIn = &pEnd[1];` |
|      65 |  9650 | `	pGen->pEnd = pTmp;` |
|      65 |  9651 | `	return PH7_OK;` |
|      35 |  9652 |  |
|       - |  9653 | `/*` |
|       - |  9654 | ` * Compile a user-defined class.` |
|       - |  9655 | ` *  According to the PHP language reference manual` |
|       - |  9656 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9657 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9658 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9659 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9660 | ` *   and functions (called "methods").` |
|       - |  9661 | ` */` |
|   94202 |  9662 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9663 |  |
|       - |  9664 | `	sxi32 rc;` |
|   94207 |  9665 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   94207 |  9666 | `	return rc;` |
|       5 |  9667 |  |
|       - |  9668 | `/*` |
|       - |  9669 | ` * Exception handling.` |
|       - |  9670 | ` *  According to the PHP language reference manual` |
|       - |  9671 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9672 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9673 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9674 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9675 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9676 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9677 | ` *    (or re-thrown) within a catch block.` |
|       - |  9678 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9679 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9680 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9681 | ` *    been defined with set_exception_handler().` |
|       - |  9682 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9683 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9684 | ` */` |
|       - |  9685 | `/*` |
|       - |  9686 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9687 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9688 | ` * indicates failure.` |
|       - |  9689 | ` */` |
|   10586 |  9690 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9691 |  |
|   10591 |  9692 | `	sxi32 rc = SXRET_OK;` |
|   10591 |  9693 | `	if( pRoot->pOp ){` |
|   10583 |  9694 | `		switch( pRoot->pOp->iOp ){` |
|    5289 |  9695 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9696 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9697 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9698 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9699 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9700 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   10583 |  9701 | `			break;` |
|     ! 0 |  9702 | `		default:` |
|       - |  9703 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9704 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9705 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9706 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9707 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9708 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9709 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9710 | `			}` |
|     ! 0 |  9711 | `			break;` |
|       - |  9712 | `		}` |
|    5302 |  9713 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9714 | `		/* Unexpected expression */` |
|     ! 0 |  9715 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9716 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9717 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9718 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9719 | `		}` |
|     ! 0 |  9720 | `	}` |
|   10591 |  9721 | `	return rc;` |
|       5 |  9722 |  |
|       - |  9723 | `/*` |
|       - |  9724 | ` * Compile a 'throw' statement.` |
|       - |  9725 | ` * throw: This is how you trigger an exception.` |
|       - |  9726 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9727 | ` */` |
|   10550 |  9728 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9729 |  |
|   10555 |  9730 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9731 | `	GenBlock *pBlock;` |
|       - |  9732 | `	sxu32 nIdx;` |
|       - |  9733 | `	sxi32 rc;` |
|   10555 |  9734 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9735 | `	/* Compile the expression */` |
|   10555 |  9736 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   10555 |  9737 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9738 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9739 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9740 | `			return SXERR_ABORT;` |
|       - |  9741 | `		}` |
|     ! 0 |  9742 | `		return SXRET_OK;` |
|       - |  9743 | `	}` |
|   10555 |  9744 | `	pBlock = pGen->pCurrent;` |
|       - |  9745 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   48705 |  9746 | `	while(pBlock->pParent){` |
|   48701 |  9747 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   10551 |  9748 | `			break;` |
|       - |  9749 | `		}` |
|       - |  9750 | `		/* Point to the parent block */` |
|   38155 |  9751 | `		pBlock = pBlock->pParent;` |
|       5 |  9752 | `	}` |
|       - |  9753 | `	/* Emit the throw instruction */` |
|   10555 |  9754 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9755 | `	/* Emit the jump */` |
|   10555 |  9756 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   10555 |  9757 | `	return SXRET_OK;` |
|    5280 |  9758 |  |
|       - |  9759 | `/*` |
|       - |  9760 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9761 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9762 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9763 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9764 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9765 | ` */` |
|      36 |  9766 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9767 |  |
|      38 |  9768 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9769 | `	GenBlock *pBlock;` |
|       - |  9770 | `	sxu32 nIdx;` |
|       - |  9771 | `	sxi32 rc;` |
|      18 |  9772 | `	(void)iCompileFlag;` |
|      38 |  9773 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9774 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9775 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9776 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9777 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9778 | `			return SXERR_ABORT;` |
|       - |  9779 | `		}` |
|     ! 0 |  9780 | `		return SXRET_OK;` |
|       - |  9781 | `	}` |
|      38 |  9782 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9783 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9784 | `		return SXERR_ABORT;` |
|       - |  9785 | `	}` |
|      38 |  9786 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9787 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9788 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9789 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9790 | `			return SXERR_ABORT;` |
|       - |  9791 | `		}` |
|     ! 0 |  9792 | `		return SXRET_OK;` |
|       - |  9793 | `	}` |
|       - |  9794 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9795 | `	pBlock = pGen->pCurrent;` |
|      60 |  9796 | `	while( pBlock->pParent ){` |
|      49 |  9797 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9798 | `			break;` |
|       - |  9799 | `		}` |
|      23 |  9800 | `		pBlock = pBlock->pParent;` |
|       1 |  9801 | `	}` |
|      38 |  9802 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9803 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9804 | `	return SXRET_OK;` |
|      20 |  9805 |  |
|       - |  9806 | `/*` |
|       - |  9807 | ` * Compile a 'catch' block.` |
|       - |  9808 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9809 | ` * an object containing the exception information.` |
|       - |  9810 | ` */` |
|     460 |  9811 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 |  9812 |  |
|     465 |  9813 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9814 | `	ph7_exception_block sCatch;` |
|       - |  9815 | `	SySet *pInstrContainer;` |
|       - |  9816 | `	SyString sClassName;` |
|       - |  9817 | `	GenBlock *pCatch;` |
|       - |  9818 | `	SyToken *pToken;` |
|       - |  9819 | `	SyString *pName;` |
|       - |  9820 | `	char *zDup;` |
|       - |  9821 | `	sxi32 rc;` |
|     465 |  9822 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9823 | `	/* Zero the structure */` |
|     465 |  9824 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9825 | `	/* Initialize fields */` |
|     465 |  9826 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     465 |  9827 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     465 |  9828 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9829 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9830 | `			pToken = pGen->pIn;` |
|     ! 0 |  9831 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9832 | `				pToken--;` |
|     ! 0 |  9833 | `			}` |
|     ! 0 |  9834 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9835 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9836 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9837 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9838 | `				return SXERR_ABORT;` |
|       - |  9839 | `			}` |
|     ! 0 |  9840 | `			return SXERR_INVALID;` |
|       - |  9841 | `	}` |
|       - |  9842 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     465 |  9843 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     243 |  9844 | `	for(;;){` |
|       - |  9845 | `		SyBlob sResolved;` |
|     491 |  9846 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     491 |  9847 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 |  9848 | `			SyBlobRelease(&sResolved);` |
|       6 |  9849 | `			pToken = pGen->pIn;` |
|       6 |  9850 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9851 | `				pToken--;` |
|     ! 0 |  9852 | `			}` |
|       8 |  9853 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9854 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9855 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 |  9856 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9857 | `				return SXERR_ABORT;` |
|       - |  9858 | `			}` |
|       6 |  9859 | `			return SXERR_INVALID;` |
|       - |  9860 | `		}` |
|       - |  9861 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9862 | `		 * transient SyBlob allocation. */` |
|     728 |  9863 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     482 |  9864 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     487 |  9865 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     487 |  9866 | `		SyBlobRelease(&sResolved);` |
|     487 |  9867 | `		if( zDup == 0 ){` |
|     ! 0 |  9868 | `			goto Mem;` |
|       - |  9869 | `		}` |
|     487 |  9870 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     487 |  9871 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9872 | `			goto Mem;` |
|       - |  9873 | `		}` |
|       - |  9874 | `		/* Check for '\|' (multi-catch separator) */` |
|     482 |  9875 | `		if( pGen->pIn < pGen->pEnd &&` |
|     482 |  9876 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      31 |  9877 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9878 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9879 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9880 | `			continue;` |
|       - |  9881 | `		}` |
|     461 |  9882 | `		break;` |
|     ! 0 |  9883 | `	}` |
|     456 |  9884 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     461 |  9885 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9886 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9887 | `			pToken = pGen->pIn;` |
|     ! 0 |  9888 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9889 | `				pToken--;` |
|     ! 0 |  9890 | `			}` |
|     ! 0 |  9891 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9892 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9893 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9894 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9895 | `				return SXERR_ABORT;` |
|       - |  9896 | `			}` |
|     ! 0 |  9897 | `			return SXERR_INVALID;` |
|       - |  9898 | `	}` |
|     461 |  9899 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9900 | `	/* Duplicate instance name */` |
|     461 |  9901 | `	pName = &pGen->pIn->sData;` |
|     461 |  9902 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     461 |  9903 | `	if( zDup == 0 ){` |
|     ! 0 |  9904 | `		goto Mem;` |
|       - |  9905 | `	}` |
|     461 |  9906 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     461 |  9907 | `	pGen->pIn++;` |
|     461 |  9908 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9909 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9910 | `		pToken = pGen->pIn;` |
|     ! 0 |  9911 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9912 | `			pToken--;` |
|     ! 0 |  9913 | `		}` |
|     ! 0 |  9914 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9915 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9916 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9917 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9918 | `			return SXERR_ABORT;` |
|       - |  9919 | `		}` |
|     ! 0 |  9920 | `		return SXERR_INVALID;` |
|       - |  9921 | `	}` |
|       - |  9922 | `	/* Compile the block */` |
|     461 |  9923 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9924 | `	/* Create the catch block */` |
|     461 |  9925 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     461 |  9926 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9927 | `		return SXERR_ABORT;` |
|       - |  9928 | `	}` |
|       - |  9929 | `	/* Swap bytecode container */` |
|     461 |  9930 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     461 |  9931 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9932 | `	/* Compile the block */` |
|     461 |  9933 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9934 | `	/* Fix forward jumps now the destination is resolved  */` |
|     461 |  9935 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9936 | `	/* Emit the DONE instruction */` |
|     461 |  9937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9938 | `	/* Leave the block */` |
|     461 |  9939 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9940 | `	/* Restore the default container */` |
|     461 |  9941 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9942 | `	/* Install the catch block */` |
|     461 |  9943 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     461 |  9944 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9945 | `		goto Mem;` |
|       - |  9946 | `	}` |
|     461 |  9947 | `	return SXRET_OK;` |
|     ! 0 |  9948 | `Mem:` |
|     ! 0 |  9949 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9950 | `	return SXERR_ABORT;` |
|     235 |  9951 |  |
|       - |  9952 | `/*` |
|       - |  9953 | ` * Compile a 'try' block.` |
|       - |  9954 | ` * A function using an exception should be in a "try" block.` |
|       - |  9955 | ` * If the exception does not trigger, the code will continue` |
|       - |  9956 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9957 | ` * is "thrown".` |
|       - |  9958 | ` */` |
|     478 |  9959 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 |  9960 |  |
|       - |  9961 | `	ph7_exception *pException;` |
|     483 |  9962 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9963 | `	GenBlock *pTry;` |
|       - |  9964 | `	sxu32 nJmpIdx;` |
|       - |  9965 | `	sxi32 rc;` |
|       - |  9966 | `	/* Create the exception container */` |
|     483 |  9967 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     483 |  9968 | `	if( pException == 0 ){` |
|     ! 0 |  9969 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9970 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9971 | `		return SXERR_ABORT;` |
|       - |  9972 | `	}` |
|       - |  9973 | `	/* Zero the structure */` |
|     483 |  9974 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9975 | `	/* Initialize fields */` |
|     483 |  9976 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     483 |  9977 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     483 |  9978 | `	pException->iHasFinally = 0;` |
|     483 |  9979 | `	pException->iFinallyDone = 0;` |
|     483 |  9980 | `	pException->pVm = pGen->pVm;` |
|       - |  9981 | `	/* Create the try block */` |
|     483 |  9982 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     483 |  9983 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9984 | `		return SXERR_ABORT;` |
|       - |  9985 | `	}` |
|       - |  9986 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     483 |  9987 | `	pTry->pUserData = pException;` |
|       - |  9988 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     483 |  9989 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9990 | `	/* Fix the jump later when the destination is resolved */` |
|     483 |  9991 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     483 |  9992 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9993 | `	/* Compile the block */` |
|     483 |  9994 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     483 |  9995 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9996 | `		return SXERR_ABORT;` |
|       - |  9997 | `	}` |
|       - |  9998 | `	/* Fix forward jumps now the destination is resolved */` |
|     483 |  9999 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10000 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     483 | 10001 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10002 | `	/* Leave the block */` |
|     483 | 10003 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10004 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     483 | 10005 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     476 | 10006 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10007 | `		/* Compile one or more catch blocks */` |
|     456 | 10008 | `		for(;;){` |
|     912 | 10009 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     721 | 10010 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     231 | 10011 | `					break;` |
|       - | 10012 | `			}` |
|     465 | 10013 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     465 | 10014 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10015 | `				return SXERR_ABORT;` |
|       - | 10016 | `			}` |
|       5 | 10017 | `		}` |
|     226 | 10018 | `	}` |
|       - | 10019 | `	/* Compile optional finally block */` |
|     483 | 10020 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     238 | 10021 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10022 | `		SySet *pInstrContainer;` |
|       - | 10023 | `		GenBlock *pFinBlock;` |
|      63 | 10024 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10025 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      63 | 10026 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      63 | 10027 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10028 | `			return SXERR_ABORT;` |
|       - | 10029 | `		}` |
|       - | 10030 | `		/* Swap bytecode container */` |
|      63 | 10031 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      63 | 10032 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10033 | `		/* Compile the finally body */` |
|      63 | 10034 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      63 | 10035 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10036 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10037 | `			return SXERR_ABORT;` |
|       - | 10038 | `		}` |
|       - | 10039 | `		/* Fix forward jumps now the destination is resolved */` |
|      63 | 10040 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10041 | `		/* Emit DONE to terminate the finally block */` |
|      63 | 10042 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10043 | `		/* Leave the block */` |
|      63 | 10044 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10045 | `		/* Restore the default container */` |
|      63 | 10046 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      63 | 10047 | `		pException->iHasFinally = 1;` |
|      29 | 10048 | `	}` |
|       - | 10049 | `	/* Must have at least one catch or finally */` |
|     483 | 10050 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       9 | 10051 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10052 | `			"Cannot use try without catch or finally");` |
|       9 | 10053 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10054 | `			return SXERR_ABORT;` |
|       - | 10055 | `		}` |
|       3 | 10056 | `	}` |
|     483 | 10057 | `	return SXRET_OK;` |
|     244 | 10058 |  |
|       - | 10059 | `/*` |
|       - | 10060 | ` * Compile a switch block.` |
|       - | 10061 | ` *  (See block-comment below for more information)` |
|       - | 10062 | ` */` |
|     112 | 10063 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10064 |  |
|     117 | 10065 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10066 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10067 | `		/* Unexpected token */` |
|     ! 0 | 10068 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10069 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10070 | `			return SXERR_ABORT;` |
|       - | 10071 | `		}` |
|     ! 0 | 10072 | `		pGen->pIn++;` |
|     ! 0 | 10073 | `	}` |
|     117 | 10074 | `	pGen->pIn++;` |
|       - | 10075 | `	/* First instruction to execute in this block. */` |
|     117 | 10076 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10077 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10078 | `	 * or the '}' token */` |
|     206 | 10079 | `	for(;;){` |
|     417 | 10080 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10081 | `			/* No more input to process */` |
|     ! 0 | 10082 | `			break;` |
|       - | 10083 | `		}` |
|     417 | 10084 | `		rc = SXRET_OK;` |
|     417 | 10085 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10086 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10087 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10088 | `					/* Unexpected token */` |
|     ! 0 | 10089 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10090 | `						&pGen->pIn->sData);` |
|     ! 0 | 10091 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10092 | `						return SXERR_ABORT;` |
|       - | 10093 | `					}` |
|       - | 10094 | `					/* FALL THROUGH */` |
|     ! 0 | 10095 | `				}` |
|      31 | 10096 | `				rc = SXERR_EOF;` |
|      31 | 10097 | `				break;` |
|       - | 10098 | `			}` |
|      32 | 10099 | `		}else{` |
|       - | 10100 | `			sxi32 nKwrd;` |
|       - | 10101 | `			/* Extract the keyword */` |
|     337 | 10102 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10103 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10104 | `				break;` |
|       - | 10105 | `			}` |
|     253 | 10106 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10107 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10108 | `					/* Unexpected token */` |
|     ! 0 | 10109 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10110 | `						&pGen->pIn->sData);` |
|     ! 0 | 10111 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10112 | `						return SXERR_ABORT;` |
|       - | 10113 | `					}` |
|       - | 10114 | `					/* FALL THROUGH */` |
|     ! 0 | 10115 | `				}` |
|       - | 10116 | `				/* Block compiled */` |
|       3 | 10117 | `				break;` |
|       - | 10118 | `			}` |
|       - | 10119 | `		}` |
|       - | 10120 | `		/* Compile block */` |
|     305 | 10121 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10122 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10123 | `			return SXERR_ABORT;` |
|       - | 10124 | `		}` |
|       5 | 10125 | `	}` |
|     117 | 10126 | `	return rc;` |
|      61 | 10127 |  |
|       - | 10128 | `/*` |
|       - | 10129 | ` * Compile a case eXpression.` |
|       - | 10130 | ` *  (See block-comment below for more information)` |
|       - | 10131 | ` */` |
|      92 | 10132 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10133 |  |
|       - | 10134 | `	SySet *pInstrContainer;` |
|       - | 10135 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10136 | `	sxi32 iNest = 0;` |
|       - | 10137 | `	sxi32 rc;` |
|       - | 10138 | `	/* Delimit the expression */` |
|      97 | 10139 | `	pEnd = pGen->pIn;` |
|     197 | 10140 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10141 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10142 | `			/* Increment nesting level */` |
|       3 | 10143 | `			iNest++;` |
|     196 | 10144 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10145 | `			/* Decrement nesting level */` |
|       3 | 10146 | `			iNest--;` |
|     194 | 10147 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10148 | `			break;` |
|       - | 10149 | `		}` |
|     105 | 10150 | `		pEnd++;` |
|       5 | 10151 | `	}` |
|      97 | 10152 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10153 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10154 | `		if( rc == SXERR_ABORT ){` |
|       - | 10155 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10156 | `			return SXERR_ABORT;` |
|       - | 10157 | `		}` |
|     ! 0 | 10158 | `	}` |
|       - | 10159 | `	/* Swap token stream */` |
|      97 | 10160 | `	pTmp = pGen->pEnd;` |
|      97 | 10161 | `	pGen->pEnd = pEnd;` |
|      97 | 10162 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10163 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10164 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10165 | `	/* Emit the done instruction */` |
|      97 | 10166 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10167 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10168 | `	/* Update token stream */` |
|      97 | 10169 | `	pGen->pIn  = pEnd;` |
|      97 | 10170 | `	pGen->pEnd = pTmp;` |
|      97 | 10171 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10172 | `		return SXERR_ABORT;` |
|       - | 10173 | `	}` |
|      97 | 10174 | `	return SXRET_OK;` |
|      51 | 10175 |  |
|       - | 10176 | `/*` |
|       - | 10177 | ` * Compile the smart switch statement.` |
|       - | 10178 | ` * According to the PHP language reference manual` |
|       - | 10179 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10180 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10181 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10182 | ` *  This is exactly what the switch statement is for.` |
|       - | 10183 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10184 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10185 | ` *  of the outer loop, use continue 2.` |
|       - | 10186 | ` *  Note that switch/case does loose comparision.` |
|       - | 10187 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10188 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10189 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10190 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10191 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10192 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10193 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10194 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10195 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10196 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10197 | ` *  list for the next case.` |
|       - | 10198 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10199 | ` *  or floating-point numbers and strings.` |
|       - | 10200 | ` */` |
|      28 | 10201 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10202 |  |
|       - | 10203 | `	GenBlock *pSwitchBlock;` |
|       - | 10204 | `	SyToken *pTmp,*pEnd;` |
|       - | 10205 | `	ph7_switch *pSwitch;` |
|       - | 10206 | `	sxu32 nToken;` |
|       - | 10207 | `	sxu32 nLine;` |
|       - | 10208 | `	sxi32 rc;` |
|      33 | 10209 | `	nLine = pGen->pIn->nLine;` |
|       - | 10210 | `	/* Jump the 'switch' keyword */` |
|      33 | 10211 | `	pGen->pIn++;` |
|      33 | 10212 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10213 | `		/* Syntax error */` |
|     ! 0 | 10214 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10215 | `		if( rc == SXERR_ABORT ){` |
|       - | 10216 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10217 | `			return SXERR_ABORT;` |
|       - | 10218 | `		}` |
|     ! 0 | 10219 | `		goto Synchronize;` |
|       - | 10220 | `	}` |
|       - | 10221 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10222 | `	pGen->pIn++;` |
|      33 | 10223 | `	pEnd = 0; /* cc warning */` |
|       - | 10224 | `	/* Create the loop block */` |
|      47 | 10225 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10226 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10227 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10228 | `		return SXERR_ABORT;` |
|       - | 10229 | `	}` |
|       - | 10230 | `	/* Delimit the condition */` |
|      33 | 10231 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10232 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10233 | `		/* Empty expression */` |
|     ! 0 | 10234 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10235 | `		if( rc == SXERR_ABORT ){` |
|       - | 10236 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10237 | `			return SXERR_ABORT;` |
|       - | 10238 | `		}` |
|     ! 0 | 10239 | `	}` |
|       - | 10240 | `	/* Swap token streams */` |
|      33 | 10241 | `	pTmp = pGen->pEnd;` |
|      33 | 10242 | `	pGen->pEnd = pEnd;` |
|       - | 10243 | `	/* Compile the expression */` |
|      33 | 10244 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10245 | `	if( rc == SXERR_ABORT ){` |
|       - | 10246 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10247 | `		return SXERR_ABORT;` |
|       - | 10248 | `	}` |
|       - | 10249 | `	/* Update token stream */` |
|      33 | 10250 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10251 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10252 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10253 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10254 | `			return SXERR_ABORT;` |
|       - | 10255 | `		}` |
|     ! 0 | 10256 | `		pGen->pIn++;` |
|     ! 0 | 10257 | `	}` |
|      33 | 10258 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10259 | `	pGen->pEnd = pTmp;` |
|      33 | 10260 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10261 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10262 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10263 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10264 | `				pTmp--;` |
|     ! 0 | 10265 | `			}` |
|       - | 10266 | `			/* Unexpected token */` |
|     ! 0 | 10267 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10268 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10269 | `				return SXERR_ABORT;` |
|       - | 10270 | `			}` |
|     ! 0 | 10271 | `			goto Synchronize;` |
|       - | 10272 | `	}` |
|       - | 10273 | `	/* Set the delimiter token */` |
|      33 | 10274 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10275 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10276 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10277 | `	}else{` |
|      31 | 10278 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10279 | `	}` |
|      33 | 10280 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10281 | `	/* Create the switch blocks container */` |
|      33 | 10282 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10283 | `	if( pSwitch == 0 ){` |
|       - | 10284 | `		/* Abort compilation */` |
|     ! 0 | 10285 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10286 | `		return SXERR_ABORT;` |
|       - | 10287 | `	}` |
|       - | 10288 | `	/* Zero the structure */` |
|      33 | 10289 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10290 | `	/* Initialize fields */` |
|      33 | 10291 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10292 | `	/* Emit the switch instruction */` |
|      33 | 10293 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10294 | `	/* Compile case blocks */` |
|     100 | 10295 | `	for(;;){` |
|       - | 10296 | `		sxu32 nKwrd;` |
|     119 | 10297 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10298 | `			/* No more input to process */` |
|     ! 0 | 10299 | `			break;` |
|       - | 10300 | `		}` |
|     119 | 10301 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10302 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10303 | `				/* Unexpected token */` |
|     ! 0 | 10304 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10305 | `					&pGen->pIn->sData);` |
|     ! 0 | 10306 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10307 | `					return SXERR_ABORT;` |
|       - | 10308 | `				}` |
|       - | 10309 | `				/* FALL THROUGH */` |
|     ! 0 | 10310 | `			}` |
|       - | 10311 | `			/* Block compiled */` |
|     ! 0 | 10312 | `			break;` |
|       - | 10313 | `		}` |
|       - | 10314 | `		/* Extract the keyword */` |
|     119 | 10315 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10316 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10317 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10318 | `				/* Unexpected token */` |
|     ! 0 | 10319 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10320 | `					&pGen->pIn->sData);` |
|     ! 0 | 10321 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10322 | `					return SXERR_ABORT;` |
|       - | 10323 | `				}` |
|       - | 10324 | `				/* FALL THROUGH */` |
|     ! 0 | 10325 | `			}` |
|       - | 10326 | `			/* Block compiled */` |
|       3 | 10327 | `			break;` |
|       - | 10328 | `		}` |
|     117 | 10329 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10330 | `			/*` |
|       - | 10331 | `			 * Accroding to the PHP language reference manual` |
|       - | 10332 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10333 | `			 *  that wasn't matched by the other cases.` |
|       - | 10334 | `			 */` |
|      25 | 10335 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10336 | `				/* Default case already compiled */` |
|     ! 0 | 10337 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10338 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10339 | `					return SXERR_ABORT;` |
|       - | 10340 | `				}` |
|     ! 0 | 10341 | `			}` |
|      25 | 10342 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10343 | `			/* Compile the default block */` |
|      25 | 10344 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10345 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10346 | `				return SXERR_ABORT;` |
|      25 | 10347 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10348 | `				break;` |
|       1 | 10349 | `			}` |
|      98 | 10350 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10351 | `			ph7_case_expr sCase;` |
|       - | 10352 | `			/* Standard case block */` |
|      97 | 10353 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10354 | `			/* initialize the structure */` |
|      97 | 10355 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10356 | `			/* Compile the case expression */` |
|      97 | 10357 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10358 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10359 | `				return SXERR_ABORT;` |
|       - | 10360 | `			}` |
|       - | 10361 | `			/* Compile the case block */` |
|      97 | 10362 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10363 | `			/* Insert in the switch container */` |
|      97 | 10364 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10365 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10366 | `				return SXERR_ABORT;` |
|      97 | 10367 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10368 | `				break;` |
|       - | 10369 | `			}` |
|      47 | 10370 | `		}else{` |
|       - | 10371 | `			/* Unexpected token */` |
|     ! 0 | 10372 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10373 | `				&pGen->pIn->sData);` |
|     ! 0 | 10374 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10375 | `				return SXERR_ABORT;` |
|       - | 10376 | `			}` |
|     ! 0 | 10377 | `			break;` |
|       - | 10378 | `		}` |
|       5 | 10379 | `	}` |
|       - | 10380 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10381 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10382 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10383 | `	/* Release the loop block */` |
|      33 | 10384 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10385 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10386 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10387 | `		pGen->pIn++;` |
|      14 | 10388 | `	}` |
|       - | 10389 | `	/* Statement successfully compiled */` |
|      33 | 10390 | `	return SXRET_OK;` |
|     ! 0 | 10391 | `Synchronize:` |
|       - | 10392 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10393 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10394 | `		pGen->pIn++;` |
|     ! 0 | 10395 | `	}` |
|     ! 0 | 10396 | `	return SXRET_OK;` |
|      19 | 10397 |  |
|       - | 10398 | `/*` |
|       - | 10399 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10400 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10401 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10402 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10403 | ` */` |
|       - | 10404 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10405 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10406 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10407 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10408 |  |
|       - | 10409 | `/*` |
|       - | 10410 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10411 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10412 | ` * patched entries from the pending set.` |
|       - | 10413 | ` */` |
| 2519150 | 10414 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10415 |  |
| 2519155 | 10416 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10417 | `	sxu32 nTarget;` |
|       - | 10418 | `	sxu32 *aIdx;` |
|       - | 10419 | `	sxu32 i;` |
| 2519155 | 10420 | `	if( nCur <= nBaseline ){` |
| 2519065 | 10421 | `		return;` |
|       - | 10422 | `	}` |
|      93 | 10423 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      93 | 10424 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     191 | 10425 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     101 | 10426 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     101 | 10427 | `		if( pInstr ){` |
|     101 | 10428 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 | 10429 | `		}` |
|      52 | 10430 | `	}` |
|      93 | 10431 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1259580 | 10432 |  |
|       - | 10433 |  |
|       - | 10434 | `/*` |
|       - | 10435 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10436 | ` *` |
|       - | 10437 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10438 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10439 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10440 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10441 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10442 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10443 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10444 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10445 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10446 | ` * creates it" behaviour).` |
|       - | 10447 | ` *` |
|       - | 10448 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10449 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10450 | ` */` |
|  409512 | 10451 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10452 |  |
|       - | 10453 | `	static const struct {` |
|       - | 10454 | `		const char *zName;` |
|       - | 10455 | `		sxu32 nByte;` |
|       - | 10456 | `		sxu32 mask;` |
|       - | 10457 | `	} aByRef[] = {` |
|       - | 10458 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10459 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10460 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10461 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10462 | `	};` |
|       - | 10463 | `	sxu32 i;` |
|  409517 | 10464 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1245 | 10465 | `		return 0;` |
|       - | 10466 | `	}` |
| 2041145 | 10467 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1632936 | 10468 | `		if( pName->nByte == aByRef[i].nByte` |
|  838050 | 10469 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      73 | 10470 | `			return aByRef[i].mask;` |
|       - | 10471 | `		}` |
|  816439 | 10472 | `	}` |
|  408209 | 10473 | `	return 0;` |
|  204761 | 10474 |  |
|       - | 10475 | `/*` |
|       - | 10476 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10477 | ` *` |
|       - | 10478 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10479 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10480 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10481 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10482 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10483 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10484 | ` */` |
|  409512 | 10485 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10486 |  |
|       - | 10487 | `	SyToken *p, *pEnd;` |
|  409517 | 10488 | `	pOut->zString = 0;` |
|  409517 | 10489 | `	pOut->nByte = 0;` |
|  409517 | 10490 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10491 | `		return;` |
|       - | 10492 | `	}` |
|  409517 | 10493 | `	p = pLeft->pStart;` |
|  409517 | 10494 | `	pEnd = pLeft->pEnd;` |
|       - | 10495 | `	/* Optional single leading namespace separator (absolute path). */` |
|  409517 | 10496 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      26 | 10497 | `		p++;` |
|      11 | 10498 | `	}` |
|  409517 | 10499 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1219 | 10500 | `		return;` |
|       - | 10501 | `	}` |
|       - | 10502 | `	/* Must be a single component: nothing follows the name token. */` |
|  408303 | 10503 | `	if( p + 1 != pEnd ){` |
|      30 | 10504 | `		return;` |
|       - | 10505 | `	}` |
|  408277 | 10506 | `	*pOut = p->sData;` |
|  204761 | 10507 |  |
|       - | 10508 | `/*` |
|       - | 10509 | ` * Generate bytecode for a given expression tree.` |
|       - | 10510 | ` * If something goes wrong while generating bytecode` |
|       - | 10511 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10512 | ` * this function takes care of generating the appropriate` |
|       - | 10513 | ` * error message.` |
|       - | 10514 | ` */` |
| 3395314 | 10515 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10516 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10517 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10518 | `	sxi32 iFlags /* Control flags */` |
|       - | 10519 | `	)` |
|       5 | 10520 |  |
|       - | 10521 | `	VmInstr *pInstr;` |
|       - | 10522 | `	sxu32 nJmpIdx;` |
| 3395319 | 10523 | `	sxi32 iP1 = 0;` |
| 3395319 | 10524 | `	sxu32 iP2 = 0;` |
| 3395319 | 10525 | `	void *p3  = 0;` |
|       - | 10526 | `	sxi32 iVmOp;` |
|       - | 10527 | `	sxi32 rc;` |
| 3395319 | 10528 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3395319 | 10529 | `	sxu32 nRhsNsBase = 0;` |
| 3395319 | 10530 | `	if( pNode->xCode ){` |
|       - | 10531 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10532 | `		/* Compile node */` |
| 2103177 | 10533 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2103177 | 10534 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2103177 | 10535 | `		RE_SWAP_DELIMITER(pGen);` |
| 2103177 | 10536 | `		return rc;` |
|       - | 10537 | `	}` |
| 1292147 | 10538 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10539 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10540 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10541 | `		return SXERR_ABORT;` |
|       - | 10542 | `	}` |
| 1292147 | 10543 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1292147 | 10544 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10545 | `		sxu32 nJmp = 0;` |
|       - | 10546 | `		sxu32 nNcNsBase;` |
|       - | 10547 | `		VmInstr *pInstrFix;` |
|       - | 10548 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10549 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10550 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10551 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10552 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10553 | `		if( pNode->pRight ){` |
|      59 | 10554 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10555 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10556 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10557 | `				return rc;` |
|       - | 10558 | `			}` |
|      59 | 10559 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10560 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10561 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10562 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10563 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10564 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10565 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10566 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10567 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10568 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10569 | `				pInstrFix->iP2 = 3;` |
|      13 | 10570 | `			}` |
|      28 | 10571 | `		}` |
|       - | 10572 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10573 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10574 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10575 | `		if( pNode->pLeft ){` |
|      59 | 10576 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10577 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10578 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10579 | `				return rc;` |
|       - | 10580 | `			}` |
|      59 | 10581 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10582 | `		}` |
|       - | 10583 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10584 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10585 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10586 | `		if( nJmp > 0 ){` |
|      59 | 10587 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10588 | `			if( pInstrFix ){` |
|      59 | 10589 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10590 | `			}` |
|      28 | 10591 | `		}` |
|      59 | 10592 | `		return SXRET_OK;` |
|       - | 10593 | `	}` |
| 1292091 | 10594 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10595 | `		sxu32 nJz,nJmp;` |
|       - | 10596 | `		sxu32 nTernaryNsBase;` |
|       - | 10597 | `		/* Ternary operator require special handling */` |
|       - | 10598 | `		/* Phase#1: Compile the condition */` |
|    2645 | 10599 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2645 | 10600 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2645 | 10601 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10602 | `			return rc;` |
|       - | 10603 | `		}` |
|       - | 10604 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10605 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10606 | `		 * condition expression, not leak past the ternary. */` |
|    2645 | 10607 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2645 | 10608 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2645 | 10609 | `		if( pNode->pLeft ){` |
|       - | 10610 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10611 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2577 | 10612 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10613 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2577 | 10614 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2577 | 10615 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2577 | 10616 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10617 | `				return rc;` |
|       - | 10618 | `			}` |
|    2577 | 10619 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1291 | 10620 | `		}else{` |
|       - | 10621 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10622 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10623 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10624 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10625 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10626 | `		}` |
|       - | 10627 | `		/* Phase#4: Emit the unconditional jump */` |
|    2645 | 10628 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10629 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2645 | 10630 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2645 | 10631 | `		if( pInstr ){` |
|    2645 | 10632 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1320 | 10633 | `		}` |
|    2645 | 10634 | `		if( !pNode->pLeft ){` |
|       - | 10635 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10636 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10637 | `		}` |
|       - | 10638 | `		/* Phase#6: Compile the 'else' expression */` |
|    2645 | 10639 | `		if( pNode->pRight ){` |
|    2645 | 10640 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2645 | 10641 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2645 | 10642 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10643 | `				return rc;` |
|       - | 10644 | `			}` |
|    2645 | 10645 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1320 | 10646 | `		}` |
|    2645 | 10647 | `		if( nJmp > 0 ){` |
|       - | 10648 | `			/* Phase#7: Fix the unconditional jump */` |
|    2645 | 10649 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2645 | 10650 | `			if( pInstr ){` |
|    2645 | 10651 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1320 | 10652 | `			}` |
|    1320 | 10653 | `		}` |
|       - | 10654 | `		/* All done */` |
|    2645 | 10655 | `		return SXRET_OK;` |
|       - | 10656 | `	}` |
| 1289451 | 10657 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10658 | `	/* Generate code for the left tree */` |
| 1289451 | 10659 | `	if( pNode->pLeft ){` |
| 1289413 | 10660 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1289413 | 10661 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10662 | `			ph7_expr_node **apNode;` |
|  409637 | 10663 | `			int hasSpread = 0;` |
|  409637 | 10664 | `			int hasNamed = 0;` |
|  409637 | 10665 | `			int bAnySpread = 0;` |
|  409637 | 10666 | `			sxu32 byRefMask = 0;` |
|       - | 10667 | `			sxi32 nArgs;` |
|       - | 10668 | `			sxi32 n;` |
|       - | 10669 | `			/* Recurse and generate bytecodes for function arguments */` |
|  409637 | 10670 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  409637 | 10671 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10672 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10673 | `			{` |
|  409637 | 10674 | `				int seenNamed = 0;` |
|  811237 | 10675 | `				for( n = 0; n < nArgs; ++n ){` |
|  401607 | 10676 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10677 | `						seenNamed = 1;` |
|     188 | 10678 | `						hasNamed = 1;` |
|  401515 | 10679 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10680 | `						bAnySpread = 1;` |
|  401413 | 10681 | `					}else if( seenNamed ){` |
|       3 | 10682 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10683 | `							"Cannot use positional argument after named argument");` |
|       3 | 10684 | `						return SXERR_SYNTAX;` |
|       - | 10685 | `					}` |
|  200805 | 10686 | `				}` |
|       - | 10687 | `			}` |
|       - | 10688 | `			/* Read-only load */` |
|  409635 | 10689 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10690 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10691 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10692 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10693 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  409635 | 10694 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  409635 | 10695 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  409630 | 10696 | `				if( pCallName->nByte == 5` |
|  224781 | 10697 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   21003 | 10698 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  399136 | 10699 | `				}else if( pCallName->nByte == 5` |
|  203783 | 10700 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10701 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10702 | `				}` |
|       - | 10703 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10704 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10705 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10706 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10707 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10708 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  409635 | 10709 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10710 | `					SyString sBuiltin;` |
|  409517 | 10711 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  409517 | 10712 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  204756 | 10713 | `				}` |
|  204815 | 10714 | `			}` |
|  811233 | 10715 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  401603 | 10716 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  401603 | 10717 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10718 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10719 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 10720 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 10721 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 10722 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 10723 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  401603 | 10724 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      53 | 10725 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      53 | 10726 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      24 | 10727 | `				}` |
|  401603 | 10728 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  401603 | 10729 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10730 | `					return rc;` |
|       - | 10731 | `				}` |
|       - | 10732 | `				/* Each argument is an independent nullsafe scope. */` |
|  401603 | 10733 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  401603 | 10734 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10735 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10736 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10737 | `					hasSpread = 1;` |
|      10 | 10738 | `				}` |
|  200804 | 10739 | `			}` |
|       - | 10740 | `			/* Total number of given arguments */` |
|  409635 | 10741 | `			iP1 = nArgs;` |
|  409635 | 10742 | `			iP2 = hasSpread;` |
|       - | 10743 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10744 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  409635 | 10745 | `			if( hasNamed ){` |
|     101 | 10746 | `				sxu32 nStrBytes = 0;` |
|       - | 10747 | `				char *zBuf;` |
|     297 | 10748 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10749 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10750 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10751 | `					}` |
|     101 | 10752 | `				}` |
|       - | 10753 | `				{` |
|     101 | 10754 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10755 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10756 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10757 | `				if( pMap ){` |
|     101 | 10758 | `					SyZero(pMap, mapSize);` |
|     101 | 10759 | `					pMap->bHasNamed = 1;` |
|     101 | 10760 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10761 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10762 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10763 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10764 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10765 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10766 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10767 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10768 | `							zBuf += nb;` |
|      91 | 10769 | `						}` |
|       - | 10770 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10771 | `					}` |
|     101 | 10772 | `					p3 = (void *)pMap;` |
|      49 | 10773 | `				}` |
|       - | 10774 | `				}` |
|      49 | 10775 | `			}` |
|       - | 10776 | `			/* Remove stale flags now */` |
|  409635 | 10777 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  204815 | 10778 | `		}` |
| 1289411 | 10779 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1289411 | 10780 | `		if( rc != SXRET_OK ){` |
|      34 | 10781 | `			return rc;` |
|       - | 10782 | `		}` |
| 1289381 | 10783 | `		if( !bIsChainOp ){` |
|       - | 10784 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10785 | `			 * target the end of that LHS chain, which is right here. */` |
|  602081 | 10786 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  301038 | 10787 | `		}` |
| 1289381 | 10788 | `		if( iVmOp == PH7_OP_CALL ){` |
|  409635 | 10789 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  409635 | 10790 | `			if( pInstr ){` |
|  409635 | 10791 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  408397 | 10792 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10793 | `					sxu32 nQual;` |
|  408397 | 10794 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10795 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10796 | `					 * so the later NEW handler (if any) can see it. */` |
|  408397 | 10797 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10798 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10799 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10800 | `					 * imports — class imports must NOT affect function` |
|       - | 10801 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10802 | `					 * before NEW; we store the original literal index in the` |
|       - | 10803 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10804 | `					 * the unqualified name and re-qualify with class imports. */` |
|  408397 | 10805 | `					if( bAbsolute ){` |
|      26 | 10806 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      15 | 10807 | `					}else{` |
|  408375 | 10808 | `						int fromImport = 0;` |
|  408375 | 10809 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  408375 | 10810 | `						pInstr->iP2 = (sxi32)nQual;` |
|  408375 | 10811 | `						if( nQual != nOrig ){` |
|       - | 10812 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10813 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 10814 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 10815 | `							if( !fromImport ){` |
|       - | 10816 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 10817 | `								if( p3 == 0 ){` |
|      67 | 10818 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10819 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 10820 | `									if( pMap ){` |
|      67 | 10821 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 10822 | `										p3 = (void *)pMap;` |
|      31 | 10823 | `									}` |
|      31 | 10824 | `								}` |
|      67 | 10825 | `								if( p3 ){` |
|      67 | 10826 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10827 | `								}` |
|      31 | 10828 | `							}` |
|      36 | 10829 | `						}` |
|       5 | 10830 | `					}` |
|  205439 | 10831 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10832 | `					/* Method call,flag that */` |
|     963 | 10833 | `					pInstr->iP2 = 1;` |
|     479 | 10834 | `				}` |
|  204820 | 10835 | `			}` |
| 1084566 | 10836 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10837 | `			ph7_expr_node **apNode;` |
|       - | 10838 | `			sxi32 n;` |
|   88837 | 10839 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 10840 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 10841 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 10842 | `			/* Recurse and generate bytecodes for array index */` |
|   88837 | 10843 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  160325 | 10844 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   71493 | 10845 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   71493 | 10846 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   71493 | 10847 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10848 | `					return rc;` |
|       - | 10849 | `				}` |
|       - | 10850 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   71493 | 10851 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   35749 | 10852 | `			}` |
|   88837 | 10853 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   71493 | 10854 | `				iP1 = 1; /* Node have an index associated with it */` |
|   35744 | 10855 | `			}` |
|   88837 | 10856 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 10857 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 10858 | `				iP2 = 4;` |
|   88718 | 10859 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 10860 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 10861 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 10862 | `				iP2 = 5;` |
|   88574 | 10863 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 10864 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 10865 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 10866 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 10867 | `				iP2 = 6;` |
|   88537 | 10868 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10869 | `				/* Create an empty entry when the desired index is not found */` |
|   34989 | 10870 | `				iP2 = 1;` |
|   17497 | 10871 | `			}` |
|  835335 | 10872 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10873 | `			/* POP the left node */` |
|      32 | 10874 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10875 | `		}` |
|  644688 | 10876 | `	}` |
| 1289419 | 10877 | `	rc = SXRET_OK;` |
| 1289419 | 10878 | `	nJmpIdx = 0;` |
|       - | 10879 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10880 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10881 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1289419 | 10882 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     329 | 10883 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     329 | 10884 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     329 | 10885 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     329 | 10886 | `			int isSpecial = 0;` |
|     329 | 10887 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     241 | 10888 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     241 | 10889 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     236 | 10890 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     234 | 10891 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     111 | 10892 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 10893 | `					isSpecial = 1;` |
|      44 | 10894 | `				}` |
|     140 | 10895 | `			}` |
|     373 | 10896 | `			pInstr->iP1 = 0;` |
|     373 | 10897 | `			if( !isSpecial ){` |
|     197 | 10898 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      96 | 10899 | `			}` |
|       - | 10900 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10901 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     285 | 10902 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     197 | 10903 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     197 | 10904 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10905 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 10906 | `					return SXRET_OK;` |
|       - | 10907 | `				}` |
|      75 | 10908 | `			}` |
|     119 | 10909 | `		}` |
|     195 | 10910 | `	}` |
|       - | 10911 | `	/* Generate code for the right tree */` |
| 1289341 | 10912 | `	if( pNode->pRight ){` |
|  711723 | 10913 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10914 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   10845 | 10915 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  706303 | 10916 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10917 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3633 | 10918 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  699069 | 10919 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10920 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 10921 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 10922 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  697242 | 10923 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10924 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10925 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10926 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10927 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10928 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10929 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     101 | 10930 | `			sxu32 nNsJmp = 0;` |
|     101 | 10931 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     101 | 10932 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  697082 | 10933 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  288943 | 10934 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  144469 | 10935 | `		}` |
|  711723 | 10936 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  711723 | 10937 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  711723 | 10938 | `		if( !bIsChainOp ){` |
|       - | 10939 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10940 | `			 * operator instruction is emitted. */` |
|  522927 | 10941 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  261461 | 10942 | `		}` |
|  711723 | 10943 | `		if( iVmOp == PH7_OP_STORE ){` |
|  285235 | 10944 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  285204 | 10945 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10946 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10947 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10948 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10949 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10950 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10951 | `				 */` |
|      74 | 10952 | `				iVmOp = 0;` |
|  285200 | 10953 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  285165 | 10954 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10955 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   79777 | 10956 | `					iP2 = 1;` |
|   39891 | 10957 | `				}else{` |
|  205393 | 10958 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10959 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   34917 | 10960 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   34917 | 10961 | `						iP1 = pInstr->iP1;` |
|   17461 | 10962 | `					}else{` |
|  170481 | 10963 | `						p3 = pInstr->p3;` |
|       - | 10964 | `					}` |
|       - | 10965 | `					/* POP the last dynamic load instruction */` |
|  205393 | 10966 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10967 | `				}` |
|  142585 | 10968 | `			}` |
|  569108 | 10969 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 10970 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 10971 | `			if( pInstr ){` |
|      54 | 10972 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10973 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10974 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10975 | `					 */` |
|      17 | 10976 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 10977 | `					iP1 = pInstr->iP1;` |
|      17 | 10978 | `					iP2 = pInstr->iP2;` |
|      17 | 10979 | `					p3  = pInstr->p3;` |
|       9 | 10980 | `				}else{` |
|      38 | 10981 | `					p3 = pInstr->p3;` |
|       - | 10982 | `				}` |
|      26 | 10983 | `			}` |
|      26 | 10984 | `		}` |
|  355859 | 10985 | `	}` |
| 1289336 | 10986 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    9335 | 10987 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 10988 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 10989 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      21 | 10990 | `		iVmOp = 0;` |
|      10 | 10991 | `	}` |
| 1289341 | 10992 | `	if( iVmOp > 0 ){` |
| 1289097 | 10993 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14197 | 10994 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10995 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10375 | 10996 | `				iP1 = 1;` |
|    5190 | 10997 | `			}` |
| 1282001 | 10998 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10999 | `			/* Namespace-qualify the class name for NEW */ {` |
|   18527 | 11000 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   18527 | 11001 | `				VmInstr *pCallInstr = 0;` |
|   18527 | 11002 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   18429 | 11003 | `					pCallInstr = pPeek;` |
|   18429 | 11004 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    9212 | 11005 | `				}` |
|   18527 | 11006 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   18525 | 11007 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11008 | `					sxu32 nLitForClass;` |
|       - | 11009 | `					/* If the CALL handler already qualified the name using` |
|       - | 11010 | `					 * function imports, recover the original unqualified` |
|       - | 11011 | `					 * literal so we can re-qualify with class imports. */` |
|   18525 | 11012 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11013 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11014 | `					}else{` |
|   18493 | 11015 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11016 | `					}` |
|   18525 | 11017 | `					pPeek->iP1 = 0;` |
|   18525 | 11018 | `					if( !bAbsolute ){` |
|   18507 | 11019 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9256 | 11020 | `					}else{` |
|      22 | 11021 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11022 | `					}` |
|    9260 | 11023 | `				}` |
|       - | 11024 | `			}` |
|   18527 | 11025 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   18527 | 11026 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11027 | `				VmInstr *pPrev;` |
|   18429 | 11028 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   18429 | 11029 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11030 | `					/* Pop the call instruction, preserve named-arg map */` |
|   18429 | 11031 | `					iP1 = pInstr->iP1;` |
|   18429 | 11032 | `					if( pInstr->p3 ){` |
|      43 | 11033 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11034 | `					}` |
|   18429 | 11035 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    9212 | 11036 | `				}` |
|    9217 | 11037 | `			}` |
| 1265644 | 11038 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11039 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11040 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     169 | 11041 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     169 | 11042 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     169 | 11043 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     169 | 11044 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     169 | 11045 | `				int isSpecialIs = 0;` |
|     169 | 11046 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     165 | 11047 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     165 | 11048 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     160 | 11049 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     165 | 11050 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      81 | 11051 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11052 | `						isSpecialIs = 1;` |
|       5 | 11053 | `					}` |
|      81 | 11054 | `				}` |
|     171 | 11055 | `				pInstr->iP1 = 0;` |
|     171 | 11056 | `				if( !isSpecialIs && !bAbsolute ){` |
|     149 | 11057 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      72 | 11058 | `				}` |
|      86 | 11059 | `			}` |
| 1256304 | 11060 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11061 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11062 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11063 | `			 * should not trigger constant lookup. */` |
|  188801 | 11064 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  188801 | 11065 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  188759 | 11066 | `				pInstr->iP1 = 0;` |
|   94377 | 11067 | `			}` |
|  188801 | 11068 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11069 | `				/* Static member access,remember that */` |
|     251 | 11070 | `				iP1 = 1;` |
|     251 | 11071 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     251 | 11072 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 11073 | `					p3 = pInstr->p3;` |
|      38 | 11074 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 11075 | `				}` |
|     123 | 11076 | `			}` |
|   94398 | 11077 | `		}` |
|       - | 11078 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11079 | `		 * This is the primary emit path for user-visible calls. */` |
| 1289095 | 11080 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  428157 | 11081 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  214076 | 11082 | `		}` |
|       - | 11083 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1289095 | 11084 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  644545 | 11085 | `	}` |
| 1289339 | 11086 | `	if( nJmpIdx > 0 ){` |
|       - | 11087 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   14597 | 11088 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   14597 | 11089 | `		if( pInstr ){` |
|   14597 | 11090 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7296 | 11091 | `		}` |
|    7296 | 11092 | `	}` |
| 1289339 | 11093 | `	return rc;` |
| 1697643 | 11094 |  |
|       - | 11095 | `/*` |
|       - | 11096 | ` * Compile a PHP expression.` |
|       - | 11097 | ` * According to the PHP language reference manual:` |
|       - | 11098 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11099 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11100 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11101 | ` *  is "anything that has a value".` |
|       - | 11102 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11103 | ` * function takes care of generating the appropriate error` |
|       - | 11104 | ` * message.` |
|       - | 11105 | ` */` |
|  913294 | 11106 | `static sxi32 PH7_CompileExpr(` |
|       - | 11107 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11108 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11109 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11110 | `	)` |
|       5 | 11111 |  |
|       - | 11112 | `	ph7_expr_node *pRoot;` |
|       - | 11113 | `	SySet sExprNode;` |
|       - | 11114 | `	SyToken *pEnd;` |
|       - | 11115 | `	sxi32 nExpr;` |
|       - | 11116 | `	sxi32 iNest;` |
|       - | 11117 | `	sxi32 rc;` |
|       - | 11118 | `	sxu32 nNullsafeBase;` |
|       - | 11119 | `	/* Initialize worker variables */` |
|  913299 | 11120 | `	nExpr = 0;` |
|  913299 | 11121 | `	pRoot = 0;` |
|       - | 11122 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11123 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  913299 | 11124 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  913299 | 11125 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  913299 | 11126 | `	SySetAlloc(&sExprNode,0x10);` |
|  913299 | 11127 | `	rc = SXRET_OK;` |
|       - | 11128 | `	/* Delimit the expression */` |
|  913299 | 11129 | `	pEnd = pGen->pIn;` |
|  913299 | 11130 | `	iNest = 0;` |
| 6111133 | 11131 | `	while( pEnd < pGen->pEnd ){` |
| 5799909 | 11132 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11133 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     463 | 11134 | `			iNest++;` |
| 5799680 | 11135 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     471 | 11136 | `			iNest--;` |
| 5799218 | 11137 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  602403 | 11138 | `			if( iNest <= 0 ){` |
|  602075 | 11139 | `				break;` |
|       - | 11140 | `			}` |
|     164 | 11141 | `		}` |
| 5197839 | 11142 | `		pEnd++;` |
|       5 | 11143 | `	}` |
|  913299 | 11144 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   21243 | 11145 | `		SyToken *pEnd2 = pGen->pIn;` |
|   21243 | 11146 | `		iNest = 0;` |
|       - | 11147 | `		/* Stop at the first comma */` |
|   42775 | 11148 | `		while( pEnd2 < pEnd ){` |
|   21543 | 11149 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      67 | 11150 | `				iNest++;` |
|   21512 | 11151 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      67 | 11152 | `				iNest--;` |
|   21450 | 11153 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11154 | `				if( iNest <= 0 ){` |
|       7 | 11155 | `					break;` |
|       - | 11156 | `				}` |
|      23 | 11157 | `			}` |
|   21537 | 11158 | `			pEnd2++;` |
|       5 | 11159 | `		}` |
|   21243 | 11160 | `		if( pEnd2 <pEnd ){` |
|       7 | 11161 | `			pEnd = pEnd2;` |
|       3 | 11162 | `		}` |
|   10619 | 11163 | `	}` |
|  913299 | 11164 | `	if( pEnd > pGen->pIn ){` |
|  913289 | 11165 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11166 | `		/* Swap delimiter */` |
|  913289 | 11167 | `		pGen->pEnd = pEnd;` |
|       - | 11168 | `		/* Try to get an expression tree */` |
|  913289 | 11169 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  913289 | 11170 | `		if( rc == SXRET_OK && pRoot ){` |
|  913107 | 11171 | `			rc = SXRET_OK;` |
|  913107 | 11172 | `			if( xTreeValidator ){` |
|       - | 11173 | `				/* Call the upper layer validator callback */` |
|   25079 | 11174 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   12537 | 11175 | `			}` |
|  913107 | 11176 | `			if( rc != SXERR_ABORT ){` |
|       - | 11177 | `				/* Generate code for the given tree */` |
|  913107 | 11178 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11179 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11180 | `				 * expression so they short-circuit to its end. */` |
|  913107 | 11181 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  456551 | 11182 | `			}` |
|  913107 | 11183 | `			nExpr = 1;` |
|  456551 | 11184 | `		}` |
|       - | 11185 | `		/* Release the whole tree */` |
|  913289 | 11186 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11187 | `		/* Synchronize token stream */` |
|  913289 | 11188 | `		pGen->pEnd = pTmp;` |
|  913289 | 11189 | `		pGen->pIn  = pEnd;` |
|  913289 | 11190 | `		if( rc == SXERR_ABORT ){` |
|      14 | 11191 | `			SySetRelease(&sExprNode);` |
|      14 | 11192 | `			return SXERR_ABORT;` |
|       - | 11193 | `		}` |
|  456637 | 11194 | `	}` |
|  913289 | 11195 | `	SySetRelease(&sExprNode);` |
|  913289 | 11196 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  456652 | 11197 |  |
|       - | 11198 | `/*` |
|       - | 11199 | ` * Return a pointer to the node construct handler associated` |
|       - | 11200 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11201 | ` */` |
|  231596 | 11202 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11203 |  |
|  231601 | 11204 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11205 | `		/* Numeric literal: Either real or integer */` |
|  121851 | 11206 | `		return PH7_CompileNumLiteral;` |
|  109755 | 11207 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11208 | `		/* Double quoted string */` |
|   22399 | 11209 | `		return PH7_CompileString;` |
|   87361 | 11210 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11211 | `		/* Single quoted string */` |
|   87245 | 11212 | `		return PH7_CompileSimpleString;` |
|     121 | 11213 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11214 | `		/* Heredoc */` |
|      68 | 11215 | `		return PH7_CompileHereDoc;` |
|      56 | 11216 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11217 | `		/* Nowdoc */` |
|      50 | 11218 | `		return PH7_CompileNowDoc;` |
|       8 | 11219 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11220 | `		/* Backtick quoted string */` |
|       6 | 11221 | `		return PH7_CompileBacktic;` |
|       - | 11222 | `	}` |
|       3 | 11223 | `	return 0;` |
|  115803 | 11224 |  |
|       - | 11225 | `/*` |
|       - | 11226 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11227 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11228 | ` * in write context" parse error.` |
|       - | 11229 | ` */` |
|    6824 | 11230 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11231 |  |
|       - | 11232 | `	sxi32 rc;` |
|    6829 | 11233 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6827 | 11234 | `		return SXRET_OK;` |
|       - | 11235 | `	}` |
|       5 | 11236 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11237 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11238 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11239 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3417 | 11240 |  |
|       - | 11241 | `/*` |
|       - | 11242 | ` * Compile an unset() statement.` |
|       - | 11243 | ` * unset($var, $arr[$key], ...);` |
|       - | 11244 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11245 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11246 | ` * parent array before extracting the element to unset.` |
|       - | 11247 | ` */` |
|    2946 | 11248 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11249 |  |
|    2951 | 11250 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2951 | 11251 | `	sxu32 nIdx = 0;` |
|       - | 11252 | `	SyString sName;` |
|       - | 11253 | `	sxi32 rc;` |
|       - | 11254 | `	/* Jump the 'unset' keyword */` |
|    2951 | 11255 | `	pGen->pIn++;` |
|       - | 11256 | `	/* Save delimiter */` |
|    2951 | 11257 | `	pTmp = pGen->pEnd;` |
|       - | 11258 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2951 | 11259 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2951 | 11260 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11261 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11262 | `		SyToken *pClose;` |
|    2951 | 11263 | `		pGen->pIn++;   /* Skip '(' */` |
|    2951 | 11264 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2951 | 11265 | `		pEnd = pClose; /* Stop at ')' */` |
|    1473 | 11266 | `	}` |
|    2951 | 11267 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11268 | `	/* Resolve the 'unset' builtin name once */` |
|    2951 | 11269 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     363 | 11270 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     363 | 11271 | `		if( pObj == 0 ){` |
|     ! 0 | 11272 | `			return SXERR_ABORT;` |
|       - | 11273 | `		}` |
|     363 | 11274 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     363 | 11275 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     179 | 11276 | `	}` |
|       - | 11277 | `	/* Compile each comma-separated argument */` |
|    9777 | 11278 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6831 | 11279 | `		if( pGen->pIn < pNext ){` |
|    6831 | 11280 | `			pGen->pEnd = pNext;` |
|    6831 | 11281 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11282 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11283 | `				GenStateUnsetValidator);` |
|    6831 | 11284 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11285 | `				return SXERR_ABORT;` |
|       - | 11286 | `			}` |
|    6831 | 11287 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11288 | `				/* Emit call for this single argument */` |
|    6829 | 11289 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6829 | 11290 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6829 | 11291 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3412 | 11292 | `			}` |
|    3413 | 11293 | `		}` |
|       - | 11294 | `		/* Jump trailing commas */` |
|   10713 | 11295 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3887 | 11296 | `			pNext++;` |
|       5 | 11297 | `		}` |
|    6831 | 11298 | `		pGen->pIn = pNext;` |
|       5 | 11299 | `	}` |
|       - | 11300 | `	/* Skip past the closing ')' if present */` |
|    2951 | 11301 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2951 | 11302 | `		pGen->pIn++;` |
|    1473 | 11303 | `	}` |
|       - | 11304 | `	/* Restore token stream */` |
|    2951 | 11305 | `	pGen->pEnd = pTmp;` |
|    2951 | 11306 | `	return SXRET_OK;` |
|    1478 | 11307 |  |
|       - | 11308 | `/*` |
|       - | 11309 | ` * PHP Language construct table.` |
|       - | 11310 | ` */` |
|       - | 11311 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11312 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11313 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11314 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11315 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11316 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11317 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11318 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11319 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11320 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11321 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11322 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11323 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11324 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11325 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11326 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11327 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11328 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11329 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11330 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11331 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11332 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11333 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11334 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11335 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11336 | `};` |
|       - | 11337 | `/*` |
|       - | 11338 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11339 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11340 | ` */` |
|  615870 | 11341 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11342 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11343 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11344 | `	)` |
|       5 | 11345 |  |
|  615875 | 11346 | `	sxu32 n = 0;` |
| 3180756 | 11347 | `	for(;;){` |
| 6361517 | 11348 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  132525 | 11349 | `			break;` |
|       - | 11350 | `		}` |
| 6228997 | 11351 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  483355 | 11352 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11353 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11354 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11355 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11356 | `					return 0;` |
|       - | 11357 | `				}` |
|     ! 0 | 11358 | `			}` |
|  483350 | 11359 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11360 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11361 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11362 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11363 | `				return 0;` |
|       - | 11364 | `			}` |
|       - | 11365 | `			/* Return a pointer to the handler.` |
|       - | 11366 | `			*/` |
|  483355 | 11367 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11368 | `		}` |
| 5745647 | 11369 | `		n++;` |
|       5 | 11370 | `	}` |
|  132525 | 11371 | `	if( pLookahed ){` |
|  132525 | 11372 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   38007 | 11373 | `			return PH7_CompileClassInterface;` |
|   94523 | 11374 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   94207 | 11375 | `			return PH7_CompileClass;` |
|     321 | 11376 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      65 | 11377 | `			return PH7_CompileTrait;` |
|       - | 11378 | `		}` |
|       - | 11379 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11380 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11381 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11382 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     128 | 11383 | `	}` |
|       - | 11384 | `	/* Not a language construct */` |
|     261 | 11385 | `	return 0;` |
|  307940 | 11386 |  |
|       - | 11387 | `/*` |
|       - | 11388 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11389 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11390 | ` */` |
|     256 | 11391 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11392 |  |
|       - | 11393 | `	int rc;` |
|     261 | 11394 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     261 | 11395 | `	if( rc == FALSE ){` |
|     146 | 11396 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     145 | 11397 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11398 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11399 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11400 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11401 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11402 | `			*/` |
|       - | 11403 | `			){` |
|     143 | 11404 | `				rc = TRUE;` |
|      69 | 11405 | `		}` |
|      73 | 11406 | `	}` |
|     261 | 11407 | `	return rc;` |
|       5 | 11408 |  |
|       - | 11409 | `/*` |
|       - | 11410 | ` * Compile a PHP chunk.` |
|       - | 11411 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11412 | ` * takes care of generating the appropriate error message.` |
|       - | 11413 | ` */` |
|  737858 | 11414 | `static sxi32 GenStateCompileChunk(` |
|       - | 11415 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11416 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11417 | `	)` |
|       5 | 11418 |  |
|       - | 11419 | `	ProcLangConstruct xCons;` |
|       - | 11420 | `	sxi32 rc;` |
|  737863 | 11421 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  574359 | 11422 | `	for(;;){` |
|  943293 | 11423 | `		int bStmtIsDeclare = 0;` |
|  943293 | 11424 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11425 | `			/* No more input to process */` |
|   14033 | 11426 | `			break;` |
|       - | 11427 | `		}` |
|       - | 11428 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11429 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  929265 | 11430 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  615901 | 11431 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  615901 | 11432 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11433 | `				bStmtIsDeclare = 1;` |
|      20 | 11434 | `			}` |
|  307948 | 11435 | `		}` |
|  929265 | 11436 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11437 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11438 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  205405 | 11439 | `			pGen->bStrictTypesLocked = 1;` |
|  102700 | 11440 | `		}` |
|  929265 | 11441 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11442 | `			/* Compile block */` |
|      21 | 11443 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      21 | 11444 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11445 | `				break;` |
|       - | 11446 | `			}` |
|      13 | 11447 | `		}else{` |
|  929249 | 11448 | `			xCons = 0;` |
|  929249 | 11449 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11450 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11451 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11452 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      57 | 11453 | `				xCons = PH7_CompileClassModifiers;` |
|  929223 | 11454 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  615875 | 11455 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11456 | `				/* Try to extract a language construct handler */` |
|  615875 | 11457 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  615875 | 11458 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11459 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11460 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11461 | `						&pGen->pIn->sData);` |
|       9 | 11462 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11463 | `						break;` |
|       - | 11464 | `					}` |
|       - | 11465 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11466 | `					 * this erroneous statement.` |
|       - | 11467 | `					 */` |
|       9 | 11468 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11469 | `				}` |
|  621262 | 11470 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   51355 | 11471 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11472 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11473 | `				xCons = PH7_CompileLabel;` |
|      56 | 11474 | `			}` |
|  929249 | 11475 | `			if( xCons == 0 ){` |
|       - | 11476 | `				/* Assume an expression an try to compile it */` |
|  313463 | 11477 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  313463 | 11478 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11479 | `					/* Pop l-value */` |
|  313313 | 11480 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  156654 | 11481 | `				}` |
|  156734 | 11482 | `			}else{` |
|       - | 11483 | `				/* Go compile the sucker */` |
|  615791 | 11484 | `				rc = xCons(&(*pGen));` |
|       - | 11485 | `			}` |
|  929249 | 11486 | `			if( rc == SXERR_ABORT ){` |
|       - | 11487 | `				/* Request to abort compilation */` |
|      14 | 11488 | `				break;` |
|       - | 11489 | `			}` |
|       - | 11490 | `		}` |
|       - | 11491 | `		/* Ignore trailing semi-colons ';' */` |
| 1503351 | 11492 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  574101 | 11493 | `			pGen->pIn++;` |
|       5 | 11494 | `		}` |
|  929255 | 11495 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11496 | `			/* Compile a single statement and return */` |
|  723825 | 11497 | `			break;` |
|       - | 11498 | `		}` |
|       - | 11499 | `		/* LOOP ONE */` |
|       - | 11500 | `		/* LOOP TWO */` |
|       - | 11501 | `		/* LOOP THREE */` |
|       - | 11502 | `		/* LOOP FOUR */` |
|       5 | 11503 | `	}` |
|       - | 11504 | `	/* Return compilation status */` |
|  737863 | 11505 | `	return rc;` |
|       5 | 11506 |  |
|       - | 11507 | `/*` |
|       - | 11508 | ` * Compile a Raw PHP chunk.` |
|       - | 11509 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11510 | ` * takes care of generating the appropriate error message.` |
|       - | 11511 | ` */` |
|   14040 | 11512 | `static sxi32 PH7_CompilePHP(` |
|       - | 11513 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11514 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11515 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11516 | `	)` |
|       5 | 11517 |  |
|   14045 | 11518 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11519 | `	sxi32 rc;` |
|       - | 11520 | `	/* Reset the token set */` |
|   14045 | 11521 | `	SySetReset(&(*pTokenSet));` |
|       - | 11522 | `	/* Mark as the default token set */` |
|   14045 | 11523 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11524 | `	/* Advance the stream cursor */` |
|   14045 | 11525 | `	pGen->pRawIn++;` |
|       - | 11526 | `	/* Tokenize the PHP chunk first */` |
|   14045 | 11527 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11528 | `	/* Point to the head and tail of the token stream. */` |
|   14045 | 11529 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14045 | 11530 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14045 | 11531 | `	if( is_expr ){` |
|     ! 0 | 11532 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11533 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11534 | `			/* A simple expression,compile it */` |
|     ! 0 | 11535 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11536 | `		}` |
|       - | 11537 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11538 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11539 | `		return SXRET_OK;` |
|       - | 11540 | `	}` |
|   14045 | 11541 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11542 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11543 | `		/*` |
|       - | 11544 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11545 | `		 * According to the PHP reference manual:` |
|       - | 11546 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11547 | `		 *  immediately follow` |
|       - | 11548 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11549 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11550 | `		 * Symisc extension:` |
|       - | 11551 | `		 *   This short syntax works with all PHP opening` |
|       - | 11552 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11553 | `		 *   only short tag.` |
|       - | 11554 | `		 */` |
|       - | 11555 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11556 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11557 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11558 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11559 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11560 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11561 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11562 | `		}` |
|       3 | 11563 | `		return SXRET_OK;` |
|       - | 11564 | `	}` |
|       - | 11565 | `	/* Compile the PHP chunk */` |
|   14043 | 11566 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11567 | `	/* Fix exceptions jumps */` |
|   14043 | 11568 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11569 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14043 | 11570 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11571 | `		rc = SXERR_ABORT;` |
|       1 | 11572 | `	}` |
|       - | 11573 | `	/* Reset container */` |
|   14043 | 11574 | `	SySetReset(&pGen->aGoto);` |
|   14043 | 11575 | `	SySetReset(&pGen->aLabel);` |
|   14043 | 11576 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11577 | `	/* Compilation result */` |
|   14043 | 11578 | `	return rc;` |
|    7025 | 11579 |  |
|       - | 11580 | `/*` |
|       - | 11581 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11582 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11583 | ` * This is the only compile interface exported from this file.` |
|       - | 11584 | ` */` |
|   16910 | 11585 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11586 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11587 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11588 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11589 | `	)` |
|       5 | 11590 |  |
|       - | 11591 | `	SySet aPhpToken,aRawToken;` |
|       - | 11592 | `	ph7_gen_state *pCodeGen;` |
|       - | 11593 | `	ph7_value *pRawObj;` |
|       - | 11594 | `	sxu32 nObjIdx;` |
|       - | 11595 | `	sxi32 nRawObj;` |
|       - | 11596 | `	int is_expr;` |
|       - | 11597 | `	sxi8 bSavedStrict;` |
|       - | 11598 | `	sxi8 bSavedStrictLocked;` |
|       - | 11599 | `	sxi32 rc;` |
|   16915 | 11600 | `	if( pScript->nByte < 1 ){` |
|       - | 11601 | `		/* Nothing to compile */` |
|     ! 0 | 11602 | `		return PH7_OK;` |
|       - | 11603 | `	}` |
|       - | 11604 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11605 | `	 * file's flags so include/require restore them on return. */` |
|   16915 | 11606 | `	pCodeGen = &pVm->sCodeGen;` |
|   16915 | 11607 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   16915 | 11608 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   16915 | 11609 | `	pCodeGen->bStrictTypes = 0;` |
|   16915 | 11610 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11611 | `	/* Initialize the tokens containers */` |
|   16915 | 11612 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16915 | 11613 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   16915 | 11614 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   16915 | 11615 | `	is_expr = 0;` |
|   16915 | 11616 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11617 | `		SyToken sTmp;` |
|       - | 11618 | `		/* PHP only: -*/` |
|    3523 | 11619 | `		sTmp.nLine = 1;` |
|    3523 | 11620 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3523 | 11621 | `		sTmp.pUserData = 0;` |
|    3523 | 11622 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3523 | 11623 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3523 | 11624 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11625 | `			/* A simple PHP expression */` |
|     ! 0 | 11626 | `			is_expr = 1;` |
|     ! 0 | 11627 | `		}` |
|    1764 | 11628 | `	}else{` |
|       - | 11629 | `		/* Tokenize raw text */` |
|   13397 | 11630 | `		SySetAlloc(&aRawToken,32);` |
|   13397 | 11631 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11632 | `	}` |
|       - | 11633 | `	/* Process high-level tokens */` |
|   16915 | 11634 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   16915 | 11635 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   16915 | 11636 | `	rc = PH7_OK;` |
|   16915 | 11637 | `	if( is_expr ){` |
|       - | 11638 | `		/* Compile the expression */` |
|     ! 0 | 11639 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11640 | `		goto cleanup;` |
|       - | 11641 | `	}` |
|   16915 | 11642 | `	nObjIdx = 0;` |
|       - | 11643 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11644 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11645 | `	 * preventing namespace bleeding across include()d files. */` |
|   16915 | 11646 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11647 | `	/* Start the compilation process */` |
|   15157 | 11648 | `	for(;;){` |
|   44347 | 11649 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   16903 | 11650 | `			break; /* No more tokens to process */` |
|       - | 11651 | `		}` |
|   27449 | 11652 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11653 | `			/* Compile the PHP chunk */` |
|   14045 | 11654 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14045 | 11655 | `			if( rc == SXERR_ABORT ){` |
|      16 | 11656 | `				break;` |
|       - | 11657 | `			}` |
|   14033 | 11658 | `			continue;` |
|       - | 11659 | `		}` |
|       - | 11660 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13409 | 11661 | `		nRawObj = 0;` |
|   26855 | 11662 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11663 | `			/* Consume the raw chunk without any processing */` |
|   13451 | 11664 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13451 | 11665 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11666 | `				rc = SXERR_MEM;` |
|     ! 0 | 11667 | `				break;` |
|       - | 11668 | `			}` |
|       - | 11669 | `			/* Mark as constant and emit the load constant instruction */` |
|   13451 | 11670 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13451 | 11671 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13451 | 11672 | `			++nRawObj;` |
|   13451 | 11673 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11674 | `		}` |
|   13409 | 11675 | `		if( nRawObj > 0 ){` |
|       - | 11676 | `			/* Emit the consume instruction */` |
|   13409 | 11677 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6702 | 11678 | `		}` |
|    8460 | 11679 | `	}` |
|    8455 | 11680 | `cleanup:` |
|   16915 | 11681 | `	SySetRelease(&aRawToken);` |
|   16915 | 11682 | `	SySetRelease(&aPhpToken);` |
|       - | 11683 | `	/* Restore outer file's strict_types scope */` |
|   16915 | 11684 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   16915 | 11685 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   16915 | 11686 | `	return rc;` |
|    8460 | 11687 |  |
|       - | 11688 | `/*` |
|       - | 11689 | ` * Utility routines.Initialize the code generator.` |
|       - | 11690 | ` */` |
|    3450 | 11691 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11692 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11693 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11694 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11695 | `	)` |
|       5 | 11696 |  |
|    3455 | 11697 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11698 | `	/* Zero the structure */` |
|    3455 | 11699 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11700 | `	/* Initial state */` |
|    3455 | 11701 | `	pGen->pVm  = &(*pVm);` |
|    3455 | 11702 | `	pGen->xErr = xErr;` |
|    3455 | 11703 | `	pGen->pErrData = pErrData;` |
|    3455 | 11704 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3455 | 11705 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3455 | 11706 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3455 | 11707 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3455 | 11708 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11709 | `	/* Error log buffer */` |
|    3455 | 11710 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11711 | `	/* General purpose working buffer */` |
|    3455 | 11712 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11713 | `	/* Namespace state */` |
|    3455 | 11714 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3455 | 11715 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3455 | 11716 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3455 | 11717 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11718 | `	/* Create the global scope */` |
|    3455 | 11719 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11720 | `	/* Point to the global scope */` |
|    3455 | 11721 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3455 | 11722 | `	return SXRET_OK;` |
|       5 | 11723 |  |
|       - | 11724 | `/*` |
|       - | 11725 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11726 | ` */` |
|   20020 | 11727 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11728 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11729 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11730 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11731 | `	)` |
|       5 | 11732 |  |
|   20025 | 11733 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11734 | `	GenBlock *pBlock,*pParent;` |
|       - | 11735 | `	/* Reset state */` |
|   20025 | 11736 | `	SySetReset(&pGen->aLabel);` |
|   20025 | 11737 | `	SySetReset(&pGen->aGoto);` |
|   20025 | 11738 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20025 | 11739 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20025 | 11740 | `	SyBlobRelease(&pGen->sWorker);` |
|   20025 | 11741 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20025 | 11742 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20025 | 11743 | `	SyHashRelease(&pGen->hUseImports);` |
|   20025 | 11744 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20025 | 11745 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20025 | 11746 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20025 | 11747 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20025 | 11748 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11749 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11750 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11751 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11752 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11753 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11754 | `	 * number of unique names, which is acceptable. */` |
|       - | 11755 | `	/* Point to the global scope */` |
|   20025 | 11756 | `	pBlock = pGen->pCurrent;` |
|   20025 | 11757 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11758 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11759 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11760 | `		pBlock = pParent;` |
|     ! 0 | 11761 | `	}` |
|   20025 | 11762 | `	pGen->xErr = xErr;` |
|   20025 | 11763 | `	pGen->pErrData = pErrData;` |
|   20025 | 11764 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20025 | 11765 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20025 | 11766 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20025 | 11767 | `	pGen->nErr = 0;` |
|   20025 | 11768 | `	return SXRET_OK;` |
|       5 | 11769 |  |
|       - | 11770 | `/*` |
|       - | 11771 | ` * Generate a compile-time error message.` |
|       - | 11772 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11773 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11774 | ` * abort compilation immediately.` |
|       - | 11775 | ` */` |
|     602 | 11776 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11777 |  |
|     607 | 11778 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     607 | 11779 | `	const char *zErr = "Error";` |
|       - | 11780 | `	SyString *pFile;` |
|       - | 11781 | `	va_list ap;` |
|       - | 11782 | `	sxi32 rc;` |
|       - | 11783 | `	/* Reset the working buffer */` |
|     607 | 11784 | `	SyBlobReset(pWorker);` |
|       - | 11785 | `	/* Peek the processed file path if available */` |
|     607 | 11786 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     607 | 11787 | `	if( nErrType == E_ERROR ){` |
|       - | 11788 | `		/* Increment the error counter */` |
|     501 | 11789 | `		pGen->nErr++;` |
|     501 | 11790 | `		if( pGen->nErr > 15 ){` |
|       - | 11791 | `			/* Error count limit reached */` |
|       5 | 11792 | `			if( pGen->xErr ){` |
|       5 | 11793 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11794 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11795 | `				if( pFile ){` |
|       5 | 11796 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11797 | `				}` |
|       5 | 11798 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11799 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11800 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11801 | `				}` |
|       2 | 11802 | `			}` |
|       - | 11803 | `			/* Abort immediately */` |
|       5 | 11804 | `			return SXERR_ABORT;` |
|       - | 11805 | `		}` |
|     246 | 11806 | `	}` |
|     603 | 11807 | `	if( pGen->xErr == 0 ){` |
|       - | 11808 | `		/* No available error consumer,return immediately */` |
|       3 | 11809 | `		return SXRET_OK;` |
|       - | 11810 | `	}` |
|     600 | 11811 | `	switch(nErrType){` |
|     494 | 11812 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 11813 | `	case E_WARNING: zErr = "Warning";     break;` |
|      76 | 11814 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 11815 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11816 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11817 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11818 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11819 | `	default:` |
|     ! 0 | 11820 | `		break;` |
|       - | 11821 | `	}` |
|     600 | 11822 | `	rc = SXRET_OK;` |
|       - | 11823 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     600 | 11824 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     600 | 11825 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     600 | 11826 | `	va_start(ap,zFormat);` |
|     600 | 11827 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     600 | 11828 | `	va_end(ap);` |
|     600 | 11829 | `	if( pFile ){` |
|     600 | 11830 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     298 | 11831 | `	}` |
|       - | 11832 | `	/* Append a new line */` |
|     600 | 11833 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     600 | 11834 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11835 | `		/* Consume the generated error message */` |
|     600 | 11836 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     298 | 11837 | `	}` |
|     600 | 11838 | `	return rc;` |
|     306 | 11839 |  |
|       - | 11840 |  |
