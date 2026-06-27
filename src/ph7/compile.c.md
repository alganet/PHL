# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5653/7019 lines (80.54%)

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
|    3818 |   131 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       5 |   132 |  |
|    3823 |   133 | `	GenBlock *pBlock = pCurrent;` |
|   10871 |   134 | `	for(;;){` |
|   21747 |   135 | `		if( pBlock->iFlags & iBlockType ){` |
|    3715 |   136 | `			iCount--; /* Decrement nesting level */` |
|    3715 |   137 | `			if( iCount < 1 ){` |
|       - |   138 | `				/* Block meet with the desired criteria */` |
|    3689 |   139 | `				return pBlock;` |
|       - |   140 | `			}` |
|      13 |   141 | `		}` |
|       - |   142 | `		/* Point to the upper block */` |
|   18063 |   143 | `		pBlock = pBlock->pParent;` |
|   18063 |   144 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   145 | `			/* Forbidden */` |
|      72 |   146 | `			break;` |
|       - |   147 | `		}` |
|       5 |   148 | `	}` |
|       - |   149 | `	/* No such block */` |
|     139 |   150 | `	return 0;` |
|    1914 |   151 |  |
|       - |   152 | `/*` |
|       - |   153 | ` * Initialize a freshly allocated block instance.` |
|       - |   154 | ` */` |
|  843348 |   155 | `static void GenStateInitBlock(` |
|       - |   156 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   157 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   158 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   159 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   160 | `	void *pUserData      /* Upper layer private data */` |
|       - |   161 | `	)` |
|       5 |   162 |  |
|       - |   163 | `	/* Initialize block fields */` |
|  843353 |   164 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  843353 |   165 | `	pBlock->pUserData   = pUserData;` |
|  843353 |   166 | `	pBlock->pGen        = pGen;` |
|  843353 |   167 | `	pBlock->iFlags      = iType;` |
|  843353 |   168 | `	pBlock->pParent     = 0;` |
|  843353 |   169 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  843353 |   170 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  843353 |   171 |  |
|       - |   172 | `/*` |
|       - |   173 | ` * Allocate a new block instance.` |
|       - |   174 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   175 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   176 | ` * processing on failure.` |
|       - |   177 | ` */` |
|  839808 |   178 | `static sxi32 GenStateEnterBlock(` |
|       - |   179 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   180 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   181 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   182 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   183 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   184 | `	)` |
|       5 |   185 |  |
|       - |   186 | `	GenBlock *pBlock;` |
|       - |   187 | `	/* Allocate a new block instance */` |
|  839813 |   188 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  839813 |   189 | `	if( pBlock == 0 ){` |
|       - |   190 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   191 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   192 | `		 */` |
|     ! 0 |   193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   194 | `		/* Abort processing immediately */` |
|     ! 0 |   195 | `		return SXERR_ABORT;` |
|       - |   196 | `	}` |
|       - |   197 | `	/* Zero the structure */` |
|  839813 |   198 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  839813 |   199 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   200 | `	/* Link to the parent block */` |
|  839813 |   201 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   202 | `	/* Mark as the current block */` |
|  839813 |   203 | `	pGen->pCurrent = pBlock;` |
|  839813 |   204 | `	if( ppBlock ){` |
|       - |   205 | `		/* Write a pointer to the new instance */` |
|  408023 |   206 | `		*ppBlock = pBlock;` |
|  204009 |   207 | `	}` |
|  839813 |   208 | `	return SXRET_OK;` |
|  419909 |   209 |  |
|       - |   210 | `/*` |
|       - |   211 | ` * Release block fields without freeing the whole instance.` |
|       - |   212 | ` */` |
|  839800 |   213 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       5 |   214 |  |
|  839805 |   215 | `	SySetRelease(&pBlock->aPostContFix);` |
|  839805 |   216 | `	SySetRelease(&pBlock->aJumpFix);` |
|  839805 |   217 |  |
|       - |   218 | `/*` |
|       - |   219 | ` * Release a block.` |
|       - |   220 | ` */` |
|  839800 |   221 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       5 |   222 |  |
|  839805 |   223 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  839805 |   224 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   225 | `	/* Free the instance */` |
|  839805 |   226 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  839805 |   227 |  |
|       - |   228 | `/*` |
|       - |   229 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   230 | ` */` |
|  839800 |   231 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       5 |   232 |  |
|  839805 |   233 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  839805 |   234 | `	if( pBlock == 0 ){` |
|       - |   235 | `		/* No more block to pop */` |
|     ! 0 |   236 | `		return SXERR_EMPTY;` |
|       - |   237 | `	}` |
|       - |   238 | `	/* Point to the upper block */` |
|  839805 |   239 | `	pGen->pCurrent = pBlock->pParent;` |
|  839805 |   240 | `	if( ppBlock ){` |
|       - |   241 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   242 | `		*ppBlock = pBlock;` |
|     ! 0 |   243 | `	}else{` |
|       - |   244 | `		/* Safely release the block */` |
|  839805 |   245 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   246 | `	}` |
|  839805 |   247 | `	return SXRET_OK;` |
|  419905 |   248 |  |
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
|  239928 |   259 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       5 |   260 |  |
|       - |   261 | `	JumpFixup sJumpFix;` |
|       - |   262 | `	sxi32 rc;` |
|       - |   263 | `	/* Init the JumpFixup structure */` |
|  239933 |   264 | `	sJumpFix.nJumpType = nJumpType;` |
|  239933 |   265 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   266 | `	/* Insert in the jump fixup table */` |
|  239933 |   267 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  239933 |   268 | `	return rc;` |
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
|  584898 |   281 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       5 |   282 |  |
|       - |   283 | `	JumpFixup *aFix;` |
|       - |   284 | `	VmInstr *pInstr;` |
|       - |   285 | `	sxu32 nFixed;` |
|       - |   286 | `	sxu32 n;` |
|       - |   287 | `	/* Point to the jump fixup table */` |
|  584903 |   288 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   289 | `	/* Fix the desired jumps */` |
| 1053975 |   290 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  469077 |   291 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   292 | `			/* Already fixed */` |
|  185533 |   293 | `			continue;` |
|       - |   294 | `		}` |
|  283549 |   295 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   296 | `			/* Not of our interest */` |
|   43623 |   297 | `			continue;` |
|       - |   298 | `		}` |
|       - |   299 | `		/* Point to the instruction to fix */` |
|  239931 |   300 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  239931 |   301 | `		if( pInstr ){` |
|  239931 |   302 | `			pInstr->iP2 = nJumpDest;` |
|  239931 |   303 | `			nFixed++;` |
|       - |   304 | `			/* Mark as fixed */` |
|  239931 |   305 | `			aFix[n].nJumpType = -1;` |
|  119963 |   306 | `		}` |
|  119968 |   307 | `	}` |
|       - |   308 | `	/* Total number of fixed jumps */` |
|  584903 |   309 | `	return nFixed;` |
|       5 |   310 |  |
|       - |   311 | `/*` |
|       - |   312 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   313 | ` * The goto statement can be used to jump to another section` |
|       - |   314 | ` * in the program.` |
|       - |   315 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   316 | ` * statement for more information.` |
|       - |   317 | ` */` |
|  239524 |   318 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       5 |   319 |  |
|       - |   320 | `	JumpFixup *pJump,*aJumps;` |
|       - |   321 | `	Label *pLabel,*aLabel;` |
|       - |   322 | `	VmInstr *pInstr;` |
|       - |   323 | `	sxi32 rc;` |
|       - |   324 | `	sxu32 n;` |
|       - |   325 | `	/* Point to the goto table */` |
|  239529 |   326 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   327 | `	/* Fix */` |
|  239675 |   328 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  239527 |   353 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  239659 |   354 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     137 |   355 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   356 | `			/* Emit a warning */` |
|      40 |   357 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   358 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   359 | `		}` |
|      71 |   360 | `	}` |
|  239527 |   361 | `	return SXRET_OK;` |
|  119767 |   362 |  |
|       - |   363 | `/*` |
|       - |   364 | ` * Check if a given token value is installed in the literal table.` |
|       - |   365 | ` */` |
|  753600 |   366 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       5 |   367 |  |
|       - |   368 | `	SyHashEntry *pEntry;` |
|  753605 |   369 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  753605 |   370 | `	if( pEntry == 0 ){` |
|  332151 |   371 | `		return SXERR_NOTFOUND;` |
|       - |   372 | `	}` |
|  421459 |   373 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  421459 |   374 | `	return SXRET_OK;` |
|  376805 |   375 |  |
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
|  332146 |   386 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       5 |   387 |  |
|  332151 |   388 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  332151 |   389 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  166073 |   390 | `	}` |
|  332151 |   391 | `	return SXRET_OK;` |
|       5 |   392 |  |
|       - |   393 | `/*` |
|       - |   394 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   395 | ` * in the constant table.` |
|       - |   396 | ` */` |
|  124124 |   397 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       5 |   398 |  |
|       - |   399 | `	ph7_value *pObj;` |
|  124129 |   400 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   401 | `	/* Reserve a new constant */` |
|  124129 |   402 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  124129 |   403 | `	if( pObj == 0 ){` |
|     ! 0 |   404 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   405 | `		return 0;` |
|       - |   406 | `	}` |
|  124129 |   407 | `	*pIdx = nIdx;` |
|       - |   408 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   409 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   410 | `	 */` |
|  124129 |   411 | `	return pObj;` |
|   62067 |   412 |  |
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
|  453470 |   427 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       5 |   428 |  |
|       - |   429 | `	VmCallArgMap *pMap;` |
|  453475 |   430 | `	if( !pGen->bStrictTypes ) return p3;` |
|      33 |   431 | `	if( p3 == 0 ){` |
|      31 |   432 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      31 |   433 | `		if( pMap == 0 ) return 0;` |
|      31 |   434 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      31 |   435 | `		p3 = (void *)pMap;` |
|      14 |   436 | `	}` |
|      33 |   437 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      33 |   438 | `	return p3;` |
|  226740 |   439 |  |
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
|  124788 |   498 | `static int GenStateFindBadNumericSeparator(` |
|       - |   499 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       5 |   500 |  |
|  124793 |   501 | `	const char *z = pRaw->zString;` |
|  124793 |   502 | `	sxu32 n = pRaw->nByte;` |
|  124793 |   503 | `	int base = 10;` |
|       - |   504 | `	sxu32 i, start;` |
|  124793 |   505 | `	if( n < 2 ) return 0;` |
|   10349 |   506 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   507 | `		base = 16;` |
|   10314 |   508 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   509 | `		base = 2;` |
|     139 |   510 | `	}` |
|   37421 |   511 | `	for( i = 0; i < n; ++i ){` |
|   27091 |   512 | `		if( z[i] != '_' ) continue;` |
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
|   10335 |   529 | `	return 0;` |
|   62399 |   530 |  |
|       - |   531 | `/*` |
|       - |   532 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   533 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   534 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   535 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   536 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   537 | ` * so callers can bail from the current construct).` |
|       - |   538 | ` */` |
|  124788 |   539 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       5 |   540 |  |
|  124793 |   541 | `	const char *zBad = 0;` |
|  124793 |   542 | `	sxu32 nBad = 0;` |
|       - |   543 | `	SyString sBad;` |
|       - |   544 | `	sxi32 rc;` |
|  124793 |   545 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  124779 |   546 | `		return SXRET_OK;` |
|       - |   547 | `	}` |
|      18 |   548 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      18 |   549 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   550 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      18 |   551 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   552 | `		return SXERR_ABORT;` |
|       - |   553 | `	}` |
|      18 |   554 | `	return SXERR_SYNTAX;` |
|   62399 |   555 |  |
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
|  124774 |   572 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   573 | `	SyMemBackend *pAlloc,` |
|       - |   574 | `	const SyString *pToken,` |
|       - |   575 | `	char *zScratch, sxu32 nScratch,` |
|       - |   576 | `	SyString *pOut, char **pzAlloc)` |
|       5 |   577 |  |
|       - |   578 | `	sxu32 i, j;` |
|  124779 |   579 | `	int hasUnderscore = 0;` |
|       - |   580 | `	char *zBuf;` |
|  124779 |   581 | `	*pzAlloc = 0;` |
|  264229 |   582 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  139707 |   583 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   69730 |   584 | `	}` |
|  124779 |   585 | `	if( !hasUnderscore ){` |
|  124527 |   586 | `		SyStringDupPtr(pOut, pToken);` |
|  124527 |   587 | `		return SXRET_OK;` |
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
|   62392 |   604 |  |
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
|  124760 |   621 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   622 |  |
|  124765 |   623 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  124765 |   624 | `	sxu32 nIdx = 0;` |
|       - |   625 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  124765 |   626 | `	char *zAlloc = 0;` |
|       - |   627 | `	SyString sNum;` |
|       - |   628 | `	sxi32 rc;` |
|   62380 |   629 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  124765 |   630 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  124765 |   631 | `	if( rc != SXRET_OK ){` |
|      14 |   632 | `		return rc;` |
|       - |   633 | `	}` |
|  187130 |   634 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   62375 |   635 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  124755 |   636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   637 | `		return SXERR_ABORT;` |
|       - |   638 | `	}` |
|  124755 |   639 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   640 | `		ph7_value *pObj;` |
|       - |   641 | `		sxi64 iValue;` |
|  124129 |   642 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  124129 |   643 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  124129 |   644 | `		if( pObj == 0 ){` |
|     ! 0 |   645 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   646 | `			return SXERR_ABORT;` |
|       - |   647 | `		}` |
|  124129 |   648 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   62067 |   649 | `	}else{` |
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
|  124755 |   662 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   663 | `	/* Emit the load constant instruction */` |
|  124755 |   664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   665 | `	/* Node successfully compiled */` |
|  124755 |   666 | `	return SXRET_OK;` |
|   62385 |   667 |  |
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
|   92772 |   679 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |   680 |  |
|   92777 |   681 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   682 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   683 | `	ph7_value *pObj;` |
|       - |   684 | `	sxu32 nIdx;` |
|   92777 |   685 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   686 | `	/* Delimit the string */` |
|   92777 |   687 | `	zIn  = pStr->zString;` |
|   92777 |   688 | `	zEnd = &zIn[pStr->nByte];` |
|   92777 |   689 | `	if( zIn >= zEnd ){` |
|       - |   690 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   691 | `		 * rather than reserving a new object each time. */` |
|    7251 |   692 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    7251 |   693 | `		return SXRET_OK;` |
|       - |   694 | `	}` |
|   85531 |   695 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   696 | `		/* Already processed,emit the load constant instruction` |
|       - |   697 | `		 * and return.` |
|       - |   698 | `		 */` |
|   32293 |   699 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   32293 |   700 | `		return SXRET_OK;` |
|       - |   701 | `	}` |
|       - |   702 | `	/* Reserve a new constant */` |
|   53243 |   703 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   53243 |   704 | `	if( pObj == 0 ){` |
|     ! 0 |   705 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   706 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   707 | `		return SXERR_ABORT;` |
|       - |   708 | `	}` |
|   53243 |   709 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   710 | `	/* Compile the node */` |
|   53293 |   711 | `	for(;;){` |
|  106591 |   712 | `		if( zIn >= zEnd ){` |
|       - |   713 | `			/* End of input */` |
|   53243 |   714 | `			break;` |
|       - |   715 | `		}` |
|   53353 |   716 | `		zCur = zIn;` |
|  947537 |   717 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  894189 |   718 | `			zIn++;` |
|       5 |   719 | `		}` |
|   53353 |   720 | `		if( zIn > zCur ){` |
|       - |   721 | `			/* Append raw contents*/` |
|   53329 |   722 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   26662 |   723 | `		}` |
|   53353 |   724 | `		zIn++;` |
|   53353 |   725 | `		if( zIn < zEnd ){` |
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
|   53353 |   740 | `		zIn++;` |
|       5 |   741 | `	}` |
|       - |   742 | `	/* Emit the load constant instruction */` |
|   53243 |   743 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   53243 |   744 | `	if( pStr->nByte < 1024 ){` |
|       - |   745 | `		/* Install in the literal table */` |
|   53243 |   746 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   26619 |   747 | `	}` |
|       - |   748 | `	/* Node successfully compiled */` |
|   53243 |   749 | `	return SXRET_OK;` |
|   46391 |   750 |  |
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
|       4 |   770 |  |
|     114 |   771 | `	SyString *pIn = &pGen->pIn->sData;` |
|     114 |   772 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   773 | `	const char *zPrefix;` |
|       - |   774 | `	const char *z, *zEnd;` |
|       - |   775 | `	char *zBuf, *zDst;` |
|     114 |   776 | `	if( nIndent == 0 ){` |
|       - |   777 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      68 |   778 | `		*pOut = *pIn;` |
|      68 |   779 | `		return SXRET_OK;` |
|       - |   780 | `	}` |
|       - |   781 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   782 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   783 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   784 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   785 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   786 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      47 |   787 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      47 |   788 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   789 | `		zPrefix += 2;` |
|     ! 0 |   790 | `	}else{` |
|      47 |   791 | `		zPrefix += 1;` |
|       - |   792 | `	}` |
|       - |   793 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      47 |   794 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      47 |   795 | `	if( zBuf == 0 ){` |
|     ! 0 |   796 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   797 | `		return SXERR_ABORT;` |
|       - |   798 | `	}` |
|      47 |   799 | `	zDst = zBuf;` |
|      47 |   800 | `	z = pIn->zString;` |
|      47 |   801 | `	zEnd = z + pIn->nByte;` |
|     129 |   802 | `	while( z < zEnd ){` |
|      71 |   803 | `		const char *zLine = z;` |
|       - |   804 | `		sxu32 nLine;` |
|       - |   805 | `		int bEmpty;` |
|     799 |   806 | `		while( z < zEnd && z[0] != '\n' ){` |
|     731 |   807 | `			z++;` |
|       3 |   808 | `		}` |
|      71 |   809 | `		nLine = (sxu32)(z - zLine);` |
|      71 |   810 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      71 |   811 | `		if( !bEmpty ){` |
|       - |   812 | `			sxu32 i;` |
|      67 |   813 | `			if( nLine < nIndent ){` |
|     ! 0 |   814 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   815 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   816 | `					nIndent);` |
|     ! 0 |   817 | `				return SXERR_ABORT;` |
|       - |   818 | `			}` |
|     269 |   819 | `			for( i = 0; i < nIndent; i++ ){` |
|     213 |   820 | `				if( zLine[i] != zPrefix[i] ){` |
|      10 |   821 | `					unsigned char c = (unsigned char)zLine[i];` |
|      10 |   822 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |   823 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   824 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |   825 | `					}else{` |
|       7 |   826 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   827 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   828 | `							nIndent);` |
|       - |   829 | `					}` |
|      10 |   830 | `					return SXERR_ABORT;` |
|       - |   831 | `				}` |
|     103 |   832 | `			}` |
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
|      59 |   847 |  |
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
|    2212 |   916 | `static sxi32 GenStateProcessStringExpression(` |
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
|    2217 |   927 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   928 | `	/* Preallocate some slots */` |
|    2217 |   929 | `	SySetAlloc(&sToken,0x08);` |
|       - |   930 | `	/* Tokenize the text */` |
|    2217 |   931 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   932 | `	/* Swap delimiter */` |
|    2217 |   933 | `	pTmpIn  = pGen->pIn;` |
|    2217 |   934 | `	pTmpEnd = pGen->pEnd;` |
|    2217 |   935 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    2217 |   936 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   937 | `	/* Compile the expression */` |
|    2217 |   938 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   939 | `	/* Restore token stream */` |
|    2217 |   940 | `	pGen->pIn  = pTmpIn;` |
|    2217 |   941 | `	pGen->pEnd = pTmpEnd;` |
|       - |   942 | `	/* Release the token set */` |
|    2217 |   943 | `	SySetRelease(&sToken);` |
|       - |   944 | `	/* Compilation result */` |
|    2217 |   945 | `	return rc;` |
|       5 |   946 |  |
|       - |   947 | `/*` |
|       - |   948 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   949 | ` */` |
|   24438 |   950 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       5 |   951 |  |
|       - |   952 | `	ph7_value *pConstObj;` |
|   24443 |   953 | `	sxu32 nIdx = 0;` |
|       - |   954 | `	/* Reserve a new constant */` |
|   24443 |   955 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   24443 |   956 | `	if( pConstObj == 0 ){` |
|     ! 0 |   957 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   958 | `		return 0;` |
|       - |   959 | `	}` |
|   24443 |   960 | `	(*pCount)++;` |
|   24443 |   961 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   962 | `	/* Emit the load constant instruction */` |
|   24443 |   963 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   24443 |   964 | `	return pConstObj;` |
|   12224 |   965 |  |
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
|   22974 |  1004 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       5 |  1005 |  |
|   22979 |  1006 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1007 | `	const char *zIn,*zCur,*zEnd;` |
|   22979 |  1008 | `	ph7_value *pObj = 0;` |
|       - |  1009 | `	sxi32 iCons;` |
|       - |  1010 | `	sxi32 rc;` |
|       - |  1011 | `	/* Delimit the string */` |
|   22979 |  1012 | `	zIn  = pStr->zString;` |
|   22979 |  1013 | `	zEnd = &zIn[pStr->nByte];` |
|   22979 |  1014 | `	if( zIn >= zEnd ){` |
|       - |  1015 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1016 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1017 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1018 | `		 */` |
|     313 |  1019 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     313 |  1020 | `		return SXRET_OK;` |
|       - |  1021 | `	}` |
|   22671 |  1022 | `	zCur = 0;` |
|       - |  1023 | `	/* Compile the node */` |
|   22671 |  1024 | `	iCons = 0;` |
|   12439 |  1025 | `	for(;;){` |
|   37225 |  1026 | `		zCur = zIn;` |
|  176511 |  1027 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  141503 |  1028 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      67 |  1029 | `				break;` |
|  141379 |  1030 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    2092 |  1031 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|    1047 |  1032 | `					break;` |
|       - |  1033 | `			}` |
|  139291 |  1034 | `			zIn++;` |
|       5 |  1035 | `		}` |
|   37225 |  1036 | `		if( zIn > zCur ){` |
|   17395 |  1037 | `			if( pObj == 0 ){` |
|   16921 |  1038 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   16921 |  1039 | `				if( pObj == 0 ){` |
|     ! 0 |  1040 | `					return SXERR_ABORT;` |
|       - |  1041 | `				}` |
|    8458 |  1042 | `			}` |
|   17395 |  1043 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    8695 |  1044 | `		}` |
|   37225 |  1045 | `		if( zIn >= zEnd ){` |
|   22671 |  1046 | `			break;` |
|       - |  1047 | `		}` |
|   14559 |  1048 | `		if( zIn[0] == '\\' ){` |
|   12347 |  1049 | `			const char *zPtr = 0;` |
|       - |  1050 | `			sxu32 n;` |
|   12347 |  1051 | `			zIn++;` |
|   12347 |  1052 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1053 | `				break;` |
|       - |  1054 | `			}` |
|   12347 |  1055 | `			if( pObj == 0 ){` |
|    7527 |  1056 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    7527 |  1057 | `				if( pObj == 0 ){` |
|     ! 0 |  1058 | `					return SXERR_ABORT;` |
|       - |  1059 | `				}` |
|    3761 |  1060 | `			}` |
|   12347 |  1061 | `			n = sizeof(char); /* size of conversion */` |
|   12347 |  1062 | `			switch( zIn[0] ){` |
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
|    5688 |  1083 | `			case 'n':` |
|       - |  1084 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|   11381 |  1085 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|   11381 |  1086 | `				break;` |
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
|   12347 |  1154 | `			zIn += n;` |
|   12347 |  1155 | `			continue;` |
|       - |  1156 | `		}` |
|    2217 |  1157 | `		if( zIn[0] == '{' ){` |
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
|    2089 |  1191 | `			const char *zExpr = zIn;` |
|       - |  1192 | `			/* Assemble variable name */` |
|    1051 |  1193 | `			for(;;){` |
|       - |  1194 | `				/* Jump leading dollars */` |
|    4191 |  1195 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    2089 |  1196 | `					zIn++;` |
|       5 |  1197 | `				}` |
|    1051 |  1198 | `				for(;;){` |
|   11682 |  1199 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8529 |  1200 | `						zIn++;` |
|       5 |  1201 | `					}` |
|    2107 |  1202 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1203 | `						/* UTF-8 stream */` |
|     ! 0 |  1204 | `						zIn++;` |
|     ! 0 |  1205 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1206 | `							zIn++;` |
|     ! 0 |  1207 | `						}` |
|     ! 0 |  1208 | `						continue;` |
|       - |  1209 | `					}` |
|    2107 |  1210 | `					break;` |
|     ! 0 |  1211 | `				}` |
|    2107 |  1212 | `				if( zIn >= zEnd ){` |
|     197 |  1213 | `					break;` |
|       - |  1214 | `				}` |
|    1915 |  1215 | `				if( zIn[0] == '[' ){` |
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
|    1905 |  1233 | `				}else if(zIn[0] == '{' ){` |
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
|    1901 |  1251 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1252 | `					/* Member access operator '->' */` |
|      21 |  1253 | `					zIn += 2;` |
|    1892 |  1254 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1255 | `					/* Static member access operator '::' */` |
|     ! 0 |  1256 | `					zIn += 2;` |
|     ! 0 |  1257 | `				}else{` |
|     944 |  1258 | `					break;` |
|       - |  1259 | `				}` |
|       3 |  1260 | `			}` |
|       - |  1261 | `			/* Process the expression */` |
|    2089 |  1262 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    2089 |  1263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1264 | `				return SXERR_ABORT;` |
|       - |  1265 | `			}` |
|    2089 |  1266 | `			if( rc != SXERR_EMPTY ){` |
|    2087 |  1267 | `				++iCons;` |
|    1041 |  1268 | `			}` |
|       - |  1269 | `		}` |
|       - |  1270 | `		/* Invalidate the previously used constant */` |
|    2217 |  1271 | `		pObj = 0;` |
|       5 |  1272 | `	}/*for(;;)*/` |
|   22671 |  1273 | `	if( iCons > 1 ){` |
|       - |  1274 | `		/* Concatenate all compiled constants */` |
|    1657 |  1275 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     826 |  1276 | `	}` |
|       - |  1277 | `	/* Node successfully compiled */` |
|   22671 |  1278 | `	return SXRET_OK;` |
|   11492 |  1279 |  |
|       - |  1280 | `/*` |
|       - |  1281 | ` * Compile a double quoted string.` |
|       - |  1282 | ` *  See the block-comment above for more information.` |
|       - |  1283 | ` */` |
|   22914 |  1284 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1285 |  |
|       - |  1286 | `	sxi32 rc;` |
|   22919 |  1287 | `	rc = GenStateCompileString(&(*pGen));` |
|   11457 |  1288 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1289 | `	/* Compilation result */` |
|   22919 |  1290 | `	return rc;` |
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
|      62 |  1308 | `	sOrig = pGen->pIn->sData;` |
|      62 |  1309 | `	pGen->pIn->sData = sStripped;` |
|      62 |  1310 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 |  1311 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1312 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 |  1313 | `	return rc;` |
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
|   21558 |  1334 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   21563 |  1345 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1346 | `	/* Compile the expression*/` |
|   21563 |  1347 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1348 | `	/* Restore token stream */` |
|   21563 |  1349 | `	RE_SWAP_DELIMITER(pGen);` |
|   21563 |  1350 | `	return rc;` |
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
|       5 |  1361 |  |
|      41 |  1362 | `	sxi32 rc = SXRET_OK;` |
|      41 |  1363 | `	if( pRoot->pOp ){` |
|      19 |  1364 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1365 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      17 |  1366 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1367 | `			/* Unexpected expression */` |
|      14 |  1368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1369 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      14 |  1370 | `			if( rc != SXERR_ABORT ){` |
|      14 |  1371 | `				rc = SXERR_INVALID;` |
|       5 |  1372 | `			}` |
|      10 |  1373 | `		}` |
|      31 |  1374 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1375 | `		/* Unexpected expression */` |
|       3 |  1376 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1377 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1378 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1379 | `			rc = SXERR_INVALID;` |
|       1 |  1380 | `		}` |
|       1 |  1381 | `	}` |
|      41 |  1382 | `	return rc;` |
|       5 |  1383 |  |
|       - |  1384 | `/*` |
|       - |  1385 | ` * Find the top-level '=>' (PH7_TK_ARRAY_OP) that separates an array/list entry's` |
|       - |  1386 | ` * key from its value within [pStart,pEnd). The scan skips any '=>' nested inside` |
|       - |  1387 | ` * brackets/parens/braces, inside an arrow-function signature (fn(...) =>), or` |
|       - |  1388 | ` * inside a match() {...} arm — none of which are key/value separators. Returns a` |
|       - |  1389 | ` * pointer to the '=>' token, or pEnd if the entry has no top-level separator.` |
|       - |  1390 | ` */` |
|   23918 |  1391 | `static SyToken * GenStateFindTopLevelArrow(SyToken *pStart,SyToken *pEnd)` |
|       5 |  1392 |  |
|   23923 |  1393 | `	SyToken *pCur = pStart;` |
|   23923 |  1394 | `	sxi32 iNest = 0;` |
|   67737 |  1395 | `	while( pCur < pEnd ){` |
|   49227 |  1396 | `		if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    5409 |  1397 | `			return pCur;` |
|       - |  1398 | `		}` |
|       - |  1399 | `		/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1400 | `		 * The '=>' inside an arrow function introduces the expression body,` |
|       - |  1401 | `		 * not an entry separator. Skip past the signature.` |
|       - |  1402 | `		 */` |
|   43823 |  1403 | `		if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   43817 |  1464 | `		if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     326 |  1465 | `			iNest++;` |
|   43656 |  1466 | `		}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1467 | `			/* Don't worry about mismatched brackets here, the expression` |
|       - |  1468 | `			 * parser will shortly detect any syntax error. */` |
|     326 |  1469 | `			iNest--;` |
|     161 |  1470 | `		}` |
|   43817 |  1471 | `		pCur++;` |
|       5 |  1472 | `	}` |
|   18515 |  1473 | `	return pEnd;` |
|   11964 |  1474 |  |
|       - |  1475 | `/*` |
|       - |  1476 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1477 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1478 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1479 | ` */` |
|   31098 |  1480 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       5 |  1481 |  |
|       - |  1482 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1483 | `	SyToken *pKey,*pCur;` |
|   31103 |  1484 | `	sxi32 iEmitRef = 0;` |
|   31103 |  1485 | `	sxi32 iSpread = 0;` |
|   31103 |  1486 | `	sxi32 nPair = 0;` |
|       - |  1487 | `	sxi32 rc;` |
|   31103 |  1488 | `	xValidator = 0;` |
|   25436 |  1489 | `	for(;;){` |
|       - |  1490 | `		/* Jump leading commas */` |
|   57695 |  1491 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    6823 |  1492 | `			pGen->pIn++;` |
|       5 |  1493 | `		}` |
|   50877 |  1494 | `		pCur = pGen->pIn;` |
|   50877 |  1495 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1496 | `			/* No more entry to process */` |
|   31087 |  1497 | `			break;` |
|       - |  1498 | `		}` |
|   19795 |  1499 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1500 | `			continue;` |
|       - |  1501 | `		}` |
|       - |  1502 | `		/* Compile the key if available */` |
|   19795 |  1503 | `		pKey = pCur;` |
|   19795 |  1504 | `		pCur = GenStateFindTopLevelArrow(pCur,pGen->pIn);` |
|   19795 |  1505 | `		rc = SXERR_EMPTY;` |
|   19795 |  1506 | `		if( pCur < pGen->pIn ){` |
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
|   18990 |  1522 | `		}else if( pKey == pCur ){` |
|       - |  1523 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1524 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1525 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1526 | `		}else{` |
|       - |  1527 | `			/* Reset back the cursor and point to the entry value */` |
|   18195 |  1528 | `			pCur = pKey;` |
|       - |  1529 | `		}` |
|   19785 |  1530 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1531 | `			/* No available key,load NULL */` |
|   18197 |  1532 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    9096 |  1533 | `		}` |
|   19785 |  1534 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   19783 |  1553 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   19783 |  1554 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
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
|   19779 |  1567 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   19779 |  1568 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1569 | `			return SXERR_ABORT;` |
|       - |  1570 | `		}` |
|   19779 |  1571 | `		if( iSpread ){` |
|       - |  1572 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      65 |  1573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   19748 |  1574 | `		}else if( iEmitRef ){` |
|       - |  1575 | `			/* Emit the load reference instruction */` |
|      41 |  1576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      18 |  1577 | `		}` |
|   19779 |  1578 | `		xValidator = 0;` |
|   19779 |  1579 | `		iEmitRef = 0;` |
|   19779 |  1580 | `		iSpread = 0;` |
|   19779 |  1581 | `		nPair++;` |
|       5 |  1582 | `	}` |
|       - |  1583 | `	/* Emit the load map instruction */` |
|   31087 |  1584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1585 | `	/* Node successfully compiled */` |
|   31087 |  1586 | `	return SXRET_OK;` |
|   15554 |  1587 |  |
|       - |  1588 | `/*` |
|       - |  1589 | ` * Compile the 'array' language construct.` |
|       - |  1590 | ` *	 According to the PHP language reference manual` |
|       - |  1591 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1592 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1593 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1594 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1595 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1596 | ` */` |
|   30128 |  1597 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1598 |  |
|       - |  1599 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   30133 |  1600 | `	pGen->pIn += 2;` |
|   30133 |  1601 | `	pGen->pEnd--;` |
|   15064 |  1602 | `	SXUNUSED(iCompileFlag);` |
|   30133 |  1603 | `	return GenStateCompileArrayBody(pGen);` |
|       5 |  1604 |  |
|       - |  1605 | `/*` |
|       - |  1606 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1607 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1608 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1609 | ` */` |
|     970 |  1610 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1611 |  |
|       - |  1612 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     975 |  1613 | `	pGen->pIn++;` |
|     975 |  1614 | `	pGen->pEnd--;` |
|     485 |  1615 | `	SXUNUSED(iCompileFlag);` |
|     975 |  1616 | `	return GenStateCompileArrayBody(pGen);` |
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
|     270 |  1952 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  1953 |  |
|       - |  1954 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1955 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1956 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1957 | `							  * one thread is allowed to compile the script.` |
|       - |  1958 | `						      */` |
|       - |  1959 | `	SyString sName;` |
|       - |  1960 | `	sxu32 nLen;` |
|       - |  1961 | `	sxi32 rc;` |
|     135 |  1962 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1963 |  |
|     275 |  1964 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     275 |  1965 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1966 | `		pGen->pIn++;` |
|     ! 0 |  1967 | `	}` |
|       - |  1968 | `	/* Generate a unique name */` |
|     275 |  1969 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1970 | `	/* Make sure the generated name is unique */` |
|     275 |  1971 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1972 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1973 | `	}` |
|     275 |  1974 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  1975 | `	/* Compile the lambda body */` |
|     275 |  1976 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     275 |  1977 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1978 | `		return SXERR_ABORT;` |
|       - |  1979 | `	}` |
|       - |  1980 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  1981 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  1982 | `	 * the handler wraps either in a Closure instance. */` |
|     275 |  1983 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  1984 | `	/* Node successfully compiled */` |
|     275 |  1985 | `	return SXRET_OK;` |
|     140 |  1986 |  |
|       - |  1987 | `/*` |
|       - |  1988 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1989 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1990 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1991 | ` */` |
|     166 |  1992 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1993 | `	ph7_gen_state *pGen,` |
|       - |  1994 | `	ph7_vm_func *pFunc,` |
|       - |  1995 | `	const char *zName,` |
|       - |  1996 | `	sxu32 nByte,` |
|       - |  1997 | `	SyString *aShadow,` |
|       - |  1998 | `	sxu32 nShadow)` |
|       2 |  1999 |  |
|       - |  2000 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2001 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  2002 | `	sxu32 n, nEnv;` |
|       - |  2003 | `	char *zDup;` |
|     168 |  2004 | `	if( nByte == 0 ){` |
|     ! 0 |  2005 | `		return SXRET_OK;` |
|       - |  2006 | `	}` |
|     166 |  2007 | `	if( nByte == sizeof("this")-1` |
|      89 |  2008 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  2009 | `		return SXRET_OK;` |
|       - |  2010 | `	}` |
|     202 |  2011 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     148 |  2012 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     145 |  2013 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     114 |  2014 | `			return SXRET_OK;` |
|       - |  2015 | `		}` |
|      19 |  2016 | `	}` |
|      53 |  2017 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  2018 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  2019 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  2020 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  2021 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  2022 | `			return SXRET_OK;` |
|       - |  2023 | `		}` |
|      15 |  2024 | `	}` |
|      53 |  2025 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  2026 | `	if( zDup == 0 ){` |
|     ! 0 |  2027 | `		return SXERR_ABORT;` |
|       - |  2028 | `	}` |
|      53 |  2029 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  2030 | `	sEnv.iFlags = 0;` |
|      53 |  2031 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  2032 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  2033 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  2034 | `	return SXRET_OK;` |
|      85 |  2035 |  |
|       - |  2036 | `/*` |
|       - |  2037 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  2038 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  2039 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  2040 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  2041 | ` */` |
|      20 |  2042 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  2043 | `	ph7_gen_state *pGen,` |
|       - |  2044 | `	ph7_vm_func *pFunc,` |
|       - |  2045 | `	const char *zIn,` |
|       - |  2046 | `	const char *zEnd,` |
|       - |  2047 | `	SyString *aShadow,` |
|       - |  2048 | `	sxu32 nShadow)` |
|       1 |  2049 |  |
|       - |  2050 | `	sxi32 rc;` |
|     181 |  2051 | `	while( zIn < zEnd ){` |
|     161 |  2052 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  2053 | `			zIn++;` |
|     ! 0 |  2054 | `			if( zIn < zEnd ){` |
|     ! 0 |  2055 | `				zIn++;` |
|     ! 0 |  2056 | `			}` |
|     ! 0 |  2057 | `			continue;` |
|       - |  2058 | `		}` |
|     160 |  2059 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  2060 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  2061 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  2062 | `			const char *zName;` |
|      13 |  2063 | `			zIn++; /* skip '$' */` |
|      13 |  2064 | `			zName = zIn;` |
|      39 |  2065 | `			while( zIn < zEnd ){` |
|      35 |  2066 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  2067 | `				if( c >= 0xc0 ){` |
|     ! 0 |  2068 | `					zIn++;` |
|     ! 0 |  2069 | `					while( zIn < zEnd` |
|     ! 0 |  2070 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  2071 | `						zIn++;` |
|     ! 0 |  2072 | `					}` |
|     ! 0 |  2073 | `					continue;` |
|       - |  2074 | `				}` |
|      35 |  2075 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  2076 | `					break;` |
|       - |  2077 | `				}` |
|      27 |  2078 | `				zIn++;` |
|       1 |  2079 | `			}` |
|      13 |  2080 | `			if( zIn > zName ){` |
|      19 |  2081 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  2082 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  2083 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2084 | `					return SXERR_ABORT;` |
|       - |  2085 | `				}` |
|       6 |  2086 | `			}` |
|      13 |  2087 | `			continue;` |
|       - |  2088 | `		}` |
|     149 |  2089 | `		zIn++;` |
|       1 |  2090 | `	}` |
|      21 |  2091 | `	return SXRET_OK;` |
|      11 |  2092 |  |
|       - |  2093 | `/*` |
|       - |  2094 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  2095 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  2096 | ` *   - plain $<id> pairs` |
|       - |  2097 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  2098 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  2099 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  2100 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  2101 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  2102 | ` *     are never mistakenly captured.` |
|       - |  2103 | ` */` |
|     162 |  2104 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  2105 | `	ph7_gen_state *pGen,` |
|       - |  2106 | `	ph7_vm_func *pFunc,` |
|       - |  2107 | `	SyToken *pStart,` |
|       - |  2108 | `	SyToken *pEnd,` |
|       - |  2109 | `	SyString *aShadow,` |
|       - |  2110 | `	sxu32 nShadow)` |
|       2 |  2111 |  |
|     164 |  2112 | `	SyToken *pScan = pStart;` |
|       - |  2113 | `	sxi32 rc;` |
|     584 |  2114 | `	while( pScan < pEnd ){` |
|     422 |  2115 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      31 |  2116 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      10 |  2117 | `				pScan->sData.zString,` |
|      20 |  2118 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      10 |  2119 | `				aShadow,nShadow);` |
|      21 |  2120 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2121 | `				return SXERR_ABORT;` |
|       - |  2122 | `			}` |
|      21 |  2123 | `			pScan++;` |
|      21 |  2124 | `			continue;` |
|       - |  2125 | `		}` |
|     402 |  2126 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2127 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2128 | `			SyToken *pFnKw = pScan;` |
|      20 |  2129 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2130 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2131 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2132 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2133 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2134 | `			}` |
|      21 |  2135 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2136 | `				SyToken *pInnerSigStart;` |
|       - |  2137 | `				SyToken *pInnerSigEnd;` |
|       - |  2138 | `				SyToken *pInnerBodyEnd;` |
|       - |  2139 | `				SyString *aInnerShadow;` |
|       - |  2140 | `				sxu32 nInnerShadow;` |
|       - |  2141 | `				sxu32 nInnerParamMax;` |
|       - |  2142 | `				SyToken *p;` |
|       - |  2143 | `				int iNestInner;` |
|      19 |  2144 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2145 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2146 | `					pScan++;` |
|     ! 0 |  2147 | `				}` |
|      19 |  2148 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2149 | `					pScan++;` |
|     ! 0 |  2150 | `					continue;` |
|       - |  2151 | `				}` |
|      19 |  2152 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2153 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2154 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2155 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2156 | `					pScan = pEnd;` |
|     ! 0 |  2157 | `					continue;` |
|       - |  2158 | `				}` |
|       - |  2159 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2160 | `				nInnerParamMax = 0;` |
|      57 |  2161 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2162 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2163 | `						nInnerParamMax++;` |
|       6 |  2164 | `					}` |
|      20 |  2165 | `				}` |
|      19 |  2166 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2167 | `					&pGen->pVm->sAllocator,` |
|      18 |  2168 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2169 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2170 | `					return SXERR_ABORT;` |
|       - |  2171 | `				}` |
|      19 |  2172 | `				nInnerShadow = 0;` |
|      25 |  2173 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2174 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2175 | `				}` |
|      57 |  2176 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2177 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2178 | `						continue;` |
|       - |  2179 | `					}` |
|      13 |  2180 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2181 | `						break;` |
|       - |  2182 | `					}` |
|      13 |  2183 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2184 | `						continue;` |
|       - |  2185 | `					}` |
|      13 |  2186 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2187 | `				}` |
|      19 |  2188 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2189 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2190 | `					pScan++;` |
|     ! 0 |  2191 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2192 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2193 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2194 | `						pScan++;` |
|     ! 0 |  2195 | `					}` |
|     ! 0 |  2196 | `					if( pScan < pEnd` |
|     ! 0 |  2197 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2198 | `						pScan++;` |
|     ! 0 |  2199 | `					}` |
|     ! 0 |  2200 | `				}` |
|      19 |  2201 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2202 | `					pScan++; /* past '=>' */` |
|       9 |  2203 | `				}` |
|      19 |  2204 | `				pInnerBodyEnd = pScan;` |
|      19 |  2205 | `				iNestInner = 0;` |
|     131 |  2206 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2207 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2208 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2209 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2210 | `						break;` |
|       - |  2211 | `					}` |
|     113 |  2212 | `					if( pInnerBodyEnd->nType &` |
|       - |  2213 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2214 | `						iNestInner++;` |
|     112 |  2215 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2216 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2217 | `						iNestInner--;` |
|       1 |  2218 | `					}` |
|     113 |  2219 | `					pInnerBodyEnd++;` |
|       1 |  2220 | `				}` |
|       - |  2221 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2222 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2223 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2224 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2225 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2226 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2227 | `				 *` |
|       - |  2228 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2229 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2230 | `				 * range after the '=' sign. */` |
|       - |  2231 | `				{` |
|      19 |  2232 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2233 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2234 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2235 | `						SyToken *pEq = 0;` |
|      13 |  2236 | `						int iNestArg = 0;` |
|      49 |  2237 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2238 | `							if( iNestArg == 0` |
|      39 |  2239 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2240 | `								break;` |
|       - |  2241 | `							}` |
|      37 |  2242 | `							if( pArgEnd->nType &` |
|       - |  2243 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2244 | `								iNestArg++;` |
|      37 |  2245 | `							}else if( pArgEnd->nType &` |
|       - |  2246 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2247 | `								iNestArg--;` |
|     ! 0 |  2248 | `							}` |
|      36 |  2249 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2250 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2251 | `								pEq = pArgEnd;` |
|       3 |  2252 | `							}` |
|      37 |  2253 | `							pArgEnd++;` |
|       1 |  2254 | `						}` |
|      13 |  2255 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2256 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2257 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2258 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2259 | `								return SXERR_ABORT;` |
|       - |  2260 | `							}` |
|       3 |  2261 | `						}` |
|      13 |  2262 | `						pArgStart = pArgEnd;` |
|      12 |  2263 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2264 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2265 | `							pArgStart++;` |
|       1 |  2266 | `						}` |
|       1 |  2267 | `					}` |
|       - |  2268 | `				}` |
|      28 |  2269 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2270 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2271 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2272 | `					return SXERR_ABORT;` |
|       - |  2273 | `				}` |
|      19 |  2274 | `				pScan = pInnerBodyEnd;` |
|      19 |  2275 | `				continue;` |
|       - |  2276 | `			}` |
|       1 |  2277 | `		}` |
|     384 |  2278 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     230 |  2279 | `			pScan++;` |
|     230 |  2280 | `			continue;` |
|       - |  2281 | `		}` |
|       - |  2282 | `		{` |
|       - |  2283 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     156 |  2284 | `			SyToken *pDollar = pScan;` |
|     231 |  2285 | `			while( &pDollar[1] < pEnd` |
|     156 |  2286 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2287 | `				pDollar++;` |
|     ! 0 |  2288 | `			}` |
|     156 |  2289 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2290 | `				break;` |
|       - |  2291 | `			}` |
|     156 |  2292 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2293 | `				pScan = pDollar + 1;` |
|     ! 0 |  2294 | `				continue;` |
|       - |  2295 | `			}` |
|     233 |  2296 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     154 |  2297 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      77 |  2298 | `				aShadow,nShadow);` |
|     156 |  2299 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2300 | `				return SXERR_ABORT;` |
|       - |  2301 | `			}` |
|     156 |  2302 | `			pScan = pDollar + 2;` |
|       - |  2303 | `		}` |
|       2 |  2304 | `	}` |
|     164 |  2305 | `	return SXRET_OK;` |
|      83 |  2306 |  |
|       - |  2307 | `/*` |
|       - |  2308 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2309 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2310 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2311 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2312 | ` * $this is also made available.` |
|       - |  2313 | ` */` |
|     144 |  2314 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  2315 |  |
|       - |  2316 | `	ph7_vm_func *pFunc;` |
|       - |  2317 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2318 | `	GenBlock *pBlock;` |
|       - |  2319 | `	SySet *pInstrContainer;` |
|       - |  2320 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2321 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2322 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2323 | `	SyToken *pSavedEnd;` |
|       - |  2324 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2325 | `	char zName[512];` |
|       - |  2326 | `	static int iCnt = 1;` |
|       - |  2327 | `	char *zDup;` |
|       - |  2328 | `	sxu32 nLen;` |
|       - |  2329 | `	sxu32 nLine;` |
|     148 |  2330 | `	sxi32 iFlags = 0;` |
|     148 |  2331 | `	int bStatic = 0;` |
|       - |  2332 | `	sxi32 rc;` |
|       - |  2333 | `	sxu32 n;` |
|      72 |  2334 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2335 |  |
|     148 |  2336 | `	nLine = pGen->pIn->nLine;` |
|       - |  2337 | `	/* Optional 'static' prefix */` |
|     144 |  2338 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     148 |  2339 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2340 | `		bStatic = 1;` |
|       3 |  2341 | `		pGen->pIn++;` |
|       1 |  2342 | `	}` |
|       - |  2343 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     144 |  2344 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     148 |  2345 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2346 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2347 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2348 | `		return SXERR_SYNTAX;` |
|       - |  2349 | `	}` |
|     148 |  2350 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2351 | `	/* Optional '&' — return by reference */` |
|     148 |  2352 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2353 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2354 | `		pGen->pIn++;` |
|     ! 0 |  2355 | `	}` |
|       - |  2356 | `	/* Expect '(' */` |
|     148 |  2357 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2358 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2359 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2360 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2361 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2362 | `		}else{` |
|     ! 0 |  2363 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2364 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2365 | `		}` |
|       3 |  2366 | `		return SXERR_SYNTAX;` |
|       - |  2367 | `	}` |
|     146 |  2368 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2369 | `	/* Delimit the parameter list */` |
|     146 |  2370 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     146 |  2371 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2372 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2373 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2374 | `		return SXERR_SYNTAX;` |
|       - |  2375 | `	}` |
|       - |  2376 | `	/* Allocate the function state */` |
|     143 |  2377 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     143 |  2378 | `	if( pFunc == 0 ){` |
|     ! 0 |  2379 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2380 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2381 | `		return SXERR_ABORT;` |
|       - |  2382 | `	}` |
|       - |  2383 | `	/* Generate a unique lambda name */` |
|     143 |  2384 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     245 |  2385 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     104 |  2386 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2387 | `	}` |
|     143 |  2388 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     143 |  2389 | `	if( zDup == 0 ){` |
|     ! 0 |  2390 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2391 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2392 | `		return SXERR_ABORT;` |
|       - |  2393 | `	}` |
|     143 |  2394 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2395 | `	/* Collect function arguments */` |
|     143 |  2396 | `	if( pGen->pIn < pSigEnd ){` |
|     101 |  2397 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     101 |  2398 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2399 | `			return SXERR_ABORT;` |
|       - |  2400 | `		}` |
|      49 |  2401 | `	}` |
|       - |  2402 | `	/* Point past ')' and parse optional return type */` |
|     143 |  2403 | `	pGen->pIn = &pSigEnd[1];` |
|     143 |  2404 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     143 |  2405 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2406 | `		return SXERR_ABORT;` |
|     143 |  2407 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2408 | `		return SXERR_SYNTAX;` |
|       - |  2409 | `	}` |
|       - |  2410 | `	/* Expect '=>' */` |
|     143 |  2411 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2412 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2413 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2414 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2415 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2416 | `		}else{` |
|     ! 0 |  2417 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2418 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2419 | `		}` |
|       3 |  2420 | `		return SXERR_SYNTAX;` |
|       - |  2421 | `	}` |
|     140 |  2422 | `	pGen->pIn++; /* Jump '=>' */` |
|     140 |  2423 | `	pBodyStart = pGen->pIn;` |
|     140 |  2424 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2425 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2426 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2427 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2428 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     140 |  2429 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2430 | `	{` |
|     140 |  2431 | `		SyString *aShadow = 0;` |
|     140 |  2432 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     140 |  2433 | `		if( nShadow > 0 ){` |
|      98 |  2434 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      96 |  2435 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      98 |  2436 | `			if( aShadow == 0 ){` |
|     ! 0 |  2437 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2438 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2439 | `				return SXERR_ABORT;` |
|       - |  2440 | `			}` |
|     216 |  2441 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     120 |  2442 | `				aShadow[n] = aArgs[n].sName;` |
|      61 |  2443 | `			}` |
|      48 |  2444 | `		}` |
|     209 |  2445 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      69 |  2446 | `			aShadow,nShadow);` |
|     140 |  2447 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2448 | `			return SXERR_ABORT;` |
|       - |  2449 | `		}` |
|       - |  2450 | `	}` |
|       - |  2451 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2452 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2453 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2454 | `	 * $this. */` |
|     140 |  2455 | `	if( !bStatic ){` |
|       - |  2456 | `		char *zThisDup;` |
|     138 |  2457 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     138 |  2458 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2459 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2460 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2461 | `			return SXERR_ABORT;` |
|       - |  2462 | `		}` |
|     138 |  2463 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     138 |  2464 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     138 |  2465 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     138 |  2466 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     138 |  2467 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      68 |  2468 | `	}` |
|       - |  2469 | `	/* Arrow functions are always closures */` |
|     140 |  2470 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2471 | `	/* Compile the body expression as an implicit return */` |
|     209 |  2472 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      69 |  2473 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     140 |  2474 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2475 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2476 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2477 | `		return SXERR_ABORT;` |
|       - |  2478 | `	}` |
|     140 |  2479 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     140 |  2480 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     140 |  2481 | `	pSavedEnd = pGen->pEnd;` |
|     140 |  2482 | `	pGen->pIn = pBodyStart;` |
|     140 |  2483 | `	pGen->pEnd = pBodyEnd;` |
|     140 |  2484 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     140 |  2485 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2486 | `		return SXERR_ABORT;` |
|       - |  2487 | `	}` |
|       - |  2488 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2489 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2490 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2491 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     140 |  2492 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     140 |  2493 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     140 |  2494 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     140 |  2495 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     140 |  2496 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2497 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     140 |  2498 | `	pGen->pIn = pBodyEnd;` |
|     140 |  2499 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2500 | `	/* Emit the load-closure instruction */` |
|     140 |  2501 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     140 |  2502 | `	return SXRET_OK;` |
|      76 |  2503 |  |
|       - |  2504 | `/*` |
|       - |  2505 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2506 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2507 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2508 | ` * expression's value.` |
|       - |  2509 | ` */` |
|     346 |  2510 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2511 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  2512 |  |
|       - |  2513 | `	SySet *pInstrContainer;` |
|       - |  2514 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2515 | `	GenBlock *pArmBlock;` |
|       - |  2516 | `	sxi32 rc;` |
|     349 |  2517 | `	pTmpIn  = pGen->pIn;` |
|     349 |  2518 | `	pTmpEnd = pGen->pEnd;` |
|     349 |  2519 | `	pGen->pIn  = pStart;` |
|     349 |  2520 | `	pGen->pEnd = pStop;` |
|     349 |  2521 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     349 |  2522 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2523 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2524 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2525 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2526 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2527 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     522 |  2528 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2529 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     349 |  2530 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2531 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2532 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2533 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2534 | `		return SXERR_ABORT;` |
|       - |  2535 | `	}` |
|     349 |  2536 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     349 |  2537 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     349 |  2538 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     349 |  2539 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     349 |  2540 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     349 |  2541 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     349 |  2542 | `	pGen->pIn  = pTmpIn;` |
|     349 |  2543 | `	pGen->pEnd = pTmpEnd;` |
|     349 |  2544 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2545 | `		return SXERR_ABORT;` |
|       - |  2546 | `	}` |
|     349 |  2547 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2548 | `		return SXERR_EMPTY;` |
|       - |  2549 | `	}` |
|     349 |  2550 | `	return SXRET_OK;` |
|     176 |  2551 |  |
|       - |  2552 | `/*` |
|       - |  2553 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2554 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2555 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2556 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2557 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2558 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2559 | ` */` |
|       - |  2560 | `/*` |
|       - |  2561 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2562 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2563 | ` * caller can bail out of the current expression.` |
|       - |  2564 | ` */` |
|       2 |  2565 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2566 |  |
|       - |  2567 | `	va_list ap;` |
|       - |  2568 | `	sxi32 rc;` |
|       - |  2569 | `	SyBlob sMsg;` |
|       3 |  2570 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2571 | `	va_start(ap,zFmt);` |
|       3 |  2572 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2573 | `	va_end(ap);` |
|       3 |  2574 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2575 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2576 | `	SyBlobRelease(&sMsg);` |
|       3 |  2577 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2578 | `		return SXERR_ABORT;` |
|       - |  2579 | `	}` |
|       3 |  2580 | `	return SXERR_SYNTAX;` |
|       2 |  2581 |  |
|       - |  2582 | `/*` |
|       - |  2583 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2584 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2585 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2586 | ` */` |
|     348 |  2587 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  2588 |  |
|     352 |  2589 | `	SyToken *pCur = pStart;` |
|     352 |  2590 | `	int iNest = 0;` |
|     814 |  2591 | `	while( pCur < pEnd ){` |
|     780 |  2592 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2593 | `			iNest++;` |
|     774 |  2594 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2595 | `			iNest--;` |
|     762 |  2596 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     317 |  2597 | `			return pCur;` |
|       - |  2598 | `		}` |
|     466 |  2599 | `		pCur++;` |
|       4 |  2600 | `	}` |
|      37 |  2601 | `	return pEnd;` |
|     178 |  2602 |  |
|      70 |  2603 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2604 |  |
|       - |  2605 | `	ph7_match *pMatch;` |
|       - |  2606 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      75 |  2607 | `	int bHasDefault = 0;` |
|       - |  2608 | `	sxu32 nLine;` |
|       - |  2609 | `	sxi32 rc;` |
|      35 |  2610 | `	SXUNUSED(iCompileFlag);` |
|      75 |  2611 | `	nLine = pGen->pIn->nLine;` |
|      75 |  2612 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2613 | `	/* Expect '(' */` |
|      75 |  2614 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2615 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2616 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2617 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2618 | `	}` |
|      75 |  2619 | `	pGen->pIn++; /* Jump '(' */` |
|      75 |  2620 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      75 |  2621 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2622 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2623 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2624 | `	}` |
|      75 |  2625 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2626 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2627 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2628 | `	}` |
|       - |  2629 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      75 |  2630 | `	pSavedEnd = pGen->pEnd;` |
|      75 |  2631 | `	pGen->pEnd = pSubjEnd;` |
|      75 |  2632 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      75 |  2633 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2634 | `		return SXERR_ABORT;` |
|       - |  2635 | `	}` |
|      75 |  2636 | `	pGen->pEnd = pSavedEnd;` |
|      75 |  2637 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2638 | `	/* Expect '{' */` |
|      75 |  2639 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2640 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2641 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2642 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2643 | `	}` |
|      75 |  2644 | `	pGen->pIn++; /* Jump '{' */` |
|      75 |  2645 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      75 |  2646 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2647 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2648 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2649 | `	}` |
|       - |  2650 | `	/* Allocate ph7_match container */` |
|      75 |  2651 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      75 |  2652 | `	if( pMatch == 0 ){` |
|     ! 0 |  2653 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2654 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2655 | `		return SXERR_ABORT;` |
|       - |  2656 | `	}` |
|      75 |  2657 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      75 |  2658 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2659 | `	/* Iterate arms */` |
|     253 |  2660 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2661 | `		ph7_match_arm sArm;` |
|       - |  2662 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     186 |  2663 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     186 |  2664 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     186 |  2665 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     186 |  2666 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2667 | `		/* 'default' arm? */` |
|     182 |  2668 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  2669 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2670 | `			if( bHasDefault ){` |
|       3 |  2671 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2672 | `					"Match expressions may only contain one default arm");` |
|       4 |  2673 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2674 | `			}` |
|      20 |  2675 | `			sArm.bDefault = 1;` |
|      20 |  2676 | `			bHasDefault = 1;` |
|      20 |  2677 | `			pGen->pIn++;` |
|      20 |  2678 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2679 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2680 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2681 | `			}` |
|      20 |  2682 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2683 | `		}else{` |
|       - |  2684 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     166 |  2685 | `			pCondStart = pGen->pIn;` |
|     166 |  2686 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2687 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     174 |  2688 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2689 | `				SySet sCondBc;` |
|       9 |  2690 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2691 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2692 | `						"syntax error, empty match condition expression");` |
|       - |  2693 | `				}` |
|       9 |  2694 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2695 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2696 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2697 | `					return SXERR_ABORT;` |
|       - |  2698 | `				}` |
|       9 |  2699 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2700 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2701 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2702 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2703 | `			}` |
|     166 |  2704 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2705 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2706 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2707 | `			}` |
|     163 |  2708 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2709 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2710 | `					"syntax error, empty match condition expression");` |
|       - |  2711 | `			}` |
|       - |  2712 | `			{` |
|       - |  2713 | `				SySet sCondBc;` |
|     163 |  2714 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     163 |  2715 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     163 |  2716 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2717 | `					return SXERR_ABORT;` |
|       - |  2718 | `				}` |
|     163 |  2719 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2720 | `			}` |
|     163 |  2721 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2722 | `		}` |
|       - |  2723 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     181 |  2724 | `		pResStart = pGen->pIn;` |
|     181 |  2725 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     181 |  2726 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2727 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2728 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2729 | `		}` |
|     181 |  2730 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     181 |  2731 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2732 | `			return SXERR_ABORT;` |
|       - |  2733 | `		}` |
|     181 |  2734 | `		pGen->pIn = pResEnd;` |
|     181 |  2735 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     149 |  2736 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2737 | `		}` |
|     181 |  2738 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  2739 | `	}` |
|      69 |  2740 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      69 |  2741 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      69 |  2742 | `	return SXRET_OK;` |
|      40 |  2743 |  |
|       - |  2744 | `/*` |
|       - |  2745 | ` * Compile a backtick quoted string.` |
|       - |  2746 | ` */` |
|       4 |  2747 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2748 |  |
|       - |  2749 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2750 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2751 | `	 */` |
|       8 |  2752 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2753 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2754 | `		ph7_lib_version()` |
|       - |  2755 | `		);` |
|       - |  2756 | `	/* Load NULL */` |
|       6 |  2757 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2758 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2759 | `	/* Node successfully compiled */` |
|       6 |  2760 | `	return SXRET_OK;` |
|       2 |  2761 |  |
|       - |  2762 | `/*` |
|       - |  2763 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2764 | ` * construct.` |
|       - |  2765 | ` */` |
|      80 |  2766 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2767 |  |
|       - |  2768 | `	SyString *pName;` |
|       - |  2769 | `	sxu32 nKeyID;` |
|       - |  2770 | `	sxi32 rc;` |
|       - |  2771 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      85 |  2772 | `	pName = &pGen->pIn->sData;` |
|      85 |  2773 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  2774 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      85 |  2775 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2776 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2777 | `		/* Compile arguments one after one */` |
|       9 |  2778 | `		pTmp = pGen->pEnd;` |
|       - |  2779 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2780 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2781 | `		 *  mean that the following expression is valid:` |
|       - |  2782 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2783 | `		 */` |
|       9 |  2784 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2785 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2786 | `			if( pGen->pIn < pNext ){` |
|       9 |  2787 | `				pGen->pEnd = pNext;` |
|       9 |  2788 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2789 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2790 | `					return SXERR_ABORT;` |
|       - |  2791 | `				}` |
|       9 |  2792 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2793 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2794 | `					 * without the overhead of a function call.` |
|       - |  2795 | `					 * This is a very powerful optimization that improve` |
|       - |  2796 | `					 * performance greatly.` |
|       - |  2797 | `					 */` |
|       9 |  2798 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2799 | `				}` |
|       4 |  2800 | `			}` |
|       - |  2801 | `			/* Jump trailing commas */` |
|       9 |  2802 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2803 | `				pNext++;` |
|     ! 0 |  2804 | `			}` |
|       9 |  2805 | `			pGen->pIn = pNext;` |
|       1 |  2806 | `		}` |
|       - |  2807 | `		/* Restore token stream */` |
|       9 |  2808 | `		pGen->pEnd = pTmp;` |
|       5 |  2809 | `	}else{` |
|      77 |  2810 | `		sxi32 nArg = 0;` |
|      77 |  2811 | `		sxu32 nIdx = 0;` |
|      77 |  2812 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      77 |  2813 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2814 | `			return SXERR_ABORT;` |
|      77 |  2815 | `		}else if(rc != SXERR_EMPTY ){` |
|      77 |  2816 | `			nArg = 1;` |
|      36 |  2817 | `		}` |
|      77 |  2818 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2819 | `			ph7_value *pObj;` |
|       - |  2820 | `			/* Emit the call instruction */` |
|      29 |  2821 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  2822 | `			if( pObj == 0 ){` |
|     ! 0 |  2823 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2824 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2825 | `				return SXERR_ABORT;` |
|       - |  2826 | `			}` |
|      29 |  2827 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2828 | `			/* Install in the literal table */` |
|      29 |  2829 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      12 |  2830 | `		}` |
|       - |  2831 | `		/* Emit the call instruction */` |
|      77 |  2832 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      77 |  2833 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2834 | `	}` |
|       - |  2835 | `	/* Node successfully compiled */` |
|      85 |  2836 | `	return SXRET_OK;` |
|      45 |  2837 |  |
|       - |  2838 | `/*` |
|       - |  2839 | ` * Compile a node holding a variable declaration.` |
|       - |  2840 | ` * According to the PHP language reference` |
|       - |  2841 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2842 | ` *  The variable name is case-sensitive.` |
|       - |  2843 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2844 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2845 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2846 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2847 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2848 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2849 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2850 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2851 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2852 | ` *  the chapter on Expressions.` |
|       - |  2853 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2854 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2855 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2856 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2857 | ` *  is being assigned (the source variable).` |
|       - |  2858 | ` */` |
| 1107686 |  2859 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  2860 |  |
| 1107691 |  2861 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2862 | `	sxi32 iVv;` |
|       - |  2863 | `	sxi32 iP1;` |
|       - |  2864 | `	void *p3;` |
|       - |  2865 | `	sxi32 rc;` |
| 1107691 |  2866 | `	iVv = -1; /* Variable variable counter */` |
| 2215389 |  2867 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 1107703 |  2868 | `		pGen->pIn++;` |
| 1107703 |  2869 | `		iVv++;` |
|       5 |  2870 | `	}` |
| 1107691 |  2871 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2872 | `		/* Invalid variable name */` |
|     ! 0 |  2873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2874 | `		if( rc == SXERR_ABORT ){` |
|       - |  2875 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2876 | `			return SXERR_ABORT;` |
|       - |  2877 | `		}` |
|     ! 0 |  2878 | `		return SXRET_OK;` |
|       - |  2879 | `	}` |
| 1107691 |  2880 | `	p3  = 0;` |
| 1107691 |  2881 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2882 | `		/* Dynamic variable creation */` |
|      19 |  2883 | `		pGen->pIn++;  /* Jump the open curly */` |
|      19 |  2884 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      19 |  2885 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2886 | `			/* Empty expression */` |
|       3 |  2887 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2888 | `			return SXRET_OK;` |
|       - |  2889 | `		}` |
|       - |  2890 | `		/* Compile the expression holding the variable name */` |
|      16 |  2891 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2892 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2893 | `			return SXERR_ABORT;` |
|      16 |  2894 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2895 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2896 | `			return SXRET_OK;` |
|       - |  2897 | `		}` |
|       7 |  2898 | `	}else{` |
|       - |  2899 | `		SyHashEntry *pEntry;` |
|       - |  2900 | `		SyString *pName;` |
| 1107675 |  2901 | `		char *zName = 0;` |
|       - |  2902 | `		/* Extract variable name */` |
| 1107675 |  2903 | `		pName = &pGen->pIn->sData;` |
|       - |  2904 | `		/* Advance the stream cursor */` |
| 1107675 |  2905 | `		pGen->pIn++;` |
| 1107675 |  2906 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 1107675 |  2907 | `		if( pEntry == 0 ){` |
|       - |  2908 | `			/* Duplicate name */` |
|  148741 |  2909 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  148741 |  2910 | `			if( zName == 0 ){` |
|     ! 0 |  2911 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2912 | `				return SXERR_ABORT;` |
|       - |  2913 | `			}` |
|       - |  2914 | `			/* Install in the hashtable */` |
|  148741 |  2915 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   74373 |  2916 | `		}else{` |
|       - |  2917 | `			/* Name already available */` |
|  958939 |  2918 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2919 | `		}` |
| 1107675 |  2920 | `		p3 = (void *)zName;` |
|       - |  2921 | `	}` |
| 1107687 |  2922 | `	iP1 = 0;` |
| 1107687 |  2923 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  403109 |  2924 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2925 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  403091 |  2926 | `			iP1 = 1;` |
|  201543 |  2927 | `		}` |
|  201552 |  2928 | `	}` |
|       - |  2929 | `	/* Emit the load instruction */` |
| 1107687 |  2930 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 1107699 |  2931 | `	while( iVv > 0 ){` |
|      13 |  2932 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2933 | `		iVv--;` |
|       1 |  2934 | `	}` |
|       - |  2935 | `	/* Node successfully compiled */` |
| 1107687 |  2936 | `	return SXRET_OK;` |
|  553848 |  2937 |  |
|       - |  2938 | `/*` |
|       - |  2939 | ` * Load a literal.` |
|       - |  2940 | ` */` |
|  780314 |  2941 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 |  2942 |  |
|  780319 |  2943 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2944 | `	ph7_value *pObj;` |
|       - |  2945 | `	SyString *pStr;` |
|       - |  2946 | `	sxu32 nIdx;` |
|       - |  2947 | `	/* Extract token value */` |
|  780319 |  2948 | `	pStr = &pToken->sData;` |
|       - |  2949 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  780319 |  2950 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  165417 |  2951 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2952 | `			/* NULL constant are always indexed at 0 */` |
|   60927 |  2953 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   60927 |  2954 | `			return SXRET_OK;` |
|  104495 |  2955 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2956 | `			/* TRUE constant are always indexed at 1 */` |
|     701 |  2957 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     701 |  2958 | `			return SXRET_OK;` |
|       5 |  2959 | `		}` |
|  728552 |  2960 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  123496 |  2961 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2962 | `			/* FALSE constant are always indexed at 2 */` |
|   46709 |  2963 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   46709 |  2964 | `			return SXRET_OK;` |
|  623623 |  2965 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|  110840 |  2966 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2967 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|   10631 |  2968 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   10631 |  2969 | `			if( pObj == 0 ){` |
|     ! 0 |  2970 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2971 | `				return SXERR_ABORT;` |
|       - |  2972 | `			}` |
|   10631 |  2973 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2974 | `			/* Emit the load constant instruction */` |
|   10631 |  2975 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   10631 |  2976 | `			return SXRET_OK;` |
|  575493 |  2977 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   35832 |  2978 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2979 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2980 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2981 | `			if( pObj == 0 ){` |
|     ! 0 |  2982 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2983 | `				return SXERR_ABORT;` |
|       - |  2984 | `			}` |
|       7 |  2985 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2986 | `				SyString sNs;` |
|       7 |  2987 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2988 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2989 | `			}else{` |
|     ! 0 |  2990 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2991 | `			}` |
|       7 |  2992 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2993 | `			return SXRET_OK;` |
|  574568 |  2994 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   14929 |  2995 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  567099 |  2996 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   19080 |  2997 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2998 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2999 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  3000 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  3001 | `				/* Point to the upper block */` |
|      11 |  3002 | `				pBlock = pBlock->pParent;` |
|       1 |  3003 | `			}` |
|      11 |  3004 | `			if( pBlock == 0 ){` |
|       - |  3005 | `				/* Called in the global scope,load NULL */` |
|       5 |  3006 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  3007 | `			}else{` |
|       - |  3008 | `				/* Extract the target function/method */` |
|       7 |  3009 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  3010 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  3011 | `					/* Not a class method,Load null */` |
|       3 |  3012 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  3013 | `				}else{` |
|       5 |  3014 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  3015 | `					if( pObj == 0 ){` |
|     ! 0 |  3016 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3017 | `						return SXERR_ABORT;` |
|       - |  3018 | `					}` |
|       5 |  3019 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  3020 | `					/* Emit the load constant instruction */` |
|       5 |  3021 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  3022 | `				}` |
|       - |  3023 | `			}` |
|      11 |  3024 | `			return SXRET_OK;` |
|       - |  3025 | `	}` |
|       - |  3026 | `	/* Query literal table */` |
|  661355 |  3027 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  3028 | `		ph7_value *pLitObj;` |
|       - |  3029 | `		/* Unknown literal,install it in the literal table */` |
|  274901 |  3030 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  274901 |  3031 | `		if( pLitObj == 0 ){` |
|     ! 0 |  3032 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3033 | `			return SXERR_ABORT;` |
|       - |  3034 | `		}` |
|  274901 |  3035 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  274901 |  3036 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  137448 |  3037 | `	}` |
|       - |  3038 | `	/* Emit the load constant instruction */` |
|  661355 |  3039 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  661355 |  3040 | `	return SXRET_OK;` |
|  390162 |  3041 |  |
|       - |  3042 | `/*` |
|       - |  3043 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  3044 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  3045 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  3046 | ` * Otherwise, load the simple literal directly.` |
|       - |  3047 | ` */` |
|  783894 |  3048 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 |  3049 |  |
|       - |  3050 | `	sxi32 rc;` |
|  783899 |  3051 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3052 | `		return SXRET_OK;` |
|       - |  3053 | `	}` |
|       - |  3054 | `	/* Check if this is a multi-token namespace path */` |
|  783899 |  3055 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  3056 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    3585 |  3057 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    3585 |  3058 | `		int isAbsolute = 0;` |
|    3585 |  3059 | `		SyBlobReset(pWorker);` |
|       - |  3060 | `		/* Check for leading backslash (absolute path) */` |
|    3585 |  3061 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    3583 |  3062 | `			isAbsolute = 1;` |
|    3583 |  3063 | `			pGen->pIn++; /* Skip leading backslash */` |
|    1789 |  3064 | `		}` |
|       - |  3065 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|    3585 |  3066 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  3067 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  3068 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  3069 | `		}` |
|       - |  3070 | `		/* Collect all path components */` |
|    3681 |  3071 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3681 |  3072 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      53 |  3073 | `				SyBlobAppend(pWorker,"\\",1);` |
|      29 |  3074 | `			}else{` |
|    3633 |  3075 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  3076 | `			}` |
|    3681 |  3077 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    3585 |  3078 | `				pGen->pIn++;` |
|    3585 |  3079 | `				break;` |
|       - |  3080 | `			}` |
|     101 |  3081 | `			pGen->pIn++;` |
|       5 |  3082 | `		}` |
|    3585 |  3083 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  3084 | `			ph7_value *pObj;` |
|       - |  3085 | `			SyString sPath;` |
|       - |  3086 | `			sxu32 nIdx;` |
|    3585 |  3087 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  3088 | `			/* Install in the literal table */` |
|    3585 |  3089 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|    3561 |  3090 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    3561 |  3091 | `				if( pObj == 0 ){` |
|     ! 0 |  3092 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  3093 | `					return SXERR_ABORT;` |
|       - |  3094 | `				}` |
|    3561 |  3095 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|    3561 |  3096 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|    1778 |  3097 | `			}` |
|       - |  3098 | `			/* Emit the load constant instruction.` |
|       - |  3099 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  3100 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    5375 |  3101 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|    1790 |  3102 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|    1790 |  3103 | `				nIdx,0,0);` |
|    3585 |  3104 | `			return SXRET_OK;` |
|       - |  3105 | `		}` |
|     ! 0 |  3106 | `	}` |
|       - |  3107 | `	/* Single-token literal: load directly */` |
|  780319 |  3108 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  780319 |  3109 | `	return rc;` |
|  391952 |  3110 |  |
|       - |  3111 | `/*` |
|       - |  3112 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  3113 | ` */` |
|  783894 |  3114 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  3115 |  |
|       - |  3116 | `	sxi32 rc;` |
|  783899 |  3117 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  783899 |  3118 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3119 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  3120 | `		return rc;` |
|       - |  3121 | `	}` |
|       - |  3122 | `	/* Node successfully compiled */` |
|  783899 |  3123 | `	return SXRET_OK;` |
|  391952 |  3124 |  |
|       - |  3125 | `/*` |
|       - |  3126 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3127 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3128 | ` */` |
|       8 |  3129 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3130 |  |
|       - |  3131 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3132 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3133 | `		pGen->pIn++;` |
|       1 |  3134 | `	}` |
|       9 |  3135 | `	return SXRET_OK;` |
|       1 |  3136 |  |
|       - |  3137 | `/*` |
|       - |  3138 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3139 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3140 | ` */` |
|     106 |  3141 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       5 |  3142 |  |
|     111 |  3143 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      30 |  3144 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3145 | `			return TRUE;` |
|      28 |  3146 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       6 |  3147 | `			return TRUE;` |
|       2 |  3148 | `		}` |
|      95 |  3149 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3150 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3151 | `			return TRUE;` |
|       - |  3152 | `		}` |
|     ! 0 |  3153 | `	}` |
|       - |  3154 | `	/* Not a reserved constant */` |
|     103 |  3155 | `	return FALSE;` |
|      58 |  3156 |  |
|       - |  3157 | `/*` |
|       - |  3158 | ` * Compile the 'const' statement.` |
|       - |  3159 | ` * According to the PHP language reference` |
|       - |  3160 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3161 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3162 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3163 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3164 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3165 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3166 | ` *  Syntax` |
|       - |  3167 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3168 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3169 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3170 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3171 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3172 | ` *  to get a list of all defined constants.` |
|       - |  3173 | ` *` |
|       - |  3174 | ` * Symisc eXtension.` |
|       - |  3175 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3176 | ` *  would allow only simple scalar value.` |
|       - |  3177 | ` *  Example` |
|       - |  3178 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3179 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3180 | ` */` |
|      32 |  3181 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       5 |  3182 |  |
|       - |  3183 | `	SySet *pConsCode,*pInstrContainer;` |
|      37 |  3184 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3185 | `	SyString *pName;` |
|       - |  3186 | `	sxi32 rc;` |
|      37 |  3187 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      37 |  3188 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3189 | `		/* Invalid constant name */` |
|       9 |  3190 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       9 |  3191 | `		if( rc == SXERR_ABORT ){` |
|       - |  3192 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3193 | `			return SXERR_ABORT;` |
|       - |  3194 | `		}` |
|       9 |  3195 | `		goto Synchronize;` |
|       - |  3196 | `	}` |
|       - |  3197 | `	/* Peek constant name */` |
|      31 |  3198 | `	pName = &pGen->pIn->sData;` |
|       - |  3199 | `	/* Make sure the constant name isn't reserved */` |
|      31 |  3200 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3201 | `		/* Reserved constant */` |
|      10 |  3202 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|      10 |  3203 | `		if( rc == SXERR_ABORT ){` |
|       - |  3204 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3205 | `			return SXERR_ABORT;` |
|       - |  3206 | `		}` |
|      10 |  3207 | `		goto Synchronize;` |
|       - |  3208 | `	}` |
|      21 |  3209 | `	pGen->pIn++;` |
|      21 |  3210 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3211 | `		/* Invalid statement*/` |
|       6 |  3212 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       6 |  3213 | `		if( rc == SXERR_ABORT ){` |
|       - |  3214 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3215 | `			return SXERR_ABORT;` |
|       - |  3216 | `		}` |
|       6 |  3217 | `		goto Synchronize;` |
|       - |  3218 | `	}` |
|      15 |  3219 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3220 | `	/* Allocate a new constant value container */` |
|      15 |  3221 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3222 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3223 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3224 | `		return SXERR_ABORT;` |
|       - |  3225 | `	}` |
|      15 |  3226 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3227 | `	/* Swap bytecode container */` |
|      15 |  3228 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3229 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3230 | `	/* Compile constant value */` |
|      15 |  3231 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3232 | `	/* Emit the done instruction */` |
|      15 |  3233 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3234 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3235 | `	if( rc == SXERR_ABORT ){` |
|       - |  3236 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3237 | `		return SXERR_ABORT;` |
|       - |  3238 | `	}` |
|      15 |  3239 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3240 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3241 | `	{` |
|       - |  3242 | `		SyBlob sFQN;` |
|       - |  3243 | `		SyString sFQNStr;` |
|      15 |  3244 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3245 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3246 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3247 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3248 | `		SyBlobRelease(&sFQN);` |
|       - |  3249 | `	}` |
|      15 |  3250 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3251 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3252 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3253 | `	}` |
|      15 |  3254 | `	return SXRET_OK;` |
|       9 |  3255 | `Synchronize:` |
|       - |  3256 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      60 |  3257 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      42 |  3258 | `		pGen->pIn++;` |
|       4 |  3259 | `	}` |
|      22 |  3260 | `	return SXRET_OK;` |
|      21 |  3261 |  |
|       - |  3262 | `/*` |
|       - |  3263 | ` * Compile the 'continue' statement.` |
|       - |  3264 | ` * According to the PHP language reference` |
|       - |  3265 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3266 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3267 | ` *  iteration.` |
|       - |  3268 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3269 | ` *  the purposes of continue.` |
|       - |  3270 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3271 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3272 | ` *  Note:` |
|       - |  3273 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3274 | ` */` |
|       - |  3275 | `/*` |
|       - |  3276 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3277 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3278 | ` * break/continue crosses a try boundary.` |
|       - |  3279 | ` *` |
|       - |  3280 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3281 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3282 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3283 | ` */` |
|    3680 |  3284 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       5 |  3285 |  |
|    3685 |  3286 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   21593 |  3287 | `	while( pBlock && pBlock != pTarget ){` |
|   17913 |  3288 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3289 | `			if( pBlock->pUserData ){` |
|       - |  3290 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3291 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3292 | `			}else{` |
|       - |  3293 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3294 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3295 | `				 * exception context from a sub-execution.` |
|       - |  3296 | `				 */` |
|     ! 0 |  3297 | `				break;` |
|       - |  3298 | `			}` |
|       1 |  3299 | `		}` |
|   17913 |  3300 | `		pBlock = pBlock->pParent;` |
|       5 |  3301 | `	}` |
|    3685 |  3302 |  |
|    3584 |  3303 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       5 |  3304 |  |
|       - |  3305 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3306 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3307 | `	sxu32 nLineLocal;` |
|       - |  3308 | `	sxi32 rc;` |
|    3589 |  3309 | `	nLineLocal = pGen->pIn->nLine;` |
|    3589 |  3310 | `	iLevel = 0;` |
|       - |  3311 | `	/* Jump the 'continue' keyword */` |
|    3589 |  3312 | `	pGen->pIn++;` |
|    3589 |  3313 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3314 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3315 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3316 | `		 */` |
|       - |  3317 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      17 |  3318 | `		char *zAlloc = 0;` |
|       - |  3319 | `		SyString sNum;` |
|      17 |  3320 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      17 |  3321 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3322 | `			return SXERR_ABORT;` |
|       - |  3323 | `		}` |
|      17 |  3324 | `		if( rc == SXRET_OK ){` |
|      20 |  3325 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3326 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3327 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3328 | `				return SXERR_ABORT;` |
|       - |  3329 | `			}` |
|      14 |  3330 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3331 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3332 | `		}` |
|      17 |  3333 | `		if( iLevel < 2 ){` |
|       3 |  3334 | `			iLevel = 0;` |
|       1 |  3335 | `		}` |
|      17 |  3336 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3337 | `	}` |
|       - |  3338 | `	/* Point to the target loop */` |
|    3589 |  3339 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3589 |  3340 | `	if( pLoop == 0 ){` |
|       - |  3341 | `		/* Illegal continue */` |
|      13 |  3342 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      13 |  3343 | `		if( rc == SXERR_ABORT ){` |
|       - |  3344 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3345 | `			return SXERR_ABORT;` |
|       - |  3346 | `		}` |
|       8 |  3347 | `	}else{` |
|    3579 |  3348 | `		sxu32 nInstrIdx = 0;` |
|       - |  3349 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3579 |  3350 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3579 |  3351 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3352 | `			/* According to the PHP language reference manual` |
|       - |  3353 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3354 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3355 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3356 | `			 */` |
|       5 |  3357 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3358 | `			if( rc == SXRET_OK ){` |
|       5 |  3359 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3360 | `			}` |
|       3 |  3361 | `		}else{` |
|       - |  3362 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3575 |  3363 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3575 |  3364 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3365 | `				JumpFixup sJumpFix;` |
|       - |  3366 | `				/* Post-continue */` |
|      14 |  3367 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3368 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3369 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3370 | `			}` |
|       - |  3371 | `		}` |
|       - |  3372 | `	}` |
|    3589 |  3373 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3374 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3375 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3376 | `	}` |
|       - |  3377 | `	/* Statement successfully compiled */` |
|    3589 |  3378 | `	return SXRET_OK;` |
|    1797 |  3379 |  |
|       - |  3380 | `/*` |
|       - |  3381 | ` * Compile the 'break' statement.` |
|       - |  3382 | ` * According to the PHP language reference` |
|       - |  3383 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3384 | ` *  structure.` |
|       - |  3385 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3386 | ` *  enclosing structures are to be broken out of.` |
|       - |  3387 | ` */` |
|     122 |  3388 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       5 |  3389 |  |
|       - |  3390 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3391 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3392 | `	sxi32 rc;` |
|     127 |  3393 | `	iLevel = 0;` |
|       - |  3394 | `	/* Jump the 'break' keyword */` |
|     127 |  3395 | `	pGen->pIn++;` |
|     127 |  3396 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3397 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3398 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3399 | `		 */` |
|       - |  3400 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      18 |  3401 | `		char *zAlloc = 0;` |
|       - |  3402 | `		SyString sNum;` |
|      18 |  3403 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      18 |  3404 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3405 | `			return SXERR_ABORT;` |
|       - |  3406 | `		}` |
|      18 |  3407 | `		if( rc == SXRET_OK ){` |
|      21 |  3408 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3409 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      15 |  3410 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3411 | `				return SXERR_ABORT;` |
|       - |  3412 | `			}` |
|      15 |  3413 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      15 |  3414 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3415 | `		}` |
|      18 |  3416 | `		if( iLevel < 2 ){` |
|       3 |  3417 | `			iLevel = 0;` |
|       1 |  3418 | `		}` |
|      18 |  3419 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3420 | `	}` |
|       - |  3421 | `	/* Extract the target loop */` |
|     127 |  3422 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     127 |  3423 | `	if( pLoop == 0 ){` |
|       - |  3424 | `		/* Illegal break */` |
|      19 |  3425 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      19 |  3426 | `		if( rc == SXERR_ABORT ){` |
|       - |  3427 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3428 | `			return SXERR_ABORT;` |
|       - |  3429 | `		}` |
|      11 |  3430 | `	}else{` |
|       - |  3431 | `		sxu32 nInstrIdx;` |
|       - |  3432 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     111 |  3433 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     111 |  3434 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     111 |  3435 | `		if( rc == SXRET_OK ){` |
|       - |  3436 | `			/* Fix the jump later when the jump destination is resolved */` |
|     111 |  3437 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      53 |  3438 | `		}` |
|       - |  3439 | `	}` |
|     127 |  3440 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3441 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3442 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3443 | `	}` |
|       - |  3444 | `	/* Statement successfully compiled */` |
|     127 |  3445 | `	return SXRET_OK;` |
|      66 |  3446 |  |
|       - |  3447 | `/*` |
|       - |  3448 | ` * Compile or record a label.` |
|       - |  3449 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3450 | ` * Example` |
|       - |  3451 | ` *  goto LABEL;` |
|       - |  3452 | ` *   echo 'Foo';` |
|       - |  3453 | ` *  LABEL:` |
|       - |  3454 | ` *   echo 'Bar';` |
|       - |  3455 | ` */` |
|     112 |  3456 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       5 |  3457 |  |
|       - |  3458 | `	GenBlock *pBlock;` |
|       - |  3459 | `	Label sLabel;` |
|       - |  3460 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     117 |  3461 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     117 |  3462 | `	if( pBlock ){` |
|       - |  3463 | `		sxi32 rc;` |
|       8 |  3464 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3465 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       6 |  3466 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3467 | `			return SXERR_ABORT;` |
|       - |  3468 | `		}` |
|       4 |  3469 | `	}else{` |
|     113 |  3470 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3471 | `		char *zDup;` |
|       - |  3472 | `		/* Initialize label fields */` |
|     113 |  3473 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3474 | `		/* Duplicate label name */` |
|     113 |  3475 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     113 |  3476 | `		if( zDup == 0 ){` |
|     ! 0 |  3477 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3478 | `			return SXERR_ABORT;` |
|       - |  3479 | `		}` |
|     113 |  3480 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     113 |  3481 | `		sLabel.bRef  = FALSE;` |
|     113 |  3482 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     113 |  3483 | `		pBlock = pGen->pCurrent;` |
|     221 |  3484 | `		while( pBlock ){` |
|     133 |  3485 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      23 |  3486 | `				break;` |
|       - |  3487 | `			}` |
|       - |  3488 | `			/* Point to the upper block */` |
|     113 |  3489 | `			pBlock = pBlock->pParent;` |
|       5 |  3490 | `		}` |
|     113 |  3491 | `		if( pBlock ){` |
|      23 |  3492 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      13 |  3493 | `		}else{` |
|      93 |  3494 | `			sLabel.pFunc = 0;` |
|       - |  3495 | `		}` |
|       - |  3496 | `		/* Insert in label set */` |
|     113 |  3497 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3498 | `	}` |
|     117 |  3499 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     117 |  3500 | `	return SXRET_OK;` |
|      61 |  3501 |  |
|       - |  3502 | `/*` |
|       - |  3503 | ` * Compile the so hated 'goto' statement.` |
|       - |  3504 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3505 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3506 | ` * a compiler it has to do this.` |
|       - |  3507 | ` * According to the PHP language reference manual` |
|       - |  3508 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3509 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3510 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3511 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3512 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3513 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3514 | ` *   of a multi-level break` |
|       - |  3515 | ` */` |
|     152 |  3516 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       5 |  3517 |  |
|       - |  3518 | `	JumpFixup sJump;` |
|       - |  3519 | `	sxi32 rc;` |
|     157 |  3520 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     157 |  3521 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3522 | `		/* Missing label */` |
|     ! 0 |  3523 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3524 | `		if( rc == SXERR_ABORT ){` |
|       - |  3525 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3526 | `			return SXERR_ABORT;` |
|       - |  3527 | `		}` |
|     ! 0 |  3528 | `		return SXRET_OK;` |
|       - |  3529 | `	}` |
|     157 |  3530 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       6 |  3531 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       6 |  3532 | `		if( rc == SXERR_ABORT ){` |
|       - |  3533 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3534 | `			return SXERR_ABORT;` |
|       - |  3535 | `		}` |
|       4 |  3536 | `	}else{` |
|     153 |  3537 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3538 | `		GenBlock *pBlock;` |
|       - |  3539 | `		char *zDup;` |
|       - |  3540 | `		/* Prepare the jump destination */` |
|     153 |  3541 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     153 |  3542 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3543 | `		/* Duplicate label name */` |
|     153 |  3544 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     153 |  3545 | `		if( zDup == 0 ){` |
|     ! 0 |  3546 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3547 | `			return SXERR_ABORT;` |
|       - |  3548 | `		}` |
|     153 |  3549 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     153 |  3550 | `		pBlock = pGen->pCurrent;` |
|     315 |  3551 | `		while( pBlock ){` |
|     199 |  3552 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      36 |  3553 | `				break;` |
|       - |  3554 | `			}` |
|       - |  3555 | `			/* Point to the upper block */` |
|     167 |  3556 | `			pBlock = pBlock->pParent;` |
|       5 |  3557 | `		}` |
|     153 |  3558 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       8 |  3559 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       8 |  3560 | `			if( rc == SXERR_ABORT ){` |
|       - |  3561 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3562 | `				return SXERR_ABORT;` |
|       - |  3563 | `			}` |
|       3 |  3564 | `		}` |
|     153 |  3565 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      30 |  3566 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      17 |  3567 | `		}else{` |
|     127 |  3568 | `			sJump.pFunc = 0;` |
|       - |  3569 | `		}` |
|       - |  3570 | `		/* Emit the unconditional jump */` |
|     153 |  3571 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     153 |  3572 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3573 | `		}` |
|       - |  3574 | `	}` |
|     157 |  3575 | `	pGen->pIn++; /* Jump the label name */` |
|     157 |  3576 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3577 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3578 | `	}` |
|       - |  3579 | `	/* Statement successfully compiled */` |
|     157 |  3580 | `	return SXRET_OK;` |
|      81 |  3581 |  |
|       - |  3582 | `/*` |
|       - |  3583 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3584 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3585 | ` * failure.` |
|       - |  3586 | ` */` |
|      20 |  3587 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3588 |  |
|       - |  3589 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3590 | `	sxu32 nRawObj;` |
|      10 |  3591 | `	sxu32 nObjIdx;` |
|       - |  3592 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3593 | `	 * a PHP block.` |
|       - |  3594 | `	 */` |
|      10 |  3595 | `Consume:` |
|      21 |  3596 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3597 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3598 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3599 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3600 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3601 | `			return SXERR_ABORT;` |
|       - |  3602 | `		}` |
|       - |  3603 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3604 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3605 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3606 | `		++nRawObj;` |
|     ! 0 |  3607 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3608 | `	}` |
|      21 |  3609 | `	if( nRawObj > 0 ){` |
|       - |  3610 | `		/* Emit the consume instruction */` |
|     ! 0 |  3611 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3612 | `	}` |
|      21 |  3613 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3614 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3615 | `		/* Reset the token set */` |
|     ! 0 |  3616 | `		SySetReset(pTokenSet);` |
|       - |  3617 | `		/* Tokenize input */` |
|     ! 0 |  3618 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3619 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3620 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3621 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3622 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3623 | `		/* Advance the stream cursor */` |
|     ! 0 |  3624 | `		pGen->pRawIn++;` |
|       - |  3625 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3626 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3627 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3628 | `			sxi32 rc;` |
|       - |  3629 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3630 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3631 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3632 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3633 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3634 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3635 | `				return SXERR_ABORT;` |
|     ! 0 |  3636 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3637 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3638 | `			}` |
|     ! 0 |  3639 | `			goto Consume;` |
|       - |  3640 | `		}` |
|     ! 0 |  3641 | `	}else{` |
|       - |  3642 | `		/* No more chunks to process */` |
|      21 |  3643 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3644 | `		return SXERR_EOF;` |
|       - |  3645 | `	}` |
|     ! 0 |  3646 | `	return SXRET_OK;` |
|      11 |  3647 |  |
|       - |  3648 | `/*` |
|       - |  3649 | ` * Compile a PHP block.` |
|       - |  3650 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3651 | ` * optionally delimited by braces {}.` |
|       - |  3652 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3653 | ` * and this function takes care of generating the appropriate error` |
|       - |  3654 | ` * message.` |
|       - |  3655 | ` */` |
|  433474 |  3656 | `static sxi32 PH7_CompileBlock(` |
|       - |  3657 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3658 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3659 | `	)` |
|       5 |  3660 |  |
|       - |  3661 | `	sxi32 rc;` |
|       - |  3662 | `	sxu32 nLine;` |
|  433479 |  3663 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  431795 |  3664 | `		nLine = pGen->pIn->nLine;` |
|  431795 |  3665 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  431795 |  3666 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3667 | `			return SXERR_ABORT;` |
|       - |  3668 | `		}` |
|  431795 |  3669 | `		pGen->pIn++;` |
|       - |  3670 | `		/* Compile until we hit the closing braces '}' */` |
|  588303 |  3671 | `		for(;;){` |
| 1176611 |  3672 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3673 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3674 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3675 | `			 	   return SXERR_ABORT;` |
|       - |  3676 | `				}` |
|      21 |  3677 | `				if( rc == SXERR_EOF ){` |
|       - |  3678 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3679 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3680 | `					break;` |
|       - |  3681 | `				}` |
|     ! 0 |  3682 | `			}` |
| 1176591 |  3683 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3684 | `				/* Closing braces found,break immediately*/` |
|  431775 |  3685 | `				pGen->pIn++;` |
|  431775 |  3686 | `				break;` |
|       - |  3687 | `			}` |
|       - |  3688 | `			/* Compile a single statement */` |
|  744821 |  3689 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  744821 |  3690 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3691 | `				return SXERR_ABORT;` |
|       - |  3692 | `			}` |
|       5 |  3693 | `		}` |
|  431795 |  3694 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  217584 |  3695 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3696 | `		pGen->pIn++;` |
|     ! 0 |  3697 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3698 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3699 | `			return SXERR_ABORT;` |
|       - |  3700 | `		}` |
|       - |  3701 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3702 | `		for(;;){` |
|     ! 0 |  3703 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3704 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3705 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3706 | `			 	   return SXERR_ABORT;` |
|       - |  3707 | `				}` |
|     ! 0 |  3708 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3709 | `					/* No more token to process */` |
|     ! 0 |  3710 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3711 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3712 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3713 | `					}` |
|     ! 0 |  3714 | `					break;` |
|       - |  3715 | `				}` |
|     ! 0 |  3716 | `			}` |
|     ! 0 |  3717 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3718 | `				sxi32 nKwrd;` |
|       - |  3719 | `				/* Keyword found */` |
|     ! 0 |  3720 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3721 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3722 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3723 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3724 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3725 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3726 | `						}` |
|     ! 0 |  3727 | `						break;` |
|       - |  3728 | `				}` |
|     ! 0 |  3729 | `			}` |
|       - |  3730 | `			/* Compile a single statement */` |
|     ! 0 |  3731 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3732 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3733 | `				return SXERR_ABORT;` |
|       - |  3734 | `			}` |
|     ! 0 |  3735 | `		}` |
|     ! 0 |  3736 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3737 | `	}else{` |
|       - |  3738 | `		/* Compile a single statement */` |
|    1689 |  3739 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1689 |  3740 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3741 | `			return SXERR_ABORT;` |
|       - |  3742 | `		}` |
|       - |  3743 | `	}` |
|       - |  3744 | `	/* Jump trailing semi-colons ';' */` |
|  433479 |  3745 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3746 | `		pGen->pIn++;` |
|     ! 0 |  3747 | `	}` |
|  433479 |  3748 | `	return SXRET_OK;` |
|  216742 |  3749 |  |
|       - |  3750 | `/*` |
|       - |  3751 | ` * Compile the gentle 'while' statement.` |
|       - |  3752 | ` * According to the PHP language reference` |
|       - |  3753 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3754 | ` *  The basic form of a while statement is:` |
|       - |  3755 | ` *  while (expr)` |
|       - |  3756 | ` *   statement` |
|       - |  3757 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3758 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3759 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3760 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3761 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3762 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3763 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3764 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3765 | ` *  while (expr):` |
|       - |  3766 | ` *    statement` |
|       - |  3767 | ` *   endwhile;` |
|       - |  3768 | ` */` |
|   14280 |  3769 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       5 |  3770 |  |
|   14285 |  3771 | `	GenBlock *pWhileBlock = 0;` |
|   14285 |  3772 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3773 | `	sxu32 nFalseJump;` |
|       - |  3774 | `	sxu32 nLine;` |
|       - |  3775 | `	sxi32 rc;` |
|   14285 |  3776 | `	nLine = pGen->pIn->nLine;` |
|       - |  3777 | `	/* Jump the 'while' keyword */` |
|   14285 |  3778 | `	pGen->pIn++;` |
|   14285 |  3779 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3780 | `		/* Syntax error */` |
|     ! 0 |  3781 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3782 | `		if( rc == SXERR_ABORT ){` |
|       - |  3783 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3784 | `			return SXERR_ABORT;` |
|       - |  3785 | `		}` |
|     ! 0 |  3786 | `		goto Synchronize;` |
|       - |  3787 | `	}` |
|       - |  3788 | `	/* Jump the left parenthesis '(' */` |
|   14285 |  3789 | `	pGen->pIn++;` |
|       - |  3790 | `	/* Create the loop block */` |
|   14285 |  3791 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   14285 |  3792 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3793 | `		return SXERR_ABORT;` |
|       - |  3794 | `	}` |
|       - |  3795 | `	/* Delimit the condition */` |
|   14285 |  3796 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14285 |  3797 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3798 | `		/* Empty expression */` |
|       3 |  3799 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3800 | `		if( rc == SXERR_ABORT ){` |
|       - |  3801 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3802 | `			return SXERR_ABORT;` |
|       - |  3803 | `		}` |
|       1 |  3804 | `	}` |
|       - |  3805 | `	/* Swap token streams */` |
|   14285 |  3806 | `	pTmp = pGen->pEnd;` |
|   14285 |  3807 | `	pGen->pEnd = pEnd;` |
|       - |  3808 | `	/* Compile the expression */` |
|   14285 |  3809 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14285 |  3810 | `	if( rc == SXERR_ABORT ){` |
|       - |  3811 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3812 | `		return SXERR_ABORT;` |
|       - |  3813 | `	}` |
|       - |  3814 | `	/* Update token stream */` |
|   14285 |  3815 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3816 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3817 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3818 | `			return SXERR_ABORT;` |
|       - |  3819 | `		}` |
|     ! 0 |  3820 | `		pGen->pIn++;` |
|     ! 0 |  3821 | `	}` |
|       - |  3822 | `	/* Synchronize pointers */` |
|   14285 |  3823 | `	pGen->pIn  = &pEnd[1];` |
|   14285 |  3824 | `	pGen->pEnd = pTmp;` |
|       - |  3825 | `	/* Emit the false jump */` |
|   14285 |  3826 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3827 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14285 |  3828 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3829 | `	/* Compile the loop body */` |
|   14285 |  3830 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   14285 |  3831 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3832 | `		return SXERR_ABORT;` |
|       - |  3833 | `	}` |
|       - |  3834 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14285 |  3835 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3836 | `	/* Fix all jumps now the destination is resolved */` |
|   14285 |  3837 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3838 | `	/* Release the loop block */` |
|   14285 |  3839 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3840 | `	/* Statement successfully compiled */` |
|   14285 |  3841 | `	return SXRET_OK;` |
|     ! 0 |  3842 | `Synchronize:` |
|       - |  3843 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3844 | `	 * compiling this erroneous block.` |
|       - |  3845 | `	 */` |
|     ! 0 |  3846 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3847 | `		pGen->pIn++;` |
|     ! 0 |  3848 | `	}` |
|     ! 0 |  3849 | `	return SXRET_OK;` |
|    7145 |  3850 |  |
|       - |  3851 | `/*` |
|       - |  3852 | ` * Compile the ugly do..while() statement.` |
|       - |  3853 | ` * According to the PHP language reference` |
|       - |  3854 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3855 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3856 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3857 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3858 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3859 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3860 | ` *  would end immediately).` |
|       - |  3861 | ` *  There is just one syntax for do-while loops:` |
|       - |  3862 | ` *  <?php` |
|       - |  3863 | ` *  $i = 0;` |
|       - |  3864 | ` *  do {` |
|       - |  3865 | ` *   echo $i;` |
|       - |  3866 | ` *  } while ($i > 0);` |
|       - |  3867 | ` * ?>` |
|       - |  3868 | ` */` |
|       2 |  3869 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3870 |  |
|       3 |  3871 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3872 | `	GenBlock *pDoBlock = 0;` |
|       - |  3873 | `	sxu32 nLine;` |
|       - |  3874 | `	sxi32 rc;` |
|       3 |  3875 | `	nLine = pGen->pIn->nLine;` |
|       - |  3876 | `	/* Jump the 'do' keyword */` |
|       3 |  3877 | `	pGen->pIn++;` |
|       - |  3878 | `	/* Create the loop block */` |
|       3 |  3879 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3880 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3881 | `		return SXERR_ABORT;` |
|       - |  3882 | `	}` |
|       - |  3883 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3884 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3885 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3886 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3887 | `		return SXERR_ABORT;` |
|       - |  3888 | `	}` |
|       3 |  3889 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3890 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3891 | `	}` |
|       3 |  3892 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3893 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3894 | `			/* Missing 'while' statement */` |
|       3 |  3895 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3896 | `			if( rc == SXERR_ABORT ){` |
|       - |  3897 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3898 | `				return SXERR_ABORT;` |
|       - |  3899 | `			}` |
|       3 |  3900 | `			goto Synchronize;` |
|       - |  3901 | `	}` |
|       - |  3902 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3903 | `	pGen->pIn++;` |
|     ! 0 |  3904 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3905 | `		/* Syntax error */` |
|     ! 0 |  3906 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3907 | `		if( rc == SXERR_ABORT ){` |
|       - |  3908 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3909 | `			return SXERR_ABORT;` |
|       - |  3910 | `		}` |
|     ! 0 |  3911 | `		goto Synchronize;` |
|       - |  3912 | `	}` |
|       - |  3913 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3914 | `	pGen->pIn++;` |
|       - |  3915 | `	/* Delimit the condition */` |
|     ! 0 |  3916 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3917 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3918 | `		/* Empty expression */` |
|     ! 0 |  3919 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3920 | `		if( rc == SXERR_ABORT ){` |
|       - |  3921 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3922 | `			return SXERR_ABORT;` |
|       - |  3923 | `		}` |
|     ! 0 |  3924 | `		goto Synchronize;` |
|       - |  3925 | `	}` |
|       - |  3926 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3927 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3928 | `		JumpFixup *aPost;` |
|       - |  3929 | `		VmInstr *pInstr;` |
|       - |  3930 | `		sxu32 nJumpDest;` |
|       - |  3931 | `		sxu32 n;` |
|     ! 0 |  3932 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3933 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3934 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3935 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3936 | `			if( pInstr ){` |
|       - |  3937 | `				/* Fix */` |
|     ! 0 |  3938 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3939 | `			}` |
|     ! 0 |  3940 | `		}` |
|     ! 0 |  3941 | `	}` |
|       - |  3942 | `	/* Swap token streams */` |
|     ! 0 |  3943 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3944 | `	pGen->pEnd = pEnd;` |
|       - |  3945 | `	/* Compile the expression */` |
|     ! 0 |  3946 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3947 | `	if( rc == SXERR_ABORT ){` |
|       - |  3948 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3949 | `		return SXERR_ABORT;` |
|       - |  3950 | `	}` |
|       - |  3951 | `	/* Update token stream */` |
|     ! 0 |  3952 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3953 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3954 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3955 | `			return SXERR_ABORT;` |
|       - |  3956 | `		}` |
|     ! 0 |  3957 | `		pGen->pIn++;` |
|     ! 0 |  3958 | `	}` |
|     ! 0 |  3959 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3960 | `	pGen->pEnd = pTmp;` |
|       - |  3961 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3962 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3963 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3964 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3965 | `	/* Release the loop block */` |
|     ! 0 |  3966 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3967 | `	/* Statement successfully compiled */` |
|     ! 0 |  3968 | `	return SXRET_OK;` |
|       1 |  3969 | `Synchronize:` |
|       - |  3970 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3971 | `	 * compiling this erroneous block.` |
|       - |  3972 | `	 */` |
|       3 |  3973 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3974 | `		pGen->pIn++;` |
|     ! 0 |  3975 | `	}` |
|       3 |  3976 | `	return SXRET_OK;` |
|       2 |  3977 |  |
|       - |  3978 | `/*` |
|       - |  3979 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3980 | ` * According to the PHP language reference` |
|       - |  3981 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3982 | ` *  The syntax of a for loop is:` |
|       - |  3983 | ` *  for (expr1; expr2; expr3)` |
|       - |  3984 | ` *   statement` |
|       - |  3985 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3986 | ` *  the beginning of the loop.` |
|       - |  3987 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3988 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3989 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3990 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3991 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3992 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3993 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3994 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3995 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3996 | ` *  of using the for truth expression.` |
|       - |  3997 | ` */` |
|   14280 |  3998 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       5 |  3999 |  |
|   14285 |  4000 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   14285 |  4001 | `	GenBlock *pForBlock = 0;` |
|       - |  4002 | `	sxu32 nFalseJump;` |
|       - |  4003 | `	sxu32 nLine;` |
|       - |  4004 | `	sxi32 rc;` |
|   14285 |  4005 | `	nLine = pGen->pIn->nLine;` |
|       - |  4006 | `	/* Jump the 'for' keyword */` |
|   14285 |  4007 | `	pGen->pIn++;` |
|   14285 |  4008 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4009 | `		/* Syntax error */` |
|     ! 0 |  4010 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  4011 | `		if( rc == SXERR_ABORT ){` |
|       - |  4012 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4013 | `			return SXERR_ABORT;` |
|       - |  4014 | `		}` |
|     ! 0 |  4015 | `		return SXRET_OK;` |
|       - |  4016 | `	}` |
|       - |  4017 | `	/* Jump the left parenthesis '(' */` |
|   14285 |  4018 | `	pGen->pIn++;` |
|       - |  4019 | `	/* Delimit the init-expr;condition;post-expr */` |
|   14285 |  4020 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   14285 |  4021 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4022 | `		/* Empty expression */` |
|     ! 0 |  4023 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  4024 | `		if( rc == SXERR_ABORT ){` |
|       - |  4025 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4026 | `			return SXERR_ABORT;` |
|       - |  4027 | `		}` |
|       - |  4028 | `		/* Synchronize */` |
|     ! 0 |  4029 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4030 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4031 | `			pGen->pIn++;` |
|     ! 0 |  4032 | `		}` |
|     ! 0 |  4033 | `		return SXRET_OK;` |
|       - |  4034 | `	}` |
|       - |  4035 | `	/* Swap token streams */` |
|   14285 |  4036 | `	pTmp = pGen->pEnd;` |
|   14285 |  4037 | `	pGen->pEnd = pEnd;` |
|       - |  4038 | `	/* Compile initialization expressions if available */` |
|   14285 |  4039 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4040 | `	/* Pop operand lvalues */` |
|   14285 |  4041 | `	if( rc == SXERR_ABORT ){` |
|       - |  4042 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4043 | `		return SXERR_ABORT;` |
|   14285 |  4044 | `	}else if( rc != SXERR_EMPTY ){` |
|   14283 |  4045 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7139 |  4046 | `	}` |
|   14285 |  4047 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4048 | `		/* Syntax error */` |
|     ! 0 |  4049 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4050 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  4051 | `		if( rc == SXERR_ABORT ){` |
|       - |  4052 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4053 | `			return SXERR_ABORT;` |
|       - |  4054 | `		}` |
|     ! 0 |  4055 | `		return SXRET_OK;` |
|       - |  4056 | `	}` |
|       - |  4057 | `	/* Jump the trailing ';' */` |
|   14285 |  4058 | `	pGen->pIn++;` |
|       - |  4059 | `	/* Create the loop block */` |
|   14285 |  4060 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   14285 |  4061 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4062 | `		return SXERR_ABORT;` |
|       - |  4063 | `	}` |
|       - |  4064 | `	/* Deffer continue jumps */` |
|   14285 |  4065 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  4066 | `	/* Compile the condition */` |
|   14285 |  4067 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14285 |  4068 | `	if( rc == SXERR_ABORT ){` |
|       - |  4069 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4070 | `		return SXERR_ABORT;` |
|   14285 |  4071 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  4072 | `		/* Emit the false jump */` |
|   14283 |  4073 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  4074 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   14283 |  4075 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    7139 |  4076 | `	}` |
|   14285 |  4077 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4078 | `		/* Syntax error */` |
|       6 |  4079 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  4080 | `			"for: Expected ';' after conditionals expressions");` |
|       6 |  4081 | `		if( rc == SXERR_ABORT ){` |
|       - |  4082 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4083 | `			return SXERR_ABORT;` |
|       - |  4084 | `		}` |
|       6 |  4085 | `		return SXRET_OK;` |
|       - |  4086 | `	}` |
|       - |  4087 | `	/* Jump the trailing ';' */` |
|   14281 |  4088 | `	pGen->pIn++;` |
|       - |  4089 | `	/* Save the post condition stream */` |
|   14281 |  4090 | `	pPostStart = pGen->pIn;` |
|       - |  4091 | `	/* Compile the loop body */` |
|   14281 |  4092 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   14281 |  4093 | `	pGen->pEnd = pTmp;` |
|   14281 |  4094 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   14281 |  4095 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  4096 | `		return SXERR_ABORT;` |
|       - |  4097 | `	}` |
|       - |  4098 | `	/* Fix post-continue jumps */` |
|   14281 |  4099 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  4100 | `		JumpFixup *aPost;` |
|       - |  4101 | `		VmInstr *pInstr;` |
|       - |  4102 | `		sxu32 nJumpDest;` |
|       - |  4103 | `		sxu32 n;` |
|      14 |  4104 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  4105 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  4106 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  4107 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  4108 | `			if( pInstr ){` |
|       - |  4109 | `				/* Fix jump */` |
|      14 |  4110 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  4111 | `			}` |
|       8 |  4112 | `		}` |
|       6 |  4113 | `	}` |
|       - |  4114 | `	/* compile the post-expressions if available */` |
|   14281 |  4115 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  4116 | `		pPostStart++;` |
|     ! 0 |  4117 | `	}` |
|   14281 |  4118 | `	if( pPostStart < pEnd ){` |
|       - |  4119 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   14281 |  4120 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   14281 |  4121 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   14281 |  4122 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  4123 | `			/* Syntax error */` |
|     ! 0 |  4124 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4125 | `			if( rc == SXERR_ABORT ){` |
|       - |  4126 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4127 | `				return SXERR_ABORT;` |
|       - |  4128 | `			}` |
|     ! 0 |  4129 | `			return SXRET_OK;` |
|       - |  4130 | `		}` |
|   14281 |  4131 | `		RE_SWAP_DELIMITER(pGen);` |
|   14281 |  4132 | `		if( rc == SXERR_ABORT ){` |
|       - |  4133 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4134 | `			return SXERR_ABORT;` |
|   14281 |  4135 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4136 | `			/* Pop operand lvalue */` |
|   14281 |  4137 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    7138 |  4138 | `		}` |
|    7138 |  4139 | `	}` |
|       - |  4140 | `	/* Emit the unconditional jump to the start of the loop */` |
|   14281 |  4141 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4142 | `	/* Fix all jumps now the destination is resolved */` |
|   14281 |  4143 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4144 | `	/* Release the loop block */` |
|   14281 |  4145 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4146 | `	/* Statement successfully compiled */` |
|   14281 |  4147 | `	return SXRET_OK;` |
|    7145 |  4148 |  |
|       - |  4149 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4150 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4151 | ` * are allowed.` |
|       - |  4152 | ` */` |
|    7654 |  4153 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  4154 |  |
|    7659 |  4155 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    7659 |  4156 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4157 | `		/* Unexpected expression */` |
|     ! 0 |  4158 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4159 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4160 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4161 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4162 | `		}` |
|     ! 0 |  4163 | `	}` |
|    7659 |  4164 | `	return rc;` |
|       5 |  4165 |  |
|       - |  4166 | `/*` |
|       - |  4167 | ` * Compile the 'foreach' statement.` |
|       - |  4168 | ` * According to the PHP language reference` |
|       - |  4169 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4170 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4171 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4172 | ` *  is a minor but useful extension of the first:` |
|       - |  4173 | ` *  foreach (array_expression as $value)` |
|       - |  4174 | ` *    statement` |
|       - |  4175 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4176 | ` *   statement` |
|       - |  4177 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4178 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4179 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4180 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4181 | ` *  to the variable $key on each loop.` |
|       - |  4182 | ` *  Note:` |
|       - |  4183 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4184 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4185 | ` *  Note:` |
|       - |  4186 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4187 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4188 | ` *  or after the foreach without resetting it.` |
|       - |  4189 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4190 | ` *  of copying the value.` |
|       - |  4191 | ` */` |
|    3922 |  4192 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       5 |  4193 |  |
|    3927 |  4194 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3927 |  4195 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3927 |  4196 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4197 | `	ph7_foreach_info *pInfo;` |
|       - |  4198 | `	sxu32 nFalseJump;` |
|       - |  4199 | `	VmInstr *pInstr;` |
|       - |  4200 | `	sxu32 nLine;` |
|       - |  4201 | `	sxi32 rc;` |
|    3927 |  4202 | `	nLine = pGen->pIn->nLine;` |
|       - |  4203 | `	/* Jump the 'foreach' keyword */` |
|    3927 |  4204 | `	pGen->pIn++;` |
|    3927 |  4205 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4206 | `		/* Syntax error */` |
|     ! 0 |  4207 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4208 | `		if( rc == SXERR_ABORT ){` |
|       - |  4209 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4210 | `			return SXERR_ABORT;` |
|       - |  4211 | `		}` |
|     ! 0 |  4212 | `		goto Synchronize;` |
|       - |  4213 | `	}` |
|       - |  4214 | `	/* Jump the left parenthesis '(' */` |
|    3927 |  4215 | `	pGen->pIn++;` |
|       - |  4216 | `	/* Create the loop block */` |
|    3927 |  4217 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3927 |  4218 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4219 | `		return SXERR_ABORT;` |
|       - |  4220 | `	}` |
|       - |  4221 | `	/* Delimit the expression */` |
|    3927 |  4222 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3927 |  4223 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4224 | `		/* Empty expression */` |
|     ! 0 |  4225 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4226 | `		if( rc == SXERR_ABORT ){` |
|       - |  4227 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4228 | `			return SXERR_ABORT;` |
|       - |  4229 | `		}` |
|       - |  4230 | `		/* Synchronize */` |
|     ! 0 |  4231 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4232 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4233 | `			pGen->pIn++;` |
|     ! 0 |  4234 | `		}` |
|     ! 0 |  4235 | `		return SXRET_OK;` |
|       - |  4236 | `	}` |
|       - |  4237 | `	/* Compile the array expression */` |
|    3927 |  4238 | `	pCur = pGen->pIn;` |
|   26963 |  4239 | `	while( pCur < pEnd ){` |
|   26963 |  4240 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3941 |  4241 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3941 |  4242 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4243 | `				/* Break with the first 'as' found */` |
|    3927 |  4244 | `				break;` |
|       - |  4245 | `			}` |
|       7 |  4246 | `		}` |
|       - |  4247 | `		/* Advance the stream cursor */` |
|   23041 |  4248 | `		pCur++;` |
|       5 |  4249 | `	}` |
|    3927 |  4250 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4251 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4252 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4253 | `		if( rc == SXERR_ABORT ){` |
|       - |  4254 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4255 | `			return SXERR_ABORT;` |
|       - |  4256 | `		}` |
|     ! 0 |  4257 | `		goto Synchronize;` |
|       - |  4258 | `	}` |
|       - |  4259 | `	/* Swap token streams */` |
|    3927 |  4260 | `	pTmp = pGen->pEnd;` |
|    3927 |  4261 | `	pGen->pEnd = pCur;` |
|    3927 |  4262 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3927 |  4263 | `	if( rc == SXERR_ABORT ){` |
|       - |  4264 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4265 | `		return SXERR_ABORT;` |
|       - |  4266 | `	}` |
|       - |  4267 | `	/* Update token stream */` |
|    3927 |  4268 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4269 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4270 | `		if( rc == SXERR_ABORT ){` |
|       - |  4271 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4272 | `			return SXERR_ABORT;` |
|       - |  4273 | `		}` |
|     ! 0 |  4274 | `		pGen->pIn++;` |
|     ! 0 |  4275 | `	}` |
|    3927 |  4276 | `	pCur++; /* Jump the 'as' keyword */` |
|    3927 |  4277 | `	pGen->pIn = pCur;` |
|    3927 |  4278 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4279 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4280 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4281 | `			return SXERR_ABORT;` |
|       - |  4282 | `		}` |
|     ! 0 |  4283 | `	}` |
|       - |  4284 | `	/* Create the foreach context */` |
|    3927 |  4285 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3927 |  4286 | `	if( pInfo == 0 ){` |
|     ! 0 |  4287 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4288 | `		return SXERR_ABORT;` |
|       - |  4289 | `	}` |
|       - |  4290 | `	/* Zero the structure */` |
|    3927 |  4291 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4292 | `	/* Initialize structure fields */` |
|    3927 |  4293 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4294 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|       - |  4295 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|       - |  4296 | `	 * '=>'. */` |
|    3927 |  4297 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|    3927 |  4298 | `	if( pCur < pEnd ){` |
|       - |  4299 | `		/* Compile the expression holding the key name */` |
|    3749 |  4300 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4301 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4302 | `			if( rc == SXERR_ABORT ){` |
|       - |  4303 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4304 | `				return SXERR_ABORT;` |
|       - |  4305 | `			}` |
|     ! 0 |  4306 | `		}else{` |
|    3749 |  4307 | `			pGen->pEnd = pCur;` |
|    3749 |  4308 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3749 |  4309 | `			if( rc == SXERR_ABORT ){` |
|       - |  4310 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4311 | `				return SXERR_ABORT;` |
|       - |  4312 | `			}` |
|    3749 |  4313 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3749 |  4314 | `			if( pInstr->p3 ){` |
|       - |  4315 | `				/* Record key name */` |
|    3749 |  4316 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1872 |  4317 | `			}` |
|    3749 |  4318 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4319 | `		}` |
|    3749 |  4320 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1872 |  4321 | `	}` |
|    3927 |  4322 | `	pGen->pEnd = pEnd;` |
|    3927 |  4323 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4324 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4325 | `		if( rc == SXERR_ABORT ){` |
|       - |  4326 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4327 | `			return SXERR_ABORT;` |
|       - |  4328 | `		}` |
|     ! 0 |  4329 | `		goto Synchronize;` |
|       - |  4330 | `	}` |
|    3927 |  4331 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4332 | `		pGen->pIn++;` |
|       - |  4333 | `		/* Pass by reference  */` |
|      11 |  4334 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4335 | `	}` |
|       - |  4336 | `	/* Check if the value target is list() */` |
|    3927 |  4337 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4338 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4339 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4340 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4341 | `		 */` |
|       - |  4342 | `		static int iForeachListCnt = 0;` |
|       - |  4343 | `		char zTmp[128];` |
|       - |  4344 | `		sxu32 nLen;` |
|       - |  4345 | `		char *zDup;` |
|      10 |  4346 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4347 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4348 | `		if( zDup == 0 ){` |
|     ! 0 |  4349 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4350 | `			return SXERR_ABORT;` |
|       - |  4351 | `		}` |
|      10 |  4352 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4353 | `		/* Save list() token boundaries */` |
|      10 |  4354 | `		pListStart = pGen->pIn;` |
|       - |  4355 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4356 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4357 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4358 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4359 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4360 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4361 | `				return SXERR_ABORT;` |
|       - |  4362 | `			}` |
|       3 |  4363 | `			goto Synchronize;` |
|       - |  4364 | `		}` |
|       7 |  4365 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4366 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4367 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4368 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4369 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4370 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4371 | `				return SXERR_ABORT;` |
|       - |  4372 | `			}` |
|     ! 0 |  4373 | `			goto Synchronize;` |
|       - |  4374 | `		}` |
|       7 |  4375 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4376 | `		pListEnd = pGen->pIn;` |
|       7 |  4377 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3922 |  4378 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4379 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4380 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4381 | `		 */` |
|       - |  4382 | `		static int iForeachShortListCnt = 0;` |
|       - |  4383 | `		char zTmp[128];` |
|       - |  4384 | `		sxu32 nLen;` |
|       - |  4385 | `		char *zDup;` |
|       5 |  4386 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       5 |  4387 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       5 |  4388 | `		if( zDup == 0 ){` |
|     ! 0 |  4389 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4390 | `			return SXERR_ABORT;` |
|       - |  4391 | `		}` |
|       5 |  4392 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4393 | `		/* Save [...] token boundaries */` |
|       5 |  4394 | `		pListStart = pGen->pIn;` |
|       - |  4395 | `		/* Advance past [...] */` |
|       5 |  4396 | `		pGen->pIn++; /* Jump '[' */` |
|       5 |  4397 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       5 |  4398 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4399 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4400 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4401 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4402 | `				return SXERR_ABORT;` |
|       - |  4403 | `			}` |
|     ! 0 |  4404 | `			goto Synchronize;` |
|       - |  4405 | `		}` |
|       5 |  4406 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       5 |  4407 | `		pListEnd = pGen->pIn;` |
|       5 |  4408 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       3 |  4409 | `	}else{` |
|       - |  4410 | `		/* Compile the expression holding the value name */` |
|    3915 |  4411 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3915 |  4412 | `		if( rc == SXERR_ABORT ){` |
|       - |  4413 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4414 | `			return SXERR_ABORT;` |
|       - |  4415 | `		}` |
|    3915 |  4416 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3915 |  4417 | `		if( pInstr->p3 ){` |
|       - |  4418 | `			/* Record value name */` |
|    3915 |  4419 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1955 |  4420 | `		}` |
|       - |  4421 | `	}` |
|       - |  4422 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3925 |  4423 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4424 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3925 |  4425 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4426 | `	/* Record the first instruction to execute */` |
|    3925 |  4427 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4428 | `	/* Emit the FOREACH_STEP instruction */` |
|    3925 |  4429 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4430 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3925 |  4431 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4432 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3925 |  4433 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4434 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4435 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4436 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4437 | `		 */` |
|      11 |  4438 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4439 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4440 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4441 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4442 | `		 */` |
|      11 |  4443 | `		pSavedIn = pGen->pIn;` |
|      11 |  4444 | `		pSavedEnd = pGen->pEnd;` |
|      11 |  4445 | `		pGen->pIn = pListStart;` |
|      11 |  4446 | `		pGen->pEnd = pListEnd;` |
|      11 |  4447 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       5 |  4448 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       3 |  4449 | `		}else{` |
|       7 |  4450 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4451 | `		}` |
|      11 |  4452 | `		pGen->pIn = pSavedIn;` |
|      11 |  4453 | `		pGen->pEnd = pSavedEnd;` |
|      11 |  4454 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4455 | `			return SXERR_ABORT;` |
|       - |  4456 | `		}` |
|       - |  4457 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|      11 |  4458 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       5 |  4459 | `	}` |
|       - |  4460 | `	/* Compile the loop body */` |
|    3925 |  4461 | `	pGen->pIn = &pEnd[1];` |
|    3925 |  4462 | `	pGen->pEnd = pTmp;` |
|    3925 |  4463 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3925 |  4464 | `	if( rc == SXERR_ABORT ){` |
|       - |  4465 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4466 | `		return SXERR_ABORT;` |
|       - |  4467 | `	}` |
|       - |  4468 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3925 |  4469 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4470 | `	/* Fix all jumps now the destination is resolved */` |
|    3925 |  4471 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4472 | `	/* Release the loop block */` |
|    3925 |  4473 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4474 | `	/* Statement successfully compiled */` |
|    3925 |  4475 | `	return SXRET_OK;` |
|       1 |  4476 | `Synchronize:` |
|       - |  4477 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4478 | `	 * compiling this erroneous block.` |
|       - |  4479 | `	 */` |
|       3 |  4480 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4481 | `		pGen->pIn++;` |
|     ! 0 |  4482 | `	}` |
|       3 |  4483 | `	return SXRET_OK;` |
|    1966 |  4484 |  |
|       - |  4485 | `/*` |
|       - |  4486 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4487 | ` * According to the PHP language reference` |
|       - |  4488 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4489 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4490 | ` *  that is similar to that of C:` |
|       - |  4491 | ` *  if (expr)` |
|       - |  4492 | ` *   statement` |
|       - |  4493 | ` *  else construct:` |
|       - |  4494 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4495 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4496 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4497 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4498 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4499 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4500 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4501 | ` *  elseif` |
|       - |  4502 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4503 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4504 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4505 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4506 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4507 | ` *   <?php` |
|       - |  4508 | ` *    if ($a > $b) {` |
|       - |  4509 | ` *     echo "a is bigger than b";` |
|       - |  4510 | ` *    } elseif ($a == $b) {` |
|       - |  4511 | ` *     echo "a is equal to b";` |
|       - |  4512 | ` *    } else {` |
|       - |  4513 | ` *     echo "a is smaller than b";` |
|       - |  4514 | ` *    }` |
|       - |  4515 | ` *    ?>` |
|       - |  4516 | ` */` |
|  148422 |  4517 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       5 |  4518 |  |
|  148427 |  4519 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  148427 |  4520 | `	GenBlock *pCondBlock = 0;` |
|       - |  4521 | `	sxu32 nJumpIdx;` |
|       - |  4522 | `	sxu32 nKeyID;` |
|       - |  4523 | `	sxi32 rc;` |
|       - |  4524 | `	/* Jump the 'if' keyword */` |
|  148427 |  4525 | `	pGen->pIn++;` |
|  148427 |  4526 | `	pToken = pGen->pIn;` |
|       - |  4527 | `	/* Create the conditional block */` |
|  148427 |  4528 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  148427 |  4529 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4530 | `		return SXERR_ABORT;` |
|       - |  4531 | `	}` |
|       - |  4532 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   81348 |  4533 | `	for(;;){` |
|  162701 |  4534 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4535 | `			/* Syntax error */` |
|     ! 0 |  4536 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4537 | `				pToken--;` |
|     ! 0 |  4538 | `			}` |
|     ! 0 |  4539 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4540 | `			if( rc == SXERR_ABORT ){` |
|       - |  4541 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4542 | `				return SXERR_ABORT;` |
|       - |  4543 | `			}` |
|     ! 0 |  4544 | `			goto Synchronize;` |
|       - |  4545 | `		}` |
|       - |  4546 | `		/* Jump the left parenthesis '(' */` |
|  162701 |  4547 | `		pToken++;` |
|       - |  4548 | `		/* Delimit the condition */` |
|  162701 |  4549 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  162701 |  4550 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4551 | `			/* Syntax error */` |
|     ! 0 |  4552 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4553 | `				pToken--;` |
|     ! 0 |  4554 | `			}` |
|     ! 0 |  4555 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4556 | `			if( rc == SXERR_ABORT ){` |
|       - |  4557 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4558 | `				return SXERR_ABORT;` |
|       - |  4559 | `			}` |
|     ! 0 |  4560 | `			goto Synchronize;` |
|       - |  4561 | `		}` |
|       - |  4562 | `		/* Swap token streams */` |
|  162701 |  4563 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4564 | `		/* Compile the condition */` |
|  162701 |  4565 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4566 | `		/* Update token stream */` |
|  162701 |  4567 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4568 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4569 | `			pGen->pIn++;` |
|     ! 0 |  4570 | `		}` |
|  162701 |  4571 | `		pGen->pIn  = &pEnd[1];` |
|  162701 |  4572 | `		pGen->pEnd = pTmp;` |
|  162701 |  4573 | `		if( rc == SXERR_ABORT ){` |
|       - |  4574 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4575 | `			return SXERR_ABORT;` |
|       - |  4576 | `		}` |
|       - |  4577 | `		/* Emit the false jump */` |
|  162701 |  4578 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4579 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  162701 |  4580 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4581 | `		/* Compile the body */` |
|  162701 |  4582 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  162701 |  4583 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4584 | `			return SXERR_ABORT;` |
|       - |  4585 | `		}` |
|  162701 |  4586 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   45355 |  4587 | `			break;` |
|       - |  4588 | `		}` |
|       - |  4589 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   72001 |  4590 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   72001 |  4591 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   46343 |  4592 | `			break;` |
|       - |  4593 | `		}` |
|       - |  4594 | `		/* Emit the unconditional jump */` |
|   25663 |  4595 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4596 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   25663 |  4597 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   25663 |  4598 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   18469 |  4599 | `			pToken = &pGen->pIn[1];` |
|   18469 |  4600 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    7132 |  4601 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    5697 |  4602 | `					break;` |
|       - |  4603 | `			}` |
|    7085 |  4604 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3540 |  4605 | `		}` |
|   14279 |  4606 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4607 | `		/* Synchronize cursors */` |
|   14279 |  4608 | `		pToken = pGen->pIn;` |
|       - |  4609 | `		/* Fix the false jump */` |
|   14279 |  4610 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       5 |  4611 | `	} /* For(;;) */` |
|       - |  4612 | `	/* Fix the false jump */` |
|  148427 |  4613 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  148427 |  4614 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   57722 |  4615 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4616 | `			/* Compile the else block */` |
|   11389 |  4617 | `			pGen->pIn++;` |
|   11389 |  4618 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   11389 |  4619 | `			if( rc == SXERR_ABORT ){` |
|       - |  4620 |  |
|     ! 0 |  4621 | `				return SXERR_ABORT;` |
|       - |  4622 | `			}` |
|    5692 |  4623 | `	}` |
|  148427 |  4624 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4625 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  148427 |  4626 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4627 | `	/* Release the conditional block */` |
|  148427 |  4628 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4629 | `	/* Statement successfully compiled */` |
|  148427 |  4630 | `	return SXRET_OK;` |
|     ! 0 |  4631 | `Synchronize:` |
|       - |  4632 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4633 | `	 */` |
|     ! 0 |  4634 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4635 | `		pGen->pIn++;` |
|     ! 0 |  4636 | `	}` |
|     ! 0 |  4637 | `	return SXRET_OK;` |
|   74216 |  4638 |  |
|       - |  4639 | `/*` |
|       - |  4640 | ` * Compile the global construct.` |
|       - |  4641 | ` * According to the PHP language reference` |
|       - |  4642 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4643 | ` *  to be used in that function.` |
|       - |  4644 | ` *  Example #1 Using global` |
|       - |  4645 | ` *  <?php` |
|       - |  4646 | ` *   $a = 1;` |
|       - |  4647 | ` *   $b = 2;` |
|       - |  4648 | ` *   function Sum()` |
|       - |  4649 | ` *   {` |
|       - |  4650 | ` *    global $a, $b;` |
|       - |  4651 | ` *    $b = $a + $b;` |
|       - |  4652 | ` *   }` |
|       - |  4653 | ` *   Sum();` |
|       - |  4654 | ` *   echo $b;` |
|       - |  4655 | ` *  ?>` |
|       - |  4656 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4657 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4658 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4659 | ` */` |
|      36 |  4660 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       5 |  4661 |  |
|      41 |  4662 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4663 | `	sxi32 nExpr;` |
|       - |  4664 | `	sxi32 rc;` |
|       - |  4665 | `	/* Jump the 'global' keyword */` |
|      41 |  4666 | `	pGen->pIn++;` |
|      41 |  4667 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4668 | `		/* Nothing to process */` |
|     ! 0 |  4669 | `		return SXRET_OK;` |
|       - |  4670 | `	}` |
|      41 |  4671 | `	pTmp = pGen->pEnd;` |
|      41 |  4672 | `	nExpr = 0;` |
|      87 |  4673 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      51 |  4674 | `		if( pGen->pIn < pNext ){` |
|      51 |  4675 | `			pGen->pEnd = pNext;` |
|      51 |  4676 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4677 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4678 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4679 | `					return SXERR_ABORT;` |
|       - |  4680 | `				}` |
|     ! 0 |  4681 | `			}else{` |
|      51 |  4682 | `				pGen->pIn++;` |
|      51 |  4683 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4684 | `					/* Emit a warning */` |
|     ! 0 |  4685 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4686 | `				}else{` |
|      51 |  4687 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      51 |  4688 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4689 | `						return SXERR_ABORT;` |
|      51 |  4690 | `					}else if(rc != SXERR_EMPTY ){` |
|      51 |  4691 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      51 |  4692 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4693 | `							/* Variable name, not a constant */` |
|      51 |  4694 | `							pLast->iP1 = 0;` |
|      23 |  4695 | `						}` |
|      51 |  4696 | `						nExpr++;` |
|      23 |  4697 | `					}` |
|       - |  4698 | `				}` |
|       - |  4699 | `			}` |
|      23 |  4700 | `		}` |
|       - |  4701 | `		/* Next expression in the stream */` |
|      51 |  4702 | `		pGen->pIn = pNext;` |
|       - |  4703 | `		/* Jump trailing commas */` |
|      61 |  4704 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      15 |  4705 | `			pGen->pIn++;` |
|       5 |  4706 | `		}` |
|       5 |  4707 | `	}` |
|       - |  4708 | `	/* Restore token stream */` |
|      41 |  4709 | `	pGen->pEnd = pTmp;` |
|      41 |  4710 | `	if( nExpr > 0 ){` |
|       - |  4711 | `		/* Emit the uplink instruction */` |
|      41 |  4712 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      18 |  4713 | `	}` |
|      41 |  4714 | `	return SXRET_OK;` |
|      23 |  4715 |  |
|       - |  4716 | `/*` |
|       - |  4717 | ` * Compile the return statement.` |
|       - |  4718 | ` * According to the PHP language reference` |
|       - |  4719 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4720 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4721 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4722 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4723 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4724 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4725 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4726 | ` *  from within the main script file, then script execution end.` |
|       - |  4727 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4728 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4729 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4730 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4731 | ` */` |
|  234990 |  4732 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       5 |  4733 |  |
|  234995 |  4734 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4735 | `	sxi32 rc;` |
|       - |  4736 | `	/* Jump the 'return' keyword */` |
|  234995 |  4737 | `	pGen->pIn++;` |
|  234995 |  4738 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4739 | `		/* Compile the expression */` |
|  234967 |  4740 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  234967 |  4741 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4742 | `			return SXERR_ABORT;` |
|  234967 |  4743 | `		}else if(rc != SXERR_EMPTY ){` |
|  234967 |  4744 | `			nRet = 1;` |
|  117481 |  4745 | `		}` |
|  117481 |  4746 | `	}` |
|       - |  4747 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|       - |  4748 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|       - |  4749 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|       - |  4750 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|       - |  4751 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  234995 |  4752 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  234995 |  4753 | `	return SXRET_OK;` |
|  117500 |  4754 |  |
|       - |  4755 | `/*` |
|       - |  4756 | ` * Compile a yield expression.` |
|       - |  4757 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4758 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4759 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4760 | ` */` |
|     170 |  4761 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       5 |  4762 |  |
|       - |  4763 | `	SyToken *pTmp, *pSplit;` |
|     175 |  4764 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|     175 |  4765 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4766 | `	sxi32 rc;` |
|      85 |  4767 | `	(void)iCompileFlag;` |
|       - |  4768 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|     175 |  4769 | `	pGen->pIn++;` |
|       - |  4770 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4771 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|       - |  4772 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|       - |  4773 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|       - |  4774 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|     188 |  4775 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     102 |  4776 | `		&& pGen->pIn->sData.nByte == 4` |
|      41 |  4777 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|      40 |  4778 | `		pGen->pIn++; /* Skip 'from' */` |
|      40 |  4779 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      40 |  4780 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4781 | `			return SXERR_ABORT;` |
|       - |  4782 | `		}` |
|      40 |  4783 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  4784 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|     ! 0 |  4785 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|       - |  4786 | `				"Missing expression after 'yield from'");` |
|     ! 0 |  4787 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4788 | `				return SXERR_ABORT;` |
|       - |  4789 | `			}` |
|     ! 0 |  4790 | `		}` |
|      40 |  4791 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|      40 |  4792 | `		return SXRET_OK;` |
|       - |  4793 | `	}` |
|     139 |  4794 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4795 | `		/* Bare yield — no value */` |
|       3 |  4796 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|       3 |  4797 | `		return SXRET_OK;` |
|       - |  4798 | `	}` |
|       - |  4799 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|     137 |  4800 | `	pSplit = 0;` |
|       - |  4801 | `	{` |
|     137 |  4802 | `		SyToken *pCur = pGen->pIn;` |
|     137 |  4803 | `		sxi32 nNest = 0;` |
|     285 |  4804 | `		while( pCur < pGen->pEnd ){` |
|     167 |  4805 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4806 | `				nNest++;` |
|     167 |  4807 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4808 | `				nNest--;` |
|     167 |  4809 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|      16 |  4810 | `				pSplit = pCur;` |
|      16 |  4811 | `				break;` |
|       - |  4812 | `			}` |
|     153 |  4813 | `			pCur++;` |
|       5 |  4814 | `		}` |
|       - |  4815 | `	}` |
|     137 |  4816 | `	pTmp = pGen->pEnd;` |
|     137 |  4817 | `	if( pSplit ){` |
|       - |  4818 | `		/* yield $key => $value */` |
|      16 |  4819 | `		pGen->pEnd = pSplit;` |
|      16 |  4820 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4821 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4822 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|      16 |  4823 | `		pGen->pEnd = pTmp;` |
|      16 |  4824 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      16 |  4825 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      16 |  4826 | `		iP1 = 1;` |
|      16 |  4827 | `		iP2 = 1;` |
|       9 |  4828 | `	}else{` |
|       - |  4829 | `		/* yield $value */` |
|     123 |  4830 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|     123 |  4831 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     123 |  4832 | `		if( rc != SXERR_EMPTY ){` |
|     123 |  4833 | `			iP1 = 1;` |
|      59 |  4834 | `		}` |
|       - |  4835 | `	}` |
|     137 |  4836 | `	pGen->pEnd = pTmp;` |
|     137 |  4837 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|     137 |  4838 | `	return SXRET_OK;` |
|      90 |  4839 |  |
|       - |  4840 | `/*` |
|       - |  4841 | ` * Compile the die/exit language construct.` |
|       - |  4842 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4843 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4844 | ` */` |
|     120 |  4845 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       5 |  4846 |  |
|     125 |  4847 | `	sxi32 nExpr = 0;` |
|       - |  4848 | `	sxi32 rc;` |
|       - |  4849 | `	/* Jump the die/exit keyword */` |
|     125 |  4850 | `	pGen->pIn++;` |
|     125 |  4851 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4852 | `		/* Compile the expression */` |
|     125 |  4853 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     125 |  4854 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4855 | `			return SXERR_ABORT;` |
|     125 |  4856 | `		}else if(rc != SXERR_EMPTY ){` |
|     125 |  4857 | `			nExpr = 1;` |
|      60 |  4858 | `		}` |
|      60 |  4859 | `	}` |
|       - |  4860 | `	/* Emit the HALT instruction */` |
|     125 |  4861 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|     125 |  4862 | `	return SXRET_OK;` |
|      65 |  4863 |  |
|       - |  4864 | `/*` |
|       - |  4865 | ` * Compile the 'echo' language construct.` |
|       - |  4866 | ` */` |
|   14388 |  4867 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       5 |  4868 |  |
|   14393 |  4869 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4870 | `	sxi32 rc;` |
|       - |  4871 | `	/* Jump the 'echo' keyword */` |
|   14393 |  4872 | `	pGen->pIn++;` |
|       - |  4873 | `	/* Compile arguments one after one */` |
|   14393 |  4874 | `	pTmp = pGen->pEnd;` |
|   31555 |  4875 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   17167 |  4876 | `		if( pGen->pIn < pNext ){` |
|   17167 |  4877 | `			pGen->pEnd = pNext;` |
|   17167 |  4878 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   17167 |  4879 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4880 | `				return SXERR_ABORT;` |
|   17167 |  4881 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4882 | `				/* Emit the consume instruction */` |
|   17143 |  4883 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    8569 |  4884 | `			}` |
|    8581 |  4885 | `		}` |
|       - |  4886 | `		/* Jump trailing commas */` |
|   19941 |  4887 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    2779 |  4888 | `			pNext++;` |
|       5 |  4889 | `		}` |
|   17167 |  4890 | `		pGen->pIn = pNext;` |
|       5 |  4891 | `	}` |
|       - |  4892 | `	/* Restore token stream */` |
|   14393 |  4893 | `	pGen->pEnd = pTmp;` |
|   14393 |  4894 | `	return SXRET_OK;` |
|    7199 |  4895 |  |
|       - |  4896 | `/*` |
|       - |  4897 | ` * Compile the static statement.` |
|       - |  4898 | ` * According to the PHP language reference` |
|       - |  4899 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4900 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4901 | ` *  when program execution leaves this scope.` |
|       - |  4902 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4903 | ` * Symisc eXtension.` |
|       - |  4904 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4905 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4906 | ` *  Example` |
|       - |  4907 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4908 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4909 | ` */` |
|       6 |  4910 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       2 |  4911 |  |
|       - |  4912 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4913 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4914 | `	GenBlock *pBlock;` |
|       - |  4915 | `	SyString *pName;` |
|       - |  4916 | `	char *zDup;` |
|       - |  4917 | `	sxu32 nLine;` |
|       - |  4918 | `	sxi32 rc;` |
|       - |  4919 | `	/* Jump the static keyword */` |
|       8 |  4920 | `	nLine = pGen->pIn->nLine;` |
|       8 |  4921 | `	pGen->pIn++;` |
|       - |  4922 | `	/* Extract the enclosing function if any */` |
|       8 |  4923 | `	pBlock = pGen->pCurrent;` |
|      14 |  4924 | `	while( pBlock ){` |
|      14 |  4925 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       8 |  4926 | `			break;` |
|       - |  4927 | `		}` |
|       - |  4928 | `		/* Point to the upper block */` |
|       8 |  4929 | `		pBlock = pBlock->pParent;` |
|       2 |  4930 | `	}` |
|       8 |  4931 | `	if( pBlock == 0 ){` |
|       - |  4932 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4933 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4934 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4935 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4936 | `				return SXERR_ABORT;` |
|       - |  4937 | `			}` |
|     ! 0 |  4938 | `			goto Synchronize;` |
|       - |  4939 | `		}` |
|       - |  4940 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4941 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4942 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4943 | `			return SXERR_ABORT;` |
|     ! 0 |  4944 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4945 | `			/* Emit the POP instruction */` |
|     ! 0 |  4946 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4947 | `		}` |
|     ! 0 |  4948 | `		return SXRET_OK;` |
|       - |  4949 | `	}` |
|       8 |  4950 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4951 | `	/* Make sure we are dealing with a valid statement */` |
|       8 |  4952 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|       4 |  4953 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4954 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4955 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4956 | `				return SXERR_ABORT;` |
|       - |  4957 | `			}` |
|       3 |  4958 | `			goto Synchronize;` |
|       - |  4959 | `	}` |
|       5 |  4960 | `	pGen->pIn++;` |
|       - |  4961 | `	/* Extract variable name */` |
|       5 |  4962 | `	pName = &pGen->pIn->sData;` |
|       5 |  4963 | `	pGen->pIn++; /* Jump the var name */` |
|       5 |  4964 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4965 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4966 | `		goto Synchronize;` |
|       - |  4967 | `	}` |
|       - |  4968 | `	/* Initialize the structure describing the static variable */` |
|       5 |  4969 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       5 |  4970 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4971 | `	/* Duplicate variable name */` |
|       5 |  4972 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       5 |  4973 | `	if( zDup == 0 ){` |
|     ! 0 |  4974 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4975 | `		return SXERR_ABORT;` |
|       - |  4976 | `	}` |
|       5 |  4977 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4978 | `	/* Check if we have an expression to compile */` |
|       5 |  4979 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4980 | `		SySet *pInstrContainer;` |
|       - |  4981 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4982 | `		 * Static variable can take any complex expression including function` |
|       - |  4983 | `		 * call as their initialization value.` |
|       - |  4984 | `		 * Example:` |
|       - |  4985 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4986 | `		 */` |
|       5 |  4987 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4988 | `		/* Swap bytecode container */` |
|       5 |  4989 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       5 |  4990 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4991 | `		/* Compile the expression */` |
|       5 |  4992 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4993 | `		/* Emit the done instruction */` |
|       5 |  4994 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4995 | `		/* Restore default bytecode container */` |
|       5 |  4996 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       2 |  4997 | `	}` |
|       - |  4998 | `	/* Finally save the compiled static variable in the appropriate container */` |
|       5 |  4999 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|       5 |  5000 | `	return SXRET_OK;` |
|       1 |  5001 | `Synchronize:` |
|       - |  5002 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  5003 | `	 * statement.` |
|       - |  5004 | `	 */` |
|       5 |  5005 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  5006 | `		pGen->pIn++;` |
|       1 |  5007 | `	}` |
|       3 |  5008 | `	return SXRET_OK;` |
|       5 |  5009 |  |
|       - |  5010 | `/*` |
|       - |  5011 | ` * Compile the var statement.` |
|       - |  5012 | ` * Symisc Extension:` |
|       - |  5013 | ` *      var statement can be used outside of a class definition.` |
|       - |  5014 | ` */` |
|       4 |  5015 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  5016 |  |
|       - |  5017 | `	sxu32 nLine;` |
|       - |  5018 | `	sxi32 rc;` |
|       5 |  5019 | `	nLine = pGen->pIn->nLine;` |
|       - |  5020 | `	/* Jump the 'var' keyword */` |
|       5 |  5021 | `	pGen->pIn++;` |
|       5 |  5022 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  5023 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  5024 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  5025 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  5026 | `			pGen->pIn++;` |
|     ! 0 |  5027 | `		}` |
|     ! 0 |  5028 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5029 | `			return SXERR_ABORT;` |
|       - |  5030 | `		}` |
|     ! 0 |  5031 | `	}else{` |
|       - |  5032 | `		/* Compile the expression */` |
|       5 |  5033 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  5034 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5035 | `			return SXERR_ABORT;` |
|       5 |  5036 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  5037 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  5038 | `		}` |
|       - |  5039 | `	}` |
|       5 |  5040 | `	return SXRET_OK;` |
|       3 |  5041 |  |
|       - |  5042 | `/*` |
|       - |  5043 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  5044 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  5045 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  5046 | ` */` |
|       - |  5047 | `/*` |
|       - |  5048 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  5049 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  5050 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  5051 | ` * qualified name and updates the instruction's operand index.` |
|       - |  5052 | ` *` |
|       - |  5053 | ` * Resolution order:` |
|       - |  5054 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  5055 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  5056 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  5057 | ` *` |
|       - |  5058 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  5059 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  5060 | ` * Returns the (possibly new) literal index.` |
|       - |  5061 | ` */` |
|  438402 |  5062 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       5 |  5063 |  |
|       - |  5064 | `	ph7_value *pLit;` |
|       - |  5065 | `	const char *zLit;` |
|       - |  5066 | `	SyString sQualified;` |
|       - |  5067 | `	sxu32 nLit;` |
|       - |  5068 | `	sxu32 k;` |
|       - |  5069 | `	sxu32 nNewIdx;` |
|       - |  5070 | `	int hasNsSep;` |
|       - |  5071 | `	SyHashEntry *pImport;` |
|       - |  5072 | `	ph7_value *pNew;` |
|  438407 |  5073 | `	if( pFromImport ){` |
|  418985 |  5074 | `		*pFromImport = 0;` |
|  209490 |  5075 | `	}` |
|  438407 |  5076 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  438407 |  5077 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  5078 | `		return nOrigIdx;` |
|       - |  5079 | `	}` |
|  438407 |  5080 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  438407 |  5081 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  5082 | `	/* Skip if already qualified (contains backslash) */` |
|  438407 |  5083 | `	hasNsSep = 0;` |
| 4741689 |  5084 | `	for( k = 0; k < nLit; k++ ){` |
| 4303295 |  5085 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 2151646 |  5086 | `	}` |
|  438407 |  5087 | `	if( hasNsSep ){` |
|      10 |  5088 | `		return nOrigIdx;` |
|       - |  5089 | `	}` |
|       - |  5090 | `	/* Check use imports first (works even outside namespaces) */` |
|  438399 |  5091 | `	SyBlobReset(&pGen->sWorker);` |
|  438399 |  5092 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  438399 |  5093 | `	if( pImport ){` |
|      41 |  5094 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      41 |  5095 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      41 |  5096 | `		if( pFromImport ){` |
|      18 |  5097 | `			*pFromImport = 1;` |
|       8 |  5098 | `		}` |
|      23 |  5099 | `	}else{` |
|  438363 |  5100 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  438273 |  5101 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  5102 | `		}` |
|       - |  5103 | `		/* Prepend current namespace */` |
|      95 |  5104 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      95 |  5105 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      95 |  5106 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  5107 | `	}` |
|       - |  5108 | `	/* Look up or create a new literal for the qualified name */` |
|     131 |  5109 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     131 |  5110 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      57 |  5111 | `		return nNewIdx; /* Already interned */` |
|       - |  5112 | `	}` |
|      79 |  5113 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      79 |  5114 | `	if( pNew == 0 ){` |
|     ! 0 |  5115 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  5116 | `	}` |
|      79 |  5117 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      79 |  5118 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      79 |  5119 | `	return nNewIdx;` |
|  219206 |  5120 |  |
|       - |  5121 | `/*` |
|       - |  5122 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  5123 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  5124 | ` */` |
|   96482 |  5125 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5126 |  |
|       - |  5127 | `	SyHashEntry *pImport;` |
|       - |  5128 | `	/* Check use imports first */` |
|   96487 |  5129 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   96487 |  5130 | `	if( pImport ){` |
|      14 |  5131 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  5132 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  5133 | `		return;` |
|       - |  5134 | `	}` |
|       - |  5135 | `	/* Prepend current namespace if active */` |
|   96475 |  5136 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  5137 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  5138 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  5139 | `	}` |
|   96475 |  5140 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   48246 |  5141 |  |
|       - |  5142 | `/*` |
|       - |  5143 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  5144 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  5145 | ` * The caller must release pOut when done.` |
|       - |  5146 | ` */` |
|  139368 |  5147 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       5 |  5148 |  |
|  139373 |  5149 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      63 |  5150 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      63 |  5151 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5152 | `	}` |
|  139373 |  5153 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  139373 |  5154 |  |
|       - |  5155 | `/*` |
|       - |  5156 | ` * Compile a namespace statement` |
|       - |  5157 | ` * According to the PHP language reference manual` |
|       - |  5158 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5159 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5160 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5161 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5162 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5163 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5164 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5165 | ` *  programming world.` |
|       - |  5166 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5167 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5168 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5169 | ` *  classes/functions/constants.` |
|       - |  5170 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5171 | ` *  readability of source code.` |
|       - |  5172 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5173 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5174 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5175 | ` *       class MyClass {}` |
|       - |  5176 | ` *       function myfunction() {}` |
|       - |  5177 | ` *       const MYCONST = 1;` |
|       - |  5178 | ` *       $a = new MyClass;` |
|       - |  5179 | ` *       $c = new \my\name\MyClass;` |
|       - |  5180 | ` *       $a = strlen('hi');` |
|       - |  5181 | ` *       $d = namespace\MYCONST;` |
|       - |  5182 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5183 | ` *       echo constant($d);` |
|       - |  5184 | ` * NOTE` |
|       - |  5185 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5186 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5187 | ` */` |
|       - |  5188 | `/*` |
|       - |  5189 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5190 | ` */` |
|      14 |  5191 | `static const char * TokenTypeName(sxu32 nType)` |
|       3 |  5192 |  |
|      17 |  5193 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|      10 |  5194 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|      10 |  5195 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|      10 |  5196 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|      10 |  5197 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|      10 |  5198 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5199 | `	return "token";` |
|      10 |  5200 |  |
|     106 |  5201 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       5 |  5202 |  |
|       - |  5203 | `	sxu32 nLine;` |
|       - |  5204 | `	sxi32 rc;` |
|     111 |  5205 | `	nLine = pGen->pIn->nLine;` |
|     111 |  5206 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5207 | `	/* Reset namespace and clear previous use imports */` |
|     111 |  5208 | `	SyBlobReset(&pGen->sNamespace);` |
|     111 |  5209 | `	SyHashRelease(&pGen->hUseImports);` |
|     111 |  5210 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5211 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     111 |  5212 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5213 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     111 |  5214 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     111 |  5215 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5216 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5217 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5218 | `		return SXRET_OK;` |
|       - |  5219 | `	}` |
|     111 |  5220 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5221 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5222 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5223 | `		return SXRET_OK;` |
|       - |  5224 | `	}` |
|     111 |  5225 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5226 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5227 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5228 | `		return SXRET_OK;` |
|       - |  5229 | `	}` |
|       - |  5230 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     259 |  5231 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     153 |  5232 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5233 | `			/* Append backslash separator */` |
|      27 |  5234 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      27 |  5235 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5236 | `			}` |
|      16 |  5237 | `		}else{` |
|       - |  5238 | `			/* Append identifier */` |
|     131 |  5239 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5240 | `		}` |
|     153 |  5241 | `		pGen->pIn++;` |
|       5 |  5242 | `	}` |
|       - |  5243 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5244 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5245 | `	{` |
|     111 |  5246 | `		char *zNsDup = 0;` |
|     111 |  5247 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     161 |  5248 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     104 |  5249 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      52 |  5250 | `		}` |
|     111 |  5251 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5252 | `	}` |
|     111 |  5253 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       8 |  5254 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5255 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5256 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       6 |  5257 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5258 | `			return SXERR_ABORT;` |
|       - |  5259 | `		}` |
|       2 |  5260 | `	}` |
|     111 |  5261 | `	return SXRET_OK;` |
|      58 |  5262 |  |
|       - |  5263 | `/*` |
|       - |  5264 | ` * Compile the 'use' statement` |
|       - |  5265 | ` * According to the PHP language reference manual` |
|       - |  5266 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5267 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5268 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5269 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5270 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5271 | ` *  a function or constant is not supported.` |
|       - |  5272 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5273 | ` * NOTE` |
|       - |  5274 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5275 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5276 | ` */` |
|      68 |  5277 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       5 |  5278 |  |
|       - |  5279 | `	sxu32 nLine;` |
|       - |  5280 | `	sxi32 rc;` |
|       - |  5281 | `	SyBlob sPath;` |
|       - |  5282 | `	SyString sAlias;` |
|       - |  5283 | `	SyToken *pLast;` |
|       - |  5284 | `	char *zDup;` |
|       - |  5285 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5286 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5287 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      73 |  5288 | `	nLine = pGen->pIn->nLine;` |
|      73 |  5289 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5290 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      73 |  5291 | `	iUseType = 0;` |
|      73 |  5292 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5293 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5294 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5295 | `			iUseType = 1;` |
|      16 |  5296 | `			pGen->pIn++;` |
|      23 |  5297 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5298 | `			iUseType = 2;` |
|      16 |  5299 | `			pGen->pIn++;` |
|       7 |  5300 | `		}` |
|      14 |  5301 | `	}` |
|       - |  5302 | `	/* Select target hash tables based on import type */` |
|      73 |  5303 | `	switch( iUseType ){` |
|       7 |  5304 | `		case 1:` |
|      16 |  5305 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5306 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5307 | `			break;` |
|       7 |  5308 | `		case 2:` |
|      16 |  5309 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5310 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5311 | `			break;` |
|      20 |  5312 | `		default:` |
|      45 |  5313 | `			pGenHash = &pGen->hUseImports;` |
|      45 |  5314 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5315 | `			break;` |
|       - |  5316 | `	}` |
|      73 |  5317 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5318 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5319 | `	for(;;){` |
|      75 |  5320 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5321 | `			break;` |
|       - |  5322 | `		}` |
|      75 |  5323 | `		SyBlobReset(&sPath);` |
|      75 |  5324 | `		pLast = 0;` |
|       - |  5325 | `		/* Collect the full namespace path */` |
|     261 |  5326 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     191 |  5327 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     131 |  5328 | `				pLast = pGen->pIn;` |
|     131 |  5329 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      65 |  5330 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5331 | `				}` |
|     131 |  5332 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5333 | `			}` |
|     191 |  5334 | `			pGen->pIn++;` |
|       5 |  5335 | `		}` |
|      75 |  5336 | `		if( pLast == 0 ){` |
|       - |  5337 | `			/* Empty path */` |
|       5 |  5338 | `			break;` |
|       - |  5339 | `		}` |
|       - |  5340 | `		/* Default alias is the last component of the path */` |
|      71 |  5341 | `		sAlias = pLast->sData;` |
|       - |  5342 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5343 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      46 |  5344 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      19 |  5345 | `			pGen->pIn++; /* Jump 'as' */` |
|      19 |  5346 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      19 |  5347 | `				sAlias = pGen->pIn->sData;` |
|      19 |  5348 | `				pGen->pIn++;` |
|       8 |  5349 | `			}` |
|       8 |  5350 | `		}` |
|       - |  5351 | `		/* Check for duplicate import alias (per-type) */` |
|      71 |  5352 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       8 |  5353 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5354 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5355 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       6 |  5356 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5357 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5358 | `				return SXERR_ABORT;` |
|       - |  5359 | `			}` |
|       2 |  5360 | `		}` |
|       - |  5361 | `		/* Register the import: alias -> FQN.` |
|       - |  5362 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5363 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5364 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     104 |  5365 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5366 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      71 |  5367 | `		if( zDup ){` |
|      71 |  5368 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      71 |  5369 | `			if( pVmHash ){` |
|       - |  5370 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5371 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      43 |  5372 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      43 |  5373 | `				if( zAliasDup ){` |
|      43 |  5374 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5375 | `				}` |
|      19 |  5376 | `			}` |
|      71 |  5377 | `			if( iUseType == 2 ){` |
|       - |  5378 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5379 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5380 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5381 | `				if( zAliasDup ){` |
|       - |  5382 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5383 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5384 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5385 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5386 | `					if( azPair ){` |
|      16 |  5387 | `						azPair[0] = zAliasDup;` |
|      16 |  5388 | `						azPair[1] = zDup;` |
|      16 |  5389 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5390 | `					}` |
|       7 |  5391 | `				}` |
|       7 |  5392 | `			}` |
|      33 |  5393 | `		}` |
|       - |  5394 | `		/* Check for comma (multiple use declarations) */` |
|      71 |  5395 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5396 | `			pGen->pIn++;` |
|       2 |  5397 | `		}else{` |
|      37 |  5398 | `			break;` |
|       - |  5399 | `		}` |
|       1 |  5400 | `	}` |
|      73 |  5401 | `	SyBlobRelease(&sPath);` |
|      73 |  5402 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5403 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5404 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5405 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5406 | `			return SXERR_ABORT;` |
|       - |  5407 | `		}` |
|       1 |  5408 | `	}` |
|      73 |  5409 | `	return SXRET_OK;` |
|      39 |  5410 |  |
|       - |  5411 | `/*` |
|       - |  5412 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5413 | ` *` |
|       - |  5414 | ` * According to the PHP language reference manual.` |
|       - |  5415 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5416 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5417 | ` *  declare (directive)` |
|       - |  5418 | ` *   statement` |
|       - |  5419 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5420 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5421 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5422 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5423 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5424 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5425 | ` * <?php` |
|       - |  5426 | ` * // these are the same:` |
|       - |  5427 | ` * // you can use this:` |
|       - |  5428 | ` * declare(ticks=1) {` |
|       - |  5429 | ` *   // entire script here` |
|       - |  5430 | ` * }` |
|       - |  5431 | ` * // or you can use this:` |
|       - |  5432 | ` * declare(ticks=1);` |
|       - |  5433 | ` * // entire script here` |
|       - |  5434 | ` * ?>` |
|       - |  5435 | ` *` |
|       - |  5436 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5437 | ` */` |
|       - |  5438 | `/*` |
|       - |  5439 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5440 | ` */` |
|      68 |  5441 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       5 |  5442 |  |
|     103 |  5443 | `	return SyStringLength(pName) == nWant` |
|      68 |  5444 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       5 |  5445 |  |
|       - |  5446 |  |
|      40 |  5447 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       5 |  5448 |  |
|      45 |  5449 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      45 |  5450 | `	SyToken *pBodyEnd = 0;` |
|       - |  5451 | `	SyToken *pBodyStart;` |
|       - |  5452 | `	SyToken *pCursor;` |
|       - |  5453 | `	int bHasStrictTypes;` |
|       - |  5454 | `	int bBlockForm;` |
|       - |  5455 | `	int bPlacementOk;` |
|       - |  5456 | `	sxi32 rc;` |
|      45 |  5457 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      45 |  5458 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5459 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5460 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5461 | `			return SXERR_ABORT;` |
|       - |  5462 | `		}` |
|       5 |  5463 | `		goto Synchro;` |
|       - |  5464 | `	}` |
|      41 |  5465 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      41 |  5466 | `	pBodyStart = pGen->pIn;` |
|       - |  5467 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      41 |  5468 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      41 |  5469 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5470 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5471 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5472 | `			return SXERR_ABORT;` |
|       - |  5473 | `		}` |
|     ! 0 |  5474 | `		return SXRET_OK;` |
|       - |  5475 | `	}` |
|       - |  5476 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5477 | `	 * now delimits the comma-separated directive list. */` |
|      41 |  5478 | `	pGen->pIn = &pBodyEnd[1];` |
|      41 |  5479 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5480 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5481 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5482 | `			return SXERR_ABORT;` |
|       - |  5483 | `		}` |
|     ! 0 |  5484 | `	}` |
|      41 |  5485 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      41 |  5486 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      41 |  5487 | `	bHasStrictTypes = 0;` |
|       - |  5488 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5489 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5490 | `	 * directive appears anywhere in the list, before validating values. */` |
|      41 |  5491 | `	pCursor = pBodyStart;` |
|      53 |  5492 | `	while( pCursor < pBodyEnd ){` |
|      49 |  5493 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      41 |  5494 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      37 |  5495 | `				bHasStrictTypes = 1;` |
|      37 |  5496 | `				break;` |
|       - |  5497 | `			}` |
|       2 |  5498 | `		}` |
|      14 |  5499 | `		pCursor++;` |
|       2 |  5500 | `	}` |
|      41 |  5501 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5502 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5503 | `			"strict_types declaration must not use block mode");` |
|       3 |  5504 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5505 | `		return SXRET_OK;` |
|       - |  5506 | `	}` |
|      39 |  5507 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       6 |  5508 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5509 | `			"strict_types declaration must be the very first statement in the script");` |
|       6 |  5510 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       6 |  5511 | `		return SXRET_OK;` |
|       - |  5512 | `	}` |
|       - |  5513 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      35 |  5514 | `	pCursor = pBodyStart;` |
|      65 |  5515 | `	while( pCursor < pBodyEnd ){` |
|       - |  5516 | `		SyToken *pNameTok;` |
|       - |  5517 | `		SyToken *pEqTok;` |
|       - |  5518 | `		SyToken *pValTok;` |
|       - |  5519 | `		SyString *pDirName;` |
|       - |  5520 | `		int bIsStrict;` |
|       - |  5521 | `		int iStrictValue;` |
|      37 |  5522 | `		pNameTok = pCursor;` |
|      37 |  5523 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5524 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5525 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5526 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5527 | `			return SXRET_OK;` |
|       - |  5528 | `		}` |
|      37 |  5529 | `		pEqTok = pNameTok + 1;` |
|      37 |  5530 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5531 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5532 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5533 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5534 | `			return SXRET_OK;` |
|       - |  5535 | `		}` |
|      37 |  5536 | `		pValTok = pEqTok + 1;` |
|      37 |  5537 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5538 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5539 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5540 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5541 | `			return SXRET_OK;` |
|       - |  5542 | `		}` |
|      37 |  5543 | `		pDirName = &pNameTok->sData;` |
|      37 |  5544 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      37 |  5545 | `		if( bIsStrict ){` |
|       - |  5546 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5547 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      33 |  5548 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5549 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5550 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5551 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5552 | `				return SXRET_OK;` |
|       - |  5553 | `			}` |
|      33 |  5554 | `			iStrictValue = -1;` |
|      33 |  5555 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      33 |  5556 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      33 |  5557 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      33 |  5558 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      31 |  5559 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      14 |  5560 | `			}` |
|      33 |  5561 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5562 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5563 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5564 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5565 | `				return SXRET_OK;` |
|       - |  5566 | `			}` |
|      30 |  5567 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      17 |  5568 | `		}else{` |
|       - |  5569 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5570 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5571 | `			 * behavior don't regress. */` |
|       8 |  5572 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5573 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5574 | `				ph7_lib_version()` |
|       - |  5575 | `				);` |
|       - |  5576 | `		}` |
|      35 |  5577 | `		pCursor = pValTok + 1;` |
|       - |  5578 | `		/* Consume separating comma (or end). */` |
|      35 |  5579 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5580 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5581 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5582 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5583 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5584 | `				return SXRET_OK;` |
|       - |  5585 | `			}` |
|       3 |  5586 | `			pCursor++;` |
|       1 |  5587 | `		}` |
|       5 |  5588 | `	}` |
|       - |  5589 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5590 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5591 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      33 |  5592 | `	return SXRET_OK;` |
|       2 |  5593 | `Synchro:` |
|       - |  5594 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5595 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5596 | `		pGen->pIn++;` |
|       1 |  5597 | `	}` |
|       5 |  5598 | `	return SXRET_OK;` |
|      25 |  5599 |  |
|       - |  5600 | `/*` |
|       - |  5601 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5602 | ` * as follows:` |
|       - |  5603 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5604 | ` * {` |
|       - |  5605 | ` *   return "Making a cup of $type.\n";` |
|       - |  5606 | ` * }` |
|       - |  5607 | ` * Symisc eXtension.` |
|       - |  5608 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5609 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5610 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5611 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5612 | ` *      {` |
|       - |  5613 | ` *       var_dump($a);` |
|       - |  5614 | ` *      }` |
|       - |  5615 | ` *     //call test without args` |
|       - |  5616 | ` *      test();` |
|       - |  5617 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5618 | ` *      Example:` |
|       - |  5619 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5620 | ` * 3 -) Function overloading!!` |
|       - |  5621 | ` *      Example:` |
|       - |  5622 | ` *      function foo($a) {` |
|       - |  5623 | ` *   	  return $a.PHP_EOL;` |
|       - |  5624 | ` *	    }` |
|       - |  5625 | ` *	    function foo($a, $b) {` |
|       - |  5626 | ` *   	  return $a + $b;` |
|       - |  5627 | ` *	    }` |
|       - |  5628 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5629 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5630 | ` *      // Same arg` |
|       - |  5631 | ` *	   function foo(string $a)` |
|       - |  5632 | ` *	   {` |
|       - |  5633 | ` *	     echo "a is a string\n";` |
|       - |  5634 | ` *	     var_dump($a);` |
|       - |  5635 | ` *	   }` |
|       - |  5636 | ` *	  function foo(int $a)` |
|       - |  5637 | ` *	  {` |
|       - |  5638 | ` *	    echo "a is integer\n";` |
|       - |  5639 | ` *	    var_dump($a);` |
|       - |  5640 | ` *	  }` |
|       - |  5641 | ` *	  function foo(array $a)` |
|       - |  5642 | ` *	  {` |
|       - |  5643 | ` * 	    echo "a is an array\n";` |
|       - |  5644 | ` * 	    var_dump($a);` |
|       - |  5645 | ` *	  }` |
|       - |  5646 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5647 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5648 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5649 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5650 | ` * introduced by the PH7 engine.` |
|       - |  5651 | ` */` |
|   67296 |  5652 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       5 |  5653 |  |
|       - |  5654 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5655 | `	SySet *pInstrContainer;` |
|       - |  5656 | `	sxi32 rc;` |
|       - |  5657 | `	/* Swap token stream */` |
|   67301 |  5658 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   67301 |  5659 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   67301 |  5660 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5661 | `	/* Compile the expression holding the argument value */` |
|   67301 |  5662 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5663 | `	/* Emit the done instruction */` |
|   67301 |  5664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   67301 |  5665 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   67301 |  5666 | `	RE_SWAP_DELIMITER(pGen);` |
|   67301 |  5667 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5668 | `		return SXERR_ABORT;` |
|       - |  5669 | `	}` |
|   67301 |  5670 | `	return SXRET_OK;` |
|   33653 |  5671 |  |
|       - |  5672 | `/*` |
|       - |  5673 | ` * Collect function arguments one after one.` |
|       - |  5674 | ` * According to the PHP language reference manual.` |
|       - |  5675 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5676 | ` * list of expressions.` |
|       - |  5677 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5678 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5679 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5680 | ` * for more information.` |
|       - |  5681 | ` * Example #1 Passing arrays to functions` |
|       - |  5682 | ` * <?php` |
|       - |  5683 | ` * function takes_array($input)` |
|       - |  5684 | ` * {` |
|       - |  5685 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5686 | ` * }` |
|       - |  5687 | ` * ?>` |
|       - |  5688 | ` * Making arguments be passed by reference` |
|       - |  5689 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5690 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5691 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5692 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5693 | ` * to the argument name in the function definition:` |
|       - |  5694 | ` * Example #2 Passing function parameters by reference` |
|       - |  5695 | ` * <?php` |
|       - |  5696 | ` * function add_some_extra(&$string)` |
|       - |  5697 | ` * {` |
|       - |  5698 | ` *   $string .= 'and something extra.';` |
|       - |  5699 | ` * }` |
|       - |  5700 | ` * $str = 'This is a string, ';` |
|       - |  5701 | ` * add_some_extra($str);` |
|       - |  5702 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5703 | ` * ?>` |
|       - |  5704 | ` *` |
|       - |  5705 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5706 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5707 | ` * on these extension.` |
|       - |  5708 | ` */` |
|   93352 |  5709 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       5 |  5710 |  |
|       - |  5711 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5712 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5713 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5714 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5715 | `	sxi32 rc;` |
|       - |  5716 |  |
|   93357 |  5717 | `	pIn = pGen->pIn;` |
|   93357 |  5718 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5719 | `	/* Process arguments one after one */` |
|  116722 |  5720 | `	for(;;){` |
|  233449 |  5721 | `		if( pIn >= pEnd ){` |
|       - |  5722 | `			/* No more arguments to process */` |
|   93343 |  5723 | `			break;` |
|       - |  5724 | `		}` |
|  140111 |  5725 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|  140111 |  5726 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|  140111 |  5727 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|  140111 |  5728 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5729 | `		/* Parse optional visibility + readonly modifiers (constructor property` |
|       - |  5730 | `		 * promotion, PHP 8.0+/8.1+). A property is promoted when a visibility` |
|       - |  5731 | ``		 * keyword and/or `readonly` is present; `readonly` may appear on either`` |
|       - |  5732 | ``		 * side of the visibility keyword (`public readonly T $x`,`` |
|       - |  5733 | ``		 * `readonly public T $x`), or alone (`readonly T $x` ⇒ public readonly). */`` |
|       - |  5734 | `		{` |
|  140111 |  5735 | `			int bReadonly = 0, bVisSeen = 0;` |
|  140111 |  5736 | `			sxi32 iVis = PH7_CLASS_PROT_PUBLIC;` |
|  140111 |  5737 | `			if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|       3 |  5738 | `				bReadonly = 1;` |
|       3 |  5739 | `				pIn++;` |
|       1 |  5740 | `			}` |
|  140111 |  5741 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   63947 |  5742 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   63947 |  5743 | `				if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      71 |  5744 | `					bVisSeen = 1;` |
|      71 |  5745 | `					iVis = (nKw == PH7_TKWRD_PRIVATE) ? PH7_CLASS_PROT_PRIVATE` |
|      95 |  5746 | `						: (nKw == PH7_TKWRD_PROTECTED) ? PH7_CLASS_PROT_PROTECTED` |
|      31 |  5747 | `						: PH7_CLASS_PROT_PUBLIC;` |
|      71 |  5748 | `					pIn++;` |
|      71 |  5749 | `					if( pIn < pEnd && GenStateIsReadonly(pIn) ){` |
|      16 |  5750 | `						bReadonly = 1;` |
|      16 |  5751 | `						pIn++;` |
|       6 |  5752 | `					}` |
|      33 |  5753 | `				}` |
|   31971 |  5754 | `			}` |
|  140111 |  5755 | `			if( bVisSeen \|\| bReadonly ){` |
|      73 |  5756 | `				if( !bCtorCtx ){` |
|       6 |  5757 | `					if( bAbstractCtx ){` |
|       3 |  5758 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5759 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5760 | `					}else{` |
|       3 |  5761 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5762 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5763 | `					}` |
|       6 |  5764 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5765 | `						return SXERR_ABORT;` |
|       - |  5766 | `					}` |
|       6 |  5767 | `					return SXERR_SYNTAX;` |
|       - |  5768 | `				}` |
|      69 |  5769 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      69 |  5770 | `				sArg.iPromoteVis = iVis;` |
|      69 |  5771 | `				if( bReadonly ){` |
|      18 |  5772 | `					sArg.iFlags \|= VM_FUNC_ARG_READONLY;` |
|       7 |  5773 | `				}` |
|      32 |  5774 | `			}` |
|       - |  5775 | `		}` |
|       - |  5776 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  179195 |  5777 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|  110927 |  5778 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   79974 |  5779 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   78169 |  5780 | `			sxu32 nLineLocal = pIn->nLine;` |
|   78169 |  5781 | `			sxi32 iTFlags = 0;` |
|   78169 |  5782 | `			pGen->pIn = pIn;` |
|   78169 |  5783 | `			rc = GenStateParseUnionTypeDecl(` |
|   39082 |  5784 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   39082 |  5785 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5786 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5787 | `				/* bAllowVoid */ 0,` |
|   39082 |  5788 | `						nLineLocal);` |
|   78169 |  5789 | `			pIn = pGen->pIn;` |
|   78169 |  5790 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5791 | `				return SXERR_ABORT;` |
|   78169 |  5792 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5793 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5794 | `				return SXERR_SYNTAX;` |
|   78167 |  5795 | `			}else if( rc == SXERR_SYNTAX ){` |
|       8 |  5796 | `				if( pIn < pEnd ){` |
|      11 |  5797 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5798 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       3 |  5799 | `						&pIn->sData);` |
|       5 |  5800 | `				}else{` |
|     ! 0 |  5801 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5802 | `						"syntax error, unexpected end of file");` |
|       - |  5803 | `				}` |
|       8 |  5804 | `				return SXERR_SYNTAX;` |
|       - |  5805 | `			}` |
|   78161 |  5806 | `			sArg.iFlags \|= iTFlags;` |
|   39078 |  5807 | `		}` |
|  140099 |  5808 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5809 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5810 | `			return rc;` |
|       - |  5811 | `		}` |
|  140099 |  5812 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5813 | `			/* Pass by reference,record that */` |
|    3573 |  5814 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3573 |  5815 | `			pIn++;` |
|    1784 |  5816 | `		}` |
|  140099 |  5817 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5818 | `			/* Variadic parameter: ...$args */` |
|      47 |  5819 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      47 |  5820 | `			pIn++;` |
|      21 |  5821 | `		}` |
|  140099 |  5822 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5823 | `			/* Invalid argument */` |
|     ! 0 |  5824 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5825 | `			return rc;` |
|       - |  5826 | `		}` |
|  140099 |  5827 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5828 | `		/* Copy argument name */` |
|  140099 |  5829 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|  140099 |  5830 | `		if( zDup == 0 ){` |
|     ! 0 |  5831 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5832 | `			return SXERR_ABORT;` |
|       - |  5833 | `		}` |
|  140099 |  5834 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|  140099 |  5835 | `		pIn++;` |
|  140099 |  5836 | `		if( pIn < pEnd ){` |
|   78655 |  5837 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5838 | `				SyToken *pDefend;` |
|   67303 |  5839 | `				sxi32 iNest = 0;` |
|   67303 |  5840 | `				pIn++; /* Jump the equal sign */` |
|   67303 |  5841 | `				pDefend = pIn;` |
|       - |  5842 | `				/* Process the default value associated with this argument */` |
|  141679 |  5843 | `				while( pDefend < pEnd ){` |
|  109789 |  5844 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   35413 |  5845 | `						break;` |
|       - |  5846 | `					}` |
|   74381 |  5847 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5848 | `						/* Increment nesting level */` |
|    3545 |  5849 | `						iNest++;` |
|   72611 |  5850 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5851 | `						/* Decrement nesting level */` |
|    3545 |  5852 | `						iNest--;` |
|    1770 |  5853 | `					}` |
|   74381 |  5854 | `					pDefend++;` |
|       5 |  5855 | `				}` |
|   67303 |  5856 | `				if( pIn >= pDefend ){` |
|       3 |  5857 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5858 | `					return rc;` |
|       - |  5859 | `				}` |
|       - |  5860 | `				/* Process default value */` |
|   67301 |  5861 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   67301 |  5862 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5863 | `					return rc;` |
|       - |  5864 | `				}` |
|       - |  5865 | `				/* Point beyond the default value */` |
|   67301 |  5866 | `				pIn = pDefend;` |
|   33648 |  5867 | `			}` |
|   78653 |  5868 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5869 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5870 | `				return rc;` |
|       - |  5871 | `			}` |
|   78653 |  5872 | `			pIn++; /* Jump the trailing comma */` |
|   39324 |  5873 | `		}` |
|       - |  5874 | `		/* Append argument signature */` |
|  140097 |  5875 | `		if( sArg.nType > 0 ){` |
|   78107 |  5876 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5877 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|   14199 |  5878 | `				int marker = 'o';` |
|   14199 |  5879 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|   14199 |  5880 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    7102 |  5881 | `			}else{` |
|       - |  5882 | `				int c;` |
|   63913 |  5883 | `				c = 'n'; /* cc warning */` |
|       - |  5884 | `				/* Type leading character */` |
|   63913 |  5885 | `				switch(sArg.nType){` |
|       3 |  5886 | `				case MEMOBJ_HASHMAP:` |
|       - |  5887 | `					/* Hashmap aka 'array' */` |
|       7 |  5888 | `					c = 'h';` |
|       7 |  5889 | `					break;` |
|    8906 |  5890 | `				case MEMOBJ_INT:` |
|       - |  5891 | `					/* Integer */` |
|   17817 |  5892 | `					c = 'i';` |
|   17817 |  5893 | `					break;` |
|       1 |  5894 | `				case MEMOBJ_BOOL:` |
|       - |  5895 | `					/* Bool */` |
|       3 |  5896 | `					c = 'b';` |
|       3 |  5897 | `					break;` |
|       2 |  5898 | `				case MEMOBJ_REAL:` |
|       - |  5899 | `					/* Float */` |
|       5 |  5900 | `					c = 'f';` |
|       5 |  5901 | `					break;` |
|   23034 |  5902 | `				case MEMOBJ_STRING:` |
|       - |  5903 | `					/* String */` |
|   46073 |  5904 | `					c = 's';` |
|   46073 |  5905 | `					break;` |
|       7 |  5906 | `				case MEMOBJ_OBJ:` |
|       - |  5907 | `					/* Object */` |
|      16 |  5908 | `					c = 'o';` |
|      14 |  5909 | `					break;` |
|       1 |  5910 | `				default:` |
|       2 |  5911 | `					break;` |
|       - |  5912 | `				}` |
|   63913 |  5913 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5914 | `			}` |
|   39056 |  5915 | `		}else{` |
|       - |  5916 | `			/* No type is associated with this parameter which mean` |
|       - |  5917 | `			 * that this function is not condidate for overloading.` |
|       - |  5918 | `			 */` |
|   61995 |  5919 | `			SyBlobRelease(&sSig);` |
|       - |  5920 | `		}` |
|       - |  5921 | `		/* Save in the argument set */` |
|  140097 |  5922 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       5 |  5923 | `	}` |
|   93343 |  5924 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5925 | `		/* Save function signature */` |
|   49753 |  5926 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   24874 |  5927 | `	}` |
|   93343 |  5928 | `	return SXRET_OK;` |
|   46681 |  5929 |  |
|       - |  5930 | `/*` |
|       - |  5931 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5932 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5933 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5934 | ` */` |
|  225332 |  5935 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5936 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5937 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5938 | `	)` |
|       5 |  5939 |  |
|       - |  5940 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5941 | `	GenBlock *pBlock;` |
|       - |  5942 | `	sxu32 nGotoOfft;` |
|       - |  5943 | `	sxi32 rc;` |
|       - |  5944 | `	/* Attach the new function */` |
|  225337 |  5945 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  225337 |  5946 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5947 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5948 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5949 | `		return SXERR_ABORT;` |
|       - |  5950 | `	}` |
|  225337 |  5951 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5952 | `	/* Swap bytecode containers */` |
|  225337 |  5953 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  225337 |  5954 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5955 | `	/* Emit constructor property promotion prologue:` |
|       - |  5956 | `	 *   $this->NAME = $NAME;` |
|       - |  5957 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5958 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5959 | `	{` |
|  225337 |  5960 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5961 | `		sxu32 i;` |
|  336967 |  5962 | `		for( i = 0; i < nArg; i++ ){` |
|  111635 |  5963 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5964 | `			char *zSrc;` |
|       - |  5965 | `			sxu32 nSrc,nName;` |
|       - |  5966 | `			SySet sToken;` |
|       - |  5967 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5968 | `			sxi32 rcPromote;` |
|  111635 |  5969 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|  111581 |  5970 | `				continue;` |
|       - |  5971 | `			}` |
|       - |  5972 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5973 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5974 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5975 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5976 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      59 |  5977 | `			nName = SyStringLength(&pArg->sName);` |
|      59 |  5978 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      59 |  5979 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      59 |  5980 | `			if( zSrc == 0 ){` |
|     ! 0 |  5981 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5982 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5983 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5984 | `				return SXERR_ABORT;` |
|       - |  5985 | `			}` |
|       - |  5986 | `			{` |
|      59 |  5987 | `				char *z = zSrc;` |
|      59 |  5988 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      59 |  5989 | `				z += sizeof("$this->")-1;` |
|      59 |  5990 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  5991 | `				z += nName;` |
|      59 |  5992 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      59 |  5993 | `				z += sizeof(" = $")-1;` |
|      59 |  5994 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      59 |  5995 | `				z += nName;` |
|      59 |  5996 | `				*z = 0;` |
|       - |  5997 | `			}` |
|      59 |  5998 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      59 |  5999 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      59 |  6000 | `			pTmpIn = pGen->pIn;` |
|      59 |  6001 | `			pTmpEnd = pGen->pEnd;` |
|      59 |  6002 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      59 |  6003 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      59 |  6004 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      59 |  6005 | `			pGen->pIn = pTmpIn;` |
|      59 |  6006 | `			pGen->pEnd = pTmpEnd;` |
|      59 |  6007 | `			SySetRelease(&sToken);` |
|      59 |  6008 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  6009 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  6010 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  6011 | `				return SXERR_ABORT;` |
|       - |  6012 | `			}` |
|       - |  6013 | `			/* Discard the assignment result — this is a statement expression. */` |
|      59 |  6014 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      32 |  6015 | `		}` |
|       - |  6016 | `	}` |
|       - |  6017 | `	/* Compile the body */` |
|  225337 |  6018 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  6019 | `	/* Fix exception jumps now the destination is resolved */` |
|  225337 |  6020 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  6021 | `	/* Emit the final return if not yet done */` |
|  225337 |  6022 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  6023 | `	/* Fix gotos jumps now the destination is resolved */` |
|  225337 |  6024 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  6025 | `		rc = SXERR_ABORT;` |
|     ! 0 |  6026 | `	}` |
|  225337 |  6027 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  6028 | `	/* Restore the default container */` |
|  225337 |  6029 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  6030 | `	/* Leave function block */` |
|  225337 |  6031 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  225337 |  6032 | `	if( rc == SXERR_ABORT ){` |
|       - |  6033 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6034 | `		return SXERR_ABORT;` |
|       - |  6035 | `	}` |
|       - |  6036 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  6037 | `	{` |
|  225337 |  6038 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  6039 | `		sxu32 i;` |
| 4350413 |  6040 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 4125181 |  6041 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|     105 |  6042 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|     105 |  6043 | `				break;` |
|       - |  6044 | `			}` |
| 2062543 |  6045 | `		}` |
|       - |  6046 | `	}` |
|       - |  6047 | `	/* All done, function body compiled */` |
|  225337 |  6048 | `	return SXRET_OK;` |
|  112671 |  6049 |  |
|       - |  6050 | `/*` |
|       - |  6051 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  6052 | ` * According to the PHP language reference manual.` |
|       - |  6053 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  6054 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  6055 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  6056 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6057 | ` *  Functions need not be defined before they are referenced.` |
|       - |  6058 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  6059 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  6060 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  6061 | ` *  calls with over 32-64 recursion levels.` |
|       - |  6062 | ` *` |
|       - |  6063 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  6064 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  6065 | ` * on these extension.` |
|       - |  6066 | ` */` |
|       - |  6067 | `/*` |
|       - |  6068 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  6069 | ` */` |
|     482 |  6070 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       5 |  6071 |  |
|       - |  6072 | `	sxu32 i;` |
|    1335 |  6073 | `	for( i = 0; i < n; i++ ){` |
|    1147 |  6074 | `		int a = zA[i], b = zB[i];` |
|    1147 |  6075 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|    1147 |  6076 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|    1147 |  6077 | `		if( a != b ) return a - b;` |
|     429 |  6078 | `	}` |
|     193 |  6079 | `	return 0;` |
|     246 |  6080 |  |
|       - |  6081 | `/*` |
|       - |  6082 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  6083 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  6084 | ` * (which are positive bit values stored in sxu32).` |
|       - |  6085 | ` */` |
|       - |  6086 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  6087 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  6088 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  6089 |  |
|       - |  6090 | `/* PHL_UNION_MAX_ALTS (max alternatives in one type declaration) is defined in` |
|       - |  6091 | ` * ph7int.h so the runtime enforcer (vm.c) shares the same bound. The atom array` |
|       - |  6092 | ` * below lives on the parser stack, so the cost is bounded: ~1 KiB. */` |
|       - |  6093 |  |
|       - |  6094 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  6095 | `struct PhlTypeAtom {` |
|       - |  6096 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  6097 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  6098 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  6099 | `	sxu32 nCanon;` |
|       - |  6100 | `	sxu32 nGroup;      /* intersection-group id: atoms sharing it are ANDed (A&B),` |
|       - |  6101 | `	                    * distinct groups are ORed; pure unions use one atom per group */` |
|       - |  6102 | `};` |
|       - |  6103 |  |
|       - |  6104 | `/*` |
|       - |  6105 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  6106 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  6107 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  6108 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  6109 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  6110 | ` * already be consumed by the caller.` |
|       - |  6111 | ` */` |
|   78988 |  6112 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       5 |  6113 |  |
|   78993 |  6114 | `	SyToken *pIn = pGen->pIn;` |
|   78993 |  6115 | `	SyZero(pOut, sizeof(*pOut));` |
|   78993 |  6116 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   78993 |  6117 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6118 | `		return SXERR_SYNTAX;` |
|       - |  6119 | `	}` |
|       - |  6120 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   78993 |  6121 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  6122 | `		pIn++;` |
|       8 |  6123 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  6124 | `			return SXERR_SYNTAX;` |
|       - |  6125 | `		}` |
|       3 |  6126 | `	}` |
|   78993 |  6127 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6128 | `		return SXERR_SYNTAX;` |
|       - |  6129 | `	}` |
|   78993 |  6130 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   64439 |  6131 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   64439 |  6132 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      32 |  6133 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   64425 |  6134 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      69 |  6135 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   64379 |  6136 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   18065 |  6137 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   55317 |  6138 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   46225 |  6139 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   23177 |  6140 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      33 |  6141 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      53 |  6142 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      27 |  6143 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      25 |  6144 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       7 |  6145 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|      11 |  6146 | `			pOut->nType = SXU32_HIGH;` |
|      11 |  6147 | `			pOut->sClass = pIn->sData;` |
|       7 |  6148 | `		}else{` |
|       3 |  6149 | `			return SXERR_SYNTAX;` |
|       - |  6150 | `		}` |
|   64437 |  6151 | `		pIn++;` |
|   32221 |  6152 | `	}else{` |
|       - |  6153 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  6154 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|   14559 |  6155 | `		SyString *pT = &pIn->sData;` |
|   14559 |  6156 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      32 |  6157 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      32 |  6158 | `			pIn++;` |
|   14545 |  6159 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|     157 |  6160 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|     157 |  6161 | `			pIn++;` |
|   14455 |  6162 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  6163 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  6164 | `			pIn++;` |
|       2 |  6165 | `		}else{` |
|       - |  6166 | `			/* Class / interface name; consume namespace path a\b\c */` |
|   14377 |  6167 | `			SyToken *pFirst = pIn;` |
|   14377 |  6168 | `			SyToken *pLast = pIn;` |
|   14377 |  6169 | `			pOut->nType = SXU32_HIGH;` |
|   14377 |  6170 | `			pOut->sClass = pIn->sData;` |
|   14377 |  6171 | `			pIn++;` |
|   21561 |  6172 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|   14380 |  6173 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6174 | `				pLast = &pIn[1];` |
|       3 |  6175 | `				pIn += 2;` |
|       1 |  6176 | `			}` |
|   14377 |  6177 | `			if( pLast != pFirst ){` |
|       3 |  6178 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6179 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6180 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6181 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6182 | `			}` |
|       - |  6183 | `		}` |
|       - |  6184 | `	}` |
|   78991 |  6185 | `	pGen->pIn = pIn;` |
|   78991 |  6186 | `	return SXRET_OK;` |
|   39499 |  6187 |  |
|       - |  6188 |  |
|       - |  6189 | `/*` |
|       - |  6190 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6191 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6192 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6193 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6194 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6195 | ` */` |
|   78834 |  6196 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       5 |  6197 |  |
|       - |  6198 | `	int i;` |
|   78839 |  6199 | `	int nNonNull = 0;` |
|   78839 |  6200 | `	int bAnyIntersection = 0;` |
|       - |  6201 | `	sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|   78839 |  6202 | `	sxu32 nMaxGroup = 0;` |
| 2601527 |  6203 | `	for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  157807 |  6204 | `	for( i = 0; i < nAtoms; i++ ){` |
|   78973 |  6205 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   78945 |  6206 | `			nNonNull++;` |
|   78945 |  6207 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ){` |
|   78945 |  6208 | `				aGroupCount[aAtoms[i].nGroup]++;` |
|   78945 |  6209 | `				if( aAtoms[i].nGroup > nMaxGroup ) nMaxGroup = aAtoms[i].nGroup;` |
|   39470 |  6210 | `			}` |
|   39470 |  6211 | `		}` |
|   39489 |  6212 | `	}` |
|  157773 |  6213 | `	for( i = 0; i < nAtoms; i++ ){` |
|   78955 |  6214 | `		if( aAtoms[i].nType != UTA_NULL_FLAG && aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      19 |  6215 | `			bAnyIntersection = 1;` |
|      19 |  6216 | `			break;` |
|       - |  6217 | `		}` |
|   39472 |  6218 | `	}` |
|   78839 |  6219 | `	if( bAnyIntersection ){` |
|       - |  6220 | `		/* Intersection / DNF rendering, in declaration (group) order: each group's` |
|       - |  6221 | ``		 * members joined by `&`; a ≥2-member group is wrapped in `()` only when the`` |
|       - |  6222 | ``		 * whole type has more than one group (so a standalone `A&B` stays bare). */`` |
|      19 |  6223 | `		sxu32 g, nGroups = 0;` |
|      19 |  6224 | `		int bFirstGroup = 1;` |
|      39 |  6225 | `		for( g = 0; g <= nMaxGroup; g++ ){ if( aGroupCount[g] > 0 ) nGroups++; }` |
|      39 |  6226 | `		for( g = 0; g <= nMaxGroup; g++ ){` |
|      23 |  6227 | `			int bFirstMember = 1;` |
|       - |  6228 | `			int bWrap;` |
|      23 |  6229 | `			if( aGroupCount[g] == 0 ) continue;` |
|       - |  6230 | ``			/* Wrap a ≥2-member group in `()` whenever it shares the type with any`` |
|       - |  6231 | ``			 * other alternative — another group OR a trailing `null` (which is not`` |
|       - |  6232 | ``			 * counted in nGroups). So `A&B` stays bare but `(A&B)\|null` keeps its`` |
|       - |  6233 | `			 * parens, matching PHP's canonical text. */` |
|      31 |  6234 | `			bWrap = (aGroupCount[g] >= 2 && (nGroups > 1 \|\| bNullable));` |
|      23 |  6235 | `			if( !bFirstGroup ) SyBlobAppend(pBlob, "\|", 1);` |
|      23 |  6236 | `			if( bWrap ) SyBlobAppend(pBlob, "(", 1);` |
|      71 |  6237 | `			for( i = 0; i < nAtoms; i++ ){` |
|      51 |  6238 | `				if( aAtoms[i].nType == UTA_NULL_FLAG \|\| aAtoms[i].nGroup != g ) continue;` |
|      39 |  6239 | `				if( !bFirstMember ) SyBlobAppend(pBlob, "&", 1);` |
|      39 |  6240 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      37 |  6241 | `					SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      20 |  6242 | `				}else{` |
|       3 |  6243 | `					SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6244 | `				}` |
|      39 |  6245 | `				bFirstMember = 0;` |
|      21 |  6246 | `			}` |
|      23 |  6247 | `			if( bWrap ) SyBlobAppend(pBlob, ")", 1);` |
|      23 |  6248 | `			bFirstGroup = 0;` |
|      13 |  6249 | `		}` |
|      19 |  6250 | `		if( bNullable ){` |
|     ! 0 |  6251 | `			SyBlobAppend(pBlob, "\|", 1);` |
|     ! 0 |  6252 | `			SyBlobAppend(pBlob, "null", 4);` |
|     ! 0 |  6253 | `		}` |
|      57 |  6254 | `		return;` |
|       - |  6255 | `	}` |
|   78823 |  6256 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6257 | `		/* Shorthand: ?T */` |
|      81 |  6258 | `		for( i = 0; i < nAtoms; i++ ){` |
|      81 |  6259 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      81 |  6260 | `			SyBlobAppend(pBlob, "?", 1);` |
|      81 |  6261 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      22 |  6262 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      13 |  6263 | `			}else{` |
|      63 |  6264 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6265 | `			}` |
|      81 |  6266 | `			return;` |
|     ! 0 |  6267 | `		}` |
|     ! 0 |  6268 | `	}` |
|       - |  6269 | `	{` |
|   78747 |  6270 | `		int bFirst = 1;` |
|       - |  6271 | `		/* 1) Classes in declaration order */` |
|  157591 |  6272 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78849 |  6273 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|   14333 |  6274 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   14333 |  6275 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|   14333 |  6276 | `				bFirst = 0;` |
|    7164 |  6277 | `			}` |
|   39427 |  6278 | `		}` |
|       - |  6279 | `		/* 2) Built-ins in canonical order */` |
|       - |  6280 | `		{` |
|       - |  6281 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6282 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6283 | `			int k;` |
|  551199 |  6284 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  881063 |  6285 | `				for( i = 0; i < nAtoms; i++ ){` |
|  472961 |  6286 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   64355 |  6287 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   64355 |  6288 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   64355 |  6289 | `						bFirst = 0;` |
|   64355 |  6290 | `						break;` |
|       - |  6291 | `					}` |
|  204308 |  6292 | `				}` |
|  236231 |  6293 | `			}` |
|       - |  6294 | `		}` |
|       - |  6295 | `		/* 3) null suffix */` |
|   78747 |  6296 | `		if( bNullable ){` |
|      20 |  6297 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|      20 |  6298 | `			SyBlobAppend(pBlob, "null", 4);` |
|       8 |  6299 | `		}` |
|       - |  6300 | `	}` |
|   39422 |  6301 |  |
|       - |  6302 |  |
|       - |  6303 | `/*` |
|       - |  6304 | `` * Parse one `\|`-separated part of a type declaration into aAtoms[*pnAtoms..],`` |
|       - |  6305 | ` * tagging each appended atom with group id iGroup. A part is one of:` |
|       - |  6306 | `` *   - a parenthesized intersection  `(` atom (`&` atom)+ `)`   (DNF group), or`` |
|       - |  6307 | `` *   - a bare atom, optionally followed by a top-level intersection atom (`&` atom)+.`` |
|       - |  6308 | ` * On return *pnMembers is the number of atoms in this part and *pbParen records` |
|       - |  6309 | ` * whether it was parenthesized.` |
|       - |  6310 | ` *` |
|       - |  6311 | `` * The `&`-vs-by-reference ambiguity (`A&B $x` intersection vs `A &$x` by-ref) is`` |
|       - |  6312 | `` * resolved by a one-token lookahead: `&` continues the intersection only when it`` |
|       - |  6313 | ` * is followed by a type atom (namespace separator / identifier / keyword);` |
|       - |  6314 | ` * otherwise it belongs to a by-ref parameter marker and the part ends, leaving` |
|       - |  6315 | `` * the `&` for the caller (compile.c param loop) to consume.`` |
|       - |  6316 | ` */` |
|   78970 |  6317 | `static sxi32 GenStateParsePart(` |
|       - |  6318 | `	ph7_gen_state *pGen, PhlTypeAtom *aAtoms, int *pnAtoms, sxu32 iGroup,` |
|       - |  6319 | `	int *pnMembers, int *pbParen, sxu32 nLine)` |
|       5 |  6320 |  |
|       - |  6321 | `	sxi32 rc;` |
|   78975 |  6322 | `	int nMembers = 0;` |
|   78975 |  6323 | `	int bParen = 0;` |
|   78975 |  6324 | `	*pnMembers = 0;` |
|   78975 |  6325 | `	*pbParen = 0;` |
|   78975 |  6326 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       6 |  6327 | `		bParen = 1;` |
|       6 |  6328 | `		pGen->pIn++; /* skip '(' */` |
|       2 |  6329 | `	}` |
|   39485 |  6330 | `	for(;;){` |
|   78993 |  6331 | `		if( *pnAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6332 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6333 | `				"Too many alternatives in type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6334 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6335 | `		}` |
|   78993 |  6336 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[*pnAtoms]);` |
|   78993 |  6337 | `		if( rc != SXRET_OK ){` |
|       3 |  6338 | `			return rc;` |
|       - |  6339 | `		}` |
|   78991 |  6340 | `		aAtoms[*pnAtoms].nGroup = iGroup;` |
|   78991 |  6341 | `		(*pnAtoms)++;` |
|   78991 |  6342 | `		nMembers++;` |
|       - |  6343 | ``		/* Continue the intersection while `&` is followed by another type atom. */`` |
|   78991 |  6344 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      24 |  6345 | `			SyToken *pNext = &pGen->pIn[1];` |
|      20 |  6346 | `			if( pNext < pGen->pEnd` |
|      24 |  6347 | `			 && (pNext->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      22 |  6348 | `				pGen->pIn++; /* skip '&' */` |
|      22 |  6349 | `				continue;` |
|       - |  6350 | `			}` |
|       1 |  6351 | `		}` |
|   78973 |  6352 | `		break;` |
|     ! 0 |  6353 | `	}` |
|   78973 |  6354 | `	if( bParen ){` |
|       6 |  6355 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6356 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6357 | `				"Malformed DNF type: expecting ')'");` |
|     ! 0 |  6358 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6359 | `		}` |
|       6 |  6360 | `		pGen->pIn++; /* skip ')' */` |
|       6 |  6361 | `		if( nMembers < 2 ){` |
|     ! 0 |  6362 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6363 | `				"Parenthesized type must be an intersection of at least two types");` |
|     ! 0 |  6364 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6365 | `		}` |
|       2 |  6366 | `	}` |
|   78973 |  6367 | `	*pnMembers = nMembers;` |
|   78973 |  6368 | `	*pbParen = bParen;` |
|   78973 |  6369 | `	return SXRET_OK;` |
|   39490 |  6370 |  |
|       - |  6371 |  |
|       - |  6372 | `/*` |
|       - |  6373 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6374 | ` *` |
|       - |  6375 | ` * Outputs:` |
|       - |  6376 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6377 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6378 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6379 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6380 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6381 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6382 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6383 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6384 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6385 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6386 | ` *` |
|       - |  6387 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6388 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6389 | ` */` |
|   78846 |  6390 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6391 | `	ph7_gen_state *pGen,` |
|       - |  6392 | `	sxu32 *pnType,` |
|       - |  6393 | `	SyString *pClass,` |
|       - |  6394 | `	SySet *pAlts,` |
|       - |  6395 | `	sxi32 *piTypeFlags,` |
|       - |  6396 | `	SyString *pTypeText,` |
|       - |  6397 | `	int iNullableFlag,` |
|       - |  6398 | `	int iUnionFlag,` |
|       - |  6399 | `	int bAllowVoid,` |
|       - |  6400 | `	sxu32 nLine` |
|       5 |  6401 | `){` |
|       - |  6402 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   78851 |  6403 | `	int nAtoms = 0;` |
|   78851 |  6404 | `	int bShortNullable = 0;` |
|   78851 |  6405 | `	int bExplicitNull = 0;` |
|       - |  6406 | `	sxi32 rc;` |
|   78851 |  6407 | `	*pnType = 0;` |
|   78851 |  6408 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   78851 |  6409 | `	*piTypeFlags = 0;` |
|   78851 |  6410 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6411 |  |
|   78851 |  6412 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6413 | `		return SXRET_OK;` |
|       - |  6414 | `	}` |
|       - |  6415 | ``	/* Optional `?` shorthand prefix */`` |
|   78846 |  6416 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      71 |  6417 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      71 |  6418 | `		bShortNullable = 1;` |
|      71 |  6419 | `		pGen->pIn++;` |
|      71 |  6420 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6421 | `			return SXERR_SYNTAX;` |
|       - |  6422 | `		}` |
|      33 |  6423 | `	}` |
|       - |  6424 | `	/* Parse the first part (a single atom, a bare top-level intersection, or a` |
|       - |  6425 | ``	 * parenthesized DNF intersection), then any further `\|`-separated parts. Each`` |
|       - |  6426 | `	 * part is one OR-group; atoms within an intersection share the group id. */` |
|       - |  6427 | `	{` |
|       - |  6428 | `		int nMembers, bParen;` |
|   78851 |  6429 | `		sxu32 iGroup = 0;` |
|   78851 |  6430 | `		rc = GenStateParsePart(pGen, aAtoms, &nAtoms, iGroup, &nMembers, &bParen, nLine);` |
|   78851 |  6431 | `		if( rc != SXRET_OK ){` |
|       4 |  6432 | `			return rc;` |
|       - |  6433 | `		}` |
|       - |  6434 | ``		/* Subsequent parts separated by `\|`. A bare (unparenthesized) intersection`` |
|       - |  6435 | ``		 * is legal only as the sole part; once a `\|` makes this a union every part`` |
|       - |  6436 | ``		 * must be a single type or a parenthesized intersection (`A&B\|C` is invalid,`` |
|       - |  6437 | ``		 * write `(A&B)\|C`). The loop-top check rejects a bare intersection followed`` |
|       - |  6438 | ``		 * by `\|`; the after-loop check rejects one as the trailing part of a union. */`` |
|  118454 |  6439 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   79037 |  6440 | `			&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|     131 |  6441 | `			if( bShortNullable ){` |
|       - |  6442 | ``				/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6443 | `				 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6444 | `				 * already reported" so callers skip their own error emission. */` |
|       3 |  6445 | `				rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6446 | `					"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6447 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6448 | `			}` |
|     129 |  6449 | `			if( nMembers >= 2 && !bParen ){` |
|     ! 0 |  6450 | `				rc = PH7_GenCompileError(pGen, E_ERROR, pGen->pIn->nLine,` |
|       - |  6451 | `					"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6452 | `				return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6453 | `			}` |
|     129 |  6454 | ``			pGen->pIn++; /* skip `\|` */`` |
|     129 |  6455 | `			rc = GenStateParsePart(pGen, aAtoms, &nAtoms, ++iGroup, &nMembers, &bParen, nLine);` |
|     129 |  6456 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6457 | `				return rc;` |
|       - |  6458 | `			}` |
|       5 |  6459 | `		}` |
|   78847 |  6460 | `		if( iGroup > 0 && nMembers >= 2 && !bParen ){` |
|     ! 0 |  6461 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6462 | `				"Unparenthesized intersection type cannot be part of a union; wrap it in parentheses");` |
|     ! 0 |  6463 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6464 | `		}` |
|       - |  6465 | `	}` |
|       - |  6466 | `	/* Validation pass.` |
|       - |  6467 | `	 *` |
|       - |  6468 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6469 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6470 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6471 | `	 */` |
|       - |  6472 | `	{` |
|       - |  6473 | `		int i, j;` |
|   78847 |  6474 | `		int bHasNonNull = 0;` |
|   78847 |  6475 | `		int bAnyIntersection = 0;` |
|       - |  6476 | `		sxu32 aGroupCount[PHL_UNION_MAX_ALTS];` |
|       - |  6477 | `		/* Tally how many atoms each OR-group holds; a group of ≥2 is an` |
|       - |  6478 | `		 * intersection. (Group ids are 0..parts-1, bounded by nAtoms.) */` |
| 2601791 |  6479 | `		for( i = 0; i < PHL_UNION_MAX_ALTS; i++ ) aGroupCount[i] = 0;` |
|  157831 |  6480 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78989 |  6481 | `			if( aAtoms[i].nGroup < PHL_UNION_MAX_ALTS ) aGroupCount[aAtoms[i].nGroup]++;` |
|   39497 |  6482 | `		}` |
|  157793 |  6483 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78969 |  6484 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){ bAnyIntersection = 1; break; }` |
|   39478 |  6485 | `		}` |
|       - |  6486 | ``		/* PHP forbids a nullable intersection via the `?` shorthand — `?A&B` must`` |
|       - |  6487 | ``		 * be written `(A&B)\|null` (handled by the explicit-null DNF path). */`` |
|   78847 |  6488 | `		if( bShortNullable && bAnyIntersection ){` |
|     ! 0 |  6489 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6490 | `				"Nullable intersection types are not supported; use (A&B)\|null instead");` |
|     ! 0 |  6491 | `			return SXERR_SYNTAX;` |
|       - |  6492 | `		}` |
|  157821 |  6493 | `		for( i = 0; i < nAtoms; i++ ){` |
|       - |  6494 | `			/* Intersection members must be class/interface types (PHP rejects` |
|       - |  6495 | ``			 * scalars, `object`, and the pseudo-types `iterable`/`callable`/`` |
|       - |  6496 | ``			 * `true`/`false` in an intersection). */`` |
|   78987 |  6497 | `			if( aGroupCount[aAtoms[i].nGroup] >= 2 ){` |
|      38 |  6498 | `				int bClassLike = (aAtoms[i].nType == SXU32_HIGH);` |
|      38 |  6499 | `				if( bClassLike ){` |
|      35 |  6500 | `					SyString *pC = &aAtoms[i].sClass;` |
|      32 |  6501 | `					if( (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"iterable",8) == 0)` |
|      32 |  6502 | `					 \|\| (pC->nByte == 8 && SyMemcmpNoCase(pC->zString,"callable",8) == 0)` |
|      32 |  6503 | `					 \|\| (pC->nByte == 4 && SyMemcmpNoCase(pC->zString,"true",4) == 0)` |
|      35 |  6504 | `					 \|\| (pC->nByte == 5 && SyMemcmpNoCase(pC->zString,"false",5) == 0) ){` |
|     ! 0 |  6505 | `						bClassLike = 0;` |
|     ! 0 |  6506 | `					}` |
|      16 |  6507 | `				}` |
|      38 |  6508 | `				if( !bClassLike ){` |
|       - |  6509 | `					const char *zName; sxu32 nName;` |
|       3 |  6510 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6511 | `						zName = aAtoms[i].sClass.zString; nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6512 | `					}else{` |
|       3 |  6513 | `						zName = aAtoms[i].zCanon; nName = aAtoms[i].nCanon;` |
|       - |  6514 | `					}` |
|       4 |  6515 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6516 | `						"Type %.*s cannot be part of an intersection type",` |
|       1 |  6517 | `						(int)nName, zName);` |
|       3 |  6518 | `					return SXERR_SYNTAX;` |
|       - |  6519 | `				}` |
|      16 |  6520 | `			}` |
|   78985 |  6521 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|     157 |  6522 | `				if( nAtoms > 1 ){` |
|       3 |  6523 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6524 | `						"Void can only be used as a standalone type");` |
|       3 |  6525 | `					return SXERR_SYNTAX;` |
|       - |  6526 | `				}` |
|     155 |  6527 | `				if( !bAllowVoid ){` |
|     ! 0 |  6528 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6529 | `						"void cannot be used here");` |
|     ! 0 |  6530 | `					return SXERR_SYNTAX;` |
|       - |  6531 | `				}` |
|     155 |  6532 | `				if( bShortNullable ){` |
|     ! 0 |  6533 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6534 | `						"Void type cannot be nullable");` |
|     ! 0 |  6535 | `					return SXERR_SYNTAX;` |
|       - |  6536 | `				}` |
|      75 |  6537 | `			}` |
|   78983 |  6538 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6539 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6540 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6541 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6542 | `				 * does not return), and folding them would mislead any` |
|       - |  6543 | `				 * future return-enforcement work. */` |
|       3 |  6544 | `				if( nAtoms > 1 ){` |
|       3 |  6545 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6546 | `						"never can only be used as a standalone type");` |
|       3 |  6547 | `					return SXERR_SYNTAX;` |
|       - |  6548 | `				}` |
|     ! 0 |  6549 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6550 | `					"never type is not yet implemented");` |
|     ! 0 |  6551 | `				return SXERR_SYNTAX;` |
|       - |  6552 | `			}` |
|   78981 |  6553 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      32 |  6554 | `				bExplicitNull = 1;` |
|      18 |  6555 | `			}else{` |
|   78953 |  6556 | `				bHasNonNull = 1;` |
|       - |  6557 | `			}` |
|       - |  6558 | `			/* Duplicate detection. Flag a repeat only within the same group` |
|       - |  6559 | ``			 * (intersection dup `A&A`) or between two singleton groups (union dup`` |
|       - |  6560 | ``			 * `int\|int` / `A\|A`); a class appearing in two distinct intersection`` |
|       - |  6561 | ``			 * groups (`(A&B)\|(A&C)`) is legal, so skip those pairs. (Exhaustive DNF`` |
|       - |  6562 | ``			 * subsumption — e.g. `(A&B)\|A` — is deferred.) */`` |
|   79161 |  6563 | `			for( j = 0; j < i; j++ ){` |
|     187 |  6564 | `				int bDup = 0;` |
|     187 |  6565 | `				int bSameGroup = (aAtoms[i].nGroup == aAtoms[j].nGroup);` |
|     359 |  6566 | `				int bBothSingleton = (aGroupCount[aAtoms[i].nGroup] == 1` |
|     182 |  6567 | `				                   && aGroupCount[aAtoms[j].nGroup] == 1);` |
|     187 |  6568 | `				if( !bSameGroup && !bBothSingleton ) continue;` |
|     179 |  6569 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      40 |  6570 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      34 |  6571 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      37 |  6572 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|      16 |  6573 | `								aAtoms[j].sClass.zString,` |
|      32 |  6574 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6575 | `							bDup = 1;` |
|     ! 0 |  6576 | `						}` |
|      21 |  6577 | `					}else{` |
|       3 |  6578 | `						bDup = 1;` |
|       - |  6579 | `					}` |
|      18 |  6580 | `				}` |
|     179 |  6581 | `				if( bDup ){` |
|       - |  6582 | `					const char *zName;` |
|       - |  6583 | `					sxu32 nName;` |
|       3 |  6584 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6585 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6586 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6587 | `					}else{` |
|       3 |  6588 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6589 | `						nName = aAtoms[i].nCanon;` |
|       - |  6590 | `					}` |
|       4 |  6591 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6592 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6593 | `					return SXERR_SYNTAX;` |
|       - |  6594 | `				}` |
|      91 |  6595 | `			}` |
|   39492 |  6596 | `		}` |
|   78839 |  6597 | `		if( !bHasNonNull && bExplicitNull ){` |
|       7 |  6598 | `			if( bShortNullable ){` |
|       - |  6599 | ``				/* `?null` is not a valid type — PHP rejects the shorthand. */`` |
|     ! 0 |  6600 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6601 | `					"Null can not be used as a standalone type");` |
|     ! 0 |  6602 | `				return SXERR_SYNTAX;` |
|       - |  6603 | `			}` |
|       - |  6604 | ``			/* Bare `null` standalone type (PHP 8.2): represent it as the null`` |
|       - |  6605 | `			 * type flag so enforcement accepts only null. The single-type fast` |
|       - |  6606 | `			 * path below leaves *pnType untouched when there is no non-null` |
|       - |  6607 | `			 * atom, so set it here. */` |
|       7 |  6608 | `			*pnType = MEMOBJ_NULL;` |
|       3 |  6609 | `		}` |
|       - |  6610 | `	}` |
|       - |  6611 | `	/* Compute nullability flag */` |
|   78839 |  6612 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      97 |  6613 | `		*piTypeFlags \|= iNullableFlag;` |
|      46 |  6614 | `	}` |
|       - |  6615 | `	/* Build canonical type text */` |
|   78839 |  6616 | `	if( pTypeText ){` |
|       - |  6617 | `		SyBlob sBlob;` |
|   78839 |  6618 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|  118224 |  6619 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   39417 |  6620 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   78839 |  6621 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|  118031 |  6622 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   78684 |  6623 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   78689 |  6624 | `			if( zDup ){` |
|   78689 |  6625 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   39342 |  6626 | `			}` |
|   39342 |  6627 | `		}` |
|   78839 |  6628 | `		SyBlobRelease(&sBlob);` |
|   39417 |  6629 | `	}` |
|       - |  6630 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6631 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6632 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6633 | `	{` |
|   78839 |  6634 | `		int nNonNull = 0;` |
|   78839 |  6635 | `		int iNonNullIdx = -1;` |
|       - |  6636 | `		int i;` |
|  157807 |  6637 | `		for( i = 0; i < nAtoms; i++ ){` |
|   78973 |  6638 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   78945 |  6639 | `				nNonNull++;` |
|   78945 |  6640 | `				iNonNullIdx = i;` |
|   39470 |  6641 | `			}` |
|   39489 |  6642 | `		}` |
|   78839 |  6643 | `		if( nNonNull <= 1 ){` |
|       - |  6644 | `			/* Fast path: store as single type. */` |
|   78747 |  6645 | `			if( iNonNullIdx >= 0 ){` |
|   78741 |  6646 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   78741 |  6647 | `				if( pA->nType == SXU32_HIGH ){` |
|   21464 |  6648 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    7153 |  6649 | `						pA->sClass.zString, pA->sClass.nByte);` |
|   14311 |  6650 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|   14311 |  6651 | `					*pnType = SXU32_HIGH;` |
|   14311 |  6652 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   71588 |  6653 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|     155 |  6654 | `					*pnType = MEMOBJ_VOID;` |
|      80 |  6655 | `				}else{` |
|       - |  6656 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6657 | `					 * pass above rejects it as not-yet-implemented. */` |
|   64285 |  6658 | `					*pnType = pA->nType;` |
|       - |  6659 | `				}` |
|   39368 |  6660 | `			}` |
|   39376 |  6661 | `		}else{` |
|       - |  6662 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      97 |  6663 | `			*piTypeFlags \|= iUnionFlag;` |
|     311 |  6664 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6665 | `				ph7_type_alt sAlt;` |
|     219 |  6666 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     209 |  6667 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     209 |  6668 | `				sAlt.nGroup = aAtoms[i].nGroup; /* preserve intersection grouping */` |
|     209 |  6669 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|     116 |  6670 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      37 |  6671 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      79 |  6672 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      79 |  6673 | `					sAlt.nType = SXU32_HIGH;` |
|      79 |  6674 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      42 |  6675 | `				}else{` |
|     135 |  6676 | `					sAlt.nType = aAtoms[i].nType;` |
|     135 |  6677 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6678 | `				}` |
|     209 |  6679 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|     107 |  6680 | `			}` |
|       - |  6681 | `		}` |
|       - |  6682 | `	}` |
|   78839 |  6683 | `	return SXRET_OK;` |
|   39428 |  6684 |  |
|       - |  6685 |  |
|       - |  6686 | `/*` |
|       - |  6687 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6688 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6689 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6690 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6691 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6692 | `` *          and union types `: T\|U`.`` |
|       - |  6693 | ` */` |
|  317580 |  6694 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       5 |  6695 |  |
|  317585 |  6696 | `	sxi32 iFlags = 0;` |
|       - |  6697 | `	sxi32 rc;` |
|       - |  6698 | `	sxu32 nLine;` |
|  317585 |  6699 | `	pFunc->nReturnType = 0;` |
|  317585 |  6700 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  317585 |  6701 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  317585 |  6702 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  317111 |  6703 | `		return SXRET_OK;` |
|       - |  6704 | `	}` |
|     479 |  6705 | `	pGen->pIn++; /* Skip ':' */` |
|     479 |  6706 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6707 | `		return SXRET_OK;` |
|       - |  6708 | `	}` |
|     479 |  6709 | `	nLine = pGen->pIn->nLine;` |
|     479 |  6710 | `	rc = GenStateParseUnionTypeDecl(` |
|     237 |  6711 | `		pGen,` |
|     237 |  6712 | `		&pFunc->nReturnType,` |
|     237 |  6713 | `		&pFunc->sReturnClass,` |
|     237 |  6714 | `		&pFunc->aReturnUnion,` |
|       - |  6715 | `		&iFlags,` |
|     237 |  6716 | `		&pFunc->sReturnTypeName,` |
|       - |  6717 | `		VM_FUNC_RETURN_NULLABLE, /* nullability flag — a null alternative isn't stored` |
|       - |  6718 | `		                          * in aReturnUnion, so the func carries it explicitly */` |
|       - |  6719 | `		/* iUnionFlag */ 0,` |
|       - |  6720 | `		/* bAllowVoid */ 1,` |
|     237 |  6721 | `		nLine);` |
|     479 |  6722 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6723 | `		return SXERR_ABORT;` |
|       - |  6724 | `	}` |
|     479 |  6725 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6726 | `		/* Error already reported */` |
|     ! 0 |  6727 | `		return SXERR_SYNTAX;` |
|       - |  6728 | `	}` |
|     479 |  6729 | `	if( rc == SXERR_SYNTAX ){` |
|       6 |  6730 | `		if( pGen->pIn < pGen->pEnd ){` |
|       8 |  6731 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6732 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6733 | `				&pGen->pIn->sData);` |
|       4 |  6734 | `		}else{` |
|     ! 0 |  6735 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6736 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6737 | `		}` |
|       6 |  6738 | `		return SXERR_SYNTAX;` |
|       - |  6739 | `	}` |
|     475 |  6740 | `	pFunc->iFlags \|= (iFlags & VM_FUNC_RETURN_NULLABLE);` |
|     475 |  6741 | `	return SXRET_OK;` |
|  158795 |  6742 |  |
|       - |  6743 |  |
|   47322 |  6744 | `static sxi32 GenStateCompileFunc(` |
|       - |  6745 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6746 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6747 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6748 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6749 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6750 | `	)` |
|       5 |  6751 |  |
|       - |  6752 | `	ph7_vm_func *pFunc;` |
|       - |  6753 | `	SyToken *pEnd;` |
|       - |  6754 | `	sxu32 nLine;` |
|       - |  6755 | `	char *zName;` |
|       - |  6756 | `	sxi32 rc;` |
|       - |  6757 | `	/* Extract line number */` |
|   47327 |  6758 | `	nLine = pGen->pIn->nLine;` |
|       - |  6759 | `	/* Jump the left parenthesis '(' */` |
|   47327 |  6760 | `	pGen->pIn++;` |
|       - |  6761 | `	/* Delimit the function signature */` |
|   47327 |  6762 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   47327 |  6763 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6764 | `		/* Syntax error */` |
|       8 |  6765 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       8 |  6766 | `		if( rc == SXERR_ABORT ){` |
|       - |  6767 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6768 | `			return SXERR_ABORT;` |
|       - |  6769 | `		}` |
|       8 |  6770 | `		pGen->pIn = pGen->pEnd;` |
|       8 |  6771 | `		return SXRET_OK;` |
|       - |  6772 | `	}` |
|       - |  6773 | `	/* Create the function state */` |
|   47321 |  6774 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   47321 |  6775 | `	if( pFunc == 0 ){` |
|     ! 0 |  6776 | `		goto OutOfMem;` |
|       - |  6777 | `	}` |
|       - |  6778 | `	/* Build the function name, prepending namespace if active */` |
|   47328 |  6779 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6780 | `		SyBlob sFQN;` |
|       - |  6781 | `		sxu32 nLen;` |
|      16 |  6782 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6783 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6784 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6785 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6786 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6787 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6788 | `		SyBlobRelease(&sFQN);` |
|      16 |  6789 | `		if( zName == 0 ){` |
|     ! 0 |  6790 | `			goto OutOfMem;` |
|       - |  6791 | `		}` |
|      16 |  6792 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6793 | `	}else{` |
|   47307 |  6794 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   47307 |  6795 | `		if( zName == 0 ){` |
|     ! 0 |  6796 | `			goto OutOfMem;` |
|       - |  6797 | `		}` |
|   47307 |  6798 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6799 | `	}` |
|   47321 |  6800 | `	if( pGen->pIn < pEnd ){` |
|       - |  6801 | `		/* Collect function arguments */` |
|   32647 |  6802 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   32647 |  6803 | `		if( rc == SXERR_ABORT ){` |
|       - |  6804 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6805 | `			return SXERR_ABORT;` |
|       - |  6806 | `		}` |
|   16321 |  6807 | `	}` |
|       - |  6808 | `	/* Point past ')' and parse optional return type ': type' */` |
|   47321 |  6809 | `	pGen->pIn = &pEnd[1];` |
|       - |  6810 | `	{` |
|   47321 |  6811 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   47321 |  6812 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6813 | `			return SXERR_ABORT;` |
|   47321 |  6814 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       6 |  6815 | `			return SXERR_SYNTAX;` |
|       - |  6816 | `		}` |
|       - |  6817 | `	}` |
|   47317 |  6818 | `	if( bHandleClosure ){` |
|       - |  6819 | `		ph7_vm_func_closure_env sEnv;` |
|     275 |  6820 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     270 |  6821 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     149 |  6822 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      23 |  6823 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6824 | `				/* Closure,record environment variable */` |
|      23 |  6825 | `				pGen->pIn++;` |
|      23 |  6826 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6827 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6828 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6829 | `						return SXERR_ABORT;` |
|       - |  6830 | `					}` |
|     ! 0 |  6831 | `				}` |
|      23 |  6832 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6833 | `				/* Compile until we hit the first closing parenthesis */` |
|      45 |  6834 | `				while( pGen->pIn < pGen->pEnd ){` |
|      45 |  6835 | `					int iFlagsLocal = 0;` |
|      45 |  6836 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      23 |  6837 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      23 |  6838 | `						break;` |
|       - |  6839 | `					}` |
|      27 |  6840 | `					nLineLocal = pGen->pIn->nLine;` |
|      27 |  6841 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6842 | `						/* Pass by reference,record that */` |
|     ! 0 |  6843 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6844 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6845 | `							);` |
|     ! 0 |  6846 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6847 | `						pGen->pIn++;` |
|     ! 0 |  6848 | `					}` |
|      22 |  6849 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      27 |  6850 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6851 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6852 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6853 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6854 | `								return SXERR_ABORT;` |
|       - |  6855 | `							}` |
|       - |  6856 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6857 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6858 | `								pGen->pIn++;` |
|     ! 0 |  6859 | `							}` |
|     ! 0 |  6860 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6861 | `								pGen->pIn++;` |
|     ! 0 |  6862 | `							}` |
|     ! 0 |  6863 | `							break;` |
|       - |  6864 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6865 | `					}else{` |
|       - |  6866 | `						SyString *pNameLocal;` |
|       - |  6867 | `						char *zDup;` |
|       - |  6868 | `						/* Duplicate variable name */` |
|      27 |  6869 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      27 |  6870 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      27 |  6871 | `						if( zDup ){` |
|       - |  6872 | `							/* Zero the structure */` |
|      27 |  6873 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      27 |  6874 | `							sEnv.iFlags = iFlagsLocal;` |
|      27 |  6875 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      27 |  6876 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      27 |  6877 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6878 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6879 | `									got_this = 1;` |
|     ! 0 |  6880 | `							}` |
|       - |  6881 | `							/* Save imported variable */` |
|      27 |  6882 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      16 |  6883 | `						}else{` |
|     ! 0 |  6884 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6885 | `							 return SXERR_ABORT;` |
|       - |  6886 | `						}` |
|       - |  6887 | `					}` |
|      27 |  6888 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      33 |  6889 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6890 | `						/* Ignore trailing commas */` |
|       7 |  6891 | `						pGen->pIn++;` |
|       1 |  6892 | `					}` |
|       5 |  6893 | `				}` |
|      23 |  6894 | `				if( !got_this ){` |
|       - |  6895 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6896 | `					 * available to the closure environment.` |
|       - |  6897 | `					 */` |
|      23 |  6898 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      23 |  6899 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      23 |  6900 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      23 |  6901 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      23 |  6902 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       9 |  6903 | `				}` |
|      23 |  6904 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6905 | `					/* Mark as closure */` |
|      23 |  6906 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       9 |  6907 | `				}` |
|       9 |  6908 | `		}` |
|     135 |  6909 | `	}` |
|       - |  6910 | `	/* Compile the body */` |
|   47317 |  6911 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   47317 |  6912 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6913 | `		return SXERR_ABORT;` |
|       - |  6914 | `	}` |
|   47317 |  6915 | `	if( ppFunc ){` |
|     275 |  6916 | `		*ppFunc = pFunc;` |
|     135 |  6917 | `	}` |
|   47317 |  6918 | `	rc = SXRET_OK;` |
|   47317 |  6919 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6920 | `		/* Finally register the function */` |
|   47299 |  6921 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   23647 |  6922 | `	}` |
|   47317 |  6923 | `	if( rc == SXRET_OK ){` |
|   47317 |  6924 | `		return SXRET_OK;` |
|       - |  6925 | `	}` |
|       - |  6926 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6927 | `OutOfMem:` |
|       - |  6928 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6929 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6930 | `	 */` |
|     ! 0 |  6931 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6932 | `	return SXERR_ABORT;` |
|   23666 |  6933 |  |
|       - |  6934 | `/*` |
|       - |  6935 | ` * Compile a standard PHP function.` |
|       - |  6936 | ` *  Refer to the block-comment above for more information.` |
|       - |  6937 | ` */` |
|   47060 |  6938 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       5 |  6939 |  |
|       - |  6940 | `	SyString *pName;` |
|       - |  6941 | `	sxi32 iFlags;` |
|       - |  6942 | `	sxu32 nLine;` |
|       - |  6943 | `	sxi32 rc;` |
|       - |  6944 |  |
|   47065 |  6945 | `	nLine = pGen->pIn->nLine;` |
|   47065 |  6946 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   47065 |  6947 | `	iFlags = 0;` |
|   47065 |  6948 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6949 | `		/* Return by reference,remember that */` |
|       7 |  6950 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6951 | `		/* Jump the '&' token */` |
|       7 |  6952 | `		pGen->pIn++;` |
|       3 |  6953 | `	}` |
|   47065 |  6954 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6955 | `		/* Invalid function name */` |
|       7 |  6956 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       7 |  6957 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6958 | `			return SXERR_ABORT;` |
|       - |  6959 | `		}` |
|       - |  6960 | `		/* Sychronize with the next semi-colon or braces*/` |
|      21 |  6961 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      15 |  6962 | `			pGen->pIn++;` |
|       1 |  6963 | `		}` |
|       7 |  6964 | `		return SXRET_OK;` |
|       - |  6965 | `	}` |
|   47059 |  6966 | `	pName = &pGen->pIn->sData;` |
|   47059 |  6967 | `	nLine = pGen->pIn->nLine;` |
|       - |  6968 | `	/* Jump the function name */` |
|   47059 |  6969 | `	pGen->pIn++;` |
|   47059 |  6970 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6971 | `		/* Syntax error */` |
|       3 |  6972 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6973 | `		if( rc == SXERR_ABORT ){` |
|       - |  6974 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6975 | `			return SXERR_ABORT;` |
|       - |  6976 | `		}` |
|       - |  6977 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6978 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6979 | `			pGen->pIn++;` |
|     ! 0 |  6980 | `		}` |
|       3 |  6981 | `		return SXRET_OK;` |
|       - |  6982 | `	}` |
|       - |  6983 | `	/* Compile function body */` |
|   47057 |  6984 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   47057 |  6985 | `	return rc;` |
|   23535 |  6986 |  |
|       - |  6987 | `/*` |
|       - |  6988 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6989 | ` * According to the PHP language reference manual` |
|       - |  6990 | ` *  Visibility:` |
|       - |  6991 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6992 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6993 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6994 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6995 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6996 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6997 | ` */` |
|  341650 |  6998 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       5 |  6999 |  |
|  341655 |  7000 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|   14271 |  7001 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  327389 |  7002 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   46077 |  7003 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  7004 | `	}` |
|       - |  7005 | `	/* Assume public by default */` |
|  281317 |  7006 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  170830 |  7007 |  |
|       - |  7008 | `/*` |
|       - |  7009 | ` * Compile a class constant.` |
|       - |  7010 | ` * According to the PHP language reference manual` |
|       - |  7011 | ` *  Class Constants` |
|       - |  7012 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  7013 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  7014 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  7015 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  7016 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  7017 | ` *   It's also possible for interfaces to have constants.` |
|       - |  7018 | ` * Symisc eXtension.` |
|       - |  7019 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  7020 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7021 | ` *  Example:` |
|       - |  7022 | ` *   class Test{` |
|       - |  7023 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7024 | ` *   };` |
|       - |  7025 | ` *   var_dump(TEST::MyConst);` |
|       - |  7026 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7027 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7028 | ` */` |
|       - |  7029 | `/*` |
|       - |  7030 | ` * Decide whether a typed class constant (PHP 8.3) declares a type before its` |
|       - |  7031 | `` * name. The classic untyped form is `const NAME = value` — a single name-like`` |
|       - |  7032 | ` * token immediately followed by '='. Anything else with a leading type token` |
|       - |  7033 | `` * (`const int X`, `const ?int X`, `const A\|B X`, `const \Ns\Foo X`) declares a`` |
|       - |  7034 | ` * type. We only commit to the type-parse when the shape is unambiguous so the` |
|       - |  7035 | ` * untyped path never runs (and never trips the type parser's diagnostics).` |
|       - |  7036 | ` */` |
|      78 |  7037 | `static int GenStateClassConstHasType(ph7_gen_state *pGen)` |
|       5 |  7038 |  |
|       - |  7039 | `	SyToken *p0, *p1;` |
|      83 |  7040 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7041 | `		return 0;` |
|       - |  7042 | `	}` |
|      83 |  7043 | `	p0 = pGen->pIn;` |
|       - |  7044 | `	/* A leading '\' (namespaced class type) or '?' (nullable) always starts a type */` |
|      83 |  7045 | `	if( p0->nType & PH7_TK_NSSEP ){` |
|     ! 0 |  7046 | `		return 1;` |
|       - |  7047 | `	}` |
|      83 |  7048 | `	if( (p0->nType & PH7_TK_OP) && p0->sData.nByte == 1 && p0->sData.zString[0] == '?' ){` |
|       5 |  7049 | `		return 1;` |
|       - |  7050 | `	}` |
|       - |  7051 | `	/* A name-like first token begins a type only when followed by another` |
|       - |  7052 | `	 * name (the constant name) or a union separator '\|'. Followed by '=',` |
|       - |  7053 | `	 * ';' or ',' it is the constant name itself (untyped). */` |
|      79 |  7054 | `	if( p0->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      79 |  7055 | `		p1 = (pGen->pIn + 1 < pGen->pEnd) ? (pGen->pIn + 1) : 0;` |
|      79 |  7056 | `		if( p1 ){` |
|      79 |  7057 | `			if( p1->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_NSSEP) ){` |
|      24 |  7058 | `				return 1;` |
|       - |  7059 | `			}` |
|      59 |  7060 | `			if( (p1->nType & PH7_TK_OP) && p1->sData.nByte == 1 && p1->sData.zString[0] == '\|' ){` |
|       5 |  7061 | `				return 1;` |
|       - |  7062 | `			}` |
|      25 |  7063 | `		}` |
|      25 |  7064 | `	}` |
|      55 |  7065 | `	return 0;` |
|      44 |  7066 |  |
|       - |  7067 | `/*` |
|       - |  7068 | ` * Copy a parsed declared type onto a freshly created class attribute (property,` |
|       - |  7069 | ` * promoted property or class constant). nType/pClass/pTypeName/iTypeFlags come` |
|       - |  7070 | ` * straight from GenStateParseUnionTypeDecl; for a union the alternatives are` |
|       - |  7071 | ` * shared from pAlts — their class-name SyStrings are VM-allocator owned and` |
|       - |  7072 | ` * outlive the temporary set, so multiple attrs in a multi-declaration chain may` |
|       - |  7073 | ` * share the same backing.` |
|       - |  7074 | ` */` |
|     202 |  7075 | `static void GenStateCopyTypeToAttr(ph7_class_attr *pAttr,sxu32 nType,` |
|       - |  7076 | `	const SyString *pClass,const SyString *pTypeName,sxi32 iTypeFlags,SySet *pAlts)` |
|       5 |  7077 |  |
|     207 |  7078 | `	pAttr->nType = nType;` |
|     207 |  7079 | `	pAttr->sClass = *pClass;` |
|     207 |  7080 | `	pAttr->sTypeName = *pTypeName;` |
|     207 |  7081 | `	if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7082 | `		sxu32 i;` |
|      66 |  7083 | `		for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      46 |  7084 | `			ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(pAlts, i);` |
|      46 |  7085 | `			SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      25 |  7086 | `		}` |
|      10 |  7087 | `	}` |
|     207 |  7088 |  |
|      78 |  7089 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7090 |  |
|      83 |  7091 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7092 | `	SySet *pInstrContainer;` |
|       - |  7093 | `	ph7_class_attr *pCons;` |
|       - |  7094 | `	SyString *pName;` |
|       - |  7095 | `	sxi32 rc;` |
|      83 |  7096 | `	sxu32 nType = 0;` |
|       - |  7097 | `	SyString sTypeClass;` |
|       - |  7098 | `	SyString sTypeText;` |
|       - |  7099 | `	SySet aUnionAlts;` |
|      83 |  7100 | `	sxi32 iTypeFlags = 0;` |
|      83 |  7101 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|      83 |  7102 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|      83 |  7103 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7104 | `	/* Extract visibility level */` |
|      83 |  7105 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7106 | `	/* Mark as constant */` |
|      83 |  7107 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      83 |  7108 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|       - |  7109 | `	/* Optional type hint (typed class constants, PHP 8.3). Parsed once and` |
|       - |  7110 | ``	 * applied to every name in a multi-declaration `const int A = 1, B = 2`. */`` |
|      97 |  7111 | `	if( GenStateClassConstHasType(pGen) ){` |
|      46 |  7112 | `		rc = GenStateParseUnionTypeDecl(pGen,&nType,&sTypeClass,&aUnionAlts,&iTypeFlags,&sTypeText,` |
|      28 |  7113 | `			PH7_CLASS_ATTR_NULLABLE,PH7_CLASS_ATTR_UNION,/* bAllowVoid */ 0,pGen->pIn->nLine);` |
|       - |  7114 | `		/* On abort the whole compilation tears down and the VM allocator (which` |
|       - |  7115 | `		 * backs aUnionAlts) is released, so abort paths below don't free it —` |
|       - |  7116 | `		 * matching the rest of this function; only the recoverable Synchronize` |
|       - |  7117 | `		 * and success paths release. */` |
|      32 |  7118 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7119 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7120 | `			goto Synchronize;` |
|      32 |  7121 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7122 | `			return SXERR_ABORT;` |
|      32 |  7123 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7124 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7125 | `				"Invalid type for class constant inside class '%z'",&pClass->sName);` |
|     ! 0 |  7126 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7127 | `				return SXERR_ABORT;` |
|       - |  7128 | `			}` |
|     ! 0 |  7129 | `			goto Synchronize;` |
|       - |  7130 | `		}` |
|      32 |  7131 | `		iTypeFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      14 |  7132 | `	}` |
|      39 |  7133 | `loop:` |
|      85 |  7134 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7135 | `		/* Invalid constant name */` |
|     ! 0 |  7136 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  7137 | `		if( rc == SXERR_ABORT ){` |
|       - |  7138 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7139 | `			return SXERR_ABORT;` |
|       - |  7140 | `		}` |
|     ! 0 |  7141 | `		goto Synchronize;` |
|       - |  7142 | `	}` |
|       - |  7143 | `	/* Peek constant name */` |
|      85 |  7144 | `	pName = &pGen->pIn->sData;` |
|       - |  7145 | `	/* Make sure the constant name isn't reserved */` |
|      85 |  7146 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  7147 | `		/* Reserved constant name */` |
|     ! 0 |  7148 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  7149 | `		if( rc == SXERR_ABORT ){` |
|       - |  7150 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7151 | `			return SXERR_ABORT;` |
|       - |  7152 | `		}` |
|     ! 0 |  7153 | `		goto Synchronize;` |
|       - |  7154 | `	}` |
|       - |  7155 | `	/* Reject pseudo-types PHP forbids on a typed constant (callable/void/never) */` |
|      85 |  7156 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      46 |  7157 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,&sTypeText,` |
|      28 |  7158 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      14 |  7159 | `			"Class constant %z::%z cannot have type %z",nLine);` |
|      32 |  7160 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7161 | `			return SXERR_ABORT;` |
|      32 |  7162 | `		}else if( rc != SXRET_OK ){` |
|       3 |  7163 | `			goto Synchronize;` |
|       - |  7164 | `		}` |
|      13 |  7165 | `	}` |
|       - |  7166 | `	/* Advance the stream cursor */` |
|      83 |  7167 | `	pGen->pIn++;` |
|      83 |  7168 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  7169 | `		/* Invalid declaration */` |
|     ! 0 |  7170 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  7171 | `		if( rc == SXERR_ABORT ){` |
|       - |  7172 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7173 | `			return SXERR_ABORT;` |
|       - |  7174 | `		}` |
|     ! 0 |  7175 | `		goto Synchronize;` |
|       - |  7176 | `	}` |
|      83 |  7177 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  7178 | `	/* Allocate a new class attribute */` |
|      83 |  7179 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|      83 |  7180 | `	if( pCons == 0 ){` |
|     ! 0 |  7181 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7182 | `		return SXERR_ABORT;` |
|       - |  7183 | `	}` |
|      83 |  7184 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|      29 |  7185 | `		GenStateCopyTypeToAttr(pCons,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      13 |  7186 | `	}` |
|       - |  7187 | `	/* Swap bytecode container */` |
|      83 |  7188 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      83 |  7189 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  7190 | `	/* Compile constant value.` |
|       - |  7191 | `	 */` |
|      83 |  7192 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      83 |  7193 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  7194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  7195 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7196 | `			return SXERR_ABORT;` |
|       - |  7197 | `		}` |
|       1 |  7198 | `	}` |
|       - |  7199 | `	/* Emit the done instruction */` |
|      83 |  7200 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      83 |  7201 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      83 |  7202 | `	if( rc == SXERR_ABORT ){` |
|       - |  7203 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  7204 | `		return SXERR_ABORT;` |
|       - |  7205 | `	}` |
|       - |  7206 | `	/* All done,install the constant */` |
|      83 |  7207 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      83 |  7208 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7209 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7210 | `		return SXERR_ABORT;` |
|       - |  7211 | `	}` |
|      83 |  7212 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7213 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|       3 |  7214 | `		pGen->pIn++; /* Jump the comma */` |
|       3 |  7215 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7216 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7217 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7218 | `				pTok--;` |
|     ! 0 |  7219 | `			}` |
|     ! 0 |  7220 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7221 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  7222 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7223 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7224 | `				return SXERR_ABORT;` |
|       - |  7225 | `			}` |
|     ! 0 |  7226 | `		}else{` |
|       3 |  7227 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|       3 |  7228 | `				goto loop;` |
|       - |  7229 | `			}` |
|       - |  7230 | `		}` |
|     ! 0 |  7231 | `	}` |
|      81 |  7232 | `	SySetRelease(&aUnionAlts);` |
|      81 |  7233 | `	return SXRET_OK;` |
|       1 |  7234 | `Synchronize:` |
|       3 |  7235 | `	SySetRelease(&aUnionAlts);` |
|       - |  7236 | `	/* Synchronize with the first semi-colon */` |
|       9 |  7237 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7238 | `		pGen->pIn++;` |
|       1 |  7239 | `	}` |
|       3 |  7240 | `	return SXERR_CORRUPT;` |
|      44 |  7241 |  |
|       - |  7242 | `/*` |
|       - |  7243 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  7244 | ` * According to the PHP language reference manual` |
|       - |  7245 | ` *  Properties` |
|       - |  7246 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  7247 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  7248 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  7249 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  7250 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  7251 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  7252 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  7253 | ` * Symisc eXtension.` |
|       - |  7254 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  7255 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  7256 | ` *  Example:` |
|       - |  7257 | ` *   class Test{` |
|       - |  7258 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  7259 | ` *   };` |
|       - |  7260 | ` *   var_dump(TEST::myVar);` |
|       - |  7261 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  7262 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  7263 | ` */` |
|       - |  7264 | `/*` |
|       - |  7265 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  7266 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  7267 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  7268 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  7269 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  7270 | ` */` |
|  178102 |  7271 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       5 |  7272 |  |
|  178107 |  7273 | `	SyToken *p = pStart;` |
|  178107 |  7274 | `	int bFirst = 1;` |
|  178107 |  7275 | `	if( p >= pEnd ) return 0;` |
|       - |  7276 | ``	/* Optional nullable `?` shorthand. */`` |
|  178107 |  7277 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      18 |  7278 | `		p++;` |
|      18 |  7279 | `		if( p >= pEnd ) return 0;` |
|       8 |  7280 | `	}` |
|       - |  7281 | ``	/* Skip a (possibly union / intersection / DNF) type to find the `$name`.`` |
|       - |  7282 | ``	 * One or more `\|`-separated parts; each part is either a parenthesized`` |
|       - |  7283 | `` 	 * intersection `( … )` or an atom optionally followed by a bare `&` `` |
|       - |  7284 | ``	 * intersection. We only need to land on the `$` to classify the member. */`` |
|   89051 |  7285 | `	for(;;){` |
|  178125 |  7286 | `		if( p < pEnd && (p->nType & PH7_TK_LPAREN) ){` |
|       - |  7287 | ``			/* Parenthesized DNF group — skip to the matching `)`. */`` |
|       3 |  7288 | `			p++;` |
|       9 |  7289 | `			while( p < pEnd && (p->nType & PH7_TK_RPAREN) == 0 ){ p++; }` |
|       3 |  7290 | `			if( p >= pEnd ) return 0;` |
|       3 |  7291 | `			p++; /* skip ')' */` |
|       2 |  7292 | `		}else{` |
|       - |  7293 | ``			/* A type atom: optional `\`, an identifier/keyword, namespace path,`` |
|       - |  7294 | ``			 * then any `&`-joined intersection members. */`` |
|  178123 |  7295 | `			if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|  178123 |  7296 | `			if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  7297 | `				return 0;` |
|       - |  7298 | `			}` |
|       - |  7299 | `			/* Reject class-body modifier keywords that aren't types (only on the` |
|       - |  7300 | `			 * first atom; visibility is already consumed, but static/final/abstract` |
|       - |  7301 | `			 * may still appear at the initial dispatch site). */` |
|  178123 |  7302 | `			if( bFirst && (p->nType & PH7_TK_KEYWORD) ){` |
|  178081 |  7303 | `				sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  178153 |  7304 | `				if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3760 |  7305 | `				 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  177927 |  7306 | `					return 0;` |
|       - |  7307 | `				}` |
|      77 |  7308 | `			}` |
|     201 |  7309 | `			p++;` |
|     203 |  7310 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7311 | `				p += 2;` |
|       1 |  7312 | `			}` |
|     297 |  7313 | `			while( p + 1 < pEnd && (p->nType & PH7_TK_AMPER)` |
|     204 |  7314 | `				&& (p[1].nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  7315 | `				p++; /* skip '&' */` |
|       3 |  7316 | `				if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|       3 |  7317 | `				if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|       3 |  7318 | `				p++;` |
|       3 |  7319 | `				while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  7320 | `					p += 2;` |
|     ! 0 |  7321 | `				}` |
|       1 |  7322 | `			}` |
|       - |  7323 | `		}` |
|     203 |  7324 | `		bFirst = 0;` |
|     198 |  7325 | `		if( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      23 |  7326 | `			&& p->sData.zString[0] == '\|' ){` |
|      22 |  7327 | ``			p++; /* next `\|`-separated part */`` |
|      22 |  7328 | `			continue;` |
|       - |  7329 | `		}` |
|     185 |  7330 | `		break;` |
|     ! 0 |  7331 | `	}` |
|     185 |  7332 | `	if( p >= pEnd ) return 0;` |
|     185 |  7333 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   89056 |  7334 |  |
|       - |  7335 |  |
|       - |  7336 | `/*` |
|       - |  7337 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  7338 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  7339 | ` * if not). Recognized forms:` |
|       - |  7340 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  7341 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  7342 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  7343 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  7344 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  7345 | ` * on unrecoverable error.` |
|       - |  7346 | ` *` |
|       - |  7347 | ` * When a type is parsed:` |
|       - |  7348 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  7349 | ` *   *pClass is set to the class name (for class types)` |
|       - |  7350 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  7351 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  7352 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  7353 | ` */` |
|     180 |  7354 | `static sxi32 GenStateParsePropertyType(` |
|       - |  7355 | `	ph7_gen_state *pGen,` |
|       - |  7356 | `	sxu32 *pnType,` |
|       - |  7357 | `	SyString *pClass,` |
|       - |  7358 | `	sxi32 *piTypeFlags,` |
|       - |  7359 | `	SyString *pTypeText,` |
|       - |  7360 | `	SySet *pAlts` |
|       5 |  7361 | `){` |
|     185 |  7362 | `	sxi32 iFlags = 0;` |
|       - |  7363 | `	sxi32 rc;` |
|     185 |  7364 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  7365 | `		return SXRET_OK;` |
|       - |  7366 | `	}` |
|       - |  7367 | `	/* If the first token is '$', there's no type */` |
|     185 |  7368 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  7369 | `		return SXRET_OK;` |
|       - |  7370 | `	}` |
|     185 |  7371 | `	rc = GenStateParseUnionTypeDecl(` |
|      90 |  7372 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  7373 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  7374 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  7375 | `		/* bAllowVoid */ 0,` |
|     180 |  7376 | `		pGen->pIn->nLine);` |
|     185 |  7377 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7378 | `		return rc;` |
|       - |  7379 | `	}` |
|       - |  7380 | `	/* Verify next token is '$' (start of property name) */` |
|     185 |  7381 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7382 | `		return SXERR_SYNTAX;` |
|       - |  7383 | `	}` |
|     185 |  7384 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     185 |  7385 | `	return SXRET_OK;` |
|      95 |  7386 |  |
|       - |  7387 |  |
|       - |  7388 | `/*` |
|       - |  7389 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  7390 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  7391 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  7392 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  7393 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  7394 | ` * by the type parser itself before reaching here.` |
|       - |  7395 | ` *` |
|       - |  7396 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  7397 | ` * use in the error message.` |
|       - |  7398 | ` */` |
|     322 |  7399 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  7400 | `	sxu32 nType,` |
|       - |  7401 | `	const SyString *pClass,` |
|       - |  7402 | `	const char **pzName,` |
|       - |  7403 | `	sxu32 *pnName)` |
|       5 |  7404 |  |
|       - |  7405 | `	const char *z;` |
|       - |  7406 | `	sxu32 n;` |
|     327 |  7407 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     277 |  7408 | `		return 0;` |
|       - |  7409 | `	}` |
|      55 |  7410 | `	z = pClass->zString;` |
|      55 |  7411 | `	n = pClass->nByte;` |
|      55 |  7412 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       8 |  7413 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  7414 | `	}` |
|       - |  7415 | ``	/* `mixed` (any value) and `iterable` (= array\|Traversable) are valid PHP`` |
|       - |  7416 | `	 * property types, enforced by value in VmEnforcePropertyTypeOnStore via` |
|       - |  7417 | ``	 * VmCheckPseudoType. Only `callable` stays disallowed (as in PHP). */`` |
|      48 |  7418 | `	return 0;` |
|     166 |  7419 |  |
|       - |  7420 |  |
|       - |  7421 | `/*` |
|       - |  7422 | ` * Validate a parsed class-member type (property, promoted parameter or class` |
|       - |  7423 | ` * constant) — the main atom plus any union alternatives — against the` |
|       - |  7424 | ` * disallowed-pseudo-types list. On rejection emits zErrFmt, a PH7 format string` |
|       - |  7425 | ` * taking three %z arguments (class name, member name, full canonical type text),` |
|       - |  7426 | ` * so each caller supplies its own PHP-exact wording ("Property C::$x cannot have` |
|       - |  7427 | ` * type T" vs "Class constant C::X cannot have type T").` |
|       - |  7428 | ` *` |
|       - |  7429 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  7430 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  7431 | ` */` |
|     264 |  7432 | `static sxi32 GenStateValidateMemberType(` |
|       - |  7433 | `	ph7_gen_state *pGen,` |
|       - |  7434 | `	ph7_class *pClass,` |
|       - |  7435 | `	const SyString *pMemberName,` |
|       - |  7436 | `	sxu32 nType,` |
|       - |  7437 | `	const SyString *pTypeClass,` |
|       - |  7438 | `	const SyString *pTypeText,` |
|       - |  7439 | `	SySet *pUnionAlts,` |
|       - |  7440 | `	const char *zErrFmt,` |
|       - |  7441 | `	sxu32 nLine)` |
|       5 |  7442 |  |
|     269 |  7443 | `	const char *zBad = 0;` |
|     269 |  7444 | `	sxu32 nBad = 0;` |
|       - |  7445 | `	SyString sFallback;` |
|       - |  7446 | `	const SyString *pBad;` |
|       - |  7447 | `	sxi32 rc;` |
|     269 |  7448 | `	int bDisallowed = 0;` |
|     269 |  7449 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       5 |  7450 | `		bDisallowed = 1;` |
|     267 |  7451 | `	}else if( pUnionAlts ){` |
|       - |  7452 | `		sxu32 i;` |
|      88 |  7453 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      62 |  7454 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      62 |  7455 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  7456 | `				bDisallowed = 1;` |
|       3 |  7457 | `				break;` |
|       - |  7458 | `			}` |
|      32 |  7459 | `		}` |
|      14 |  7460 | `	}` |
|     269 |  7461 | `	if( !bDisallowed ){` |
|     263 |  7462 | `		return SXRET_OK;` |
|       - |  7463 | `	}` |
|       - |  7464 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  7465 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  7466 | `	 * canonical spelling if the type text is unavailable. */` |
|       8 |  7467 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       8 |  7468 | `		pBad = pTypeText;` |
|       5 |  7469 | `	}else{` |
|     ! 0 |  7470 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  7471 | `		pBad = &sFallback;` |
|       - |  7472 | `	}` |
|      11 |  7473 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       3 |  7474 | `		zErrFmt,` |
|       3 |  7475 | `		&pClass->sName,pMemberName,pBad);` |
|       8 |  7476 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7477 | `		return SXERR_ABORT;` |
|       - |  7478 | `	}` |
|       8 |  7479 | `	return SXERR_SYNTAX;` |
|     137 |  7480 |  |
|       - |  7481 | `/*` |
|       - |  7482 | `` * Return TRUE if pTok is the context-sensitive `readonly` modifier. PHP does not`` |
|       - |  7483 | `` * reserve `readonly` (it remains valid as a method/function name), so it is`` |
|       - |  7484 | ` * matched as a plain identifier in the class-member modifier position rather` |
|       - |  7485 | ` * than promoted to a lexer keyword.` |
|       - |  7486 | ` */` |
| 1603234 |  7487 | `static int GenStateIsReadonly(SyToken *pTok)` |
|       5 |  7488 |  |
| 1636730 |  7489 | `	return (pTok->nType & PH7_TK_ID)` |
|  835108 |  7490 | `		&& pTok->sData.nByte == sizeof("readonly")-1` |
| 1636725 |  7491 | `		&& SyStrnicmp(pTok->sData.zString,"readonly",sizeof("readonly")-1) == 0;` |
|       5 |  7492 |  |
|   71446 |  7493 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       5 |  7494 |  |
|   71451 |  7495 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7496 | `	ph7_class_attr *pAttr;` |
|       - |  7497 | `	SyString *pName;` |
|       - |  7498 | `	sxi32 rc;` |
|   71451 |  7499 | `	sxu32 nType = 0;` |
|       - |  7500 | `	SyString sTypeClass;` |
|       - |  7501 | `	SyString sTypeText;` |
|       - |  7502 | `	SySet aUnionAlts;` |
|   71451 |  7503 | `	sxi32 iTypeFlags = 0;` |
|   71451 |  7504 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   71451 |  7505 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   71451 |  7506 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7507 | `	/* In a readonly class (PHP 8.2) every declared instance property is readonly;` |
|       - |  7508 | `	 * the per-property readonly rules below then apply uniformly (a static or` |
|       - |  7509 | `	 * untyped property, or one with a default, raises the same PHP-exact fatal). */` |
|   71451 |  7510 | `	if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      21 |  7511 | `		iFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7512 | `	}` |
|       - |  7513 | `	/* Extract visibility level */` |
|   71451 |  7514 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7515 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   71541 |  7516 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     185 |  7517 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     185 |  7518 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7519 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7520 | `			goto Synchronize;` |
|     185 |  7521 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7522 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7523 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7524 | `				&pGen->pIn->sData);` |
|     ! 0 |  7525 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7526 | `				return SXERR_ABORT;` |
|       - |  7527 | `			}` |
|     ! 0 |  7528 | `			goto Synchronize;` |
|     185 |  7529 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7530 | `			return SXERR_ABORT;` |
|       - |  7531 | `		}` |
|      90 |  7532 | `	}` |
|     ! 0 |  7533 | `loop:` |
|   71455 |  7534 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7535 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7536 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7537 | `			return SXERR_ABORT;` |
|       - |  7538 | `		}` |
|     ! 0 |  7539 | `		goto Synchronize;` |
|       - |  7540 | `	}` |
|   71455 |  7541 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   71455 |  7542 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7543 | `		/* Invalid attribute name */` |
|     ! 0 |  7544 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7545 | `		if( rc == SXERR_ABORT ){` |
|       - |  7546 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7547 | `			return SXERR_ABORT;` |
|       - |  7548 | `		}` |
|     ! 0 |  7549 | `		goto Synchronize;` |
|       - |  7550 | `	}` |
|       - |  7551 | `	/* Peek attribute name */` |
|   71455 |  7552 | `	pName = &pGen->pIn->sData;` |
|       - |  7553 | `	/* Advance the stream cursor */` |
|   71455 |  7554 | `	pGen->pIn++;` |
|   71455 |  7555 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7556 | `		/* Invalid declaration */` |
|       3 |  7557 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7558 | `		if( rc == SXERR_ABORT ){` |
|       - |  7559 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7560 | `			return SXERR_ABORT;` |
|       - |  7561 | `		}` |
|       3 |  7562 | `		goto Synchronize;` |
|       - |  7563 | `	}` |
|       - |  7564 | `	/* readonly property rules (PHP 8.1): cannot be static, must be typed, and` |
|       - |  7565 | `	 * cannot carry a default value. PHP-exact diagnostics. */` |
|   71453 |  7566 | `	if( iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      39 |  7567 | `		const char *zRoErr = 0;` |
|      39 |  7568 | `		if( iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       3 |  7569 | `			zRoErr = "Static property %z::$%z cannot be readonly";` |
|      38 |  7570 | `		}else if( (iTypeFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       6 |  7571 | `			zRoErr = "Readonly property %z::$%z must have type";` |
|      35 |  7572 | `		}else if( pGen->pIn->nType & PH7_TK_EQUAL ){` |
|       6 |  7573 | `			zRoErr = "Readonly property %z::$%z cannot have default value";` |
|       2 |  7574 | `		}` |
|      39 |  7575 | `		if( zRoErr ){` |
|      13 |  7576 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,zRoErr,&pClass->sName,pName);` |
|      13 |  7577 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7578 | `				return SXERR_ABORT;` |
|       - |  7579 | `			}` |
|      13 |  7580 | `			goto Synchronize;` |
|       - |  7581 | `		}` |
|      12 |  7582 | `	}` |
|       - |  7583 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7584 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7585 | `	 * by the type parser. */` |
|   71443 |  7586 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     272 |  7587 | `		rc = GenStateValidateMemberType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7588 | `			&sTypeText,` |
|     178 |  7589 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,` |
|      89 |  7590 | `			"Property %z::$%z cannot have type %z",nLine);` |
|     183 |  7591 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7592 | `			return SXERR_ABORT;` |
|     183 |  7593 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7594 | `			goto Synchronize;` |
|       - |  7595 | `		}` |
|      89 |  7596 | `	}` |
|       - |  7597 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   71443 |  7598 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7599 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7600 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7601 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7602 | `			return SXERR_ABORT;` |
|       - |  7603 | `		}` |
|       3 |  7604 | `		goto Synchronize;` |
|       - |  7605 | `	}` |
|       - |  7606 | `	/* Allocate a new class attribute */` |
|   71441 |  7607 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   71441 |  7608 | `	if( pAttr == 0 ){` |
|     ! 0 |  7609 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7610 | `		return SXERR_ABORT;` |
|       - |  7611 | `	}` |
|   71441 |  7612 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     181 |  7613 | `		GenStateCopyTypeToAttr(pAttr,nType,&sTypeClass,&sTypeText,iTypeFlags,&aUnionAlts);` |
|      88 |  7614 | `	}` |
|   71441 |  7615 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7616 | `		SySet *pInstrContainer;` |
|   21707 |  7617 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7618 | `		/* Swap bytecode container */` |
|   21707 |  7619 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   21707 |  7620 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7621 | `		/* Compile attribute value.` |
|       - |  7622 | `		 */` |
|   21707 |  7623 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   21707 |  7624 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7625 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7626 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7627 | `				return SXERR_ABORT;` |
|       - |  7628 | `			}` |
|     ! 0 |  7629 | `		}` |
|       - |  7630 | `		/* Emit the done instruction */` |
|   21707 |  7631 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   21707 |  7632 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10851 |  7633 | `	}` |
|       - |  7634 | `	/* All done,install the attribute */` |
|   71441 |  7635 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   71441 |  7636 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7637 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7638 | `		return SXERR_ABORT;` |
|       - |  7639 | `	}` |
|   71441 |  7640 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7641 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7642 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7643 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7644 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7645 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7646 | `				pTok--;` |
|     ! 0 |  7647 | `			}` |
|     ! 0 |  7648 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7649 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7650 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7651 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7652 | `				return SXERR_ABORT;` |
|       - |  7653 | `			}` |
|     ! 0 |  7654 | `		}else{` |
|       5 |  7655 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7656 | `				goto loop;` |
|       - |  7657 | `			}` |
|       - |  7658 | `		}` |
|     ! 0 |  7659 | `	}` |
|   71437 |  7660 | `	SySetRelease(&aUnionAlts);` |
|   71437 |  7661 | `	return SXRET_OK;` |
|       7 |  7662 | `Synchronize:` |
|       - |  7663 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7664 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      16 |  7665 | `		pGen->pIn++;` |
|       2 |  7666 | `	}` |
|      17 |  7667 | `	SySetRelease(&aUnionAlts);` |
|      17 |  7668 | `	return SXERR_CORRUPT;` |
|   35728 |  7669 |  |
|       - |  7670 | `/*` |
|       - |  7671 | ` * Compile a class method.` |
|       - |  7672 | ` *` |
|       - |  7673 | ` * Refer to the official documentation for more information` |
|       - |  7674 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7675 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7676 | ` * overloading and many more.` |
|       - |  7677 | ` */` |
|  270126 |  7678 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7679 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7680 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7681 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7682 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7683 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7684 | `	)` |
|       5 |  7685 |  |
|  270131 |  7686 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7687 | `	ph7_class_method *pMeth;` |
|       - |  7688 | `	sxi32 iFuncFlags;` |
|       - |  7689 | `	SyString *pName;` |
|       - |  7690 | `	SyToken *pEnd;` |
|       - |  7691 | `	sxi32 rc;` |
|       - |  7692 | `	/* Extract visibility level */` |
|  270131 |  7693 | `	iProtection = GetProtectionLevel(iProtection);` |
|  270131 |  7694 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  270131 |  7695 | `	iFuncFlags = 0;` |
|  270131 |  7696 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7697 | `		/* Invalid method name */` |
|     ! 0 |  7698 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7699 | `		if( rc == SXERR_ABORT ){` |
|       - |  7700 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7701 | `			return SXERR_ABORT;` |
|       - |  7702 | `		}` |
|     ! 0 |  7703 | `		goto Synchronize;` |
|       - |  7704 | `	}` |
|  270131 |  7705 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7706 | `		/* Return by reference,remember that */` |
|     ! 0 |  7707 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7708 | `		/* Jump the '&' token */` |
|     ! 0 |  7709 | `		pGen->pIn++;` |
|     ! 0 |  7710 | `	}` |
|  270131 |  7711 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7712 | `		/* Invalid method name */` |
|     ! 0 |  7713 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7714 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7715 | `			return SXERR_ABORT;` |
|       - |  7716 | `		}` |
|     ! 0 |  7717 | `		goto Synchronize;` |
|       - |  7718 | `	}` |
|       - |  7719 | `	/* Peek method name */` |
|  270131 |  7720 | `	pName = &pGen->pIn->sData;` |
|  270131 |  7721 | `	nLine = pGen->pIn->nLine;` |
|       - |  7722 | `	/* Jump the method name */` |
|  270131 |  7723 | `	pGen->pIn++;` |
|  270131 |  7724 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7725 | `		/* Abstract method */` |
|   92099 |  7726 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7727 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7728 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7729 | `				&pClass->sName,pName);` |
|     ! 0 |  7730 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7731 | `				return SXERR_ABORT;` |
|       - |  7732 | `			}` |
|     ! 0 |  7733 | `		}` |
|       - |  7734 | `		/* Assemble method signature only */` |
|   92099 |  7735 | `		doBody = FALSE;` |
|   46047 |  7736 | `	}` |
|  270131 |  7737 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7738 | `		/* Syntax error */` |
|     ! 0 |  7739 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7740 | `		if( rc == SXERR_ABORT ){` |
|       - |  7741 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7742 | `			return SXERR_ABORT;` |
|       - |  7743 | `		}` |
|     ! 0 |  7744 | `		goto Synchronize;` |
|       - |  7745 | `	}` |
|       - |  7746 | `	/* Allocate a new class_method instance */` |
|  270131 |  7747 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  270131 |  7748 | `	if( pMeth == 0 ){` |
|     ! 0 |  7749 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7750 | `		return SXERR_ABORT;` |
|       - |  7751 | `	}` |
|       - |  7752 | `	/* Jump the left parenthesis '(' */` |
|  270131 |  7753 | `	pGen->pIn++;` |
|  270131 |  7754 | `	pEnd = 0; /* cc warning */` |
|       - |  7755 | `	/* Delimit the method signature */` |
|  270131 |  7756 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  270131 |  7757 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7758 | `		/* Syntax error */` |
|       3 |  7759 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7760 | `		if( rc == SXERR_ABORT ){` |
|       - |  7761 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7762 | `			return SXERR_ABORT;` |
|       - |  7763 | `		}` |
|       3 |  7764 | `		goto Synchronize;` |
|       - |  7765 | `	}` |
|       - |  7766 | `	{` |
|  270129 |  7767 | `		int bIsCtor = 0;` |
|  270129 |  7768 | `		int bAbstractCtor = 0;` |
|  392716 |  7769 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  161735 |  7770 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  257659 |  7771 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   24945 |  7772 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7773 | `				bAbstractCtor = 1;` |
|       2 |  7774 | `			}else{` |
|   24943 |  7775 | `				bIsCtor = 1;` |
|       - |  7776 | `			}` |
|   12470 |  7777 | `		}` |
|  270129 |  7778 | `		if( pGen->pIn < pEnd ){` |
|       - |  7779 | `			/* Collect method arguments */` |
|   60617 |  7780 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   60617 |  7781 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7782 | `				return SXERR_ABORT;` |
|       - |  7783 | `			}` |
|   30306 |  7784 | `		}` |
|       - |  7785 | `	}` |
|       - |  7786 | `	/* Point past ')' and parse optional return type ': type' */` |
|  270129 |  7787 | `	pGen->pIn = &pEnd[1];` |
|       - |  7788 | `	{` |
|  270129 |  7789 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  270129 |  7790 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7791 | `			return SXERR_ABORT;` |
|  270129 |  7792 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7793 | `			goto Synchronize;` |
|       - |  7794 | `		}` |
|       - |  7795 | `	}` |
|       - |  7796 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7797 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7798 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7799 | `	{` |
|  270129 |  7800 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7801 | `		sxu32 i;` |
|  366241 |  7802 | `		for( i = 0; i < nArg; i++ ){` |
|   96127 |  7803 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7804 | `			ph7_class_attr *pAttr;` |
|   96127 |  7805 | `			sxi32 iAttrFlags = 0;` |
|       - |  7806 | `			int bArgTyped;` |
|   96127 |  7807 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   96063 |  7808 | `				continue;` |
|       - |  7809 | `			}` |
|       - |  7810 | `			/* "typed" = a single type or class name, OR a union/intersection,` |
|       - |  7811 | `			 * which leaves nType=0 / empty sClass and stores its alts in` |
|       - |  7812 | `			 * aUnionAlts. Used both to validate the type and to mark the attr. */` |
|      49 |  7813 | `			bArgTyped = pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|      70 |  7814 | `			         \|\| (pArg->iFlags & VM_FUNC_ARG_UNION);` |
|      69 |  7815 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7816 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7817 | `					"Cannot declare variadic promoted property");` |
|       3 |  7818 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7819 | `					return SXERR_ABORT;` |
|       - |  7820 | `				}` |
|       3 |  7821 | `				goto Synchronize;` |
|       - |  7822 | `			}` |
|       - |  7823 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7824 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7825 | `			 * appear as an alternative of a union type. */` |
|      67 |  7826 | `			if( bArgTyped ){` |
|      92 |  7827 | `				rc = GenStateValidateMemberType(pGen,pClass,&pArg->sName,` |
|      58 |  7828 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      58 |  7829 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      29 |  7830 | `					"Property %z::$%z cannot have type %z",nLine);` |
|      63 |  7831 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7832 | `					return SXERR_ABORT;` |
|      63 |  7833 | `				}else if( rc != SXRET_OK ){` |
|       6 |  7834 | `					goto Synchronize;` |
|       - |  7835 | `				}` |
|      27 |  7836 | `			}` |
|       - |  7837 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      63 |  7838 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7839 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7840 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7841 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7842 | `					return SXERR_ABORT;` |
|       - |  7843 | `				}` |
|       3 |  7844 | `				goto Synchronize;` |
|       - |  7845 | `			}` |
|      61 |  7846 | `			if( bArgTyped ){` |
|      57 |  7847 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      26 |  7848 | `			}` |
|      61 |  7849 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7850 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7851 | `			}` |
|      61 |  7852 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       8 |  7853 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       3 |  7854 | `			}` |
|      61 |  7855 | `			if( (pArg->iFlags & VM_FUNC_ARG_READONLY) \|\| (pClass->iFlags & PH7_CLASS_READONLY) ){` |
|       - |  7856 | `				/* A readonly promoted property must be typed (PHP 8.1); in a` |
|       - |  7857 | `				 * readonly class (8.2) every promoted property is readonly too. */` |
|      24 |  7858 | `				if( (iAttrFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|       4 |  7859 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7860 | `						"Readonly property %z::$%z must have type",&pClass->sName,&pArg->sName);` |
|       3 |  7861 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7862 | `						return SXERR_ABORT;` |
|       - |  7863 | `					}` |
|       3 |  7864 | `					goto Synchronize;` |
|       - |  7865 | `				}` |
|      22 |  7866 | `				iAttrFlags \|= PH7_CLASS_ATTR_READONLY;` |
|       9 |  7867 | `			}` |
|      59 |  7868 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      59 |  7869 | `			if( pAttr == 0 ){` |
|     ! 0 |  7870 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7871 | `				return SXERR_ABORT;` |
|       - |  7872 | `			}` |
|      59 |  7873 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      57 |  7874 | `				pAttr->nType = pArg->nType;` |
|      57 |  7875 | `				pAttr->sClass = pArg->sClass;` |
|      57 |  7876 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      57 |  7877 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7878 | `					sxu32 k;` |
|      20 |  7879 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|      14 |  7880 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|      14 |  7881 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|       8 |  7882 | `					}` |
|       3 |  7883 | `				}` |
|      26 |  7884 | `			}` |
|      59 |  7885 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      59 |  7886 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7887 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7888 | `				return SXERR_ABORT;` |
|       - |  7889 | `			}` |
|      32 |  7890 | `		}` |
|       - |  7891 | `	}` |
|  270119 |  7892 | `	if( doBody ){` |
|       - |  7893 | `		/* Compile method body */` |
|  178025 |  7894 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  178025 |  7895 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7896 | `			return SXERR_ABORT;` |
|       - |  7897 | `		}` |
|   89015 |  7898 | `	}else{` |
|       - |  7899 | `		/* Only method signature is allowed */` |
|   92099 |  7900 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7901 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7902 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7903 | `				if( rc == SXERR_ABORT ){` |
|       - |  7904 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7905 | `					return SXERR_ABORT;` |
|       - |  7906 | `				}` |
|     ! 0 |  7907 | `				return SXERR_CORRUPT;` |
|       - |  7908 | `			}` |
|       - |  7909 | `	}` |
|       - |  7910 | `	/* All done,install the method */` |
|  270119 |  7911 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  270119 |  7912 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7913 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7914 | `		return SXERR_ABORT;` |
|       - |  7915 | `	}` |
|  270119 |  7916 | `	return SXRET_OK;` |
|       6 |  7917 | `Synchronize:` |
|       - |  7918 | `	/* Synchronize with the first semi-colon */` |
|      40 |  7919 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      28 |  7920 | `		pGen->pIn++;` |
|       4 |  7921 | `	}` |
|      16 |  7922 | `	return SXERR_CORRUPT;` |
|  135068 |  7923 |  |
|       - |  7924 | `/*` |
|       - |  7925 | ` * Compile an object interface.` |
|       - |  7926 | ` *  According to the PHP language reference manual` |
|       - |  7927 | ` *   Object Interfaces:` |
|       - |  7928 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7929 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7930 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7931 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7932 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7933 | ` */` |
|   39012 |  7934 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       5 |  7935 |  |
|   39017 |  7936 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7937 | `	ph7_class *pClass,*pBase;` |
|       - |  7938 | `	SyToken *pEnd,*pTmp;` |
|       - |  7939 | `	SyString *pName;` |
|       - |  7940 | `	sxi32 nKwrd;` |
|       - |  7941 | `	sxi32 rc;` |
|       - |  7942 | `	/* Jump the 'interface' keyword */` |
|   39017 |  7943 | `	pGen->pIn++;` |
|       - |  7944 | `	/* Extract interface name */` |
|   39017 |  7945 | `	pName = &pGen->pIn->sData;` |
|       - |  7946 | `	/* Advance the stream cursor */` |
|   39017 |  7947 | `	pGen->pIn++;` |
|       - |  7948 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7949 | `		SyBlob sFQN;` |
|       - |  7950 | `		SyString sFQNStr;` |
|   39017 |  7951 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39017 |  7952 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39017 |  7953 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39017 |  7954 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39017 |  7955 | `		SyBlobRelease(&sFQN);` |
|       - |  7956 | `	}` |
|   39017 |  7957 | `	if( pClass == 0 ){` |
|     ! 0 |  7958 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7959 | `		return SXERR_ABORT;` |
|       - |  7960 | `	}` |
|       - |  7961 | `	/* Mark as an interface */` |
|   39017 |  7962 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7963 | `	/* Assume no base class is given */` |
|   39017 |  7964 | `	pBase = 0;` |
|   39017 |  7965 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   10633 |  7966 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   10633 |  7967 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7968 | `			SyBlob sResolved;` |
|       - |  7969 | `			SyString sBaseName;` |
|       - |  7970 | `			sxu32 nRefLine;` |
|       - |  7971 | `			/* Extract base interface */` |
|   10633 |  7972 | `			pGen->pIn++;` |
|   10633 |  7973 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10633 |  7974 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10633 |  7975 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7976 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7977 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7978 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7979 | `					pName);` |
|     ! 0 |  7980 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7981 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7982 | `					return SXERR_ABORT;` |
|       - |  7983 | `				}` |
|     ! 0 |  7984 | `				return SXRET_OK;` |
|       - |  7985 | `			}` |
|   15947 |  7986 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   10628 |  7987 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10633 |  7988 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7989 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7990 | `			/* Only interfaces is allowed */` |
|   10633 |  7991 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7992 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7993 | `			}` |
|   10633 |  7994 | `			if( pBase == 0 ){` |
|     ! 0 |  7995 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7996 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7997 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7998 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7999 | `					return SXERR_ABORT;` |
|       - |  8000 | `				}` |
|     ! 0 |  8001 | `			}` |
|   10633 |  8002 | `			SyBlobRelease(&sResolved);` |
|    5314 |  8003 | `		}` |
|    5314 |  8004 | `	}` |
|   39017 |  8005 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8006 | `		/* Syntax error */` |
|     ! 0 |  8007 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  8008 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8009 | `		if( rc == SXERR_ABORT ){` |
|       - |  8010 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8011 | `			return SXERR_ABORT;` |
|       - |  8012 | `		}` |
|     ! 0 |  8013 | `		return SXRET_OK;` |
|       - |  8014 | `	}` |
|   39017 |  8015 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39017 |  8016 | `	pEnd = 0; /* cc warning */` |
|       - |  8017 | `	/* Delimit the interface body */` |
|   39017 |  8018 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39017 |  8019 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8020 | `		/* Syntax error */` |
|     ! 0 |  8021 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  8022 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8023 | `		if( rc == SXERR_ABORT ){` |
|       - |  8024 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8025 | `			return SXERR_ABORT;` |
|       - |  8026 | `		}` |
|     ! 0 |  8027 | `		return SXRET_OK;` |
|       - |  8028 | `	}` |
|       - |  8029 | `	/* Swap token stream */` |
|   39017 |  8030 | `	pTmp = pGen->pEnd;` |
|   39017 |  8031 | `	pGen->pEnd = pEnd;` |
|       - |  8032 | `	/* Start the parse process` |
|       - |  8033 | `	 * Note (According to the PHP reference manual):` |
|       - |  8034 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  8035 | `	 *  Only 'public' visibility is allowed.` |
|       - |  8036 | `	 */` |
|   65549 |  8037 | `	for(;;){` |
|       - |  8038 | `		/* Jump leading/trailing semi-colons */` |
|  223189 |  8039 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   92091 |  8040 | `			pGen->pIn++;` |
|       5 |  8041 | `		}` |
|  131103 |  8042 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8043 | `			/* End of interface body */` |
|   39015 |  8044 | `			break;` |
|       - |  8045 | `		}` |
|   92093 |  8046 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8047 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8048 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  8049 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8050 | `			if( rc == SXERR_ABORT ){` |
|       - |  8051 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8052 | `				return SXERR_ABORT;` |
|       - |  8053 | `			}` |
|     ! 0 |  8054 | `			goto done;` |
|       - |  8055 | `		}` |
|       - |  8056 | `		/* Extract the current keyword */` |
|   92093 |  8057 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92093 |  8058 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  8059 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  8060 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  8061 | `			const char *zKind = "member";` |
|       3 |  8062 | `			SyString *pMemberName = 0;` |
|       3 |  8063 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  8064 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  8065 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  8066 | `					zKind = "constant";` |
|       3 |  8067 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  8068 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  8069 | `					}` |
|       1 |  8070 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8071 | `					zKind = "method";` |
|     ! 0 |  8072 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  8073 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  8074 | `					}` |
|     ! 0 |  8075 | `				}` |
|       1 |  8076 | `			}` |
|       3 |  8077 | `			if( pMemberName ){` |
|       4 |  8078 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  8079 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  8080 | `			}else{` |
|     ! 0 |  8081 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8082 | `					"Access type for interface %s must be public",zKind);` |
|       - |  8083 | `			}` |
|       3 |  8084 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8085 | `				return SXERR_ABORT;` |
|       - |  8086 | `			}` |
|       3 |  8087 | `			goto done;` |
|       - |  8088 | `		}` |
|   92091 |  8089 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8090 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8091 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8092 | `			if( rc == SXERR_ABORT ){` |
|       - |  8093 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8094 | `				return SXERR_ABORT;` |
|       - |  8095 | `			}` |
|     ! 0 |  8096 | `			goto done;` |
|       - |  8097 | `		}` |
|   92091 |  8098 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  8099 | `			/* Advance the stream cursor */` |
|   92081 |  8100 | `			pGen->pIn++;` |
|   92081 |  8101 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8102 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8103 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8104 | `				if( rc == SXERR_ABORT ){` |
|       - |  8105 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8106 | `					return SXERR_ABORT;` |
|       - |  8107 | `				}` |
|     ! 0 |  8108 | `				goto done;` |
|       - |  8109 | `			}` |
|   92081 |  8110 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   92081 |  8111 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  8112 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8113 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  8114 | `				if( rc == SXERR_ABORT ){` |
|       - |  8115 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  8116 | `					return SXERR_ABORT;` |
|       - |  8117 | `				}` |
|     ! 0 |  8118 | `				goto done;` |
|       - |  8119 | `			}` |
|   46038 |  8120 | `		}` |
|   92091 |  8121 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8122 | `			/* Parse constant */` |
|       7 |  8123 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       7 |  8124 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8125 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8126 | `					return SXERR_ABORT;` |
|       - |  8127 | `				}` |
|     ! 0 |  8128 | `				goto done;` |
|       - |  8129 | `			}` |
|       4 |  8130 | `		}else{` |
|   92085 |  8131 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   92085 |  8132 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8133 | `				/* Static method,record that */` |
|   10625 |  8134 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  8135 | `				/* Advance the stream cursor */` |
|   10625 |  8136 | `				pGen->pIn++;` |
|   10620 |  8137 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10625 |  8138 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8139 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8140 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  8141 | `						if( rc == SXERR_ABORT ){` |
|       - |  8142 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8143 | `							return SXERR_ABORT;` |
|       - |  8144 | `						}` |
|     ! 0 |  8145 | `						goto done;` |
|       - |  8146 | `				}` |
|    5310 |  8147 | `			}` |
|       - |  8148 | `			/* Process method signature (no body for interface methods) */` |
|   92085 |  8149 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   92085 |  8150 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8151 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8152 | `					return SXERR_ABORT;` |
|       - |  8153 | `				}` |
|     ! 0 |  8154 | `				goto done;` |
|       - |  8155 | `			}` |
|       - |  8156 | `		}` |
|       5 |  8157 | `	}` |
|       - |  8158 | `	/* Install the interface */` |
|   39015 |  8159 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   39015 |  8160 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  8161 | `		/* Inherit from the base interface */` |
|   10633 |  8162 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|    5314 |  8163 | `	}` |
|   39015 |  8164 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8165 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8166 | `		return SXERR_ABORT;` |
|       - |  8167 | `	}` |
|   19505 |  8168 | `done:` |
|       - |  8169 | `	/* Point beyond the interface body */` |
|   39017 |  8170 | `	pGen->pIn  = &pEnd[1];` |
|   39017 |  8171 | `	pGen->pEnd = pTmp;` |
|   39017 |  8172 | `	return PH7_OK;` |
|   19511 |  8173 |  |
|       - |  8174 | `/*` |
|       - |  8175 | ` * Compile a user-defined class.` |
|       - |  8176 | ` * According to the PHP language reference manual` |
|       - |  8177 | ` *  class` |
|       - |  8178 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  8179 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  8180 | ` *  of the properties and methods belonging to the class.` |
|       - |  8181 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  8182 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  8183 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  8184 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  8185 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  8186 | ` *  (called "methods").` |
|       - |  8187 | ` */` |
|       - |  8188 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  8189 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  8190 | `struct TraitUseEntry {` |
|       - |  8191 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  8192 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  8193 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  8194 | `};` |
|       - |  8195 | `/*` |
|       - |  8196 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  8197 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  8198 | ` */` |
|  100278 |  8199 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8200 |  |
|       - |  8201 | `	ph7_class **apIface;` |
|       - |  8202 | `	sxu32 nIface,i;` |
|       - |  8203 | `	sxi32 rc;` |
|  100283 |  8204 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  8205 | `		return SXRET_OK;` |
|       - |  8206 | `	}` |
|  100283 |  8207 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  100283 |  8208 | `	nIface = SySetUsed(&pClass->aInterface);` |
|  196105 |  8209 | `	for(i = 0; i < nIface; i++){` |
|   95827 |  8210 | `		ph7_class *pIface = apIface[i];` |
|       - |  8211 | `		SyHashEntry *pEntry;` |
|   95827 |  8212 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|  255577 |  8213 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|  159755 |  8214 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  8215 | `			ph7_class_method *pImplMeth;` |
|  159755 |  8216 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  8217 | `			/* Find the implementing method in the class */` |
|  159755 |  8218 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|  159755 |  8219 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      18 |  8220 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  8221 | `			}` |
|       - |  8222 | `			/* Check visibility: interface methods must be implemented as public */` |
|  159741 |  8223 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  8224 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8225 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  8226 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  8227 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8228 | `					return SXERR_ABORT;` |
|       - |  8229 | `				}` |
|       1 |  8230 | `			}` |
|       - |  8231 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  8232 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  8233 | `			 */` |
|       - |  8234 | `			{` |
|  159741 |  8235 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|  159741 |  8236 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|  159741 |  8237 | `				int sigError = 0;` |
|  159741 |  8238 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  8239 | `					sigError = 1;` |
|  159740 |  8240 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  8241 | `					/* Extra parameters must all have default values */` |
|       6 |  8242 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  8243 | `					sxu32 k;` |
|       8 |  8244 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       6 |  8245 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  8246 | `							sigError = 1;` |
|       3 |  8247 | `							break;` |
|       - |  8248 | `						}` |
|       2 |  8249 | `					}` |
|       2 |  8250 | `				}` |
|  159741 |  8251 | `				if( sigError ){` |
|       - |  8252 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  8253 | `					ph7_vm_func_arg *aArgs;` |
|       - |  8254 | `					sxu32 j;` |
|       6 |  8255 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       6 |  8256 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  8257 | `					/* Build implementing method signature */` |
|       6 |  8258 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      12 |  8259 | `					for(j = 0; j < nImplArgs; j++){` |
|       8 |  8260 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       8 |  8261 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       8 |  8262 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8263 | `					}` |
|       - |  8264 | `					/* Build interface method signature */` |
|       6 |  8265 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      12 |  8266 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       8 |  8267 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       8 |  8268 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       8 |  8269 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       5 |  8270 | `					}` |
|       8 |  8271 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  8272 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  8273 | `						&pClass->sName,pMName,` |
|       4 |  8274 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  8275 | `						&pIface->sName,pMName,` |
|       4 |  8276 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       6 |  8277 | `					SyBlobRelease(&sImplSig);` |
|       6 |  8278 | `					SyBlobRelease(&sIfaceSig);` |
|       6 |  8279 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8280 | `						return SXERR_ABORT;` |
|       - |  8281 | `					}` |
|       2 |  8282 | `				}` |
|       - |  8283 | `			}` |
|       5 |  8284 | `		}` |
|   47916 |  8285 | `	}` |
|  100283 |  8286 | `	return SXRET_OK;` |
|   50144 |  8287 |  |
|       - |  8288 | `/*` |
|       - |  8289 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  8290 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  8291 | ` */` |
|  100278 |  8292 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       5 |  8293 |  |
|       - |  8294 | `	ph7_class_method *pMeth;` |
|       - |  8295 | `	SyHashEntry *pEntry;` |
|       - |  8296 | `	sxu32 nAbstract;` |
|       - |  8297 | `	SyBlob sMsg;` |
|       - |  8298 | `	sxi32 rc;` |
|       - |  8299 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|  100283 |  8300 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      29 |  8301 | `		return SXRET_OK;` |
|       - |  8302 | `	}` |
|       - |  8303 | `	/* Count abstract methods */` |
|  100259 |  8304 | `	nAbstract = 0;` |
|  100259 |  8305 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  944417 |  8306 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  844163 |  8307 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  844163 |  8308 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      20 |  8309 | `			nAbstract++;` |
|       8 |  8310 | `		}` |
|       5 |  8311 | `	}` |
|  100259 |  8312 | `	if( nAbstract == 0 ){` |
|  100245 |  8313 | `		return SXRET_OK;` |
|       - |  8314 | `	}` |
|       - |  8315 | `	/* Build the error message listing all abstract methods with origins */` |
|      18 |  8316 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      18 |  8317 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  8318 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  8319 | `		&pClass->sName,nAbstract,` |
|       7 |  8320 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  8321 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  8322 | `	/* Second pass: list methods with origins */` |
|       - |  8323 | `	{` |
|      18 |  8324 | `		sxu32 nListed = 0;` |
|      18 |  8325 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      36 |  8326 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      22 |  8327 | `			ph7_class *pOrigin = 0;` |
|       - |  8328 | `			SyString *pMName;` |
|      22 |  8329 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      22 |  8330 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  8331 | `				continue;` |
|       - |  8332 | `			}` |
|      20 |  8333 | `			pMName = &pMeth->sFunc.sName;` |
|      20 |  8334 | `			if( nListed > 0 ){` |
|       3 |  8335 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  8336 | `			}` |
|       - |  8337 | `			/* Find the origin of this abstract method.` |
|       - |  8338 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  8339 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  8340 | `			 * methods. Abstract class methods only win when the class` |
|       - |  8341 | `			 * itself declared the abstract method (not inherited from` |
|       - |  8342 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  8343 | `			 * class's namespace.` |
|       - |  8344 | `			 */` |
|       - |  8345 | `			{` |
|       - |  8346 | `				ph7_class **apIface;` |
|       - |  8347 | `				ph7_class **apTrait;` |
|       - |  8348 | `				ph7_class *pWalk;` |
|       - |  8349 | `				sxu32 i;` |
|       - |  8350 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  8351 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  8352 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  8353 | `				 */` |
|      20 |  8354 | `				if( pClass->pBase ){` |
|      11 |  8355 | `					pWalk = pClass->pBase;` |
|      19 |  8356 | `					while( pWalk ){` |
|       - |  8357 | `						ph7_class_method *pParentMeth;` |
|      13 |  8358 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      13 |  8359 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  8360 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  8361 | `							 * in this class's ancestor chain.` |
|       - |  8362 | `							 */` |
|      13 |  8363 | `							int fromIface = 0;` |
|      13 |  8364 | `							ph7_class *pAnc = pWalk;` |
|      17 |  8365 | `							while( pAnc ){` |
|       - |  8366 | `								ph7_class **apPI;` |
|       - |  8367 | `								sxu32 j;` |
|      15 |  8368 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      15 |  8369 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|      10 |  8370 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|      10 |  8371 | `										fromIface = 1;` |
|      10 |  8372 | `										break;` |
|       - |  8373 | `									}` |
|     ! 0 |  8374 | `								}` |
|      15 |  8375 | `								if( fromIface ) break;` |
|       6 |  8376 | `								pAnc = pAnc->pBase;` |
|       2 |  8377 | `							}` |
|      13 |  8378 | `							if( !fromIface ){` |
|       3 |  8379 | `								pOrigin = pWalk;` |
|       3 |  8380 | `								break;` |
|       - |  8381 | `							}` |
|       4 |  8382 | `						}` |
|      10 |  8383 | `						pWalk = pWalk->pBase;` |
|       2 |  8384 | `					}` |
|       4 |  8385 | `				}` |
|       - |  8386 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  8387 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  8388 | `				 */` |
|      20 |  8389 | `				if( !pOrigin ){` |
|      18 |  8390 | `					pWalk = pClass;` |
|      40 |  8391 | `					while( pWalk && !pOrigin ){` |
|      26 |  8392 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      26 |  8393 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      16 |  8394 | `							ph7_class *pIface = apIface[i];` |
|      16 |  8395 | `							ph7_class *pDeepest = 0;` |
|      28 |  8396 | `							while( pIface ){` |
|      16 |  8397 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      16 |  8398 | `									pDeepest = pIface;` |
|       6 |  8399 | `								}` |
|      16 |  8400 | `								pIface = pIface->pBase;` |
|       4 |  8401 | `							}` |
|      16 |  8402 | `							if( pDeepest ){` |
|      16 |  8403 | `								pOrigin = pDeepest;` |
|      16 |  8404 | `								break;` |
|       - |  8405 | `							}` |
|     ! 0 |  8406 | `						}` |
|      26 |  8407 | `						pWalk = pWalk->pBase;` |
|       4 |  8408 | `					}` |
|       7 |  8409 | `				}` |
|       - |  8410 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      20 |  8411 | `				if( !pOrigin ){` |
|       3 |  8412 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  8413 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  8414 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  8415 | `							pOrigin = pClass;` |
|       3 |  8416 | `							break;` |
|       - |  8417 | `						}` |
|     ! 0 |  8418 | `					}` |
|       1 |  8419 | `				}` |
|       - |  8420 | `			}` |
|      20 |  8421 | `			if( pOrigin ){` |
|      20 |  8422 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|      12 |  8423 | `			}else{` |
|       - |  8424 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  8425 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  8426 | `			}` |
|      20 |  8427 | `			nListed++;` |
|       4 |  8428 | `		}` |
|       - |  8429 | `	}` |
|      18 |  8430 | `	SyBlobAppend(&sMsg,")",1);` |
|      25 |  8431 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  8432 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      18 |  8433 | `	SyBlobRelease(&sMsg);` |
|      18 |  8434 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8435 | `		return SXERR_ABORT;` |
|       - |  8436 | `	}` |
|      18 |  8437 | `	return SXRET_OK;` |
|   50144 |  8438 |  |
|       - |  8439 | `/*` |
|       - |  8440 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  8441 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  8442 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  8443 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  8444 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  8445 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  8446 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  8447 | ` */` |
|   96516 |  8448 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       5 |  8449 |  |
|   96521 |  8450 | `	int isAbsolute = 0;` |
|   96521 |  8451 | `	SyToken *pStart = pGen->pIn;` |
|       - |  8452 | `	SyBlob sName;` |
|   96521 |  8453 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      95 |  8454 | `		isAbsolute = 1;` |
|      95 |  8455 | `		pGen->pIn++;` |
|      45 |  8456 | `	}` |
|   96521 |  8457 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       8 |  8458 | `		pGen->pIn = pStart;` |
|       8 |  8459 | `		return SXERR_INVALID;` |
|       - |  8460 | `	}` |
|   96515 |  8461 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   96515 |  8462 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   96515 |  8463 | `	pGen->pIn++;` |
|  144783 |  8464 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   48278 |  8465 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  8466 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  8467 | `		pGen->pIn++;` |
|      13 |  8468 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  8469 | `		pGen->pIn++;` |
|       1 |  8470 | `	}` |
|   96515 |  8471 | `	if( isAbsolute ){` |
|      93 |  8472 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      49 |  8473 | `	}else{` |
|       - |  8474 | `		SyString sRaw;` |
|   96427 |  8475 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   96427 |  8476 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  8477 | `	}` |
|   96515 |  8478 | `	SyBlobRelease(&sName);` |
|   96515 |  8479 | `	return SXRET_OK;` |
|   48263 |  8480 |  |
|       - |  8481 | `/*` |
|       - |  8482 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  8483 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  8484 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  8485 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  8486 | ` * either direction cannot run unbounded.` |
|       - |  8487 | ` */` |
|       - |  8488 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|   10784 |  8489 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       5 |  8490 |  |
|       - |  8491 | `	ph7_class **apParent;` |
|       - |  8492 | `	sxu32 n;` |
|   18065 |  8493 | `	while( pInterface ){` |
|   14371 |  8494 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  8495 | `			return FALSE;` |
|       - |  8496 | `		}` |
|   17924 |  8497 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    7106 |  8498 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    7095 |  8499 | `			return TRUE;` |
|       - |  8500 | `		}` |
|    7281 |  8501 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    7281 |  8502 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  8503 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  8504 | `				return TRUE;` |
|       - |  8505 | `			}` |
|     ! 0 |  8506 | `		}` |
|    7281 |  8507 | `		pInterface = pInterface->pBase;` |
|    7281 |  8508 | `		iDepth++;` |
|       5 |  8509 | `	}` |
|    3699 |  8510 | `	return FALSE;` |
|    5397 |  8511 |  |
|   10784 |  8512 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       5 |  8513 |  |
|   10789 |  8514 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       5 |  8515 |  |
|       - |  8516 | `/*` |
|       - |  8517 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  8518 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  8519 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  8520 | ` */` |
|    7090 |  8521 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       5 |  8522 |  |
|    7099 |  8523 | `	while( pBase ){` |
|      10 |  8524 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  8525 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  8526 | `			return TRUE;` |
|       - |  8527 | `		}` |
|      10 |  8528 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  8529 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  8530 | `			return TRUE;` |
|       - |  8531 | `		}` |
|       5 |  8532 | `		pBase = pBase->pBase;` |
|       1 |  8533 | `	}` |
|    7091 |  8534 | `	return FALSE;` |
|    3550 |  8535 |  |
|       - |  8536 | `/*` |
|       - |  8537 | ` * Compile a class declaration, named or anonymous.` |
|       - |  8538 | ` *` |
|       - |  8539 | ` * For a named class pAnonName is 0 and the class name is read from the token` |
|       - |  8540 | `` * stream. For an anonymous class (`new class(args) extends B implements I {…}`)`` |
|       - |  8541 | ` * pAnonName carries the synthesized class name, the optional constructor` |
|       - |  8542 | ` * '(args)' token range is returned through ppArgStart/ppArgEnd for the caller to` |
|       - |  8543 | ` * compile, and no name token is expected. Everything after the header (extends/` |
|       - |  8544 | ` * implements, body, install) is shared by both paths.` |
|       - |  8545 | ` */` |
|  100308 |  8546 | `static sxi32 GenStateCompileClassEx(ph7_gen_state *pGen,sxi32 iFlags,` |
|       - |  8547 | `	SyString *pAnonName,SyToken **ppArgStart,SyToken **ppArgEnd)` |
|       5 |  8548 |  |
|  100313 |  8549 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8550 | `	ph7_class *pClass,*pBase;` |
|       - |  8551 | `	SyToken *pEnd,*pTmp;` |
|       - |  8552 | `	sxi32 iProtection;` |
|       - |  8553 | `	SySet aInterfaces;` |
|       - |  8554 | `	SySet aUseEntries;` |
|       - |  8555 | `	sxi32 iAttrflags;` |
|       - |  8556 | `	SyString *pName;` |
|       - |  8557 | `	sxi32 nKwrd;` |
|       - |  8558 | `	sxi32 rc;` |
|       - |  8559 | `	/* Jump the 'class' keyword */` |
|  100313 |  8560 | `	pGen->pIn++;` |
|  100313 |  8561 | `	if( pAnonName ){` |
|       - |  8562 | `		/* Anonymous class: no name token. Capture the optional constructor` |
|       - |  8563 | `		 * '(args)' range for the caller (which always supplies the out-params),` |
|       - |  8564 | `		 * then use the synthesized name. */` |
|      29 |  8565 | `		*ppArgStart = *ppArgEnd = 0;` |
|      29 |  8566 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       7 |  8567 | `			pGen->pIn++; /* Jump '(' */` |
|       7 |  8568 | `			*ppArgStart = pGen->pIn;` |
|      10 |  8569 | `			PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,` |
|       3 |  8570 | `				PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,ppArgEnd);` |
|       7 |  8571 | `			pGen->pIn = *ppArgEnd;` |
|       7 |  8572 | `			if( pGen->pIn < pGen->pEnd ){ pGen->pIn++; } /* Jump ')' */` |
|       3 |  8573 | `		}` |
|      29 |  8574 | `		pName = pAnonName;` |
|      29 |  8575 | `		pClass = PH7_NewRawClass(pGen->pVm,pAnonName,nLine);` |
|      16 |  8576 | `	}else{` |
|  100287 |  8577 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8578 | `			/* Syntax error */` |
|     ! 0 |  8579 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8580 | `			if( rc == SXERR_ABORT ){` |
|       - |  8581 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8582 | `				return SXERR_ABORT;` |
|       - |  8583 | `			}` |
|       - |  8584 | `			/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8585 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8586 | `				pGen->pIn++;` |
|     ! 0 |  8587 | `			}` |
|     ! 0 |  8588 | `			return SXRET_OK;` |
|       - |  8589 | `		}` |
|       - |  8590 | `		/* Extract class name */` |
|  100287 |  8591 | `		pName = &pGen->pIn->sData;` |
|       - |  8592 | `		/* Advance the stream cursor */` |
|  100287 |  8593 | `		pGen->pIn++;` |
|       - |  8594 | `		/* Build FQN and obtain a raw class */ {` |
|       - |  8595 | `			SyBlob sFQN;` |
|       - |  8596 | `			SyString sFQNStr;` |
|  100287 |  8597 | `			SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|  100287 |  8598 | `			GenStateBuildFQN(pGen,pName,&sFQN);` |
|  100287 |  8599 | `			SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|  100287 |  8600 | `			pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|  100287 |  8601 | `			SyBlobRelease(&sFQN);` |
|       - |  8602 | `		}` |
|       - |  8603 | `	}` |
|  100313 |  8604 | `	if( pClass == 0 ){` |
|     ! 0 |  8605 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8606 | `		return SXERR_ABORT;` |
|       - |  8607 | `	}` |
|       - |  8608 | `	/* implemented interfaces and per-use-statement trait containers */` |
|  100313 |  8609 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|  100313 |  8610 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8611 | `	/* Assume a standalone class */` |
|  100313 |  8612 | `	pBase = 0;` |
|  100313 |  8613 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   85281 |  8614 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   85281 |  8615 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8616 | `			SyBlob sResolved;` |
|       - |  8617 | `			SyString sBaseName;` |
|       - |  8618 | `			sxu32 nRefLine;` |
|   74515 |  8619 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   74515 |  8620 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   74515 |  8621 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   74515 |  8622 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8623 | `				SyBlobRelease(&sResolved);` |
|       4 |  8624 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8625 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8626 | `					pName);` |
|       3 |  8627 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8628 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8629 | `					return SXERR_ABORT;` |
|       - |  8630 | `				}` |
|       3 |  8631 | `				return SXRET_OK;` |
|       - |  8632 | `			}` |
|  111767 |  8633 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   74508 |  8634 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   74513 |  8635 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8636 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8637 | `			/* Interfaces are not allowed */` |
|   74513 |  8638 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8639 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8640 | `			}` |
|   74513 |  8641 | `			if( pBase == 0 ){` |
|     ! 0 |  8642 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8643 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8644 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8645 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8646 | `					return SXERR_ABORT;` |
|       - |  8647 | `				}` |
|     ! 0 |  8648 | `			}else{` |
|   74513 |  8649 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8650 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8651 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8652 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8653 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8654 | `						return SXERR_ABORT;` |
|       - |  8655 | `					}` |
|     ! 0 |  8656 | `				}` |
|       - |  8657 | `			}` |
|   74513 |  8658 | `			SyBlobRelease(&sResolved);` |
|   37254 |  8659 | `		}` |
|   85279 |  8660 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8661 | `			ph7_class *pInterface;` |
|       - |  8662 | `			/* Interface implementation */` |
|   10779 |  8663 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    5397 |  8664 | `			for(;;){` |
|       - |  8665 | `				SyBlob sResolved;` |
|       - |  8666 | `				SyString sIntName;` |
|       - |  8667 | `				sxu32 nRefLine;` |
|   10789 |  8668 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   10789 |  8669 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   10789 |  8670 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8671 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8672 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8673 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8674 | `						pName);` |
|     ! 0 |  8675 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8676 | `						return SXERR_ABORT;` |
|       - |  8677 | `					}` |
|     ! 0 |  8678 | `					break;` |
|       - |  8679 | `				}` |
|   21573 |  8680 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|   10784 |  8681 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   10789 |  8682 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8683 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8684 | `				/* Only interfaces are allowed */` |
|   10789 |  8685 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8686 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8687 | `				}` |
|   10789 |  8688 | `				if( pInterface == 0 ){` |
|     ! 0 |  8689 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8690 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8691 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8692 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8693 | `						return SXERR_ABORT;` |
|       - |  8694 | `					}` |
|     ! 0 |  8695 | `				}else{` |
|       - |  8696 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8697 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8698 | `					 * unless they already extend Exception or Error.` |
|       - |  8699 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8700 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8701 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|   10789 |  8702 | `					SyString *pFqn = &pClass->sName;` |
|   10789 |  8703 | `					int bIsExceptionOrError =` |
|    8935 |  8704 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   17949 |  8705 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    9021 |  8706 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3554 |  8707 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   17872 |  8708 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|   10638 |  8709 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3543 |  8710 | `						!bIsExceptionOrError ){` |
|      12 |  8711 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8712 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8713 | `							&pClass->sName);` |
|       9 |  8714 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8715 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8716 | `							return SXERR_ABORT;` |
|       - |  8717 | `						}` |
|       - |  8718 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8719 | `						 * check does not produce a duplicate fatal. */` |
|       6 |  8720 | `					}else{` |
|   10783 |  8721 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8722 | `					}` |
|       - |  8723 | `				}` |
|   10789 |  8724 | `				SyBlobRelease(&sResolved);` |
|   10789 |  8725 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    5392 |  8726 | `					break;` |
|       - |  8727 | `				}` |
|      13 |  8728 | `				pGen->pIn++;/* Jump the comma */` |
|       3 |  8729 | `			}` |
|    5387 |  8730 | `		}` |
|   42637 |  8731 | `	}` |
|  100311 |  8732 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8733 | `		/* Syntax error */` |
|     ! 0 |  8734 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8735 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8736 | `		if( rc == SXERR_ABORT ){` |
|       - |  8737 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8738 | `			return SXERR_ABORT;` |
|       - |  8739 | `		}` |
|     ! 0 |  8740 | `		return SXRET_OK;` |
|       - |  8741 | `	}` |
|  100311 |  8742 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|  100311 |  8743 | `	pEnd = 0; /* cc warning */` |
|       - |  8744 | `	/* Delimit the class body */` |
|  100311 |  8745 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|  100311 |  8746 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8747 | `		/* Syntax error */` |
|     ! 0 |  8748 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8749 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8750 | `		if( rc == SXERR_ABORT ){` |
|       - |  8751 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8752 | `			return SXERR_ABORT;` |
|       - |  8753 | `		}` |
|     ! 0 |  8754 | `		return SXRET_OK;` |
|       - |  8755 | `	}` |
|       - |  8756 | `	/* Swap token stream */` |
|  100311 |  8757 | `	pTmp = pGen->pEnd;` |
|  100311 |  8758 | `	pGen->pEnd = pEnd;` |
|       - |  8759 | `	/* Set the inherited flags */` |
|  100311 |  8760 | `	pClass->iFlags = iFlags;` |
|       - |  8761 | `	/* Start the parse process */` |
|  139173 |  8762 | `	for(;;){` |
|       - |  8763 | `		/* Jump leading/trailing semi-colons */` |
|  421353 |  8764 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   71539 |  8765 | `			pGen->pIn++;` |
|       5 |  8766 | `		}` |
|  349819 |  8767 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8768 | `			/* End of class body */` |
|  100283 |  8769 | `			break;` |
|       - |  8770 | `		}` |
|  249536 |  8771 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0` |
|  124773 |  8772 | ``			&& !GenStateIsReadonly(pGen->pIn) /* allow a leading `readonly` modifier */ ){`` |
|     ! 0 |  8773 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8774 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8775 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8776 | `			if( rc == SXERR_ABORT ){` |
|       - |  8777 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8778 | `				return SXERR_ABORT;` |
|       - |  8779 | `			}` |
|     ! 0 |  8780 | `			goto done;` |
|       - |  8781 | `		}` |
|       - |  8782 | `		/* Assume public visibility */` |
|  249541 |  8783 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  249541 |  8784 | `		iAttrflags = 0;` |
|       - |  8785 | ``		/* Optional leading `readonly` modifier (PHP 8.1) — context-sensitive, so`` |
|       - |  8786 | ``		 * it may precede the visibility keyword: `readonly public int $x`,`` |
|       - |  8787 | ``		 * `readonly int $x`. The visibility branch below also accepts it after`` |
|       - |  8788 | ``		 * the visibility keyword (`public readonly int $x`). */`` |
|  249541 |  8789 | `		if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8790 | `			int bMod = 0;` |
|     ! 0 |  8791 | `			iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8792 | `			pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       - |  8793 | `			/* If a visibility/static modifier follows, let the dispatch below` |
|       - |  8794 | ``			 * handle it; otherwise this is `readonly Type $x` (implicit public)`` |
|       - |  8795 | `			 * and we compile it directly — the type may be a keyword (int/array)` |
|       - |  8796 | `			 * that the generic keyword dispatch would misread as a method. */` |
|     ! 0 |  8797 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|     ! 0 |  8798 | `				sxi32 k = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  8799 | `				bMod = ( k == PH7_TKWRD_PUBLIC \|\| k == PH7_TKWRD_PRIVATE` |
|     ! 0 |  8800 | `					\|\| k == PH7_TKWRD_PROTECTED \|\| k == PH7_TKWRD_STATIC );` |
|     ! 0 |  8801 | `			}` |
|     ! 0 |  8802 | `			if( !bMod ){` |
|     ! 0 |  8803 | `				rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8804 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8805 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8806 | `						return SXERR_ABORT;` |
|       - |  8807 | `					}` |
|     ! 0 |  8808 | `					goto done;` |
|       - |  8809 | `				}` |
|     ! 0 |  8810 | `				continue;` |
|       - |  8811 | `			}` |
|     ! 0 |  8812 | `		}` |
|  249541 |  8813 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8814 | `			/* Extract the current keyword */` |
|  249541 |  8815 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  249541 |  8816 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8817 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8818 | `				TraitUseEntry sUse;` |
|      53 |  8819 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      53 |  8820 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      53 |  8821 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      32 |  8822 | `				for(;;){` |
|       - |  8823 | `					ph7_class *pTrait;` |
|       - |  8824 | `					SyString *pTraitName;` |
|      61 |  8825 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8826 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8827 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8828 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8829 | `							return SXERR_ABORT;` |
|       - |  8830 | `						}` |
|     ! 0 |  8831 | `						break;` |
|       - |  8832 | `					}` |
|      61 |  8833 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8834 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8835 | `						SyBlob sResolved;` |
|      61 |  8836 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      61 |  8837 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     117 |  8838 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      56 |  8839 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      61 |  8840 | `						SyBlobRelease(&sResolved);` |
|       - |  8841 | `					}` |
|       - |  8842 | `					/* Only traits are allowed */` |
|      61 |  8843 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8844 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8845 | `					}` |
|      61 |  8846 | `					if( pTrait == 0 ){` |
|     ! 0 |  8847 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8848 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8849 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8850 | `							return SXERR_ABORT;` |
|       - |  8851 | `						}` |
|     ! 0 |  8852 | `					}else{` |
|      61 |  8853 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8854 | `					}` |
|      61 |  8855 | `					pGen->pIn++; /* Advance past trait name */` |
|      61 |  8856 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      29 |  8857 | `						break;` |
|       - |  8858 | `					}` |
|      10 |  8859 | `					pGen->pIn++; /* Jump the comma */` |
|       2 |  8860 | `				}` |
|       - |  8861 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      53 |  8862 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8863 | `					SyToken *pBlock;` |
|      13 |  8864 | `					pGen->pIn++; /* Jump '{' */` |
|      13 |  8865 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|      13 |  8866 | `					sUse.pResolvStart = pGen->pIn;` |
|      13 |  8867 | `					sUse.pResolvEnd = pBlock;` |
|      13 |  8868 | `					if( pBlock < pGen->pEnd ){` |
|      13 |  8869 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       8 |  8870 | `					}else{` |
|     ! 0 |  8871 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8872 | `					}` |
|       5 |  8873 | `				}` |
|      53 |  8874 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8875 | `				/* The semicolon will be consumed by the outer loop */` |
|      53 |  8876 | `				continue;` |
|       - |  8877 | `			}` |
|  249493 |  8878 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  245707 |  8879 | `				iProtection = nKwrd;` |
|  245707 |  8880 | `				pGen->pIn++; /* Jump the visibility token */` |
|       - |  8881 | ``				/* Optional `readonly` after the visibility: `public readonly int $x`. */`` |
|  245707 |  8882 | `				if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|      20 |  8883 | `					iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|      20 |  8884 | `					pGen->pIn++; /* Jump the 'readonly' modifier */` |
|       8 |  8885 | `				}` |
|  245702 |  8886 | `				if( pGen->pIn >= pGen->pEnd` |
|  245707 |  8887 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8888 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8889 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8890 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8891 | `					if( rc == SXERR_ABORT ){` |
|       - |  8892 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8893 | `						return SXERR_ABORT;` |
|       - |  8894 | `					}` |
|     ! 0 |  8895 | `					goto done;` |
|       - |  8896 | `				}` |
|  245707 |  8897 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8898 | `					/* Attribute declaration (untyped) */` |
|   71247 |  8899 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   71247 |  8900 | `					if( rc != SXRET_OK ){` |
|       9 |  8901 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8902 | `							return SXERR_ABORT;` |
|       - |  8903 | `						}` |
|       9 |  8904 | `						goto done;` |
|       - |  8905 | `					}` |
|   71241 |  8906 | `					continue;` |
|       - |  8907 | `				}` |
|  174465 |  8908 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8909 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     169 |  8910 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     169 |  8911 | `					if( rc != SXRET_OK ){` |
|       8 |  8912 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8913 | `							return SXERR_ABORT;` |
|       - |  8914 | `						}` |
|       8 |  8915 | `						goto done;` |
|       - |  8916 | `					}` |
|     163 |  8917 | `					continue;` |
|       - |  8918 | `				}` |
|       - |  8919 | `				/* Extract the keyword */` |
|  174301 |  8920 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   87148 |  8921 | `			}` |
|  178087 |  8922 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8923 | `				/* Process constant declaration */` |
|      67 |  8924 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      67 |  8925 | `				if( rc != SXRET_OK ){` |
|       3 |  8926 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8927 | `						return SXERR_ABORT;` |
|       - |  8928 | `					}` |
|       3 |  8929 | `					goto done;` |
|       - |  8930 | `				}` |
|      35 |  8931 | `			}else{` |
|  178025 |  8932 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8933 | `					/* Static method or attribute,record that */` |
|    3593 |  8934 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3593 |  8935 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3593 |  8936 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8937 | `						/* Extract the keyword */` |
|    3585 |  8938 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3585 |  8939 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8940 | `							iProtection = nKwrd;` |
|     ! 0 |  8941 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8942 | `						}` |
|    1790 |  8943 | `					}` |
|       - |  8944 | ``					/* `readonly` after `static` (an invalid combination): detect it so the`` |
|       - |  8945 | `					 * static+readonly diagnostic fires from GenStateCompileClassAttr rather` |
|       - |  8946 | `					 * than a generic "expecting method" parse error. */` |
|    3593 |  8947 | `					if( pGen->pIn < pGen->pEnd && GenStateIsReadonly(pGen->pIn) ){` |
|     ! 0 |  8948 | `						iAttrflags \|= PH7_CLASS_ATTR_READONLY;` |
|     ! 0 |  8949 | `						pGen->pIn++; /* Jump the 'readonly' modifier */` |
|     ! 0 |  8950 | `					}` |
|    3588 |  8951 | `					if( pGen->pIn >= pGen->pEnd` |
|    3593 |  8952 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  8953 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8954 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8955 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8956 | `						if( rc == SXERR_ABORT ){` |
|       - |  8957 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8958 | `							return SXERR_ABORT;` |
|       - |  8959 | `						}` |
|     ! 0 |  8960 | `						goto done;` |
|       - |  8961 | `					}` |
|    3593 |  8962 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8963 | `						/* Attribute declaration */` |
|       8 |  8964 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  8965 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8966 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8967 | `								return SXERR_ABORT;` |
|       - |  8968 | `							}` |
|     ! 0 |  8969 | `							goto done;` |
|       - |  8970 | `						}` |
|       8 |  8971 | `						continue;` |
|       - |  8972 | `					}` |
|    3587 |  8973 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8974 | `						/* Typed static attribute declaration */` |
|      15 |  8975 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      15 |  8976 | `						if( rc != SXRET_OK ){` |
|       3 |  8977 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8978 | `								return SXERR_ABORT;` |
|       - |  8979 | `							}` |
|       3 |  8980 | `							goto done;` |
|       - |  8981 | `						}` |
|      13 |  8982 | `						continue;` |
|       - |  8983 | `					}` |
|       - |  8984 | `					/* Extract the keyword */` |
|    3575 |  8985 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  176222 |  8986 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8987 | `					/* Abstract method,record that */` |
|      12 |  8988 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8989 | `					/* Mark the whole class as abstract */` |
|      12 |  8990 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8991 | `					/* Advance the stream cursor */` |
|      12 |  8992 | `					pGen->pIn++;` |
|      12 |  8993 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8994 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8995 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8996 | `							iProtection = nKwrd;` |
|      10 |  8997 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8998 | `						}` |
|       5 |  8999 | `					}` |
|      12 |  9000 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  9001 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9002 | `							/* Static method */` |
|     ! 0 |  9003 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9004 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9005 | `					}` |
|      12 |  9006 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  9007 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9008 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9009 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  9010 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9011 | `							if( rc == SXERR_ABORT ){` |
|       - |  9012 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9013 | `								return SXERR_ABORT;` |
|       - |  9014 | `							}` |
|     ! 0 |  9015 | `							goto done;` |
|       - |  9016 | `					}` |
|      12 |  9017 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  174432 |  9018 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  9019 | `					/* final method ,record that */` |
|      17 |  9020 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|      17 |  9021 | `					pGen->pIn++; /* Jump the final keyword */` |
|      17 |  9022 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  9023 | `						/* Extract the keyword */` |
|      17 |  9024 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      17 |  9025 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 |  9026 | `							iProtection = nKwrd;` |
|       8 |  9027 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  9028 | `						}` |
|       7 |  9029 | `					}` |
|      17 |  9030 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      14 |  9031 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CONST ){` |
|       - |  9032 | `							/* final class constant (PHP 8.1). iAttrflags already carries` |
|       - |  9033 | `							 * PH7_CLASS_ATTR_FINAL; the override ban is enforced when a` |
|       - |  9034 | `							 * child class is compiled (PH7_ClassInherit). */` |
|      12 |  9035 | `							rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9036 | `							if( rc != SXRET_OK ){` |
|     ! 0 |  9037 | `								if( rc == SXERR_ABORT ){` |
|     ! 0 |  9038 | `									return SXERR_ABORT;` |
|       - |  9039 | `								}` |
|     ! 0 |  9040 | `								goto done;` |
|       - |  9041 | `							}` |
|      12 |  9042 | `							continue;` |
|       - |  9043 | `					}` |
|       5 |  9044 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  9045 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  9046 | `							/* Static method */` |
|     ! 0 |  9047 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  9048 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  9049 | `					}` |
|       5 |  9050 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9051 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9052 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9053 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  9054 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  9055 | `							if( rc == SXERR_ABORT ){` |
|       - |  9056 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  9057 | `								return SXERR_ABORT;` |
|       - |  9058 | `							}` |
|     ! 0 |  9059 | `							goto done;` |
|       - |  9060 | `					}` |
|       5 |  9061 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9062 | `				}` |
|  177997 |  9063 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9064 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9065 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  9066 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9067 | `						if( rc == SXERR_ABORT ){` |
|       - |  9068 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9069 | `							return SXERR_ABORT;` |
|       - |  9070 | `						}` |
|     ! 0 |  9071 | `						goto done;` |
|       - |  9072 | `				}` |
|  177997 |  9073 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  9074 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  9075 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  9076 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9077 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9078 | `						if( rc == SXERR_ABORT ){` |
|       - |  9079 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  9080 | `							return SXERR_ABORT;` |
|       - |  9081 | `						}` |
|     ! 0 |  9082 | `						goto done;` |
|       - |  9083 | `					}` |
|       - |  9084 | `					/* Attribute declaration */` |
|       7 |  9085 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  9086 | `				}else{` |
|       - |  9087 | `					/* Process method declaration */` |
|  177991 |  9088 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9089 | `				}` |
|  177997 |  9090 | `				if( rc != SXRET_OK ){` |
|      16 |  9091 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9092 | `						return SXERR_ABORT;` |
|       - |  9093 | `					}` |
|      16 |  9094 | `					goto done;` |
|       - |  9095 | `				}` |
|       - |  9096 | `			}` |
|   89025 |  9097 | `		}else{` |
|       - |  9098 | `			/* Attribute declaration */` |
|     ! 0 |  9099 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9100 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9101 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9102 | `					return SXERR_ABORT;` |
|       - |  9103 | `				}` |
|     ! 0 |  9104 | `				goto done;` |
|       - |  9105 | `			}` |
|       - |  9106 | `		}` |
|       5 |  9107 | `	}` |
|       - |  9108 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  9109 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  9110 | `	 */` |
|       - |  9111 | `	{` |
|       - |  9112 | `		TraitUseEntry *apUse;` |
|       - |  9113 | `		sxu32 nU;` |
|  100283 |  9114 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|  100331 |  9115 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      53 |  9116 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      53 |  9117 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      53 |  9118 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      53 |  9119 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  9120 | `			sxu32 nT;` |
|      53 |  9121 | `			if( !hasResolution ){` |
|       - |  9122 | `				/* No conflict resolution block: use standard trait application */` |
|      87 |  9123 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      49 |  9124 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      49 |  9125 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9126 | `						break;` |
|       - |  9127 | `					}` |
|      27 |  9128 | `				}` |
|      24 |  9129 | `			}else{` |
|       - |  9130 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  9131 | `				 * then use the block to resolve method conflicts.` |
|       - |  9132 | `				 */` |
|       - |  9133 | `				SyToken *pR;` |
|      25 |  9134 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      15 |  9135 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  9136 | `					ph7_class_attr *pAR;` |
|       - |  9137 | `					SyHashEntry *pER;` |
|       - |  9138 | `					SyString *pNR;` |
|      15 |  9139 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      21 |  9140 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  9141 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  9142 | `						pNR = &pAR->sName;` |
|     ! 0 |  9143 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  9144 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  9145 | `						}` |
|     ! 0 |  9146 | `					}` |
|      15 |  9147 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       9 |  9148 | `				}` |
|       - |  9149 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|      13 |  9150 | `				pR = pUse->pResolvStart;` |
|      27 |  9151 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9152 | `					SyString sTrait,sMethod;` |
|       - |  9153 | `					ph7_class *pSrcTrait;` |
|       - |  9154 | `					ph7_class_method *pMeth;` |
|       - |  9155 | `					sxi32 nRKwrd;` |
|      41 |  9156 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9157 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9158 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9159 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9160 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9161 | `					sMethod = pR->sData;` |
|      17 |  9162 | `					pR++;` |
|      17 |  9163 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9164 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9165 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9166 | `							sTrait = sMethod;` |
|       7 |  9167 | `							pR++;` |
|       7 |  9168 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9169 | `							sMethod = pR->sData;` |
|       7 |  9170 | `							pR++;` |
|       3 |  9171 | `						}` |
|       3 |  9172 | `					}` |
|      17 |  9173 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9174 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9175 | `						continue;` |
|       - |  9176 | `					}` |
|      17 |  9177 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9178 | `					pR++;` |
|      17 |  9179 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  9180 | `						pSrcTrait = 0;` |
|       7 |  9181 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  9182 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  9183 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  9184 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  9185 | `								pSrcTrait = apTrait[nT];` |
|       5 |  9186 | `								break;` |
|       - |  9187 | `							}` |
|       2 |  9188 | `						}` |
|       5 |  9189 | `						if( pSrcTrait ){` |
|       5 |  9190 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  9191 | `							if( pMeth ){` |
|       5 |  9192 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  9193 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  9194 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  9195 | `								}` |
|       2 |  9196 | `							}` |
|       2 |  9197 | `						}` |
|       2 |  9198 | `					}` |
|      35 |  9199 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9200 | `				}` |
|       - |  9201 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      25 |  9202 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  9203 | `					ph7_class_method *pMR;` |
|       - |  9204 | `					SyHashEntry *pER;` |
|       - |  9205 | `					SyString *pNR;` |
|      15 |  9206 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      41 |  9207 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      23 |  9208 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      23 |  9209 | `						pNR = &pMR->sFunc.sName;` |
|      23 |  9210 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      14 |  9211 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       6 |  9212 | `						}` |
|       3 |  9213 | `					}` |
|       9 |  9214 | `				}` |
|       - |  9215 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|      13 |  9216 | `				pR = pUse->pResolvStart;` |
|      27 |  9217 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  9218 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  9219 | `					ph7_class *pSrcTrait;` |
|       - |  9220 | `					ph7_class_method *pMeth;` |
|      27 |  9221 | `					int hasQual = 0;` |
|       - |  9222 | `					sxi32 nRKwrd;` |
|      41 |  9223 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      27 |  9224 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      17 |  9225 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      17 |  9226 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      17 |  9227 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      17 |  9228 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      17 |  9229 | `					sMethod = pR->sData;` |
|      17 |  9230 | `					pR++;` |
|      17 |  9231 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  9232 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  9233 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  9234 | `							sTrait = sMethod;` |
|       7 |  9235 | `							hasQual = 1;` |
|       7 |  9236 | `							pR++;` |
|       7 |  9237 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  9238 | `							sMethod = pR->sData;` |
|       7 |  9239 | `							pR++;` |
|       3 |  9240 | `						}` |
|       3 |  9241 | `					}` |
|      17 |  9242 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9243 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  9244 | `						continue;` |
|       - |  9245 | `					}` |
|      17 |  9246 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      17 |  9247 | `					pR++;` |
|      17 |  9248 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|      13 |  9249 | `						sxi32 iNewVis = -1;` |
|      13 |  9250 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  9251 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  9252 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  9253 | `								iNewVis = nAK;` |
|       7 |  9254 | `								pR++;` |
|       3 |  9255 | `							}` |
|       3 |  9256 | `						}` |
|      13 |  9257 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|      11 |  9258 | `							sAlias = pR->sData;` |
|      11 |  9259 | `							pR++;` |
|       4 |  9260 | `						}` |
|      13 |  9261 | `						pMeth = 0;` |
|      13 |  9262 | `						if( hasQual ){` |
|       3 |  9263 | `							pSrcTrait = 0;` |
|       5 |  9264 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  9265 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  9266 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  9267 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  9268 | `									pSrcTrait = apTrait[nT];` |
|       3 |  9269 | `									break;` |
|       - |  9270 | `								}` |
|       2 |  9271 | `							}` |
|       3 |  9272 | `							if( pSrcTrait ){` |
|       3 |  9273 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  9274 | `							}` |
|       2 |  9275 | `						}else{` |
|      10 |  9276 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  9277 | `						}` |
|      13 |  9278 | `						if( pMeth ){` |
|      13 |  9279 | `							if( sAlias.nByte > 0 ){` |
|       - |  9280 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  9281 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  9282 | `								 */` |
|       - |  9283 | `								ph7_class_method *pAlias;` |
|       - |  9284 | `								char *zAliasDup;` |
|      11 |  9285 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|      11 |  9286 | `								if( pAlias ){` |
|      11 |  9287 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|      11 |  9288 | `									if( iNewVis >= 0 ){` |
|       5 |  9289 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9290 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9291 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  9292 | `									}` |
|      11 |  9293 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      11 |  9294 | `									if( zAliasDup ){` |
|      11 |  9295 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       4 |  9296 | `									}` |
|       7 |  9297 | `								}` |
|       7 |  9298 | `							}else if( iNewVis >= 0 ){` |
|       - |  9299 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  9300 | `								ph7_class_method *pCopy;` |
|       3 |  9301 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  9302 | `								if( pCopy ){` |
|       3 |  9303 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  9304 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  9305 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  9306 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  9307 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  9308 | `									/* Replace the method in the class hash */` |
|       3 |  9309 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  9310 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  9311 | `								}` |
|       1 |  9312 | `							}` |
|       5 |  9313 | `						}` |
|       5 |  9314 | `						SXUNUSED(hasQual);` |
|       5 |  9315 | `					}` |
|      21 |  9316 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       3 |  9317 | `				}` |
|       - |  9318 | `			}` |
|      53 |  9319 | `			SySetRelease(&pUse->aTraits);` |
|      29 |  9320 | `		}` |
|       - |  9321 | `	}` |
|       - |  9322 | `	/* Install the class */` |
|  100283 |  9323 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|  100283 |  9324 | `	if( rc == SXRET_OK ){` |
|       - |  9325 | `		ph7_class **apInterface;` |
|       - |  9326 | `		sxu32 n;` |
|  100283 |  9327 | `		if( pBase ){` |
|       - |  9328 | `			/* Inherit from base class and mark as a subclass */` |
|   74513 |  9329 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   37254 |  9330 | `		}` |
|  100283 |  9331 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|  111061 |  9332 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  9333 | `			/* Implements one or more interface */` |
|   10783 |  9334 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|   10783 |  9335 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9336 | `				break;` |
|       - |  9337 | `			}` |
|    5394 |  9338 | `		}` |
|       - |  9339 | `		/* Auto-implement Stringable when class declares __toString (PHP 8.0+).` |
|       - |  9340 | `		 * Skip interfaces/traits and classes that already implement it explicitly. */` |
|  150417 |  9341 | `		if( rc == SXRET_OK` |
|  100278 |  9342 | `		 && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT)) == 0` |
|  100283 |  9343 | `		 && SyHashGet(&pClass->hMethod,"__toString",sizeof("__toString")-1) != 0 ){` |
|   85051 |  9344 | `			ph7_class *pStringable = PH7_VmExtractClass(pGen->pVm,` |
|       - |  9345 | `				"Stringable",sizeof("Stringable")-1,FALSE,0);` |
|   85051 |  9346 | `			if( pStringable ){` |
|   85051 |  9347 | `				ph7_class **apImpl = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   85051 |  9348 | `				sxu32 nImpl = SySetUsed(&pClass->aInterface);` |
|       - |  9349 | `				sxu32 i;` |
|   85051 |  9350 | `				int bAlready = 0;` |
|   92135 |  9351 | `				for( i = 0 ; i < nImpl ; i++ ){` |
|    7091 |  9352 | `					if( apImpl[i] == pStringable ){` |
|       3 |  9353 | `						bAlready = 1;` |
|       3 |  9354 | `						break;` |
|       - |  9355 | `					}` |
|    3547 |  9356 | `				}` |
|   85051 |  9357 | `				if( !bAlready ){` |
|   85049 |  9358 | `					PH7_ClassImplement(pClass,pStringable);` |
|   42522 |  9359 | `				}` |
|   42523 |  9360 | `			}` |
|   42523 |  9361 | `		}` |
|       - |  9362 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|  100283 |  9363 | `		if( rc == SXRET_OK ){` |
|  100283 |  9364 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|  100283 |  9365 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9366 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9367 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9368 | `				return SXERR_ABORT;` |
|       - |  9369 | `			}` |
|   50139 |  9370 | `		}` |
|       - |  9371 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|  100283 |  9372 | `		if( rc == SXRET_OK ){` |
|  100283 |  9373 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|  100283 |  9374 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  9375 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  9376 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  9377 | `				return SXERR_ABORT;` |
|       - |  9378 | `			}` |
|   50139 |  9379 | `		}` |
|   50139 |  9380 | `	}` |
|  100283 |  9381 | `	SySetRelease(&aUseEntries);` |
|  100283 |  9382 | `	SySetRelease(&aInterfaces);` |
|  100283 |  9383 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9384 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9385 | `		return SXERR_ABORT;` |
|       - |  9386 | `	}` |
|   50139 |  9387 | `done:` |
|       - |  9388 | `	/* Point beyond the class body */` |
|  100311 |  9389 | `	pGen->pIn = &pEnd[1];` |
|  100311 |  9390 | `	pGen->pEnd = pTmp;` |
|  100311 |  9391 | `	return PH7_OK;` |
|   50159 |  9392 |  |
|       - |  9393 | `/* Compile a named class declaration (the common case). */` |
|  100282 |  9394 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       5 |  9395 |  |
|  100287 |  9396 | `	return GenStateCompileClassEx(pGen,iFlags,0,0,0);` |
|       5 |  9397 |  |
|       - |  9398 | `/*` |
|       - |  9399 | `` * Compile an anonymous class expression: `new class(args) extends B implements I`` |
|       - |  9400 | `` * { ... }` (PHP 7.0). Mirrors PH7_CompileAnnonFunc: synthesize a unique name,`` |
|       - |  9401 | ` * compile + install the class body once (at compile time, like every other` |
|       - |  9402 | ` * class), then emit the instantiation — push the constructor arguments, load the` |
|       - |  9403 | ` * synthesized class name, and OP_NEW. The class is installed once per source` |
|       - |  9404 | ` * site, matching PHP's one-class-per-anonymous-site semantics.` |
|       - |  9405 | ` */` |
|      26 |  9406 | `PH7_PRIVATE sxi32 PH7_CompileAnnonClass(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       3 |  9407 |  |
|       - |  9408 | `	char zName[128];         /* Synthesized class name */` |
|       - |  9409 | `	static int iCnt = 1;     /* Single-threaded compile: no locking needed */` |
|       - |  9410 | `	SyString sName;` |
|       - |  9411 | `	SyToken *pArgStart,*pArgEnd;` |
|       - |  9412 | `	ph7_value *pObj;` |
|      29 |  9413 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9414 | `	sxu32 nIdx,nLen;` |
|       - |  9415 | `	sxi32 nArg,rc;` |
|      13 |  9416 | `	SXUNUSED(iCompileFlag);` |
|       - |  9417 | `	/* Generate a unique anonymous-class name (collision-checked) */` |
|      29 |  9418 | `	nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|      29 |  9419 | `	while( PH7_VmExtractClass(pGen->pVm,zName,nLen,FALSE,0) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  9420 | `		nLen = SyBufferFormat(zName,sizeof(zName),"class@anonymous_%d",iCnt++);` |
|     ! 0 |  9421 | `	}` |
|      29 |  9422 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  9423 | `	/* Compile + install the class body; capture the constructor '(args)' range.` |
|       - |  9424 | `	 * On entry pGen->pIn sits on the 'class' keyword and pGen->pEnd bounds the` |
|       - |  9425 | `	 * delimited construct; GenStateCompileClassEx restores both on success. */` |
|      29 |  9426 | `	pArgStart = pArgEnd = 0;` |
|      29 |  9427 | `	rc = GenStateCompileClassEx(pGen,0,&sName,&pArgStart,&pArgEnd);` |
|      29 |  9428 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9429 | `		return rc;` |
|       - |  9430 | `	}` |
|       - |  9431 | `	/* Emit the instantiation. OP_NEW expects the class name on the stack top` |
|       - |  9432 | `	 * with the constructor arguments beneath it, so push the args first. */` |
|      29 |  9433 | `	nArg = 0;` |
|      29 |  9434 | `	if( pArgStart < pArgEnd ){` |
|       7 |  9435 | `		SyToken *pSavedIn = pGen->pIn;` |
|       7 |  9436 | `		SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  9437 | `		SyToken *pArgNext;` |
|       7 |  9438 | `		pGen->pIn = pArgStart;` |
|       7 |  9439 | `		pGen->pEnd = pArgEnd;` |
|      13 |  9440 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pArgNext) ){` |
|       7 |  9441 | `			if( pGen->pIn < pArgNext ){` |
|       7 |  9442 | `				rc = GenStateCompileArrayEntry(pGen,pGen->pIn,pArgNext,EXPR_FLAG_RDONLY_LOAD,0);` |
|       7 |  9443 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9444 | `					pGen->pIn = pSavedIn;` |
|     ! 0 |  9445 | `					pGen->pEnd = pSavedEnd;` |
|     ! 0 |  9446 | `					return SXERR_ABORT;` |
|       - |  9447 | `				}` |
|       7 |  9448 | `				nArg++;` |
|       3 |  9449 | `			}` |
|       7 |  9450 | `			pGen->pIn = &pArgNext[1];` |
|       1 |  9451 | `		}` |
|       7 |  9452 | `		pGen->pIn = pSavedIn;` |
|       7 |  9453 | `		pGen->pEnd = pSavedEnd;` |
|       3 |  9454 | `	}` |
|       - |  9455 | `	/* Load the synthesized class name */` |
|      29 |  9456 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      29 |  9457 | `	if( pObj == 0 ){` |
|     ! 0 |  9458 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9459 | `		return SXERR_ABORT;` |
|       - |  9460 | `	}` |
|      29 |  9461 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      29 |  9462 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  9463 | `	/* Instantiate: pops the name + nArg arguments, runs __construct */` |
|      29 |  9464 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      29 |  9465 | `	return SXRET_OK;` |
|      16 |  9466 |  |
|       - |  9467 | `/*` |
|       - |  9468 | ` * Compile a user-defined abstract class.` |
|       - |  9469 | ` *  According to the PHP language reference manual` |
|       - |  9470 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  9471 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  9472 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  9473 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  9474 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  9475 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  9476 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  9477 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  9478 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  9479 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  9480 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  9481 | ` *   could differ.` |
|       - |  9482 | ` */` |
|       - |  9483 | `/*` |
|       - |  9484 | `` * Recognize a class-declaration modifier token: the `final`/`abstract` keywords`` |
|       - |  9485 | `` * or the context-sensitive `readonly` identifier (PHP 8.2). On a match, *piFlag`` |
|       - |  9486 | ` * receives the corresponding PH7_CLASS_* bit.` |
|       - |  9487 | ` */` |
|  971380 |  9488 | `static int GenStateTokenIsClassModifier(SyToken *pTok,sxi32 *piFlag)` |
|       5 |  9489 |  |
|  971385 |  9490 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|  649993 |  9491 | `		sxu32 nKw = (sxu32)SX_PTR_TO_INT(pTok->pUserData);` |
|  649993 |  9492 | `		if( nKw == PH7_TKWRD_FINAL ){ *piFlag = PH7_CLASS_FINAL; return TRUE; }` |
|  642895 |  9493 | `		if( nKw == PH7_TKWRD_ABSTRACT ){ *piFlag = PH7_CLASS_ABSTRACT; return TRUE; }` |
|  321422 |  9494 | `	}` |
|  964241 |  9495 | `	if( GenStateIsReadonly(pTok) ){ *piFlag = PH7_CLASS_READONLY; return TRUE; }` |
|  964181 |  9496 | `	return FALSE;` |
|  485695 |  9497 |  |
|       - |  9498 | `/*` |
|       - |  9499 | ` * Advance *ppIn over a leading run of class modifiers, returning the combined` |
|       - |  9500 | ` * PH7_CLASS_* flags (0 if none). If a modifier is repeated, the first repeated` |
|       - |  9501 | ` * token is reported via *ppDup (NULL when none); pass 0 for ppDup to ignore it.` |
|       - |  9502 | ` * This stays side-effect-free so it can be used for speculative look-ahead.` |
|       - |  9503 | ` */` |
|  964176 |  9504 | `static sxi32 GenStateScanClassModifiers(SyToken **ppIn,SyToken *pEnd,SyToken **ppDup)` |
|       5 |  9505 |  |
|  964181 |  9506 | `	SyToken *pIn = *ppIn,*pDup = 0;` |
|  964181 |  9507 | `	sxi32 iFlags = 0,iFlag;` |
|  971385 |  9508 | `	while( pIn < pEnd && GenStateTokenIsClassModifier(pIn,&iFlag) ){` |
|    7209 |  9509 | `		if( (iFlags & iFlag) && pDup == 0 ){` |
|       5 |  9510 | `			pDup = pIn;` |
|       2 |  9511 | `		}` |
|    7209 |  9512 | `		iFlags \|= iFlag;` |
|    7209 |  9513 | `		pIn++;` |
|       5 |  9514 | `	}` |
|  964181 |  9515 | `	*ppIn = pIn;` |
|  964181 |  9516 | `	if( ppDup ){ *ppDup = pDup; }` |
|  964181 |  9517 | `	return iFlags;` |
|       5 |  9518 |  |
|       - |  9519 | `/*` |
|       - |  9520 | ` * Test whether the token stream starts a *modified* class declaration: a run of` |
|       - |  9521 | `` * one or more `final`/`abstract`/`readonly` modifiers (in any order) terminated`` |
|       - |  9522 | `` * by the `class` keyword. Requiring at least one modifier leaves a bare`` |
|       - |  9523 | `` * `class`/`interface`/`trait` (and any expression that merely starts with`` |
|       - |  9524 | `` * `readonly`) to their existing handlers.`` |
|       - |  9525 | ` */` |
|  960584 |  9526 | `static int GenStateStartsModifiedClass(SyToken *pIn,SyToken *pEnd)` |
|       5 |  9527 |  |
|  960589 |  9528 | `	sxi32 iFlags = GenStateScanClassModifiers(&pIn,pEnd,0);` |
|  483891 |  9529 | `	return iFlags != 0 && pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD)` |
|  962382 |  9530 | `		&& (sxu32)SX_PTR_TO_INT(pIn->pUserData) == PH7_TKWRD_CLASS;` |
|       5 |  9531 |  |
|       - |  9532 | `/*` |
|       - |  9533 | ` * Compile a class declaration carrying one or more leading modifiers` |
|       - |  9534 | `` * (`final`/`abstract`/`readonly`, any order). Consumes the modifier run, leaving`` |
|       - |  9535 | `` * the cursor on the `class` keyword for GenStateCompileClass, and rejects a`` |
|       - |  9536 | `` * repeated modifier (`final final class`) or the mutually-exclusive`` |
|       - |  9537 | `` * `abstract`+`final` pair, like PHP.`` |
|       - |  9538 | ` */` |
|    3592 |  9539 | `static sxi32 PH7_CompileClassModifiers(ph7_gen_state *pGen)` |
|       5 |  9540 |  |
|       - |  9541 | `	SyToken *pDup;` |
|    3597 |  9542 | `	sxi32 iFlags = GenStateScanClassModifiers(&pGen->pIn,pGen->pEnd,&pDup);` |
|       - |  9543 | `	sxi32 rc;` |
|    3597 |  9544 | `	if( pDup ){` |
|       4 |  9545 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pDup->nLine,` |
|       2 |  9546 | `			"Multiple %z modifiers are not allowed",&pDup->sData);` |
|       3 |  9547 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9548 | `			return SXERR_ABORT;` |
|       - |  9549 | `		}` |
|       1 |  9550 | `	}` |
|    5388 |  9551 | `	if( (iFlags & (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT))` |
|    1801 |  9552 | `		== (PH7_CLASS_FINAL\|PH7_CLASS_ABSTRACT) ){` |
|       3 |  9553 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9554 | `			"Cannot use the final modifier on an abstract class");` |
|       3 |  9555 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9556 | `			return SXERR_ABORT;` |
|       - |  9557 | `		}` |
|       1 |  9558 | `	}` |
|    3597 |  9559 | `	return GenStateCompileClass(&(*pGen),iFlags);` |
|    1801 |  9560 |  |
|       - |  9561 | `/*` |
|       - |  9562 | ` * Compile a user-defined trait.` |
|       - |  9563 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  9564 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  9565 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  9566 | ` */` |
|      60 |  9567 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       5 |  9568 |  |
|      65 |  9569 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9570 | `	ph7_class *pClass;` |
|       - |  9571 | `	SyToken *pEnd,*pTmp;` |
|       - |  9572 | `	sxi32 iProtection;` |
|       - |  9573 | `	sxi32 iAttrflags;` |
|       - |  9574 | `	SyString *pName;` |
|       - |  9575 | `	sxi32 nKwrd;` |
|       - |  9576 | `	sxi32 rc;` |
|       - |  9577 | `	/* Jump the 'trait' keyword */` |
|      65 |  9578 | `	pGen->pIn++;` |
|      65 |  9579 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9580 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  9581 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9582 | `			return SXERR_ABORT;` |
|       - |  9583 | `		}` |
|     ! 0 |  9584 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  9585 | `			pGen->pIn++;` |
|     ! 0 |  9586 | `		}` |
|     ! 0 |  9587 | `		return SXRET_OK;` |
|       - |  9588 | `	}` |
|       - |  9589 | `	/* Extract trait name */` |
|      65 |  9590 | `	pName = &pGen->pIn->sData;` |
|      65 |  9591 | `	pGen->pIn++;` |
|       - |  9592 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  9593 | `		SyBlob sFQN;` |
|       - |  9594 | `		SyString sFQNStr;` |
|      65 |  9595 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      65 |  9596 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      65 |  9597 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      65 |  9598 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      65 |  9599 | `		SyBlobRelease(&sFQN);` |
|       - |  9600 | `	}` |
|      65 |  9601 | `	if( pClass == 0 ){` |
|     ! 0 |  9602 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9603 | `		return SXERR_ABORT;` |
|       - |  9604 | `	}` |
|       - |  9605 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      65 |  9606 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  9607 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  9608 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9609 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9610 | `			return SXERR_ABORT;` |
|       - |  9611 | `		}` |
|     ! 0 |  9612 | `		return SXRET_OK;` |
|       - |  9613 | `	}` |
|      65 |  9614 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      65 |  9615 | `	pEnd = 0;` |
|      65 |  9616 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      65 |  9617 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  9618 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  9619 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  9620 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9621 | `			return SXERR_ABORT;` |
|       - |  9622 | `		}` |
|     ! 0 |  9623 | `		return SXRET_OK;` |
|       - |  9624 | `	}` |
|       - |  9625 | `	/* Swap token stream */` |
|      65 |  9626 | `	pTmp = pGen->pEnd;` |
|      65 |  9627 | `	pGen->pEnd = pEnd;` |
|       - |  9628 | `	/* Mark as trait */` |
|      65 |  9629 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  9630 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      60 |  9631 | `	for(;;){` |
|     169 |  9632 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      28 |  9633 | `			pGen->pIn++;` |
|       4 |  9634 | `		}` |
|     145 |  9635 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      65 |  9636 | `			break;` |
|       - |  9637 | `		}` |
|      85 |  9638 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  9639 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9640 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9641 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  9642 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9643 | `				return SXERR_ABORT;` |
|       - |  9644 | `			}` |
|     ! 0 |  9645 | `			goto done;` |
|       - |  9646 | `		}` |
|      85 |  9647 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      85 |  9648 | `		iAttrflags = 0;` |
|      85 |  9649 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      85 |  9650 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      85 |  9651 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  9652 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  9653 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  9654 | `				for(;;){` |
|       - |  9655 | `					ph7_class *pUsedTrait;` |
|       - |  9656 | `					SyString *pUsedName;` |
|       5 |  9657 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  9658 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9659 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  9660 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9661 | `							return SXERR_ABORT;` |
|       - |  9662 | `						}` |
|     ! 0 |  9663 | `						break;` |
|       - |  9664 | `					}` |
|       5 |  9665 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  9666 | `					{` |
|       - |  9667 | `						SyBlob sResolved;` |
|       5 |  9668 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  9669 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  9670 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  9671 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  9672 | `						SyBlobRelease(&sResolved);` |
|       - |  9673 | `					}` |
|       5 |  9674 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  9675 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  9676 | `					}` |
|       5 |  9677 | `					if( pUsedTrait == 0 ){` |
|       4 |  9678 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  9679 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  9680 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9681 | `							return SXERR_ABORT;` |
|       - |  9682 | `						}` |
|       2 |  9683 | `					}else{` |
|       3 |  9684 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  9685 | `					}` |
|       5 |  9686 | `					pGen->pIn++;` |
|       5 |  9687 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  9688 | `						break;` |
|       - |  9689 | `					}` |
|     ! 0 |  9690 | `					pGen->pIn++;` |
|     ! 0 |  9691 | `				}` |
|       5 |  9692 | `				continue;` |
|       - |  9693 | `			}` |
|      81 |  9694 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      73 |  9695 | `				iProtection = nKwrd;` |
|      73 |  9696 | `				pGen->pIn++;` |
|      68 |  9697 | `				if( pGen->pIn >= pGen->pEnd` |
|      73 |  9698 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9699 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9700 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  9701 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9702 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9703 | `						return SXERR_ABORT;` |
|       - |  9704 | `					}` |
|     ! 0 |  9705 | `					goto done;` |
|       - |  9706 | `				}` |
|      73 |  9707 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      12 |  9708 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      12 |  9709 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9710 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9711 | `							return SXERR_ABORT;` |
|       - |  9712 | `						}` |
|     ! 0 |  9713 | `						goto done;` |
|       - |  9714 | `					}` |
|      12 |  9715 | `					continue;` |
|       - |  9716 | `				}` |
|      63 |  9717 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  9718 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  9719 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  9720 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9721 | `							return SXERR_ABORT;` |
|       - |  9722 | `						}` |
|     ! 0 |  9723 | `						goto done;` |
|       - |  9724 | `					}` |
|       5 |  9725 | `					continue;` |
|       - |  9726 | `				}` |
|      58 |  9727 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      27 |  9728 | `			}` |
|      66 |  9729 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  9730 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9731 | `					"Traits cannot have constants");` |
|     ! 0 |  9732 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9733 | `					return SXERR_ABORT;` |
|       - |  9734 | `				}` |
|     ! 0 |  9735 | `				goto done;` |
|     ! 0 |  9736 | `			}else{` |
|      66 |  9737 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  9738 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  9739 | `					pGen->pIn++;` |
|       5 |  9740 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  9741 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  9742 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  9743 | `							iProtection = nKwrd;` |
|     ! 0 |  9744 | `							pGen->pIn++;` |
|     ! 0 |  9745 | `						}` |
|       1 |  9746 | `					}` |
|       4 |  9747 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  9748 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP\|PH7_TK_LPAREN)) == 0 ){` |
|     ! 0 |  9749 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9750 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  9751 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9752 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9753 | `							return SXERR_ABORT;` |
|       - |  9754 | `						}` |
|     ! 0 |  9755 | `						goto done;` |
|       - |  9756 | `					}` |
|       5 |  9757 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  9758 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  9759 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9760 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9761 | `								return SXERR_ABORT;` |
|       - |  9762 | `							}` |
|     ! 0 |  9763 | `							goto done;` |
|       - |  9764 | `						}` |
|       3 |  9765 | `						continue;` |
|       - |  9766 | `					}` |
|       3 |  9767 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  9768 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9769 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9770 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9771 | `								return SXERR_ABORT;` |
|       - |  9772 | `							}` |
|     ! 0 |  9773 | `							goto done;` |
|       - |  9774 | `						}` |
|     ! 0 |  9775 | `						continue;` |
|       - |  9776 | `					}` |
|       3 |  9777 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      63 |  9778 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       6 |  9779 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       6 |  9780 | `					pGen->pIn++;` |
|       6 |  9781 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       6 |  9782 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       6 |  9783 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       6 |  9784 | `							iProtection = nKwrd;` |
|       6 |  9785 | `							pGen->pIn++;` |
|       2 |  9786 | `						}` |
|       2 |  9787 | `					}` |
|       6 |  9788 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9789 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9790 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9791 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9792 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9793 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9794 | `							return SXERR_ABORT;` |
|       - |  9795 | `						}` |
|     ! 0 |  9796 | `						goto done;` |
|       - |  9797 | `					}` |
|       6 |  9798 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9799 | `				}` |
|      64 |  9800 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9801 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9802 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9803 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9804 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9805 | `						return SXERR_ABORT;` |
|       - |  9806 | `					}` |
|     ! 0 |  9807 | `					goto done;` |
|       - |  9808 | `				}` |
|      64 |  9809 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9810 | `					pGen->pIn++;` |
|     ! 0 |  9811 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9812 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9813 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9814 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9815 | `							return SXERR_ABORT;` |
|       - |  9816 | `						}` |
|     ! 0 |  9817 | `						goto done;` |
|       - |  9818 | `					}` |
|     ! 0 |  9819 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9820 | `				}else{` |
|      64 |  9821 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9822 | `				}` |
|      64 |  9823 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9824 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9825 | `						return SXERR_ABORT;` |
|       - |  9826 | `					}` |
|     ! 0 |  9827 | `					goto done;` |
|       - |  9828 | `				}` |
|       - |  9829 | `			}` |
|      34 |  9830 | `		}else{` |
|     ! 0 |  9831 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9832 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9833 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9834 | `					return SXERR_ABORT;` |
|       - |  9835 | `				}` |
|     ! 0 |  9836 | `				goto done;` |
|       - |  9837 | `			}` |
|       - |  9838 | `		}` |
|       4 |  9839 | `	}` |
|       - |  9840 | `	/* Install the trait */` |
|      65 |  9841 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      65 |  9842 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9843 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9844 | `		return SXERR_ABORT;` |
|       - |  9845 | `	}` |
|      30 |  9846 | `done:` |
|       - |  9847 | `	/* Point beyond the trait body */` |
|      65 |  9848 | `	pGen->pIn = &pEnd[1];` |
|      65 |  9849 | `	pGen->pEnd = pTmp;` |
|      65 |  9850 | `	return PH7_OK;` |
|      35 |  9851 |  |
|       - |  9852 | `/*` |
|       - |  9853 | ` * Compile a user-defined class.` |
|       - |  9854 | ` *  According to the PHP language reference manual` |
|       - |  9855 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9856 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9857 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9858 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9859 | ` *   and functions (called "methods").` |
|       - |  9860 | ` */` |
|   96690 |  9861 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       5 |  9862 |  |
|       - |  9863 | `	sxi32 rc;` |
|   96695 |  9864 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   96695 |  9865 | `	return rc;` |
|       5 |  9866 |  |
|       - |  9867 | `/*` |
|       - |  9868 | ` * Exception handling.` |
|       - |  9869 | ` *  According to the PHP language reference manual` |
|       - |  9870 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9871 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9872 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9873 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9874 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9875 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9876 | ` *    (or re-thrown) within a catch block.` |
|       - |  9877 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9878 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9879 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9880 | ` *    been defined with set_exception_handler().` |
|       - |  9881 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9882 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9883 | ` */` |
|       - |  9884 | `/*` |
|       - |  9885 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9886 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9887 | ` * indicates failure.` |
|       - |  9888 | ` */` |
|   14462 |  9889 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       5 |  9890 |  |
|   14467 |  9891 | `	sxi32 rc = SXRET_OK;` |
|   14467 |  9892 | `	if( pRoot->pOp ){` |
|   14457 |  9893 | `		switch( pRoot->pOp->iOp ){` |
|    7226 |  9894 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9895 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9896 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9897 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9898 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9899 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|   14457 |  9900 | `			break;` |
|     ! 0 |  9901 | `		default:` |
|       - |  9902 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9903 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9904 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9905 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9906 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9907 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9908 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9909 | `			}` |
|     ! 0 |  9910 | `			break;` |
|       - |  9911 | `		}` |
|    7241 |  9912 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9913 | `		/* Unexpected expression */` |
|     ! 0 |  9914 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9915 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9916 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9917 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9918 | `		}` |
|     ! 0 |  9919 | `	}` |
|   14467 |  9920 | `	return rc;` |
|       5 |  9921 |  |
|       - |  9922 | `/*` |
|       - |  9923 | ` * Compile a 'throw' statement.` |
|       - |  9924 | ` * throw: This is how you trigger an exception.` |
|       - |  9925 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9926 | ` */` |
|   14426 |  9927 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       5 |  9928 |  |
|   14431 |  9929 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9930 | `	GenBlock *pBlock;` |
|       - |  9931 | `	sxu32 nIdx;` |
|       - |  9932 | `	sxi32 rc;` |
|   14431 |  9933 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9934 | `	/* Compile the expression */` |
|   14431 |  9935 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   14431 |  9936 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9937 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9938 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9939 | `			return SXERR_ABORT;` |
|       - |  9940 | `		}` |
|     ! 0 |  9941 | `		return SXRET_OK;` |
|       - |  9942 | `	}` |
|   14431 |  9943 | `	pBlock = pGen->pCurrent;` |
|       - |  9944 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   57177 |  9945 | `	while(pBlock->pParent){` |
|   57173 |  9946 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   14427 |  9947 | `			break;` |
|       - |  9948 | `		}` |
|       - |  9949 | `		/* Point to the parent block */` |
|   42751 |  9950 | `		pBlock = pBlock->pParent;` |
|       5 |  9951 | `	}` |
|       - |  9952 | `	/* Emit the throw instruction */` |
|   14431 |  9953 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9954 | `	/* Emit the jump */` |
|   14431 |  9955 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   14431 |  9956 | `	return SXRET_OK;` |
|    7218 |  9957 |  |
|       - |  9958 | `/*` |
|       - |  9959 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9960 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9961 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9962 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9963 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9964 | ` */` |
|      36 |  9965 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9966 |  |
|      38 |  9967 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9968 | `	GenBlock *pBlock;` |
|       - |  9969 | `	sxu32 nIdx;` |
|       - |  9970 | `	sxi32 rc;` |
|      18 |  9971 | `	(void)iCompileFlag;` |
|      38 |  9972 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9973 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9974 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9975 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9976 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9977 | `			return SXERR_ABORT;` |
|       - |  9978 | `		}` |
|     ! 0 |  9979 | `		return SXRET_OK;` |
|       - |  9980 | `	}` |
|      38 |  9981 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9982 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9983 | `		return SXERR_ABORT;` |
|       - |  9984 | `	}` |
|      38 |  9985 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9986 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9987 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9988 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9989 | `			return SXERR_ABORT;` |
|       - |  9990 | `		}` |
|     ! 0 |  9991 | `		return SXRET_OK;` |
|       - |  9992 | `	}` |
|       - |  9993 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9994 | `	pBlock = pGen->pCurrent;` |
|      60 |  9995 | `	while( pBlock->pParent ){` |
|      49 |  9996 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9997 | `			break;` |
|       - |  9998 | `		}` |
|      23 |  9999 | `		pBlock = pBlock->pParent;` |
|       1 | 10000 | `	}` |
|      38 | 10001 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 | 10002 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 | 10003 | `	return SXRET_OK;` |
|      20 | 10004 |  |
|       - | 10005 | `/*` |
|       - | 10006 | ` * Compile a 'catch' block.` |
|       - | 10007 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - | 10008 | ` * an object containing the exception information.` |
|       - | 10009 | ` */` |
|     566 | 10010 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       5 | 10011 |  |
|     571 | 10012 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10013 | `	ph7_exception_block sCatch;` |
|       - | 10014 | `	SySet *pInstrContainer;` |
|       - | 10015 | `	SyString sClassName;` |
|       - | 10016 | `	GenBlock *pCatch;` |
|       - | 10017 | `	SyToken *pToken;` |
|       - | 10018 | `	SyString *pName;` |
|       - | 10019 | `	char *zDup;` |
|       - | 10020 | `	sxi32 rc;` |
|     571 | 10021 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - | 10022 | `	/* Zero the structure */` |
|     571 | 10023 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - | 10024 | `	/* Initialize fields */` |
|     571 | 10025 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     571 | 10026 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     571 | 10027 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - | 10028 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10029 | `			pToken = pGen->pIn;` |
|     ! 0 | 10030 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10031 | `				pToken--;` |
|     ! 0 | 10032 | `			}` |
|     ! 0 | 10033 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10034 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10035 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10036 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10037 | `				return SXERR_ABORT;` |
|       - | 10038 | `			}` |
|     ! 0 | 10039 | `			return SXERR_INVALID;` |
|       - | 10040 | `	}` |
|       - | 10041 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     571 | 10042 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     297 | 10043 | `	for(;;){` |
|       - | 10044 | `		SyBlob sResolved;` |
|     599 | 10045 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     599 | 10046 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       6 | 10047 | `			SyBlobRelease(&sResolved);` |
|       6 | 10048 | `			pToken = pGen->pIn;` |
|       6 | 10049 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10050 | `				pToken--;` |
|     ! 0 | 10051 | `			}` |
|       8 | 10052 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10053 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 | 10054 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       6 | 10055 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10056 | `				return SXERR_ABORT;` |
|       - | 10057 | `			}` |
|       6 | 10058 | `			return SXERR_INVALID;` |
|       - | 10059 | `		}` |
|       - | 10060 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - | 10061 | `		 * transient SyBlob allocation. */` |
|     890 | 10062 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     590 | 10063 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     595 | 10064 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     595 | 10065 | `		SyBlobRelease(&sResolved);` |
|     595 | 10066 | `		if( zDup == 0 ){` |
|     ! 0 | 10067 | `			goto Mem;` |
|       - | 10068 | `		}` |
|     595 | 10069 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     595 | 10070 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10071 | `			goto Mem;` |
|       - | 10072 | `		}` |
|       - | 10073 | `		/* Check for '\|' (multi-catch separator) */` |
|     604 | 10074 | `		if( pGen->pIn < pGen->pEnd &&` |
|     590 | 10075 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      33 | 10076 | `			pGen->pIn->sData.nByte == 1 &&` |
|      28 | 10077 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      30 | 10078 | `			pGen->pIn++; /* Consume the '\|' */` |
|      30 | 10079 | `			continue;` |
|       - | 10080 | `		}` |
|     567 | 10081 | `		break;` |
|     ! 0 | 10082 | `	}` |
|     843 | 10083 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     567 | 10084 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - | 10085 | `			/* Unexpected token,break immediately */` |
|     ! 0 | 10086 | `			pToken = pGen->pIn;` |
|     ! 0 | 10087 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10088 | `				pToken--;` |
|     ! 0 | 10089 | `			}` |
|     ! 0 | 10090 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10091 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10092 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10093 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10094 | `				return SXERR_ABORT;` |
|       - | 10095 | `			}` |
|     ! 0 | 10096 | `			return SXERR_INVALID;` |
|       - | 10097 | `	}` |
|     567 | 10098 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - | 10099 | `	/* Duplicate instance name */` |
|     567 | 10100 | `	pName = &pGen->pIn->sData;` |
|     567 | 10101 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     567 | 10102 | `	if( zDup == 0 ){` |
|     ! 0 | 10103 | `		goto Mem;` |
|       - | 10104 | `	}` |
|     567 | 10105 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     567 | 10106 | `	pGen->pIn++;` |
|     567 | 10107 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - | 10108 | `		/* Unexpected token,break immediately */` |
|     ! 0 | 10109 | `		pToken = pGen->pIn;` |
|     ! 0 | 10110 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 | 10111 | `			pToken--;` |
|     ! 0 | 10112 | `		}` |
|     ! 0 | 10113 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - | 10114 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 | 10115 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 | 10116 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10117 | `			return SXERR_ABORT;` |
|       - | 10118 | `		}` |
|     ! 0 | 10119 | `		return SXERR_INVALID;` |
|       - | 10120 | `	}` |
|       - | 10121 | `	/* Compile the block */` |
|     567 | 10122 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - | 10123 | `	/* Create the catch block */` |
|     567 | 10124 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     567 | 10125 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10126 | `		return SXERR_ABORT;` |
|       - | 10127 | `	}` |
|       - | 10128 | `	/* Swap bytecode container */` |
|     567 | 10129 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     567 | 10130 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - | 10131 | `	/* Compile the block */` |
|     567 | 10132 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - | 10133 | `	/* Fix forward jumps now the destination is resolved  */` |
|     567 | 10134 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10135 | `	/* Emit the DONE instruction */` |
|     567 | 10136 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10137 | `	/* Leave the block */` |
|     567 | 10138 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10139 | `	/* Restore the default container */` |
|     567 | 10140 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10141 | `	/* Install the catch block */` |
|     567 | 10142 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     567 | 10143 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10144 | `		goto Mem;` |
|       - | 10145 | `	}` |
|     567 | 10146 | `	return SXRET_OK;` |
|     ! 0 | 10147 | `Mem:` |
|     ! 0 | 10148 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10149 | `	return SXERR_ABORT;` |
|     288 | 10150 |  |
|       - | 10151 | `/*` |
|       - | 10152 | ` * Compile a 'try' block.` |
|       - | 10153 | ` * A function using an exception should be in a "try" block.` |
|       - | 10154 | ` * If the exception does not trigger, the code will continue` |
|       - | 10155 | ` * as normal. However if the exception triggers, an exception` |
|       - | 10156 | ` * is "thrown".` |
|       - | 10157 | ` */` |
|     604 | 10158 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       5 | 10159 |  |
|       - | 10160 | `	ph7_exception *pException;` |
|     609 | 10161 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - | 10162 | `	GenBlock *pTry;` |
|       - | 10163 | `	sxu32 nJmpIdx;` |
|       - | 10164 | `	sxi32 rc;` |
|       - | 10165 | `	/* Create the exception container */` |
|     609 | 10166 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     609 | 10167 | `	if( pException == 0 ){` |
|     ! 0 | 10168 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 | 10169 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 10170 | `		return SXERR_ABORT;` |
|       - | 10171 | `	}` |
|       - | 10172 | `	/* Zero the structure */` |
|     609 | 10173 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - | 10174 | `	/* Initialize fields */` |
|     609 | 10175 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     609 | 10176 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     609 | 10177 | `	pException->iHasFinally = 0;` |
|     609 | 10178 | `	pException->iFinallyDone = 0;` |
|     609 | 10179 | `	pException->pVm = pGen->pVm;` |
|       - | 10180 | `	/* Create the try block */` |
|     609 | 10181 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     609 | 10182 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10183 | `		return SXERR_ABORT;` |
|       - | 10184 | `	}` |
|       - | 10185 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     609 | 10186 | `	pTry->pUserData = pException;` |
|       - | 10187 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     609 | 10188 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - | 10189 | `	/* Fix the jump later when the destination is resolved */` |
|     609 | 10190 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     609 | 10191 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - | 10192 | `	/* Compile the block */` |
|     609 | 10193 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     609 | 10194 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10195 | `		return SXERR_ABORT;` |
|       - | 10196 | `	}` |
|       - | 10197 | `	/* Fix forward jumps now the destination is resolved */` |
|     609 | 10198 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10199 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     609 | 10200 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - | 10201 | `	/* Leave the block */` |
|     609 | 10202 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10203 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     609 | 10204 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     602 | 10205 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - | 10206 | `		/* Compile one or more catch blocks */` |
|     562 | 10207 | `		for(;;){` |
|    1124 | 10208 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     907 | 10209 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     284 | 10210 | `					break;` |
|       - | 10211 | `			}` |
|     571 | 10212 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     571 | 10213 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10214 | `				return SXERR_ABORT;` |
|       - | 10215 | `			}` |
|       5 | 10216 | `		}` |
|     279 | 10217 | `	}` |
|       - | 10218 | `	/* Compile optional finally block */` |
|     609 | 10219 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     328 | 10220 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - | 10221 | `		SySet *pInstrContainer;` |
|       - | 10222 | `		GenBlock *pFinBlock;` |
|     107 | 10223 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - | 10224 | `		/* Create the finally block for jump fixup bookkeeping */` |
|     107 | 10225 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|     107 | 10226 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10227 | `			return SXERR_ABORT;` |
|       - | 10228 | `		}` |
|       - | 10229 | `		/* Swap bytecode container */` |
|     107 | 10230 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     107 | 10231 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - | 10232 | `		/* Compile the finally body */` |
|     107 | 10233 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     107 | 10234 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10235 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 | 10236 | `			return SXERR_ABORT;` |
|       - | 10237 | `		}` |
|       - | 10238 | `		/* Fix forward jumps now the destination is resolved */` |
|     107 | 10239 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10240 | `		/* Emit DONE to terminate the finally block */` |
|     107 | 10241 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - | 10242 | `		/* Leave the block */` |
|     107 | 10243 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - | 10244 | `		/* Restore the default container */` |
|     107 | 10245 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     107 | 10246 | `		pException->iHasFinally = 1;` |
|      51 | 10247 | `	}` |
|       - | 10248 | `	/* Must have at least one catch or finally */` |
|     609 | 10249 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       8 | 10250 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - | 10251 | `			"Cannot use try without catch or finally");` |
|       8 | 10252 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10253 | `			return SXERR_ABORT;` |
|       - | 10254 | `		}` |
|       3 | 10255 | `	}` |
|     609 | 10256 | `	return SXRET_OK;` |
|     307 | 10257 |  |
|       - | 10258 | `/*` |
|       - | 10259 | ` * Compile a switch block.` |
|       - | 10260 | ` *  (See block-comment below for more information)` |
|       - | 10261 | ` */` |
|     112 | 10262 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       5 | 10263 |  |
|     117 | 10264 | `	sxi32 rc = SXRET_OK;` |
|     117 | 10265 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - | 10266 | `		/* Unexpected token */` |
|     ! 0 | 10267 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10268 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10269 | `			return SXERR_ABORT;` |
|       - | 10270 | `		}` |
|     ! 0 | 10271 | `		pGen->pIn++;` |
|     ! 0 | 10272 | `	}` |
|     117 | 10273 | `	pGen->pIn++;` |
|       - | 10274 | `	/* First instruction to execute in this block. */` |
|     117 | 10275 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - | 10276 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - | 10277 | `	 * or the '}' token */` |
|     206 | 10278 | `	for(;;){` |
|     417 | 10279 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10280 | `			/* No more input to process */` |
|     ! 0 | 10281 | `			break;` |
|       - | 10282 | `		}` |
|     417 | 10283 | `		rc = SXRET_OK;` |
|     417 | 10284 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      85 | 10285 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      31 | 10286 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - | 10287 | `					/* Unexpected token */` |
|     ! 0 | 10288 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10289 | `						&pGen->pIn->sData);` |
|     ! 0 | 10290 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10291 | `						return SXERR_ABORT;` |
|       - | 10292 | `					}` |
|       - | 10293 | `					/* FALL THROUGH */` |
|     ! 0 | 10294 | `				}` |
|      31 | 10295 | `				rc = SXERR_EOF;` |
|      31 | 10296 | `				break;` |
|       - | 10297 | `			}` |
|      32 | 10298 | `		}else{` |
|       - | 10299 | `			sxi32 nKwrd;` |
|       - | 10300 | `			/* Extract the keyword */` |
|     337 | 10301 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     337 | 10302 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      47 | 10303 | `				break;` |
|       - | 10304 | `			}` |
|     253 | 10305 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10306 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - | 10307 | `					/* Unexpected token */` |
|     ! 0 | 10308 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 | 10309 | `						&pGen->pIn->sData);` |
|     ! 0 | 10310 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10311 | `						return SXERR_ABORT;` |
|       - | 10312 | `					}` |
|       - | 10313 | `					/* FALL THROUGH */` |
|     ! 0 | 10314 | `				}` |
|       - | 10315 | `				/* Block compiled */` |
|       3 | 10316 | `				break;` |
|       - | 10317 | `			}` |
|       - | 10318 | `		}` |
|       - | 10319 | `		/* Compile block */` |
|     305 | 10320 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     305 | 10321 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10322 | `			return SXERR_ABORT;` |
|       - | 10323 | `		}` |
|       5 | 10324 | `	}` |
|     117 | 10325 | `	return rc;` |
|      61 | 10326 |  |
|       - | 10327 | `/*` |
|       - | 10328 | ` * Compile a case eXpression.` |
|       - | 10329 | ` *  (See block-comment below for more information)` |
|       - | 10330 | ` */` |
|      92 | 10331 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       5 | 10332 |  |
|       - | 10333 | `	SySet *pInstrContainer;` |
|       - | 10334 | `	SyToken *pEnd,*pTmp;` |
|      97 | 10335 | `	sxi32 iNest = 0;` |
|       - | 10336 | `	sxi32 rc;` |
|       - | 10337 | `	/* Delimit the expression */` |
|      97 | 10338 | `	pEnd = pGen->pIn;` |
|     197 | 10339 | `	while( pEnd < pGen->pEnd ){` |
|     197 | 10340 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - | 10341 | `			/* Increment nesting level */` |
|       3 | 10342 | `			iNest++;` |
|     196 | 10343 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - | 10344 | `			/* Decrement nesting level */` |
|       3 | 10345 | `			iNest--;` |
|     194 | 10346 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      97 | 10347 | `			break;` |
|       - | 10348 | `		}` |
|     105 | 10349 | `		pEnd++;` |
|       5 | 10350 | `	}` |
|      97 | 10351 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 | 10352 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 | 10353 | `		if( rc == SXERR_ABORT ){` |
|       - | 10354 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10355 | `			return SXERR_ABORT;` |
|       - | 10356 | `		}` |
|     ! 0 | 10357 | `	}` |
|       - | 10358 | `	/* Swap token stream */` |
|      97 | 10359 | `	pTmp = pGen->pEnd;` |
|      97 | 10360 | `	pGen->pEnd = pEnd;` |
|      97 | 10361 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      97 | 10362 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      97 | 10363 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - | 10364 | `	/* Emit the done instruction */` |
|      97 | 10365 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      97 | 10366 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - | 10367 | `	/* Update token stream */` |
|      97 | 10368 | `	pGen->pIn  = pEnd;` |
|      97 | 10369 | `	pGen->pEnd = pTmp;` |
|      97 | 10370 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 10371 | `		return SXERR_ABORT;` |
|       - | 10372 | `	}` |
|      97 | 10373 | `	return SXRET_OK;` |
|      51 | 10374 |  |
|       - | 10375 | `/*` |
|       - | 10376 | ` * Compile the smart switch statement.` |
|       - | 10377 | ` * According to the PHP language reference manual` |
|       - | 10378 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - | 10379 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - | 10380 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - | 10381 | ` *  This is exactly what the switch statement is for.` |
|       - | 10382 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - | 10383 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - | 10384 | ` *  of the outer loop, use continue 2.` |
|       - | 10385 | ` *  Note that switch/case does loose comparision.` |
|       - | 10386 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - | 10387 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - | 10388 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - | 10389 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - | 10390 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - | 10391 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - | 10392 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - | 10393 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - | 10394 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - | 10395 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - | 10396 | ` *  list for the next case.` |
|       - | 10397 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - | 10398 | ` *  or floating-point numbers and strings.` |
|       - | 10399 | ` */` |
|      28 | 10400 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       5 | 10401 |  |
|       - | 10402 | `	GenBlock *pSwitchBlock;` |
|       - | 10403 | `	SyToken *pTmp,*pEnd;` |
|       - | 10404 | `	ph7_switch *pSwitch;` |
|       - | 10405 | `	sxu32 nToken;` |
|       - | 10406 | `	sxu32 nLine;` |
|       - | 10407 | `	sxi32 rc;` |
|      33 | 10408 | `	nLine = pGen->pIn->nLine;` |
|       - | 10409 | `	/* Jump the 'switch' keyword */` |
|      33 | 10410 | `	pGen->pIn++;` |
|      33 | 10411 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - | 10412 | `		/* Syntax error */` |
|     ! 0 | 10413 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 | 10414 | `		if( rc == SXERR_ABORT ){` |
|       - | 10415 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10416 | `			return SXERR_ABORT;` |
|       - | 10417 | `		}` |
|     ! 0 | 10418 | `		goto Synchronize;` |
|       - | 10419 | `	}` |
|       - | 10420 | `	/* Jump the left parenthesis '(' */` |
|      33 | 10421 | `	pGen->pIn++;` |
|      33 | 10422 | `	pEnd = 0; /* cc warning */` |
|       - | 10423 | `	/* Create the loop block */` |
|      47 | 10424 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 | 10425 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      33 | 10426 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 10427 | `		return SXERR_ABORT;` |
|       - | 10428 | `	}` |
|       - | 10429 | `	/* Delimit the condition */` |
|      33 | 10430 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      33 | 10431 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - | 10432 | `		/* Empty expression */` |
|     ! 0 | 10433 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 | 10434 | `		if( rc == SXERR_ABORT ){` |
|       - | 10435 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 10436 | `			return SXERR_ABORT;` |
|       - | 10437 | `		}` |
|     ! 0 | 10438 | `	}` |
|       - | 10439 | `	/* Swap token streams */` |
|      33 | 10440 | `	pTmp = pGen->pEnd;` |
|      33 | 10441 | `	pGen->pEnd = pEnd;` |
|       - | 10442 | `	/* Compile the expression */` |
|      33 | 10443 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      33 | 10444 | `	if( rc == SXERR_ABORT ){` |
|       - | 10445 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 | 10446 | `		return SXERR_ABORT;` |
|       - | 10447 | `	}` |
|       - | 10448 | `	/* Update token stream */` |
|      33 | 10449 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 | 10450 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 | 10451 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 | 10452 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 10453 | `			return SXERR_ABORT;` |
|       - | 10454 | `		}` |
|     ! 0 | 10455 | `		pGen->pIn++;` |
|     ! 0 | 10456 | `	}` |
|      33 | 10457 | `	pGen->pIn  = &pEnd[1];` |
|      33 | 10458 | `	pGen->pEnd = pTmp;` |
|      33 | 10459 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 | 10460 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 | 10461 | `			pTmp = pGen->pIn;` |
|     ! 0 | 10462 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 | 10463 | `				pTmp--;` |
|     ! 0 | 10464 | `			}` |
|       - | 10465 | `			/* Unexpected token */` |
|     ! 0 | 10466 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 | 10467 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10468 | `				return SXERR_ABORT;` |
|       - | 10469 | `			}` |
|     ! 0 | 10470 | `			goto Synchronize;` |
|       - | 10471 | `	}` |
|       - | 10472 | `	/* Set the delimiter token */` |
|      33 | 10473 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 | 10474 | `		nToken = PH7_TK_KEYWORD;` |
|       - | 10475 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 | 10476 | `	}else{` |
|      31 | 10477 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - | 10478 | `	}` |
|      33 | 10479 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - | 10480 | `	/* Create the switch blocks container */` |
|      33 | 10481 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      33 | 10482 | `	if( pSwitch == 0 ){` |
|       - | 10483 | `		/* Abort compilation */` |
|     ! 0 | 10484 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 | 10485 | `		return SXERR_ABORT;` |
|       - | 10486 | `	}` |
|       - | 10487 | `	/* Zero the structure */` |
|      33 | 10488 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - | 10489 | `	/* Initialize fields */` |
|      33 | 10490 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - | 10491 | `	/* Emit the switch instruction */` |
|      33 | 10492 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - | 10493 | `	/* Compile case blocks */` |
|     100 | 10494 | `	for(;;){` |
|       - | 10495 | `		sxu32 nKwrd;` |
|     119 | 10496 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10497 | `			/* No more input to process */` |
|     ! 0 | 10498 | `			break;` |
|       - | 10499 | `		}` |
|     119 | 10500 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 | 10501 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - | 10502 | `				/* Unexpected token */` |
|     ! 0 | 10503 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10504 | `					&pGen->pIn->sData);` |
|     ! 0 | 10505 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10506 | `					return SXERR_ABORT;` |
|       - | 10507 | `				}` |
|       - | 10508 | `				/* FALL THROUGH */` |
|     ! 0 | 10509 | `			}` |
|       - | 10510 | `			/* Block compiled */` |
|     ! 0 | 10511 | `			break;` |
|       - | 10512 | `		}` |
|       - | 10513 | `		/* Extract the keyword */` |
|     119 | 10514 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     119 | 10515 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 | 10516 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - | 10517 | `				/* Unexpected token */` |
|     ! 0 | 10518 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10519 | `					&pGen->pIn->sData);` |
|     ! 0 | 10520 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10521 | `					return SXERR_ABORT;` |
|       - | 10522 | `				}` |
|       - | 10523 | `				/* FALL THROUGH */` |
|     ! 0 | 10524 | `			}` |
|       - | 10525 | `			/* Block compiled */` |
|       3 | 10526 | `			break;` |
|       - | 10527 | `		}` |
|     117 | 10528 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - | 10529 | `			/*` |
|       - | 10530 | `			 * Accroding to the PHP language reference manual` |
|       - | 10531 | `			 *  A special case is the default case. This case matches anything` |
|       - | 10532 | `			 *  that wasn't matched by the other cases.` |
|       - | 10533 | `			 */` |
|      25 | 10534 | `			if( pSwitch->nDefault > 0 ){` |
|       - | 10535 | `				/* Default case already compiled */` |
|     ! 0 | 10536 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 | 10537 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 10538 | `					return SXERR_ABORT;` |
|       - | 10539 | `				}` |
|     ! 0 | 10540 | `			}` |
|      25 | 10541 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - | 10542 | `			/* Compile the default block */` |
|      25 | 10543 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      25 | 10544 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10545 | `				return SXERR_ABORT;` |
|      25 | 10546 | `			}else if( rc == SXERR_EOF ){` |
|      23 | 10547 | `				break;` |
|       1 | 10548 | `			}` |
|      98 | 10549 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - | 10550 | `			ph7_case_expr sCase;` |
|       - | 10551 | `			/* Standard case block */` |
|      97 | 10552 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - | 10553 | `			/* initialize the structure */` |
|      97 | 10554 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - | 10555 | `			/* Compile the case expression */` |
|      97 | 10556 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      97 | 10557 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10558 | `				return SXERR_ABORT;` |
|       - | 10559 | `			}` |
|       - | 10560 | `			/* Compile the case block */` |
|      97 | 10561 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - | 10562 | `			/* Insert in the switch container */` |
|      97 | 10563 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      97 | 10564 | `			if( rc == SXERR_ABORT){` |
|     ! 0 | 10565 | `				return SXERR_ABORT;` |
|      97 | 10566 | `			}else if( rc == SXERR_EOF ){` |
|       9 | 10567 | `				break;` |
|       - | 10568 | `			}` |
|      47 | 10569 | `		}else{` |
|       - | 10570 | `			/* Unexpected token */` |
|     ! 0 | 10571 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 | 10572 | `				&pGen->pIn->sData);` |
|     ! 0 | 10573 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10574 | `				return SXERR_ABORT;` |
|       - | 10575 | `			}` |
|     ! 0 | 10576 | `			break;` |
|       - | 10577 | `		}` |
|       5 | 10578 | `	}` |
|       - | 10579 | `	/* Fix all jumps now the destination is resolved */` |
|      33 | 10580 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      33 | 10581 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10582 | `	/* Release the loop block */` |
|      33 | 10583 | `	GenStateLeaveBlock(pGen,0);` |
|      33 | 10584 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - | 10585 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      33 | 10586 | `		pGen->pIn++;` |
|      14 | 10587 | `	}` |
|       - | 10588 | `	/* Statement successfully compiled */` |
|      33 | 10589 | `	return SXRET_OK;` |
|     ! 0 | 10590 | `Synchronize:` |
|       - | 10591 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 | 10592 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 | 10593 | `		pGen->pIn++;` |
|     ! 0 | 10594 | `	}` |
|     ! 0 | 10595 | `	return SXRET_OK;` |
|      19 | 10596 |  |
|       - | 10597 | `/*` |
|       - | 10598 | ` * Chain operators participate in a postfix member-access chain.` |
|       - | 10599 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - | 10600 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - | 10601 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - | 10602 | ` */` |
|       - | 10603 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - | 10604 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - | 10605 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - | 10606 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - | 10607 |  |
|       - | 10608 | `/*` |
|       - | 10609 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - | 10610 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - | 10611 | ` * patched entries from the pending set.` |
|       - | 10612 | ` */` |
| 2593926 | 10613 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       5 | 10614 |  |
| 2593931 | 10615 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - | 10616 | `	sxu32 nTarget;` |
|       - | 10617 | `	sxu32 *aIdx;` |
|       - | 10618 | `	sxu32 i;` |
| 2593931 | 10619 | `	if( nCur <= nBaseline ){` |
| 2593837 | 10620 | `		return;` |
|       - | 10621 | `	}` |
|      97 | 10622 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      97 | 10623 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     199 | 10624 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     105 | 10625 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     105 | 10626 | `		if( pInstr ){` |
|     105 | 10627 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      51 | 10628 | `		}` |
|      54 | 10629 | `	}` |
|      97 | 10630 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1296968 | 10631 |  |
|       - | 10632 |  |
|       - | 10633 | `/*` |
|       - | 10634 | ` * By-reference out-parameters of builtin functions.` |
|       - | 10635 | ` *` |
|       - | 10636 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|       - | 10637 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|       - | 10638 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|       - | 10639 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|       - | 10640 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|       - | 10641 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|       - | 10642 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|       - | 10643 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|       - | 10644 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|       - | 10645 | ` * creates it" behaviour).` |
|       - | 10646 | ` *` |
|       - | 10647 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|       - | 10648 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|       - | 10649 | ` */` |
|  423796 | 10650 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|       5 | 10651 |  |
|       - | 10652 | `	static const struct {` |
|       - | 10653 | `		const char *zName;` |
|       - | 10654 | `		sxu32 nByte;` |
|       - | 10655 | `		sxu32 mask;` |
|       - | 10656 | `	} aByRef[] = {` |
|       - | 10657 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10658 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|       - | 10659 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10660 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|       - | 10661 | `	};` |
|       - | 10662 | `	sxu32 i;` |
|  423801 | 10663 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    1375 | 10664 | `		return 0;` |
|       - | 10665 | `	}` |
| 2111915 | 10666 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 1689552 | 10667 | `		if( pName->nByte == aByRef[i].nByte` |
|  866929 | 10668 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      73 | 10669 | `			return aByRef[i].mask;` |
|       - | 10670 | `		}` |
|  844747 | 10671 | `	}` |
|  422363 | 10672 | `	return 0;` |
|  211903 | 10673 |  |
|       - | 10674 | `/*` |
|       - | 10675 | ` * Recover the bare global-builtin name from a call's callee node.` |
|       - | 10676 | ` *` |
|       - | 10677 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|       - | 10678 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|       - | 10679 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|       - | 10680 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|       - | 10681 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|       - | 10682 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|       - | 10683 | ` */` |
|  423796 | 10684 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|       5 | 10685 |  |
|       - | 10686 | `	SyToken *p, *pEnd;` |
|  423801 | 10687 | `	pOut->zString = 0;` |
|  423801 | 10688 | `	pOut->nByte = 0;` |
|  423801 | 10689 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|     ! 0 | 10690 | `		return;` |
|       - | 10691 | `	}` |
|  423801 | 10692 | `	p = pLeft->pStart;` |
|  423801 | 10693 | `	pEnd = pLeft->pEnd;` |
|       - | 10694 | `	/* Optional single leading namespace separator (absolute path). */` |
|  423801 | 10695 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|    3567 | 10696 | `		p++;` |
|    1781 | 10697 | `	}` |
|  423801 | 10698 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    1347 | 10699 | `		return;` |
|       - | 10700 | `	}` |
|       - | 10701 | `	/* Must be a single component: nothing follows the name token. */` |
|  422459 | 10702 | `	if( p + 1 != pEnd ){` |
|      32 | 10703 | `		return;` |
|       - | 10704 | `	}` |
|  422431 | 10705 | `	*pOut = p->sData;` |
|  211903 | 10706 |  |
|       - | 10707 | `/*` |
|       - | 10708 | ` * Generate bytecode for a given expression tree.` |
|       - | 10709 | ` * If something goes wrong while generating bytecode` |
|       - | 10710 | ` * for the expression tree (A very unlikely scenario)` |
|       - | 10711 | ` * this function takes care of generating the appropriate` |
|       - | 10712 | ` * error message.` |
|       - | 10713 | ` */` |
| 3496668 | 10714 | `static sxi32 GenStateEmitExprCode(` |
|       - | 10715 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10716 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - | 10717 | `	sxi32 iFlags /* Control flags */` |
|       - | 10718 | `	)` |
|       5 | 10719 |  |
|       - | 10720 | `	VmInstr *pInstr;` |
|       - | 10721 | `	sxu32 nJmpIdx;` |
| 3496673 | 10722 | `	sxi32 iP1 = 0;` |
| 3496673 | 10723 | `	sxu32 iP2 = 0;` |
| 3496673 | 10724 | `	void *p3  = 0;` |
|       - | 10725 | `	sxi32 iVmOp;` |
|       - | 10726 | `	sxi32 rc;` |
| 3496673 | 10727 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 3496673 | 10728 | `	sxu32 nRhsNsBase = 0;` |
| 3496673 | 10729 | `	if( pNode->xCode ){` |
|       - | 10730 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - | 10731 | `		/* Compile node */` |
| 2164109 | 10732 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 2164109 | 10733 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 2164109 | 10734 | `		RE_SWAP_DELIMITER(pGen);` |
| 2164109 | 10735 | `		return rc;` |
|       - | 10736 | `	}` |
| 1332569 | 10737 | `	if( pNode->pOp == 0 ){` |
|     ! 0 | 10738 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - | 10739 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 | 10740 | `		return SXERR_ABORT;` |
|       - | 10741 | `	}` |
| 1332569 | 10742 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1332569 | 10743 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      59 | 10744 | `		sxu32 nJmp = 0;` |
|       - | 10745 | `		sxu32 nNcNsBase;` |
|       - | 10746 | `		VmInstr *pInstrFix;` |
|       - | 10747 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - | 10748 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - | 10749 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - | 10750 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - | 10751 | `		 * stack slot carries a writable nIdx. */` |
|      59 | 10752 | `		if( pNode->pRight ){` |
|      59 | 10753 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10754 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      59 | 10755 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10756 | `				return rc;` |
|       - | 10757 | `			}` |
|      59 | 10758 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - | 10759 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - | 10760 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - | 10761 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - | 10762 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - | 10763 | `			 * the store, so the parent array does not need to be copied at` |
|       - | 10764 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - | 10765 | `			 * cascade for the actual write path stays correct. */` |
|      59 | 10766 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      59 | 10767 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      29 | 10768 | `				pInstrFix->iP2 = 3;` |
|      13 | 10769 | `			}` |
|      28 | 10770 | `		}` |
|       - | 10771 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      59 | 10772 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - | 10773 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      59 | 10774 | `		if( pNode->pLeft ){` |
|      59 | 10775 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      59 | 10776 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      59 | 10777 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10778 | `				return rc;` |
|       - | 10779 | `			}` |
|      59 | 10780 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      28 | 10781 | `		}` |
|       - | 10782 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      59 | 10783 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - | 10784 | `		/* Patch the short-circuit jump to land after the store. */` |
|      59 | 10785 | `		if( nJmp > 0 ){` |
|      59 | 10786 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      59 | 10787 | `			if( pInstrFix ){` |
|      59 | 10788 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      28 | 10789 | `			}` |
|      28 | 10790 | `		}` |
|      59 | 10791 | `		return SXRET_OK;` |
|       - | 10792 | `	}` |
| 1332513 | 10793 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - | 10794 | `		sxu32 nJz,nJmp;` |
|       - | 10795 | `		sxu32 nTernaryNsBase;` |
|       - | 10796 | `		/* Ternary operator require special handling */` |
|       - | 10797 | `		/* Phase#1: Compile the condition */` |
|    2651 | 10798 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 10799 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2651 | 10800 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 10801 | `			return rc;` |
|       - | 10802 | `		}` |
|       - | 10803 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - | 10804 | `		 * compiling the condition must short-circuit to the end of the` |
|       - | 10805 | `		 * condition expression, not leak past the ternary. */` |
|    2651 | 10806 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2651 | 10807 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2651 | 10808 | `		if( pNode->pLeft ){` |
|       - | 10809 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - | 10810 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2583 | 10811 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10812 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2583 | 10813 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2583 | 10814 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2583 | 10815 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10816 | `				return rc;` |
|       - | 10817 | `			}` |
|    2583 | 10818 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1294 | 10819 | `		}else{` |
|       - | 10820 | `			/* Elvis operator: (expr) ?: (else)` |
|       - | 10821 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - | 10822 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 | 10823 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 | 10824 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - | 10825 | `		}` |
|       - | 10826 | `		/* Phase#4: Emit the unconditional jump */` |
|    2651 | 10827 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - | 10828 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2651 | 10829 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2651 | 10830 | `		if( pInstr ){` |
|    2651 | 10831 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 10832 | `		}` |
|    2651 | 10833 | `		if( !pNode->pLeft ){` |
|       - | 10834 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 | 10835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 | 10836 | `		}` |
|       - | 10837 | `		/* Phase#6: Compile the 'else' expression */` |
|    2651 | 10838 | `		if( pNode->pRight ){` |
|    2651 | 10839 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2651 | 10840 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2651 | 10841 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 10842 | `				return rc;` |
|       - | 10843 | `			}` |
|    2651 | 10844 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1323 | 10845 | `		}` |
|    2651 | 10846 | `		if( nJmp > 0 ){` |
|       - | 10847 | `			/* Phase#7: Fix the unconditional jump */` |
|    2651 | 10848 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2651 | 10849 | `			if( pInstr ){` |
|    2651 | 10850 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1323 | 10851 | `			}` |
|    1323 | 10852 | `		}` |
|       - | 10853 | `		/* All done */` |
|    2651 | 10854 | `		return SXRET_OK;` |
|       - | 10855 | `	}` |
| 1329867 | 10856 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10857 | `	/* Generate code for the left tree */` |
| 1329867 | 10858 | `	if( pNode->pLeft ){` |
| 1329829 | 10859 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1329829 | 10860 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10861 | `			ph7_expr_node **apNode;` |
|  423921 | 10862 | `			int hasSpread = 0;` |
|  423921 | 10863 | `			int hasNamed = 0;` |
|  423921 | 10864 | `			int bAnySpread = 0;` |
|  423921 | 10865 | `			sxu32 byRefMask = 0;` |
|       - | 10866 | `			sxi32 nArgs;` |
|       - | 10867 | `			sxi32 n;` |
|       - | 10868 | `			/* Recurse and generate bytecodes for function arguments */` |
|  423921 | 10869 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  423921 | 10870 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10871 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10872 | `			{` |
|  423921 | 10873 | `				int seenNamed = 0;` |
|  839375 | 10874 | `				for( n = 0; n < nArgs; ++n ){` |
|  415461 | 10875 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     188 | 10876 | `						seenNamed = 1;` |
|     188 | 10877 | `						hasNamed = 1;` |
|  415369 | 10878 | `					}else if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      23 | 10879 | `						bAnySpread = 1;` |
|  415267 | 10880 | `					}else if( seenNamed ){` |
|       3 | 10881 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10882 | `							"Cannot use positional argument after named argument");` |
|       3 | 10883 | `						return SXERR_SYNTAX;` |
|       - | 10884 | `					}` |
|  207732 | 10885 | `				}` |
|       - | 10886 | `			}` |
|       - | 10887 | `			/* Read-only load */` |
|  423919 | 10888 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|       - | 10889 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|       - | 10890 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|       - | 10891 | `			 * objects dispatch to the right method (offsetExists for both;` |
|       - | 10892 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|  423919 | 10893 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|  423919 | 10894 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  423914 | 10895 | `				if( pCallName->nByte == 5` |
|  232423 | 10896 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|   21543 | 10897 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|  413150 | 10898 | `				}else if( pCallName->nByte == 5` |
|  210885 | 10899 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|      83 | 10900 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|      39 | 10901 | `				}` |
|       - | 10902 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|       - | 10903 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|       - | 10904 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|       - | 10905 | `				 * write back through. Skipped when spread/named args are present:` |
|       - | 10906 | `				 * the compile-time positional index no longer maps to the` |
|       - | 10907 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|  423919 | 10908 | `				if( !bAnySpread && !hasNamed ){` |
|       - | 10909 | `					SyString sBuiltin;` |
|  423801 | 10910 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|  423801 | 10911 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|  211898 | 10912 | `				}` |
|  211957 | 10913 | `			}` |
|  839371 | 10914 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  415457 | 10915 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  415457 | 10916 | `				sxi32 iArgFlags = iFlags & ~EXPR_FLAG_LOAD_IDX_STORE;` |
|       - | 10917 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|       - | 10918 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|       - | 10919 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|       - | 10920 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|       - | 10921 | `				 * builtin to write back through. A plain $var target is unaffected` |
|       - | 10922 | `				 * (iP1=0 either way). See PLAN.md §2 for the full rationale. */` |
|  415457 | 10923 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|      53 | 10924 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      53 | 10925 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      24 | 10926 | `				}` |
|  415457 | 10927 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  415457 | 10928 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10929 | `					return rc;` |
|       - | 10930 | `				}` |
|       - | 10931 | `				/* Each argument is an independent nullsafe scope. */` |
|  415457 | 10932 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  415457 | 10933 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10934 | `					/* Emit spread opcode to unpack this array argument */` |
|      23 | 10935 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      23 | 10936 | `					hasSpread = 1;` |
|      10 | 10937 | `				}` |
|  207731 | 10938 | `			}` |
|       - | 10939 | `			/* Total number of given arguments */` |
|  423919 | 10940 | `			iP1 = nArgs;` |
|  423919 | 10941 | `			iP2 = hasSpread;` |
|       - | 10942 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10943 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  423919 | 10944 | `			if( hasNamed ){` |
|     101 | 10945 | `				sxu32 nStrBytes = 0;` |
|       - | 10946 | `				char *zBuf;` |
|     297 | 10947 | `				for( n = 0; n < nArgs; ++n ){` |
|     199 | 10948 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10949 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10950 | `					}` |
|     101 | 10951 | `				}` |
|       - | 10952 | `				{` |
|     101 | 10953 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     101 | 10954 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10955 | `					&pGen->pVm->sAllocator, mapSize);` |
|     101 | 10956 | `				if( pMap ){` |
|     101 | 10957 | `					SyZero(pMap, mapSize);` |
|     101 | 10958 | `					pMap->bHasNamed = 1;` |
|     101 | 10959 | `					pMap->nTotal = (sxu32)nArgs;` |
|     101 | 10960 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     101 | 10961 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     297 | 10962 | `					for( n = 0; n < nArgs; ++n ){` |
|     199 | 10963 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     185 | 10964 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     185 | 10965 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     185 | 10966 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     185 | 10967 | `							zBuf += nb;` |
|      91 | 10968 | `						}` |
|       - | 10969 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     101 | 10970 | `					}` |
|     101 | 10971 | `					p3 = (void *)pMap;` |
|      49 | 10972 | `				}` |
|       - | 10973 | `				}` |
|      49 | 10974 | `			}` |
|       - | 10975 | `			/* Remove stale flags now */` |
|  423919 | 10976 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  211957 | 10977 | `		}` |
| 1329827 | 10978 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1329827 | 10979 | `		if( rc != SXRET_OK ){` |
|      34 | 10980 | `			return rc;` |
|       - | 10981 | `		}` |
| 1329797 | 10982 | `		if( !bIsChainOp ){` |
|       - | 10983 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10984 | `			 * target the end of that LHS chain, which is right here. */` |
|  620941 | 10985 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  310468 | 10986 | `		}` |
| 1329797 | 10987 | `		if( iVmOp == PH7_OP_CALL ){` |
|  423919 | 10988 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  423919 | 10989 | `			if( pInstr ){` |
|  423919 | 10990 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  422547 | 10991 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10992 | `					sxu32 nQual;` |
|  422547 | 10993 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10994 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10995 | `					 * so the later NEW handler (if any) can see it. */` |
|  422547 | 10996 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10997 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10998 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10999 | `					 * imports — class imports must NOT affect function` |
|       - | 11000 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 11001 | `					 * before NEW; we store the original literal index in the` |
|       - | 11002 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 11003 | `					 * the unqualified name and re-qualify with class imports. */` |
|  422547 | 11004 | `					if( bAbsolute ){` |
|    3567 | 11005 | `						pInstr->iP2 = (sxi32)nOrig;` |
|    1786 | 11006 | `					}else{` |
|  418985 | 11007 | `						int fromImport = 0;` |
|  418985 | 11008 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  418985 | 11009 | `						pInstr->iP2 = (sxi32)nQual;` |
|  418985 | 11010 | `						if( nQual != nOrig ){` |
|       - | 11011 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 11012 | `							 * NEW handler can recover the unqualified name. */` |
|      77 | 11013 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      77 | 11014 | `							if( !fromImport ){` |
|       - | 11015 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      67 | 11016 | `								if( p3 == 0 ){` |
|      67 | 11017 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 11018 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      67 | 11019 | `									if( pMap ){` |
|      67 | 11020 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      67 | 11021 | `										p3 = (void *)pMap;` |
|      31 | 11022 | `									}` |
|      31 | 11023 | `								}` |
|      67 | 11024 | `								if( p3 ){` |
|      67 | 11025 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 11026 | `								}` |
|      31 | 11027 | `							}` |
|      36 | 11028 | `						}` |
|       5 | 11029 | `					}` |
|  212648 | 11030 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 11031 | `					/* Method call,flag that */` |
|    1081 | 11032 | `					pInstr->iP2 = 1;` |
|     538 | 11033 | `				}` |
|  211962 | 11034 | `			}` |
| 1117840 | 11035 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 11036 | `			ph7_expr_node **apNode;` |
|       - | 11037 | `			sxi32 n;` |
|   91087 | 11038 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|       - | 11039 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|       - | 11040 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       - | 11041 | `			/* Recurse and generate bytecodes for array index */` |
|   91087 | 11042 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  164375 | 11043 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   73293 | 11044 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   73293 | 11045 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   73293 | 11046 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 11047 | `					return rc;` |
|       - | 11048 | `				}` |
|       - | 11049 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   73293 | 11050 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   36649 | 11051 | `			}` |
|   91087 | 11052 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   73293 | 11053 | `				iP1 = 1; /* Node have an index associated with it */` |
|   36644 | 11054 | `			}` |
|   91087 | 11055 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       - | 11056 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     243 | 11057 | `				iP2 = 4;` |
|   90968 | 11058 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       - | 11059 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|       - | 11060 | `				 * so the trailing unset() builtin can drop the slot. */` |
|      54 | 11061 | `				iP2 = 5;` |
|   90824 | 11062 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       - | 11063 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|       - | 11064 | `				 * short-circuit on missing keys without invoking offsetGet` |
|       - | 11065 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|      29 | 11066 | `				iP2 = 6;` |
|   90787 | 11067 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 11068 | `				/* Create an empty entry when the desired index is not found */` |
|   35889 | 11069 | `				iP2 = 1;` |
|   17947 | 11070 | `			}` |
|  860342 | 11071 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 11072 | `			/* POP the left node */` |
|      32 | 11073 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 11074 | `		}` |
|  664896 | 11075 | `	}` |
| 1329835 | 11076 | `	rc = SXRET_OK;` |
| 1329835 | 11077 | `	nJmpIdx = 0;` |
|       - | 11078 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 11079 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 11080 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1329835 | 11081 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     331 | 11082 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     331 | 11083 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     331 | 11084 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     331 | 11085 | `			int isSpecial = 0;` |
|     331 | 11086 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     243 | 11087 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     243 | 11088 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     253 | 11089 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     221 | 11090 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     112 | 11091 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      93 | 11092 | `					isSpecial = 1;` |
|      44 | 11093 | `				}` |
|     141 | 11094 | `			}` |
|     375 | 11095 | `			pInstr->iP1 = 0;` |
|     375 | 11096 | `			if( !isSpecial ){` |
|     199 | 11097 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      97 | 11098 | `			}` |
|       - | 11099 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 11100 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     287 | 11101 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     199 | 11102 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     199 | 11103 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      44 | 11104 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      46 | 11105 | `					return SXRET_OK;` |
|       - | 11106 | `				}` |
|      76 | 11107 | `			}` |
|     120 | 11108 | `		}` |
|     196 | 11109 | `	}` |
|       - | 11110 | `	/* Generate code for the right tree */` |
| 1329757 | 11111 | `	if( pNode->pRight ){` |
|  729953 | 11112 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 11113 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|   11115 | 11114 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  724398 | 11115 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 11116 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3723 | 11117 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  716984 | 11118 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 11119 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     129 | 11120 | `			iVmOp = 0; /* No binary operator to emit */` |
|     129 | 11121 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  715114 | 11122 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 11123 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 11124 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 11125 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 11126 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 11127 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 11128 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     105 | 11129 | `			sxu32 nNsJmp = 0;` |
|     105 | 11130 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     105 | 11131 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  714950 | 11132 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  296321 | 11133 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  148158 | 11134 | `		}` |
|  729953 | 11135 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  729953 | 11136 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  729953 | 11137 | `		if( !bIsChainOp ){` |
|       - | 11138 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 11139 | `			 * operator instruction is emitted. */` |
|  536135 | 11140 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  268065 | 11141 | `		}` |
|  729953 | 11142 | `		if( iVmOp == PH7_OP_STORE ){` |
|  292523 | 11143 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  292492 | 11144 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 11145 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 11146 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 11147 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 11148 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 11149 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 11150 | `				 */` |
|      74 | 11151 | `				iVmOp = 0;` |
|  292488 | 11152 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  292453 | 11153 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 11154 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   81871 | 11155 | `					iP2 = 1;` |
|   40938 | 11156 | `				}else{` |
|  210587 | 11157 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11158 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   35817 | 11159 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   35817 | 11160 | `						iP1 = pInstr->iP1;` |
|   17911 | 11161 | `					}else{` |
|  174775 | 11162 | `						p3 = pInstr->p3;` |
|       - | 11163 | `					}` |
|       - | 11164 | `					/* POP the last dynamic load instruction */` |
|  210587 | 11165 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 11166 | `				}` |
|  146229 | 11167 | `			}` |
|  583694 | 11168 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      54 | 11169 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      54 | 11170 | `			if( pInstr ){` |
|      54 | 11171 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 11172 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 11173 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 11174 | `					 */` |
|      17 | 11175 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      17 | 11176 | `					iP1 = pInstr->iP1;` |
|      17 | 11177 | `					iP2 = pInstr->iP2;` |
|      17 | 11178 | `					p3  = pInstr->p3;` |
|       9 | 11179 | `				}else{` |
|      38 | 11180 | `					p3 = pInstr->p3;` |
|       - | 11181 | `				}` |
|      26 | 11182 | `			}` |
|      26 | 11183 | `		}` |
|  364974 | 11184 | `	}` |
| 1329752 | 11185 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|   11415 | 11186 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|       - | 11187 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|       - | 11188 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|      29 | 11189 | `		iVmOp = 0;` |
|      13 | 11190 | `	}` |
| 1329757 | 11191 | `	if( iVmOp > 0 ){` |
| 1329507 | 11192 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   14557 | 11193 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 11194 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|   10645 | 11195 | `				iP1 = 1;` |
|    5325 | 11196 | `			}` |
| 1322231 | 11197 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 11198 | `			/* Namespace-qualify the class name for NEW */ {` |
|   22641 | 11199 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   22641 | 11200 | `				VmInstr *pCallInstr = 0;` |
|   22641 | 11201 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   22509 | 11202 | `					pCallInstr = pPeek;` |
|   22509 | 11203 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|   11252 | 11204 | `				}` |
|   22641 | 11205 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   22639 | 11206 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 11207 | `					sxu32 nLitForClass;` |
|       - | 11208 | `					/* If the CALL handler already qualified the name using` |
|       - | 11209 | `					 * function imports, recover the original unqualified` |
|       - | 11210 | `					 * literal so we can re-qualify with class imports. */` |
|   22639 | 11211 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      37 | 11212 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      21 | 11213 | `					}else{` |
|   22607 | 11214 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 11215 | `					}` |
|   22639 | 11216 | `					pPeek->iP1 = 0;` |
|   22639 | 11217 | `					if( !bAbsolute ){` |
|   19081 | 11218 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    9543 | 11219 | `					}else{` |
|    3563 | 11220 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 11221 | `					}` |
|   11317 | 11222 | `				}` |
|       - | 11223 | `			}` |
|   22641 | 11224 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   22641 | 11225 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 11226 | `				VmInstr *pPrev;` |
|   22509 | 11227 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   22509 | 11228 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 11229 | `					/* Pop the call instruction, preserve named-arg map */` |
|   22509 | 11230 | `					iP1 = pInstr->iP1;` |
|   22509 | 11231 | `					if( pInstr->p3 ){` |
|      43 | 11232 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      19 | 11233 | `					}` |
|   22509 | 11234 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|   11252 | 11235 | `				}` |
|   11257 | 11236 | `			}` |
| 1303637 | 11237 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 11238 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 11239 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     177 | 11240 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     177 | 11241 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     177 | 11242 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     177 | 11243 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     177 | 11244 | `				int isSpecialIs = 0;` |
|     177 | 11245 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     173 | 11246 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     173 | 11247 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     173 | 11248 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     168 | 11249 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      85 | 11250 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      12 | 11251 | `						isSpecialIs = 1;` |
|       5 | 11252 | `					}` |
|      85 | 11253 | `				}` |
|     179 | 11254 | `				pInstr->iP1 = 0;` |
|     179 | 11255 | `				if( !isSpecialIs && !bAbsolute ){` |
|     157 | 11256 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      76 | 11257 | `				}` |
|      90 | 11258 | `			}` |
| 1292236 | 11259 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 11260 | `			/* Prevent constant expansion for member/property names.` |
|       - | 11261 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 11262 | `			 * should not trigger constant lookup. */` |
|  193823 | 11263 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  193823 | 11264 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  193781 | 11265 | `				pInstr->iP1 = 0;` |
|   96888 | 11266 | `			}` |
|  193823 | 11267 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 11268 | `				/* Static member access,remember that */` |
|     253 | 11269 | `				iP1 = 1;` |
|     253 | 11270 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     253 | 11271 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      38 | 11272 | `					p3 = pInstr->p3;` |
|      38 | 11273 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      17 | 11274 | `				}` |
|     124 | 11275 | `			}` |
|   96909 | 11276 | `		}` |
|       - | 11277 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 11278 | `		 * This is the primary emit path for user-visible calls. */` |
| 1329505 | 11279 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  446555 | 11280 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  223275 | 11281 | `		}` |
|       - | 11282 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1329505 | 11283 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  664750 | 11284 | `	}` |
| 1329755 | 11285 | `	if( nJmpIdx > 0 ){` |
|       - | 11286 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   14957 | 11287 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   14957 | 11288 | `		if( pInstr ){` |
|   14957 | 11289 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    7476 | 11290 | `		}` |
|    7476 | 11291 | `	}` |
| 1329755 | 11292 | `	return rc;` |
| 1748320 | 11293 |  |
|       - | 11294 | `/*` |
|       - | 11295 | ` * Compile a PHP expression.` |
|       - | 11296 | ` * According to the PHP language reference manual:` |
|       - | 11297 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 11298 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 11299 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 11300 | ` *  is "anything that has a value".` |
|       - | 11301 | ` * If something goes wrong while compiling the expression,this` |
|       - | 11302 | ` * function takes care of generating the appropriate error` |
|       - | 11303 | ` * message.` |
|       - | 11304 | ` */` |
|  940330 | 11305 | `static sxi32 PH7_CompileExpr(` |
|       - | 11306 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11307 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 11308 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 11309 | `	)` |
|       5 | 11310 |  |
|       - | 11311 | `	ph7_expr_node *pRoot;` |
|       - | 11312 | `	SySet sExprNode;` |
|       - | 11313 | `	SyToken *pEnd;` |
|       - | 11314 | `	sxi32 nExpr;` |
|       - | 11315 | `	sxi32 iNest;` |
|       - | 11316 | `	sxi32 rc;` |
|       - | 11317 | `	sxu32 nNullsafeBase;` |
|       - | 11318 | `	/* Initialize worker variables */` |
|  940335 | 11319 | `	nExpr = 0;` |
|  940335 | 11320 | `	pRoot = 0;` |
|       - | 11321 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 11322 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  940335 | 11323 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  940335 | 11324 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  940335 | 11325 | `	SySetAlloc(&sExprNode,0x10);` |
|  940335 | 11326 | `	rc = SXRET_OK;` |
|       - | 11327 | `	/* Delimit the expression */` |
|  940335 | 11328 | `	pEnd = pGen->pIn;` |
|  940335 | 11329 | `	iNest = 0;` |
| 6292629 | 11330 | `	while( pEnd < pGen->pEnd ){` |
| 5973645 | 11331 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11332 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     489 | 11333 | `			iNest++;` |
| 5973403 | 11334 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     497 | 11335 | `			iNest--;` |
| 5972915 | 11336 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  621699 | 11337 | `			if( iNest <= 0 ){` |
|  621351 | 11338 | `				break;` |
|       - | 11339 | `			}` |
|     174 | 11340 | `		}` |
| 5352299 | 11341 | `		pEnd++;` |
|       5 | 11342 | `	}` |
|  940335 | 11343 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   21785 | 11344 | `		SyToken *pEnd2 = pGen->pIn;` |
|   21785 | 11345 | `		iNest = 0;` |
|       - | 11346 | `		/* Stop at the first comma */` |
|   43859 | 11347 | `		while( pEnd2 < pEnd ){` |
|   22085 | 11348 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      67 | 11349 | `				iNest++;` |
|   22054 | 11350 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      67 | 11351 | `				iNest--;` |
|   21992 | 11352 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      57 | 11353 | `				if( iNest <= 0 ){` |
|       7 | 11354 | `					break;` |
|       - | 11355 | `				}` |
|      23 | 11356 | `			}` |
|   22079 | 11357 | `			pEnd2++;` |
|       5 | 11358 | `		}` |
|   21785 | 11359 | `		if( pEnd2 <pEnd ){` |
|       7 | 11360 | `			pEnd = pEnd2;` |
|       3 | 11361 | `		}` |
|   10890 | 11362 | `	}` |
|  940335 | 11363 | `	if( pEnd > pGen->pIn ){` |
|  940325 | 11364 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 11365 | `		/* Swap delimiter */` |
|  940325 | 11366 | `		pGen->pEnd = pEnd;` |
|       - | 11367 | `		/* Try to get an expression tree */` |
|  940325 | 11368 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  940325 | 11369 | `		if( rc == SXRET_OK && pRoot ){` |
|  940143 | 11370 | `			rc = SXRET_OK;` |
|  940143 | 11371 | `			if( xTreeValidator ){` |
|       - | 11372 | `				/* Call the upper layer validator callback */` |
|   29137 | 11373 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   14566 | 11374 | `			}` |
|  940143 | 11375 | `			if( rc != SXERR_ABORT ){` |
|       - | 11376 | `				/* Generate code for the given tree */` |
|  940143 | 11377 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 11378 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 11379 | `				 * expression so they short-circuit to its end. */` |
|  940143 | 11380 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  470069 | 11381 | `			}` |
|  940143 | 11382 | `			nExpr = 1;` |
|  470069 | 11383 | `		}` |
|       - | 11384 | `		/* Release the whole tree */` |
|  940325 | 11385 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 11386 | `		/* Synchronize token stream */` |
|  940325 | 11387 | `		pGen->pEnd = pTmp;` |
|  940325 | 11388 | `		pGen->pIn  = pEnd;` |
|  940325 | 11389 | `		if( rc == SXERR_ABORT ){` |
|      12 | 11390 | `			SySetRelease(&sExprNode);` |
|      12 | 11391 | `			return SXERR_ABORT;` |
|       - | 11392 | `		}` |
|  470155 | 11393 | `	}` |
|  940325 | 11394 | `	SySetRelease(&sExprNode);` |
|  940325 | 11395 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  470170 | 11396 |  |
|       - | 11397 | `/*` |
|       - | 11398 | ` * Return a pointer to the node construct handler associated` |
|       - | 11399 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 11400 | ` */` |
|  240658 | 11401 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       5 | 11402 |  |
|  240663 | 11403 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 11404 | `		/* Numeric literal: Either real or integer */` |
|  124855 | 11405 | `		return PH7_CompileNumLiteral;` |
|  115813 | 11406 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 11407 | `		/* Double quoted string */` |
|   22925 | 11408 | `		return PH7_CompileString;` |
|   92893 | 11409 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 11410 | `		/* Single quoted string */` |
|   92777 | 11411 | `		return PH7_CompileSimpleString;` |
|     120 | 11412 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 11413 | `		/* Heredoc */` |
|      68 | 11414 | `		return PH7_CompileHereDoc;` |
|      56 | 11415 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 11416 | `		/* Nowdoc */` |
|      50 | 11417 | `		return PH7_CompileNowDoc;` |
|       8 | 11418 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 11419 | `		/* Backtick quoted string */` |
|       6 | 11420 | `		return PH7_CompileBacktic;` |
|       - | 11421 | `	}` |
|       3 | 11422 | `	return 0;` |
|  120334 | 11423 |  |
|       - | 11424 | `/*` |
|       - | 11425 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 11426 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 11427 | ` * in write context" parse error.` |
|       - | 11428 | ` */` |
|    6822 | 11429 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       5 | 11430 |  |
|       - | 11431 | `	sxi32 rc;` |
|    6827 | 11432 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6825 | 11433 | `		return SXRET_OK;` |
|       - | 11434 | `	}` |
|       5 | 11435 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 11436 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 11437 | `		"Can't use nullsafe operator in write context");` |
|       3 | 11438 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3416 | 11439 |  |
|       - | 11440 | `/*` |
|       - | 11441 | ` * Compile an unset() statement.` |
|       - | 11442 | ` * unset($var, $arr[$key], ...);` |
|       - | 11443 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 11444 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 11445 | ` * parent array before extracting the element to unset.` |
|       - | 11446 | ` */` |
|    2946 | 11447 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       5 | 11448 |  |
|    2951 | 11449 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2951 | 11450 | `	sxu32 nIdx = 0;` |
|       - | 11451 | `	SyString sName;` |
|       - | 11452 | `	sxi32 rc;` |
|       - | 11453 | `	/* Jump the 'unset' keyword */` |
|    2951 | 11454 | `	pGen->pIn++;` |
|       - | 11455 | `	/* Save delimiter */` |
|    2951 | 11456 | `	pTmp = pGen->pEnd;` |
|       - | 11457 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2951 | 11458 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2951 | 11459 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 11460 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 11461 | `		SyToken *pClose;` |
|    2951 | 11462 | `		pGen->pIn++;   /* Skip '(' */` |
|    2951 | 11463 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2951 | 11464 | `		pEnd = pClose; /* Stop at ')' */` |
|    1473 | 11465 | `	}` |
|    2951 | 11466 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 11467 | `	/* Resolve the 'unset' builtin name once */` |
|    2951 | 11468 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     363 | 11469 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     363 | 11470 | `		if( pObj == 0 ){` |
|     ! 0 | 11471 | `			return SXERR_ABORT;` |
|       - | 11472 | `		}` |
|     363 | 11473 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     363 | 11474 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     179 | 11475 | `	}` |
|       - | 11476 | `	/* Compile each comma-separated argument */` |
|    9775 | 11477 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6829 | 11478 | `		if( pGen->pIn < pNext ){` |
|    6829 | 11479 | `			pGen->pEnd = pNext;` |
|    6829 | 11480 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 11481 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|       - | 11482 | `				GenStateUnsetValidator);` |
|    6829 | 11483 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11484 | `				return SXERR_ABORT;` |
|       - | 11485 | `			}` |
|    6829 | 11486 | `			if( rc != SXERR_EMPTY ){` |
|       - | 11487 | `				/* Emit call for this single argument */` |
|    6827 | 11488 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6827 | 11489 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6827 | 11490 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3411 | 11491 | `			}` |
|    3412 | 11492 | `		}` |
|       - | 11493 | `		/* Jump trailing commas */` |
|   10709 | 11494 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3885 | 11495 | `			pNext++;` |
|       5 | 11496 | `		}` |
|    6829 | 11497 | `		pGen->pIn = pNext;` |
|       5 | 11498 | `	}` |
|       - | 11499 | `	/* Skip past the closing ')' if present */` |
|    2951 | 11500 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2951 | 11501 | `		pGen->pIn++;` |
|    1473 | 11502 | `	}` |
|       - | 11503 | `	/* Restore token stream */` |
|    2951 | 11504 | `	pGen->pEnd = pTmp;` |
|    2951 | 11505 | `	return SXRET_OK;` |
|    1478 | 11506 |  |
|       - | 11507 | `/*` |
|       - | 11508 | ` * PHP Language construct table.` |
|       - | 11509 | ` */` |
|       - | 11510 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 11511 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 11512 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 11513 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 11514 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 11515 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 11516 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 11517 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 11518 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 11519 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 11520 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 11521 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 11522 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 11523 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 11524 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 11525 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 11526 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 11527 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 11528 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 11529 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 11530 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 11531 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 11532 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 11533 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 11534 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 11535 | `};` |
|       - | 11536 | `/*` |
|       - | 11537 | ` * Return a pointer to the statement handler routine associated` |
|       - | 11538 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 11539 | ` */` |
|  635664 | 11540 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 11541 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 11542 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 11543 | `	)` |
|       5 | 11544 |  |
|  635669 | 11545 | `	sxu32 n = 0;` |
| 3295766 | 11546 | `	for(;;){` |
| 6591537 | 11547 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|  136051 | 11548 | `			break;` |
|       - | 11549 | `		}` |
| 6455491 | 11550 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  499623 | 11551 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 11552 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 11553 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 11554 | `					/* 'static' (class context),return null */` |
|     ! 0 | 11555 | `					return 0;` |
|       - | 11556 | `				}` |
|     ! 0 | 11557 | `			}` |
|  499618 | 11558 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       6 | 11559 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       8 | 11560 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 11561 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 11562 | `				return 0;` |
|       - | 11563 | `			}` |
|       - | 11564 | `			/* Return a pointer to the handler.` |
|       - | 11565 | `			*/` |
|  499623 | 11566 | `			return aLangConstruct[n].xConstruct;` |
|       - | 11567 | `		}` |
| 5955873 | 11568 | `		n++;` |
|       5 | 11569 | `	}` |
|  136051 | 11570 | `	if( pLookahed ){` |
|  136051 | 11571 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   39017 | 11572 | `			return PH7_CompileClassInterface;` |
|   97039 | 11573 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   96695 | 11574 | `			return PH7_CompileClass;` |
|     349 | 11575 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      65 | 11576 | `			return PH7_CompileTrait;` |
|       - | 11577 | `		}` |
|       - | 11578 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|       - | 11579 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|       - | 11580 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|       - | 11581 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     142 | 11582 | `	}` |
|       - | 11583 | `	/* Not a language construct */` |
|     289 | 11584 | `	return 0;` |
|  317837 | 11585 |  |
|       - | 11586 | `/*` |
|       - | 11587 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 11588 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 11589 | ` */` |
|     284 | 11590 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       5 | 11591 |  |
|       - | 11592 | `	int rc;` |
|     289 | 11593 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     289 | 11594 | `	if( rc == FALSE ){` |
|     174 | 11595 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     173 | 11596 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 11597 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 11598 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 11599 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 11600 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 11601 | `			*/` |
|       - | 11602 | `			){` |
|     171 | 11603 | `				rc = TRUE;` |
|      83 | 11604 | `		}` |
|      87 | 11605 | `	}` |
|     289 | 11606 | `	return rc;` |
|       5 | 11607 |  |
|       - | 11608 | `/*` |
|       - | 11609 | ` * Compile a PHP chunk.` |
|       - | 11610 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11611 | ` * takes care of generating the appropriate error message.` |
|       - | 11612 | ` */` |
|  760692 | 11613 | `static sxi32 GenStateCompileChunk(` |
|       - | 11614 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 11615 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 11616 | `	)` |
|       5 | 11617 |  |
|       - | 11618 | `	ProcLangConstruct xCons;` |
|       - | 11619 | `	sxi32 rc;` |
|  760697 | 11620 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  594436 | 11621 | `	for(;;){` |
|  974787 | 11622 | `		int bStmtIsDeclare = 0;` |
|  974787 | 11623 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 11624 | `			/* No more input to process */` |
|   14187 | 11625 | `			break;` |
|       - | 11626 | `		}` |
|       - | 11627 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 11628 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  960605 | 11629 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  639235 | 11630 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  639235 | 11631 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      45 | 11632 | `				bStmtIsDeclare = 1;` |
|      20 | 11633 | `			}` |
|  319615 | 11634 | `		}` |
|  960605 | 11635 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 11636 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 11637 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  214065 | 11638 | `			pGen->bStrictTypesLocked = 1;` |
|  107030 | 11639 | `		}` |
|  960605 | 11640 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 11641 | `			/* Compile block */` |
|      20 | 11642 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      20 | 11643 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 11644 | `				break;` |
|       - | 11645 | `			}` |
|      12 | 11646 | `		}else{` |
|  960589 | 11647 | `			xCons = 0;` |
|  960589 | 11648 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|       - | 11649 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|       - | 11650 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|       - | 11651 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|    3597 | 11652 | `				xCons = PH7_CompileClassModifiers;` |
|  958793 | 11653 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  635669 | 11654 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 11655 | `				/* Try to extract a language construct handler */` |
|  635669 | 11656 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  635669 | 11657 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 11658 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 11659 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 11660 | `						&pGen->pIn->sData);` |
|       9 | 11661 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 11662 | `						break;` |
|       - | 11663 | `					}` |
|       - | 11664 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 11665 | `					 * this erroneous statement.` |
|       - | 11666 | `					 */` |
|       9 | 11667 | `					xCons = PH7_ErrorRecover;` |
|       4 | 11668 | `				}` |
|  639165 | 11669 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   52675 | 11670 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 11671 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     117 | 11672 | `				xCons = PH7_CompileLabel;` |
|      56 | 11673 | `			}` |
|  960589 | 11674 | `			if( xCons == 0 ){` |
|       - | 11675 | `				/* Assume an expression an try to compile it */` |
|  321497 | 11676 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  321497 | 11677 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 11678 | `					/* Pop l-value */` |
|  321347 | 11679 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  160671 | 11680 | `				}` |
|  160751 | 11681 | `			}else{` |
|       - | 11682 | `				/* Go compile the sucker */` |
|  639097 | 11683 | `				rc = xCons(&(*pGen));` |
|       - | 11684 | `			}` |
|  960589 | 11685 | `			if( rc == SXERR_ABORT ){` |
|       - | 11686 | `				/* Request to abort compilation */` |
|      12 | 11687 | `				break;` |
|       - | 11688 | `			}` |
|       - | 11689 | `		}` |
|       - | 11690 | `		/* Ignore trailing semi-colons ';' */` |
| 1553065 | 11691 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  592475 | 11692 | `			pGen->pIn++;` |
|       5 | 11693 | `		}` |
|  960595 | 11694 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 11695 | `			/* Compile a single statement and return */` |
|  746505 | 11696 | `			break;` |
|       - | 11697 | `		}` |
|       - | 11698 | `		/* LOOP ONE */` |
|       - | 11699 | `		/* LOOP TWO */` |
|       - | 11700 | `		/* LOOP THREE */` |
|       - | 11701 | `		/* LOOP FOUR */` |
|       5 | 11702 | `	}` |
|       - | 11703 | `	/* Return compilation status */` |
|  760697 | 11704 | `	return rc;` |
|       5 | 11705 |  |
|       - | 11706 | `/*` |
|       - | 11707 | ` * Compile a Raw PHP chunk.` |
|       - | 11708 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 11709 | ` * takes care of generating the appropriate error message.` |
|       - | 11710 | ` */` |
|   14194 | 11711 | `static sxi32 PH7_CompilePHP(` |
|       - | 11712 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 11713 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 11714 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 11715 | `	)` |
|       5 | 11716 |  |
|   14199 | 11717 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 11718 | `	sxi32 rc;` |
|       - | 11719 | `	/* Reset the token set */` |
|   14199 | 11720 | `	SySetReset(&(*pTokenSet));` |
|       - | 11721 | `	/* Mark as the default token set */` |
|   14199 | 11722 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 11723 | `	/* Advance the stream cursor */` |
|   14199 | 11724 | `	pGen->pRawIn++;` |
|       - | 11725 | `	/* Tokenize the PHP chunk first */` |
|   14199 | 11726 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 11727 | `	/* Point to the head and tail of the token stream. */` |
|   14199 | 11728 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   14199 | 11729 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   14199 | 11730 | `	if( is_expr ){` |
|     ! 0 | 11731 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 11732 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 11733 | `			/* A simple expression,compile it */` |
|     ! 0 | 11734 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 11735 | `		}` |
|       - | 11736 | `		/* Emit the DONE instruction */` |
|     ! 0 | 11737 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 11738 | `		return SXRET_OK;` |
|       - | 11739 | `	}` |
|   14199 | 11740 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 11741 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 11742 | `		/*` |
|       - | 11743 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 11744 | `		 * According to the PHP reference manual:` |
|       - | 11745 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 11746 | `		 *  immediately follow` |
|       - | 11747 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 11748 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 11749 | `		 * Symisc extension:` |
|       - | 11750 | `		 *   This short syntax works with all PHP opening` |
|       - | 11751 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 11752 | `		 *   only short tag.` |
|       - | 11753 | `		 */` |
|       - | 11754 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 11755 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 11756 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 11757 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 11758 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 11759 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 11760 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 11761 | `		}` |
|       3 | 11762 | `		return SXRET_OK;` |
|       - | 11763 | `	}` |
|       - | 11764 | `	/* Compile the PHP chunk */` |
|   14197 | 11765 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 11766 | `	/* Fix exceptions jumps */` |
|   14197 | 11767 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 11768 | `	/* Fix gotos now, the jump destination is resolved */` |
|   14197 | 11769 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 11770 | `		rc = SXERR_ABORT;` |
|       1 | 11771 | `	}` |
|       - | 11772 | `	/* Reset container */` |
|   14197 | 11773 | `	SySetReset(&pGen->aGoto);` |
|   14197 | 11774 | `	SySetReset(&pGen->aLabel);` |
|   14197 | 11775 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 11776 | `	/* Compilation result */` |
|   14197 | 11777 | `	return rc;` |
|    7102 | 11778 |  |
|       - | 11779 | `/*` |
|       - | 11780 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 11781 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 11782 | ` * This is the only compile interface exported from this file.` |
|       - | 11783 | ` */` |
|   17122 | 11784 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 11785 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 11786 | `	SyString *pScript,  /* Script to compile */` |
|       - | 11787 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 11788 | `	)` |
|       5 | 11789 |  |
|       - | 11790 | `	SySet aPhpToken,aRawToken;` |
|       - | 11791 | `	ph7_gen_state *pCodeGen;` |
|       - | 11792 | `	ph7_value *pRawObj;` |
|       - | 11793 | `	sxu32 nObjIdx;` |
|       - | 11794 | `	sxi32 nRawObj;` |
|       - | 11795 | `	int is_expr;` |
|       - | 11796 | `	sxi8 bSavedStrict;` |
|       - | 11797 | `	sxi8 bSavedStrictLocked;` |
|       - | 11798 | `	sxi32 rc;` |
|   17127 | 11799 | `	if( pScript->nByte < 1 ){` |
|       - | 11800 | `		/* Nothing to compile */` |
|     ! 0 | 11801 | `		return PH7_OK;` |
|       - | 11802 | `	}` |
|       - | 11803 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 11804 | `	 * file's flags so include/require restore them on return. */` |
|   17127 | 11805 | `	pCodeGen = &pVm->sCodeGen;` |
|   17127 | 11806 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   17127 | 11807 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   17127 | 11808 | `	pCodeGen->bStrictTypes = 0;` |
|   17127 | 11809 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 11810 | `	/* Initialize the tokens containers */` |
|   17127 | 11811 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17127 | 11812 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   17127 | 11813 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   17127 | 11814 | `	is_expr = 0;` |
|   17127 | 11815 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 11816 | `		SyToken sTmp;` |
|       - | 11817 | `		/* PHP only: -*/` |
|    3613 | 11818 | `		sTmp.nLine = 1;` |
|    3613 | 11819 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3613 | 11820 | `		sTmp.pUserData = 0;` |
|    3613 | 11821 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3613 | 11822 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3613 | 11823 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 11824 | `			/* A simple PHP expression */` |
|     ! 0 | 11825 | `			is_expr = 1;` |
|     ! 0 | 11826 | `		}` |
|    1809 | 11827 | `	}else{` |
|       - | 11828 | `		/* Tokenize raw text */` |
|   13519 | 11829 | `		SySetAlloc(&aRawToken,32);` |
|   13519 | 11830 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 11831 | `	}` |
|       - | 11832 | `	/* Process high-level tokens */` |
|   17127 | 11833 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   17127 | 11834 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   17127 | 11835 | `	rc = PH7_OK;` |
|   17127 | 11836 | `	if( is_expr ){` |
|       - | 11837 | `		/* Compile the expression */` |
|     ! 0 | 11838 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 11839 | `		goto cleanup;` |
|       - | 11840 | `	}` |
|   17127 | 11841 | `	nObjIdx = 0;` |
|       - | 11842 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 11843 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 11844 | `	 * preventing namespace bleeding across include()d files. */` |
|   17127 | 11845 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 11846 | `	/* Start the compilation process */` |
|   15324 | 11847 | `	for(;;){` |
|   44835 | 11848 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   17115 | 11849 | `			break; /* No more tokens to process */` |
|       - | 11850 | `		}` |
|   27725 | 11851 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 11852 | `			/* Compile the PHP chunk */` |
|   14199 | 11853 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   14199 | 11854 | `			if( rc == SXERR_ABORT ){` |
|      15 | 11855 | `				break;` |
|       - | 11856 | `			}` |
|   14187 | 11857 | `			continue;` |
|       - | 11858 | `		}` |
|       - | 11859 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   13531 | 11860 | `		nRawObj = 0;` |
|   27099 | 11861 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 11862 | `			/* Consume the raw chunk without any processing */` |
|   13573 | 11863 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   13573 | 11864 | `			if( pRawObj == 0 ){` |
|     ! 0 | 11865 | `				rc = SXERR_MEM;` |
|     ! 0 | 11866 | `				break;` |
|       - | 11867 | `			}` |
|       - | 11868 | `			/* Mark as constant and emit the load constant instruction */` |
|   13573 | 11869 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   13573 | 11870 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   13573 | 11871 | `			++nRawObj;` |
|   13573 | 11872 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       5 | 11873 | `		}` |
|   13531 | 11874 | `		if( nRawObj > 0 ){` |
|       - | 11875 | `			/* Emit the consume instruction */` |
|   13531 | 11876 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6763 | 11877 | `		}` |
|    8566 | 11878 | `	}` |
|    8561 | 11879 | `cleanup:` |
|   17127 | 11880 | `	SySetRelease(&aRawToken);` |
|   17127 | 11881 | `	SySetRelease(&aPhpToken);` |
|       - | 11882 | `	/* Restore outer file's strict_types scope */` |
|   17127 | 11883 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   17127 | 11884 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   17127 | 11885 | `	return rc;` |
|    8566 | 11886 |  |
|       - | 11887 | `/*` |
|       - | 11888 | ` * Utility routines.Initialize the code generator.` |
|       - | 11889 | ` */` |
|    3540 | 11890 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 11891 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11892 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11893 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11894 | `	)` |
|       5 | 11895 |  |
|    3545 | 11896 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11897 | `	/* Zero the structure */` |
|    3545 | 11898 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 11899 | `	/* Initial state */` |
|    3545 | 11900 | `	pGen->pVm  = &(*pVm);` |
|    3545 | 11901 | `	pGen->xErr = xErr;` |
|    3545 | 11902 | `	pGen->pErrData = pErrData;` |
|    3545 | 11903 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3545 | 11904 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3545 | 11905 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3545 | 11906 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3545 | 11907 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11908 | `	/* Error log buffer */` |
|    3545 | 11909 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11910 | `	/* General purpose working buffer */` |
|    3545 | 11911 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11912 | `	/* Namespace state */` |
|    3545 | 11913 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3545 | 11914 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3545 | 11915 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3545 | 11916 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11917 | `	/* Create the global scope */` |
|    3545 | 11918 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11919 | `	/* Point to the global scope */` |
|    3545 | 11920 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3545 | 11921 | `	return SXRET_OK;` |
|       5 | 11922 |  |
|       - | 11923 | `/*` |
|       - | 11924 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11925 | ` */` |
|   20316 | 11926 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11927 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11928 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11929 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11930 | `	)` |
|       5 | 11931 |  |
|   20321 | 11932 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11933 | `	GenBlock *pBlock,*pParent;` |
|       - | 11934 | `	/* Reset state */` |
|   20321 | 11935 | `	SySetReset(&pGen->aLabel);` |
|   20321 | 11936 | `	SySetReset(&pGen->aGoto);` |
|   20321 | 11937 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   20321 | 11938 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   20321 | 11939 | `	SyBlobRelease(&pGen->sWorker);` |
|   20321 | 11940 | `	SyBlobRelease(&pGen->sNamespace);` |
|   20321 | 11941 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   20321 | 11942 | `	SyHashRelease(&pGen->hUseImports);` |
|   20321 | 11943 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   20321 | 11944 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   20321 | 11945 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   20321 | 11946 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   20321 | 11947 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11948 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11949 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11950 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11951 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11952 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11953 | `	 * number of unique names, which is acceptable. */` |
|       - | 11954 | `	/* Point to the global scope */` |
|   20321 | 11955 | `	pBlock = pGen->pCurrent;` |
|   20321 | 11956 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11957 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11958 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11959 | `		pBlock = pParent;` |
|     ! 0 | 11960 | `	}` |
|   20321 | 11961 | `	pGen->xErr = xErr;` |
|   20321 | 11962 | `	pGen->pErrData = pErrData;` |
|   20321 | 11963 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   20321 | 11964 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   20321 | 11965 | `	pGen->pIn = pGen->pEnd = 0;` |
|   20321 | 11966 | `	pGen->nErr = 0;` |
|   20321 | 11967 | `	return SXRET_OK;` |
|       5 | 11968 |  |
|       - | 11969 | `/*` |
|       - | 11970 | ` * Generate a compile-time error message.` |
|       - | 11971 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11972 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11973 | ` * abort compilation immediately.` |
|       - | 11974 | ` */` |
|     610 | 11975 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       5 | 11976 |  |
|     615 | 11977 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     615 | 11978 | `	const char *zErr = "Error";` |
|       - | 11979 | `	SyString *pFile;` |
|       - | 11980 | `	va_list ap;` |
|       - | 11981 | `	sxi32 rc;` |
|       - | 11982 | `	/* Reset the working buffer */` |
|     615 | 11983 | `	SyBlobReset(pWorker);` |
|       - | 11984 | `	/* Peek the processed file path if available */` |
|     615 | 11985 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     615 | 11986 | `	if( nErrType == E_ERROR ){` |
|       - | 11987 | `		/* Increment the error counter */` |
|     507 | 11988 | `		pGen->nErr++;` |
|     507 | 11989 | `		if( pGen->nErr > 15 ){` |
|       - | 11990 | `			/* Error count limit reached */` |
|       6 | 11991 | `			if( pGen->xErr ){` |
|       6 | 11992 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       6 | 11993 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       6 | 11994 | `				if( pFile ){` |
|       6 | 11995 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11996 | `				}` |
|       6 | 11997 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       6 | 11998 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       6 | 11999 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 12000 | `				}` |
|       2 | 12001 | `			}` |
|       - | 12002 | `			/* Abort immediately */` |
|       6 | 12003 | `			return SXERR_ABORT;` |
|       - | 12004 | `		}` |
|     249 | 12005 | `	}` |
|     611 | 12006 | `	if( pGen->xErr == 0 ){` |
|       - | 12007 | `		/* No available error consumer,return immediately */` |
|       3 | 12008 | `		return SXRET_OK;` |
|       - | 12009 | `	}` |
|     608 | 12010 | `	switch(nErrType){` |
|     500 | 12011 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      30 | 12012 | `	case E_WARNING: zErr = "Warning";     break;` |
|      78 | 12013 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      11 | 12014 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 12015 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 12016 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 12017 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 12018 | `	default:` |
|     ! 0 | 12019 | `		break;` |
|       - | 12020 | `	}` |
|     608 | 12021 | `	rc = SXRET_OK;` |
|       - | 12022 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     608 | 12023 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     608 | 12024 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     608 | 12025 | `	va_start(ap,zFormat);` |
|     608 | 12026 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     608 | 12027 | `	va_end(ap);` |
|     608 | 12028 | `	if( pFile ){` |
|     608 | 12029 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     302 | 12030 | `	}` |
|       - | 12031 | `	/* Append a new line */` |
|     608 | 12032 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     608 | 12033 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 12034 | `		/* Consume the generated error message */` |
|     608 | 12035 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     302 | 12036 | `	}` |
|     608 | 12037 | `	return rc;` |
|     310 | 12038 |  |
|       - | 12039 |  |
