# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5147/6494 lines (79.26%)

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
|    3280 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3282 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    9267 |   131 | `	for(;;){` |
|   18536 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    3174 |   133 | `			iCount--; /* Decrement nesting level */` |
|    3174 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    3148 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   15390 |   140 | `		pBlock = pBlock->pParent;` |
|   15390 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1642 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  708808 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  708810 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  708810 |   162 | `	pBlock->pUserData   = pUserData;` |
|  708810 |   163 | `	pBlock->pGen        = pGen;` |
|  708810 |   164 | `	pBlock->iFlags      = iType;` |
|  708810 |   165 | `	pBlock->pParent     = 0;` |
|  708810 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  708810 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  708810 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  705802 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  705804 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  705804 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  705804 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  705804 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  705804 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  705804 |   200 | `	pGen->pCurrent = pBlock;` |
|  705804 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  342806 |   203 | `		*ppBlock = pBlock;` |
|  171402 |   204 | `	}` |
|  705804 |   205 | `	return SXRET_OK;` |
|  352903 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  705794 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  705796 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  705796 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  705796 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  705794 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  705796 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  705796 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  705796 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  705796 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  705794 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  705796 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  705796 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  705796 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  705796 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  705796 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  705796 |   244 | `	return SXRET_OK;` |
|  352899 |   245 |  |
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
|  200602 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  200604 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  200604 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  200604 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  200604 |   265 | `	return rc;` |
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
|  494036 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  494038 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  889496 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  395460 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  157992 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  237470 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   36870 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  200602 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  200602 |   298 | `		if( pInstr ){` |
|  200602 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  200602 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  200602 |   302 | `			aFix[n].nJumpType = -1;` |
|  100300 |   303 | `		}` |
|  100302 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  494038 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  200660 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  200662 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  200808 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  200660 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  200792 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  200660 |   358 | `	return SXRET_OK;` |
|  100332 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  634362 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  634364 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  634364 |   367 | `	if( pEntry == 0 ){` |
|  275644 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  358722 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  358722 |   371 | `	return SXRET_OK;` |
|  317183 |   372 |  |
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
|  275642 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  275644 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  275644 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  137821 |   387 | `	}` |
|  275644 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|  105608 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|  105610 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|  105610 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  105610 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|  105610 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|  105610 |   408 | `	return pObj;` |
|   52806 |   409 |  |
|       - |   410 | `/*` |
|       - |   411 | ` * Implementation of the PHP language constructs.` |
|       - |   412 | ` */` |
|       - |   413 | `/*` |
|       - |   414 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|       - |   415 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|       - |   416 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|       - |   417 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|       - |   418 | ` *` |
|       - |   419 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|       - |   420 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|       - |   421 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|       - |   422 | ` * surrounding callsites' zero-check fallback pattern.` |
|       - |   423 | ` */` |
|  379314 |   424 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       2 |   425 |  |
|       - |   426 | `	VmCallArgMap *pMap;` |
|  379316 |   427 | `	if( !pGen->bStrictTypes ) return p3;` |
|      28 |   428 | `	if( p3 == 0 ){` |
|      28 |   429 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      28 |   430 | `		if( pMap == 0 ) return 0;` |
|      28 |   431 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      28 |   432 | `		p3 = (void *)pMap;` |
|      13 |   433 | `	}` |
|      28 |   434 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      28 |   435 | `	return p3;` |
|  189659 |   436 |  |
|       - |   437 | `/* Forward declaration */` |
|       - |   438 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |   439 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
|       - |   440 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn);` |
|       - |   441 | `/* Forward decl: union type parser is defined later in this file. */` |
|       - |   442 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |   443 | `	ph7_gen_state *pGen,` |
|       - |   444 | `	sxu32 *pnType,` |
|       - |   445 | `	SyString *pClass,` |
|       - |   446 | `	SySet *pAlts,` |
|       - |   447 | `	sxi32 *piTypeFlags,` |
|       - |   448 | `	SyString *pTypeText,` |
|       - |   449 | `	int iNullableFlag,` |
|       - |   450 | `	int iUnionFlag,` |
|       - |   451 | `	int bAllowVoid,` |
|       - |   452 | `	sxu32 nLine` |
|       - |   453 | `);` |
|       - |   454 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc);` |
|       - |   455 | `static const char * TokenTypeName(sxu32 nType);` |
|       - |   456 | `/*` |
|       - |   457 | ` * Stack-scratch size for stripping PHP 7.4 numeric separators. A typical` |
|       - |   458 | ` * literal (INT64_MAX decimal is 19 digits, binary 64-bit with per-nibble` |
|       - |   459 | ` * separators is ~80 chars) fits comfortably, so the fast path never touches` |
|       - |   460 | ` * the heap. The language itself imposes no upper bound on the length of a` |
|       - |   461 | ` * well-formed literal — the stripper falls back to a VM-allocator buffer` |
|       - |   462 | ` * for anything larger, so correctness is preserved even for pathological` |
|       - |   463 | ` * inputs like a thousand-digit number.` |
|       - |   464 | ` */` |
|       - |   465 | `#define GEN_NUM_SCRATCH 128` |
|       - |   466 | `/*` |
|       - |   467 | ` * Return TRUE if c is a valid digit for the given numeric base.` |
|       - |   468 | ` *   base 16 => SyisHex (0-9, a-f, A-F)` |
|       - |   469 | ` *   base  2 => 0 or 1` |
|       - |   470 | ` *   base 10 => SyisDigit (0-9, also used for octal literals which share the` |
|       - |   471 | ` *              decimal scan in the lexer)` |
|       - |   472 | ` */` |
|    1076 |   473 | `static int GenStateIsBaseDigit(int c, int base)` |
|       2 |   474 |  |
|    1078 |   475 | `	if( base == 16 ){ return SyisHex(c); }` |
|     980 |   476 | `	if( base == 2 ){ return c == '0' \|\| c == '1'; }` |
|     702 |   477 | `	return SyisDigit(c);` |
|     540 |   478 |  |
|       - |   479 | `/*` |
|       - |   480 | ` * Given the raw text of a numeric literal token, locate a misplaced PHP 7.4` |
|       - |   481 | ` * underscore separator so the caller can report the malformed portion with` |
|       - |   482 | ` * the exact wording PHP uses:` |
|       - |   483 | ` *` |
|       - |   484 | ` *   syntax error, unexpected identifier "X"` |
|       - |   485 | ` *` |
|       - |   486 | ` * The lexer guarantees that every underscore it consumed as a separator is` |
|       - |   487 | ` * surrounded by valid base digits; anything else sits in the trailing run` |
|       - |   488 | ` * absorbed by the lexer specifically to let this validator see and report` |
|       - |   489 | ` * it. That invariant means the malformed span is exactly [bad .. nByte) —` |
|       - |   490 | ` * no forward rescan needed.` |
|       - |   491 | ` *` |
|       - |   492 | ` * Returns 1 and fills pBadStart / pBadLen when the literal is malformed;` |
|       - |   493 | ` * returns 0 when it is well-formed.` |
|       - |   494 | ` */` |
|  106158 |   495 | `static int GenStateFindBadNumericSeparator(` |
|       - |   496 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   497 |  |
|  106160 |   498 | `	const char *z = pRaw->zString;` |
|  106160 |   499 | `	sxu32 n = pRaw->nByte;` |
|  106160 |   500 | `	int base = 10;` |
|       - |   501 | `	sxu32 i, start;` |
|  106160 |   502 | `	if( n < 2 ) return 0;` |
|    8960 |   503 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   504 | `		base = 16;` |
|    8925 |   505 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   506 | `		base = 2;` |
|     139 |   507 | `	}` |
|   32924 |   508 | `	for( i = 0; i < n; ++i ){` |
|   23980 |   509 | `		if( z[i] != '_' ) continue;` |
|     814 |   510 | `		if( i > 0 && i + 1 < n` |
|     543 |   511 | `			&& GenStateIsBaseDigit((unsigned char)z[i-1], base)` |
|     540 |   512 | `			&& GenStateIsBaseDigit((unsigned char)z[i+1], base) ){` |
|     533 |   513 | `			continue; /* well-placed separator */` |
|       - |   514 | `		}` |
|       - |   515 | `		/* First misplaced underscore — the lexer already absorbed the full` |
|       - |   516 | `		 * malformed tail, so it runs from here to the end of the token. */` |
|      15 |   517 | `		start = i;` |
|      20 |   518 | `		if( start > 0 && (z[start-1] == 'x' \|\| z[start-1] == 'X'` |
|      12 |   519 | `			\|\| z[start-1] == 'b' \|\| z[start-1] == 'B') ){` |
|       5 |   520 | `			start--; /* include the base letter for 0x_... / 0b_... */` |
|       2 |   521 | `		}` |
|      15 |   522 | `		*pBadStart = &z[start];` |
|      15 |   523 | `		*pBadLen = n - start;` |
|      15 |   524 | `		return 1;` |
|     ! 0 |   525 | `	}` |
|    8946 |   526 | `	return 0;` |
|   53081 |   527 |  |
|       - |   528 | `/*` |
|       - |   529 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   530 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   531 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   532 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   533 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   534 | ` * so callers can bail from the current construct).` |
|       - |   535 | ` */` |
|  106158 |   536 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   537 |  |
|  106160 |   538 | `	const char *zBad = 0;` |
|  106160 |   539 | `	sxu32 nBad = 0;` |
|       - |   540 | `	SyString sBad;` |
|       - |   541 | `	sxi32 rc;` |
|  106160 |   542 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  106146 |   543 | `		return SXRET_OK;` |
|       - |   544 | `	}` |
|      15 |   545 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   546 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   547 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   548 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   549 | `		return SXERR_ABORT;` |
|       - |   550 | `	}` |
|      15 |   551 | `	return SXERR_SYNTAX;` |
|   53081 |   552 |  |
|       - |   553 | `/*` |
|       - |   554 | ` * Strip PHP 7.4 numeric literal separators (underscores between digits) from` |
|       - |   555 | ` * a numeric token's text and yield a SyString suitable for the low-level` |
|       - |   556 | ` * converters (SyStrToInt64 / SyStrToReal / etc.).` |
|       - |   557 | ` *` |
|       - |   558 | ` * Fast path: if the token contains no '_', *pOut aliases pToken with no copy` |
|       - |   559 | ` * and *pzAlloc is set to NULL.` |
|       - |   560 | ` * Stack path: if the cleaned bytes fit in zScratch, they are written there` |
|       - |   561 | ` * and *pzAlloc is set to NULL.` |
|       - |   562 | ` * Heap path: for literals larger than the scratch buffer, a fresh buffer is` |
|       - |   563 | ` * allocated from pAlloc, returned via *pzAlloc, and must be released by the` |
|       - |   564 | ` * caller with SyMemBackendFree once the converter is done.` |
|       - |   565 | ` *` |
|       - |   566 | ` * Returns SXRET_OK on success, SXERR_ABORT on allocator failure (in which` |
|       - |   567 | ` * case *pOut is left untouched and the caller must not read it).` |
|       - |   568 | ` */` |
|  106144 |   569 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   570 | `	SyMemBackend *pAlloc,` |
|       - |   571 | `	const SyString *pToken,` |
|       - |   572 | `	char *zScratch, sxu32 nScratch,` |
|       - |   573 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   574 |  |
|       - |   575 | `	sxu32 i, j;` |
|  106146 |   576 | `	int hasUnderscore = 0;` |
|       - |   577 | `	char *zBuf;` |
|  106146 |   578 | `	*pzAlloc = 0;` |
|  225244 |   579 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  119352 |   580 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   59551 |   581 | `	}` |
|  106146 |   582 | `	if( !hasUnderscore ){` |
|  105894 |   583 | `		SyStringDupPtr(pOut, pToken);` |
|  105894 |   584 | `		return SXRET_OK;` |
|       - |   585 | `	}` |
|     253 |   586 | `	if( pToken->nByte <= nScratch ){` |
|     251 |   587 | `		zBuf = zScratch;` |
|     126 |   588 | `	}else{` |
|       3 |   589 | `		zBuf = (char *)SyMemBackendAlloc(pAlloc, pToken->nByte);` |
|       3 |   590 | `		if( zBuf == 0 ){` |
|     ! 0 |   591 | `			return SXERR_ABORT;` |
|       - |   592 | `		}` |
|       3 |   593 | `		*pzAlloc = zBuf;` |
|       - |   594 | `	}` |
|     253 |   595 | `	j = 0;` |
|    2895 |   596 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|    2643 |   597 | `		if( pToken->zString[i] != '_' ){ zBuf[j++] = pToken->zString[i]; }` |
|    1322 |   598 | `	}` |
|     253 |   599 | `	SyStringInitFromBuf(pOut, zBuf, j);` |
|     253 |   600 | `	return SXRET_OK;` |
|   53074 |   601 |  |
|       - |   602 | `/*` |
|       - |   603 | ` * Compile a numeric [i.e: integer or real] literal.` |
|       - |   604 | ` * Notes on the integer type.` |
|       - |   605 | ` *  According to the PHP language reference manual` |
|       - |   606 | ` *  Integers can be specified in decimal (base 10), hexadecimal (base 16), octal (base 8)` |
|       - |   607 | ` *  or binary (base 2) notation, optionally preceded by a sign (- or +).` |
|       - |   608 | ` *  To use octal notation, precede the number with a 0 (zero). To use hexadecimal` |
|       - |   609 | ` *  notation precede the number with 0x. To use binary notation precede the number with 0b.` |
|       - |   610 | ` * Symisc eXtension to the integer type.` |
|       - |   611 | ` *  PH7 introduced platform-independant 64-bit integer unlike the standard PHP engine` |
|       - |   612 | ` *  where the size of an integer is platform-dependent.That is,the size of an integer` |
|       - |   613 | ` *  is 8 bytes and the maximum integer size is 0x7FFFFFFFFFFFFFFF for all platforms` |
|       - |   614 | ` *  [i.e: either 32bit or 64bit].` |
|       - |   615 | ` *  For more information on this powerfull extension please refer to the official` |
|       - |   616 | ` *  documentation.` |
|       - |   617 | ` */` |
|  106130 |   618 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   619 |  |
|  106132 |   620 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  106132 |   621 | `	sxu32 nIdx = 0;` |
|       - |   622 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  106132 |   623 | `	char *zAlloc = 0;` |
|       - |   624 | `	SyString sNum;` |
|       - |   625 | `	sxi32 rc;` |
|   53065 |   626 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  106132 |   627 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  106132 |   628 | `	if( rc != SXRET_OK ){` |
|      11 |   629 | `		return rc;` |
|       - |   630 | `	}` |
|  159182 |   631 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   53060 |   632 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  106122 |   633 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   634 | `		return SXERR_ABORT;` |
|       - |   635 | `	}` |
|  106122 |   636 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   637 | `		ph7_value *pObj;` |
|       - |   638 | `		sxi64 iValue;` |
|  105610 |   639 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  105610 |   640 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  105610 |   641 | `		if( pObj == 0 ){` |
|     ! 0 |   642 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   643 | `			return SXERR_ABORT;` |
|       - |   644 | `		}` |
|  105610 |   645 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   52806 |   646 | `	}else{` |
|       - |   647 | `		/* Real number */` |
|       - |   648 | `		ph7_value *pObj;` |
|       - |   649 | `		/* Reserve a new constant */` |
|     514 |   650 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     514 |   651 | `		if( pObj == 0 ){` |
|     ! 0 |   652 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   653 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   654 | `			return SXERR_ABORT;` |
|       - |   655 | `		}` |
|     514 |   656 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     514 |   657 | `		PH7_MemObjToReal(pObj);` |
|       - |   658 | `	}` |
|  106122 |   659 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   660 | `	/* Emit the load constant instruction */` |
|  106122 |   661 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   662 | `	/* Node successfully compiled */` |
|  106122 |   663 | `	return SXRET_OK;` |
|   53067 |   664 |  |
|       - |   665 | `/*` |
|       - |   666 | ` * Compile a single quoted string.` |
|       - |   667 | ` * According to the PHP language reference manual:` |
|       - |   668 | ` *` |
|       - |   669 | ` *   The simplest way to specify a string is to enclose it in single quotes (the character ' ).` |
|       - |   670 | ` *   To specify a literal single quote, escape it with a backslash (\). To specify a literal` |
|       - |   671 | ` *   backslash, double it (\\). All other instances of backslash will be treated as a literal` |
|       - |   672 | ` *   backslash: this means that the other escape sequences you might be used to, such as \r` |
|       - |   673 | ` *   or \n, will be output literally as specified rather than having any special meaning.` |
|       - |   674 | ` *` |
|       - |   675 | ` */` |
|   76436 |   676 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   677 |  |
|   76438 |   678 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   679 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   680 | `	ph7_value *pObj;` |
|       - |   681 | `	sxu32 nIdx;` |
|   76438 |   682 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   683 | `	/* Delimit the string */` |
|   76438 |   684 | `	zIn  = pStr->zString;` |
|   76438 |   685 | `	zEnd = &zIn[pStr->nByte];` |
|   76438 |   686 | `	if( zIn >= zEnd ){` |
|       - |   687 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   688 | `		 * rather than reserving a new object each time. */` |
|    6156 |   689 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    6156 |   690 | `		return SXRET_OK;` |
|       - |   691 | `	}` |
|   70284 |   692 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   693 | `		/* Already processed,emit the load constant instruction` |
|       - |   694 | `		 * and return.` |
|       - |   695 | `		 */` |
|   28008 |   696 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   28008 |   697 | `		return SXRET_OK;` |
|       - |   698 | `	}` |
|       - |   699 | `	/* Reserve a new constant */` |
|   42278 |   700 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   42278 |   701 | `	if( pObj == 0 ){` |
|     ! 0 |   702 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   703 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   704 | `		return SXERR_ABORT;` |
|       - |   705 | `	}` |
|   42278 |   706 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   707 | `	/* Compile the node */` |
|   42318 |   708 | `	for(;;){` |
|   84638 |   709 | `		if( zIn >= zEnd ){` |
|       - |   710 | `			/* End of input */` |
|   42278 |   711 | `			break;` |
|       - |   712 | `		}` |
|   42362 |   713 | `		zCur = zIn;` |
|  667030 |   714 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  624670 |   715 | `			zIn++;` |
|       2 |   716 | `		}` |
|   42362 |   717 | `		if( zIn > zCur ){` |
|       - |   718 | `			/* Append raw contents*/` |
|   42342 |   719 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   21170 |   720 | `		}` |
|   42362 |   721 | `		zIn++;` |
|   42362 |   722 | `		if( zIn < zEnd ){` |
|     105 |   723 | `			if( zIn[0] == '\\' ){` |
|       - |   724 | `				/* A literal backslash */` |
|      23 |   725 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      94 |   726 | `			}else if( zIn[0] == '\'' ){` |
|       - |   727 | `				/* A single quote */` |
|      11 |   728 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       6 |   729 | `			}else{` |
|       - |   730 | `				/* verbatim copy */` |
|      73 |   731 | `				zIn--;` |
|      73 |   732 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char)*2);` |
|      73 |   733 | `				zIn++;` |
|       - |   734 | `			}` |
|      52 |   735 | `		}` |
|       - |   736 | `		/* Advance the stream cursor */` |
|   42362 |   737 | `		zIn++;` |
|       2 |   738 | `	}` |
|       - |   739 | `	/* Emit the load constant instruction */` |
|   42278 |   740 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   42278 |   741 | `	if( pStr->nByte < 1024 ){` |
|       - |   742 | `		/* Install in the literal table */` |
|   42278 |   743 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   21138 |   744 | `	}` |
|       - |   745 | `	/* Node successfully compiled */` |
|   42278 |   746 | `	return SXRET_OK;` |
|   38220 |   747 |  |
|       - |   748 | `/*` |
|       - |   749 | ` * PHP 7.3 flexible heredoc/nowdoc closing-marker indent stripping.` |
|       - |   750 | ` *` |
|       - |   751 | ` * When the lexer matched the closing marker with leading whitespace on its` |
|       - |   752 | ` * own line, it stored the indent count in pGen->pIn->pUserData. The marker's` |
|       - |   753 | ` * indent prefix bytes sit immediately after the stripped body (at` |
|       - |   754 | ` * pIn->sData.zString + pIn->sData.nByte + 1 for LF, +2 for CRLF) in the` |
|       - |   755 | ` * original source buffer — the buffer is stable through compilation.` |
|       - |   756 | ` *` |
|       - |   757 | `` * For each body line, we remove exactly `nIndent` leading bytes that must`` |
|       - |   758 | ` * byte-for-byte match the marker's prefix. Empty lines (0 bytes or bare \r)` |
|       - |   759 | ` * bypass validation. Mismatches raise the exact PHP 7.3+ parse errors:` |
|       - |   760 | ` *   - "Invalid body indentation level (expecting an indentation level of` |
|       - |   761 | ` *     at least N)" — line too short, or first differing byte is not` |
|       - |   762 | ` *     whitespace.` |
|       - |   763 | ` *   - "Invalid indentation - tabs and spaces cannot be mixed" — first` |
|       - |   764 | ` *     differing byte is whitespace but differs from the marker prefix.` |
|       - |   765 | ` */` |
|     106 |   766 | `static sxi32 GenStateStripHeredocIndent(ph7_gen_state *pGen, SyString *pOut)` |
|       2 |   767 |  |
|     108 |   768 | `	SyString *pIn = &pGen->pIn->sData;` |
|     108 |   769 | `	sxu32 nIndent = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |   770 | `	const char *zPrefix;` |
|       - |   771 | `	const char *z, *zEnd;` |
|       - |   772 | `	char *zBuf, *zDst;` |
|     108 |   773 | `	if( nIndent == 0 ){` |
|       - |   774 | `		/* Legacy column-0 marker: zero-copy fast path */` |
|      64 |   775 | `		*pOut = *pIn;` |
|      64 |   776 | `		return SXRET_OK;` |
|       - |   777 | `	}` |
|       - |   778 | `	/* Recover the marker indent prefix from the original source buffer.` |
|       - |   779 | `	 * Skip the terminator the lexer stripped: one '\n' plus an optional` |
|       - |   780 | `	 * preceding '\r'. Note: when the body is empty (pIn->nByte == 0) the` |
|       - |   781 | `	 * lexer stripped nothing, so this offset is one byte past the true` |
|       - |   782 | `	 * marker-indent start. That is harmless — the strip loop below never` |
|       - |   783 | `	 * runs (z == zEnd), and zPrefix is never dereferenced. */` |
|      46 |   784 | `	zPrefix = pIn->zString + pIn->nByte;` |
|      46 |   785 | `	if( zPrefix[0] == '\r' && zPrefix[1] == '\n' ){` |
|     ! 0 |   786 | `		zPrefix += 2;` |
|     ! 0 |   787 | `	}else{` |
|      46 |   788 | `		zPrefix += 1;` |
|       - |   789 | `	}` |
|       - |   790 | `	/* Allocate scratch buffer sized to the original body (always enough). */` |
|      46 |   791 | `	zBuf = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator, pIn->nByte + 1);` |
|      46 |   792 | `	if( zBuf == 0 ){` |
|     ! 0 |   793 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   794 | `		return SXERR_ABORT;` |
|       - |   795 | `	}` |
|      46 |   796 | `	zDst = zBuf;` |
|      46 |   797 | `	z = pIn->zString;` |
|      46 |   798 | `	zEnd = z + pIn->nByte;` |
|     128 |   799 | `	while( z < zEnd ){` |
|      70 |   800 | `		const char *zLine = z;` |
|       - |   801 | `		sxu32 nLine;` |
|       - |   802 | `		int bEmpty;` |
|     798 |   803 | `		while( z < zEnd && z[0] != '\n' ){` |
|     730 |   804 | `			z++;` |
|       2 |   805 | `		}` |
|      70 |   806 | `		nLine = (sxu32)(z - zLine);` |
|      70 |   807 | `		bEmpty = (nLine == 0) \|\| (nLine == 1 && zLine[0] == '\r');` |
|      70 |   808 | `		if( !bEmpty ){` |
|       - |   809 | `			sxu32 i;` |
|      66 |   810 | `			if( nLine < nIndent ){` |
|     ! 0 |   811 | `				PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   812 | `					"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|     ! 0 |   813 | `					nIndent);` |
|     ! 0 |   814 | `				return SXERR_ABORT;` |
|       - |   815 | `			}` |
|     268 |   816 | `			for( i = 0; i < nIndent; i++ ){` |
|     212 |   817 | `				if( zLine[i] != zPrefix[i] ){` |
|       9 |   818 | `					unsigned char c = (unsigned char)zLine[i];` |
|       9 |   819 | `					if( c == ' ' \|\| c == '\t' ){` |
|       5 |   820 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   821 | `							"Invalid indentation - tabs and spaces cannot be mixed");` |
|       3 |   822 | `					}else{` |
|       7 |   823 | `						PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |   824 | `							"Invalid body indentation level (expecting an indentation level of at least %u)",` |
|       2 |   825 | `							nIndent);` |
|       - |   826 | `					}` |
|       9 |   827 | `					return SXERR_ABORT;` |
|       - |   828 | `				}` |
|     103 |   829 | `			}` |
|      57 |   830 | `			SyMemcpy((const void *)(zLine + nIndent), (void *)zDst, nLine - nIndent);` |
|      57 |   831 | `			zDst += nLine - nIndent;` |
|      33 |   832 | `		}else if( nLine == 1 ){` |
|       - |   833 | `			/* Preserve the stray '\r' on an otherwise empty line */` |
|     ! 0 |   834 | `			*zDst++ = '\r';` |
|     ! 0 |   835 | `		}` |
|      61 |   836 | `		if( z < zEnd ){` |
|      25 |   837 | `			*zDst++ = '\n';` |
|      25 |   838 | `			z++;` |
|      12 |   839 | `		}` |
|       1 |   840 | `	}` |
|      37 |   841 | `	pOut->zString = zBuf;` |
|      37 |   842 | `	pOut->nByte = (sxu32)(zDst - zBuf);` |
|      37 |   843 | `	return SXRET_OK;` |
|      55 |   844 |  |
|       - |   845 | `/*` |
|       - |   846 | ` * Compile a nowdoc string.` |
|       - |   847 | ` * According to the PHP language reference manual:` |
|       - |   848 | ` *` |
|       - |   849 | ` *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.` |
|       - |   850 | ` *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.` |
|       - |   851 | ` *  The construct is ideal for embedding PHP code or other large blocks of text without the` |
|       - |   852 | ` *  need for escaping. It shares some features in common with the SGML <![CDATA[ ]]>` |
|       - |   853 | ` *  construct, in that it declares a block of text which is not for parsing.` |
|       - |   854 | ` *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier` |
|       - |   855 | ` *  which follows is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc` |
|       - |   856 | ` *  identifiers also apply to nowdoc identifiers, especially those regarding the appearance` |
|       - |   857 | ` *  of the closing identifier.` |
|       - |   858 | ` */` |
|      42 |   859 | `PH7_PRIVATE sxi32 PH7_CompileNowDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   860 |  |
|       - |   861 | `	SyString sStripped;` |
|       - |   862 | `	SyString *pStr;` |
|       - |   863 | `	ph7_value *pObj;` |
|       - |   864 | `	sxu32 nIdx;` |
|       - |   865 | `	sxi32 rc;` |
|      44 |   866 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      44 |   867 | `	if( rc != SXRET_OK ){` |
|       5 |   868 | `		return rc;` |
|       - |   869 | `	}` |
|      40 |   870 | `	pStr = &sStripped;` |
|      40 |   871 | `	nIdx = 0; /* Prevent compiler warning */` |
|      40 |   872 | `	if( pStr->nByte <= 0 ){` |
|       - |   873 | `		/* Empty string,load NULL */` |
|       7 |   874 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |   875 | `		return SXRET_OK;` |
|       - |   876 | `	}` |
|       - |   877 | `	/* Reserve a new constant */` |
|      34 |   878 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      34 |   879 | `	if( pObj == 0 ){` |
|     ! 0 |   880 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   881 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   882 | `		return SXERR_ABORT;` |
|       - |   883 | `	}` |
|       - |   884 | `	/* No processing is done here, simply a memcpy() operation */` |
|      34 |   885 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,pStr);` |
|       - |   886 | `	/* Emit the load constant instruction */` |
|      34 |   887 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   888 | `	/* Node successfully compiled */` |
|      34 |   889 | `	return SXRET_OK;` |
|      23 |   890 |  |
|       - |   891 | `/*` |
|       - |   892 | ` * Process variable expression [i.e: "$var","${var}"] embedded in a double quoted/heredoc string.` |
|       - |   893 | ` * According to the PHP language reference manual` |
|       - |   894 | ` *   When a string is specified in double quotes or with heredoc,variables are parsed within it.` |
|       - |   895 | ` *  There are two types of syntax: a simple one and a complex one. The simple syntax is the most` |
|       - |   896 | ` *  common and convenient. It provides a way to embed a variable, an array value, or an object` |
|       - |   897 | ` *  property in a string with a minimum of effort.` |
|       - |   898 | ` *  Simple syntax` |
|       - |   899 | ` *   If a dollar sign ($) is encountered, the parser will greedily take as many tokens as possible` |
|       - |   900 | ` *   to form a valid variable name. Enclose the variable name in curly braces to explicitly specify` |
|       - |   901 | ` *   the end of the name.` |
|       - |   902 | ` *   Similarly, an array index or an object property can be parsed. With array indices, the closing` |
|       - |   903 | ` *   square bracket (]) marks the end of the index. The same rules apply to object properties` |
|       - |   904 | ` *   as to simple variables.` |
|       - |   905 | ` *  Complex (curly) syntax` |
|       - |   906 | ` *   This isn't called complex because the syntax is complex, but because it allows for the use` |
|       - |   907 | ` *   of complex expressions.` |
|       - |   908 | ` *   Any scalar variable, array element or object property with a string representation can be` |
|       - |   909 | ` *   included via this syntax. Simply write the expression the same way as it would appear outside` |
|       - |   910 | ` *   the string, and then wrap it in { and }. Since { can not be escaped, this syntax will only` |
|       - |   911 | ` *   be recognised when the $ immediately follows the {. Use {\$ to get a literal {$` |
|       - |   912 | ` */` |
|    1964 |   913 | `static sxi32 GenStateProcessStringExpression(` |
|       - |   914 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   915 | `	sxu32 nLine,         /* Line number */` |
|       - |   916 | `	const char *zIn,     /* Raw expression */` |
|       - |   917 | `	const char *zEnd     /* End of the expression */` |
|       - |   918 | `	)` |
|       2 |   919 |  |
|       - |   920 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |   921 | `	SySet sToken;` |
|       - |   922 | `	sxi32 rc;` |
|       - |   923 | `	/* Initialize the token set */` |
|    1966 |   924 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   925 | `	/* Preallocate some slots */` |
|    1966 |   926 | `	SySetAlloc(&sToken,0x08);` |
|       - |   927 | `	/* Tokenize the text */` |
|    1966 |   928 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   929 | `	/* Swap delimiter */` |
|    1966 |   930 | `	pTmpIn  = pGen->pIn;` |
|    1966 |   931 | `	pTmpEnd = pGen->pEnd;` |
|    1966 |   932 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1966 |   933 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   934 | `	/* Compile the expression */` |
|    1966 |   935 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   936 | `	/* Restore token stream */` |
|    1966 |   937 | `	pGen->pIn  = pTmpIn;` |
|    1966 |   938 | `	pGen->pEnd = pTmpEnd;` |
|       - |   939 | `	/* Release the token set */` |
|    1966 |   940 | `	SySetRelease(&sToken);` |
|       - |   941 | `	/* Compilation result */` |
|    1966 |   942 | `	return rc;` |
|       2 |   943 |  |
|       - |   944 | `/*` |
|       - |   945 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   946 | ` */` |
|   20282 |   947 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   948 |  |
|       - |   949 | `	ph7_value *pConstObj;` |
|   20284 |   950 | `	sxu32 nIdx = 0;` |
|       - |   951 | `	/* Reserve a new constant */` |
|   20284 |   952 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   20284 |   953 | `	if( pConstObj == 0 ){` |
|     ! 0 |   954 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   955 | `		return 0;` |
|       - |   956 | `	}` |
|   20284 |   957 | `	(*pCount)++;` |
|   20284 |   958 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   959 | `	/* Emit the load constant instruction */` |
|   20284 |   960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   20284 |   961 | `	return pConstObj;` |
|   10143 |   962 |  |
|       - |   963 | `/*` |
|       - |   964 | ` * Compile a double quoted/heredoc string.` |
|       - |   965 | ` * According to the PHP language reference manual` |
|       - |   966 | ` * Heredoc` |
|       - |   967 | ` *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier` |
|       - |   968 | ` *  is provided, then a newline. The string itself follows, and then the same identifier again` |
|       - |   969 | ` *  to close the quotation.` |
|       - |   970 | ` *  The closing identifier must begin in the first column of the line. Also, the identifier must` |
|       - |   971 | ` *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric` |
|       - |   972 | ` *  characters and underscores, and must start with a non-digit character or underscore.` |
|       - |   973 | ` *  Warning` |
|       - |   974 | ` *  It is very important to note that the line with the closing identifier must contain` |
|       - |   975 | ` *  no other characters, except possibly a semicolon (;). That means especially that the identifier` |
|       - |   976 | ` *  may not be indented, and there may not be any spaces or tabs before or after the semicolon.` |
|       - |   977 | ` *  It's also important to realize that the first character before the closing identifier must` |
|       - |   978 | ` *  be a newline as defined by the local operating system. This is \n on UNIX systems, including Mac OS X.` |
|       - |   979 | ` *  The closing delimiter (possibly followed by a semicolon) must also be followed by a newline.` |
|       - |   980 | ` *  If this rule is broken and the closing identifier is not "clean", it will not be considered a closing` |
|       - |   981 | ` *  identifier, and PHP will continue looking for one. If a proper closing identifier is not found before` |
|       - |   982 | ` *  the end of the current file, a parse error will result at the last line.` |
|       - |   983 | ` *  Heredocs can not be used for initializing class properties.` |
|       - |   984 | ` * Double quoted` |
|       - |   985 | ` *  If the string is enclosed in double-quotes ("), PHP will interpret more escape sequences for special characters:` |
|       - |   986 | ` *  Escaped characters Sequence 	Meaning` |
|       - |   987 | ` *  \n linefeed (LF or 0x0A (10) in ASCII)` |
|       - |   988 | ` *  \r carriage return (CR or 0x0D (13) in ASCII)` |
|       - |   989 | ` *  \t horizontal tab (HT or 0x09 (9) in ASCII)` |
|       - |   990 | ` *  \v vertical tab (VT or 0x0B (11) in ASCII)` |
|       - |   991 | ` *  \f form feed (FF or 0x0C (12) in ASCII)` |
|       - |   992 | ` *  \\ backslash` |
|       - |   993 | ` *  \$ dollar sign` |
|       - |   994 | ` *  \" double-quote` |
|       - |   995 | ` *  \[0-7]{1,3} 	the sequence of characters matching the regular expression is a character in octal notation` |
|       - |   996 | ` *  \x[0-9A-Fa-f]{1,2} 	the sequence of characters matching the regular expression is a character in hexadecimal notation` |
|       - |   997 | ` * As in single quoted strings, escaping any other character will result in the backslash being printed too.` |
|       - |   998 | ` * The most important feature of double-quoted strings is the fact that variable names will be expanded.` |
|       - |   999 | ` * See string parsing for details.` |
|       - |  1000 | ` */` |
|   18920 |  1001 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  1002 |  |
|   18922 |  1003 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1004 | `	const char *zIn,*zCur,*zEnd;` |
|   18922 |  1005 | `	ph7_value *pObj = 0;` |
|       - |  1006 | `	sxi32 iCons;` |
|       - |  1007 | `	sxi32 rc;` |
|       - |  1008 | `	/* Delimit the string */` |
|   18922 |  1009 | `	zIn  = pStr->zString;` |
|   18922 |  1010 | `	zEnd = &zIn[pStr->nByte];` |
|   18922 |  1011 | `	if( zIn >= zEnd ){` |
|       - |  1012 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1013 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1014 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1015 | `		 */` |
|     268 |  1016 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     268 |  1017 | `		return SXRET_OK;` |
|       - |  1018 | `	}` |
|   18656 |  1019 | `	zCur = 0;` |
|       - |  1020 | `	/* Compile the node */` |
|   18656 |  1021 | `	iCons = 0;` |
|   10309 |  1022 | `	for(;;){` |
|   31090 |  1023 | `		zCur = zIn;` |
|  153310 |  1024 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  124186 |  1025 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      63 |  1026 | `				break;` |
|  124066 |  1027 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1846 |  1028 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     923 |  1029 | `					break;` |
|       - |  1030 | `			}` |
|  122222 |  1031 | `			zIn++;` |
|       2 |  1032 | `		}` |
|   31090 |  1033 | `		if( zIn > zCur ){` |
|   14184 |  1034 | `			if( pObj == 0 ){` |
|   13858 |  1035 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   13858 |  1036 | `				if( pObj == 0 ){` |
|     ! 0 |  1037 | `					return SXERR_ABORT;` |
|       - |  1038 | `				}` |
|    6928 |  1039 | `			}` |
|   14184 |  1040 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    7091 |  1041 | `		}` |
|   31090 |  1042 | `		if( zIn >= zEnd ){` |
|   18656 |  1043 | `			break;` |
|       - |  1044 | `		}` |
|   12436 |  1045 | `		if( zIn[0] == '\\' ){` |
|   10472 |  1046 | `			const char *zPtr = 0;` |
|       - |  1047 | `			sxu32 n;` |
|   10472 |  1048 | `			zIn++;` |
|   10472 |  1049 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1050 | `				break;` |
|       - |  1051 | `			}` |
|   10472 |  1052 | `			if( pObj == 0 ){` |
|    6428 |  1053 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    6428 |  1054 | `				if( pObj == 0 ){` |
|     ! 0 |  1055 | `					return SXERR_ABORT;` |
|       - |  1056 | `				}` |
|    3213 |  1057 | `			}` |
|   10472 |  1058 | `			n = sizeof(char); /* size of conversion */` |
|   10472 |  1059 | `			switch( zIn[0] ){` |
|       3 |  1060 | `			case '$':` |
|       - |  1061 | `				/* Dollar sign */` |
|       7 |  1062 | `				PH7_MemObjStringAppend(pObj,"$",sizeof(char));` |
|       7 |  1063 | `				break;` |
|      38 |  1064 | `			case '\\':` |
|       - |  1065 | `				/* A literal backslash */` |
|      78 |  1066 | `				PH7_MemObjStringAppend(pObj,"\\",sizeof(char));` |
|      78 |  1067 | `				break;` |
|       2 |  1068 | `			case 'a':` |
|       - |  1069 | `				/* The "alert" character (BEL)[ctrl+g] ASCII code 7 */` |
|       5 |  1070 | `				PH7_MemObjStringAppend(pObj,"\a",sizeof(char));` |
|       5 |  1071 | `				break;` |
|       2 |  1072 | `			case 'b':` |
|       - |  1073 | `				/* Backspace (BS)[ctrl+h] ASCII code 8 */` |
|       5 |  1074 | `				PH7_MemObjStringAppend(pObj,"\b",sizeof(char));` |
|       5 |  1075 | `				break;` |
|       4 |  1076 | `			case 'f':` |
|       - |  1077 | `				/* Form-feed (FF)[ctrl+l] ASCII code 12 */` |
|       9 |  1078 | `				PH7_MemObjStringAppend(pObj,"\f",sizeof(char));` |
|       9 |  1079 | `				break;` |
|    4821 |  1080 | `			case 'n':` |
|       - |  1081 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    9644 |  1082 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    9644 |  1083 | `				break;` |
|      19 |  1084 | `			case 'r':` |
|       - |  1085 | `				/* Carriage return (CR)[ctrl+m] ASCII code 13 */` |
|      40 |  1086 | `				PH7_MemObjStringAppend(pObj,"\r",sizeof(char));` |
|      40 |  1087 | `				break;` |
|      24 |  1088 | `			case 't':` |
|       - |  1089 | `				/* Horizontal tab (HT)[ctrl+i] ASCII code 9 */` |
|      50 |  1090 | `				PH7_MemObjStringAppend(pObj,"\t",sizeof(char));` |
|      50 |  1091 | `				break;` |
|       3 |  1092 | `			case 'v':` |
|       - |  1093 | `				/* Vertical tab(VT)[ctrl+k] ASCII code 11 */` |
|       7 |  1094 | `				PH7_MemObjStringAppend(pObj,"\v",sizeof(char));` |
|       7 |  1095 | `				break;` |
|       1 |  1096 | `			case '\'':` |
|       - |  1097 | `				/* Single quote */` |
|       3 |  1098 | `				PH7_MemObjStringAppend(pObj,"'",sizeof(char));` |
|       3 |  1099 | `				break;` |
|      54 |  1100 | `			case '"':` |
|       - |  1101 | `				/* Double quote */` |
|     110 |  1102 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     110 |  1103 | `				break;` |
|       6 |  1104 | `			case '0':` |
|       - |  1105 | `				/* NUL byte */` |
|      13 |  1106 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      13 |  1107 | `				break;` |
|     232 |  1108 | `			case 'x':` |
|     465 |  1109 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1110 | `					int c;` |
|       - |  1111 | `					/* Hex digit */` |
|     451 |  1112 | `					c = SyHexToint(zIn[1]) << 4;` |
|     451 |  1113 | `					if( &zIn[2] < zEnd ){` |
|     451 |  1114 | `						c +=  SyHexToint(zIn[2]);` |
|     225 |  1115 | `					}` |
|       - |  1116 | `					/* Output char */` |
|     451 |  1117 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     451 |  1118 | `					n += sizeof(char) * 2;` |
|     226 |  1119 | `				}else{` |
|       - |  1120 | `					/* Output literal character  */` |
|      15 |  1121 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1122 | `				}` |
|     465 |  1123 | `				break;` |
|      15 |  1124 | `			case 'o':` |
|      31 |  1125 | `				if( &zIn[1] < zEnd && (unsigned char)zIn[1] < 0xc0 && SyisDigit(zIn[1]) && (zIn[1] - '0') < 8 ){` |
|       - |  1126 | `					/* Octal digit stream */` |
|       - |  1127 | `					int c;` |
|      21 |  1128 | `					c = 0;` |
|      21 |  1129 | `					zIn++;` |
|      61 |  1130 | `					for( zPtr = zIn ; zPtr < &zIn[3*sizeof(char)] ; zPtr++ ){` |
|      55 |  1131 | `						if( zPtr >= zEnd \|\| (unsigned char)zPtr[0] >= 0xc0 \|\| !SyisDigit(zPtr[0]) \|\| (zPtr[0] - '0') > 7 ){` |
|       8 |  1132 | `							break;` |
|       - |  1133 | `						}` |
|      41 |  1134 | `						c = c * 8 + (zPtr[0] - '0');` |
|      21 |  1135 | `					}` |
|      21 |  1136 | `					if ( c > 0 ){` |
|      15 |  1137 | `						PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|       7 |  1138 | `					}` |
|      21 |  1139 | `					n = (sxu32)(zPtr-zIn);` |
|      11 |  1140 | `				}else{` |
|       - |  1141 | `					/* Output literal character  */` |
|      11 |  1142 | `					PH7_MemObjStringAppend(pObj,"o",sizeof(char));` |
|       - |  1143 | `				}` |
|      31 |  1144 | `				break;` |
|      11 |  1145 | `			default:` |
|       - |  1146 | `				/* Output without a slash */` |
|      23 |  1147 | `				PH7_MemObjStringAppend(pObj,zIn,sizeof(char));` |
|      22 |  1148 | `				break;` |
|       - |  1149 | `			}` |
|       - |  1150 | `			/* Advance the stream cursor */` |
|   10472 |  1151 | `			zIn += n;` |
|   10472 |  1152 | `			continue;` |
|       - |  1153 | `		}` |
|    1966 |  1154 | `		if( zIn[0] == '{' ){` |
|       - |  1155 | `			/* Curly syntax */` |
|       - |  1156 | `			const char *zExpr;` |
|     124 |  1157 | `			sxi32 iNest = 1;` |
|     124 |  1158 | `			zIn++;` |
|     124 |  1159 | `			zExpr = zIn;` |
|       - |  1160 | `			/* Synchronize with the next closing curly braces */` |
|    1300 |  1161 | `			while( zIn < zEnd ){` |
|    1300 |  1162 | `				if( zIn[0] == '{' ){` |
|       - |  1163 | `					/* Increment nesting level */` |
|       9 |  1164 | `					iNest++;` |
|    1296 |  1165 | `				}else if(zIn[0] == '}' ){` |
|       - |  1166 | `					/* Decrement nesting level */` |
|     132 |  1167 | `					iNest--;` |
|     132 |  1168 | `					if( iNest <= 0 ){` |
|     124 |  1169 | `						break;` |
|       - |  1170 | `					}` |
|       4 |  1171 | `				}` |
|    1178 |  1172 | `				zIn++;` |
|       2 |  1173 | `			}` |
|       - |  1174 | `			/* Process the expression */` |
|     124 |  1175 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     124 |  1176 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1177 | `				return SXERR_ABORT;` |
|       - |  1178 | `			}` |
|     124 |  1179 | `			if( rc != SXERR_EMPTY ){` |
|     124 |  1180 | `				++iCons;` |
|      61 |  1181 | `			}` |
|     124 |  1182 | `			if( zIn < zEnd ){` |
|       - |  1183 | `				/* Jump the trailing curly */` |
|     124 |  1184 | `				zIn++;` |
|      61 |  1185 | `			}` |
|      63 |  1186 | `		}else{` |
|       - |  1187 | `			/* Simple syntax */` |
|    1844 |  1188 | `			const char *zExpr = zIn;` |
|       - |  1189 | `			/* Assemble variable name */` |
|     927 |  1190 | `			for(;;){` |
|       - |  1191 | `				/* Jump leading dollars */` |
|    3698 |  1192 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1844 |  1193 | `					zIn++;` |
|       2 |  1194 | `				}` |
|     927 |  1195 | `				for(;;){` |
|   10851 |  1196 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8070 |  1197 | `						zIn++;` |
|       2 |  1198 | `					}` |
|    1856 |  1199 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1200 | `						/* UTF-8 stream */` |
|     ! 0 |  1201 | `						zIn++;` |
|     ! 0 |  1202 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1203 | `							zIn++;` |
|     ! 0 |  1204 | `						}` |
|     ! 0 |  1205 | `						continue;` |
|       - |  1206 | `					}` |
|    1856 |  1207 | `					break;` |
|     ! 0 |  1208 | `				}` |
|    1856 |  1209 | `				if( zIn >= zEnd ){` |
|     122 |  1210 | `					break;` |
|       - |  1211 | `				}` |
|    1736 |  1212 | `				if( zIn[0] == '[' ){` |
|       9 |  1213 | `					sxi32 iSquare = 1;` |
|       9 |  1214 | `					zIn++;` |
|      17 |  1215 | `					while( zIn < zEnd ){` |
|      17 |  1216 | `						if( zIn[0] == '[' ){` |
|     ! 0 |  1217 | `							iSquare++;` |
|      17 |  1218 | `						}else if (zIn[0] == ']' ){` |
|       9 |  1219 | `							iSquare--;` |
|       9 |  1220 | `							if( iSquare <= 0 ){` |
|       9 |  1221 | `								break;` |
|       - |  1222 | `							}` |
|     ! 0 |  1223 | `						}` |
|       9 |  1224 | `						zIn++;` |
|       1 |  1225 | `					}` |
|       9 |  1226 | `					if( zIn < zEnd ){` |
|       9 |  1227 | `						zIn++;` |
|       4 |  1228 | `					}` |
|       9 |  1229 | `					break;` |
|    1728 |  1230 | `				}else if(zIn[0] == '{' ){` |
|       6 |  1231 | `					sxi32 iCurly = 1;` |
|       6 |  1232 | `					zIn++;` |
|      18 |  1233 | `					while( zIn < zEnd ){` |
|      16 |  1234 | `						if( zIn[0] == '{' ){` |
|     ! 0 |  1235 | `							iCurly++;` |
|      16 |  1236 | `						}else if (zIn[0] == '}' ){` |
|       3 |  1237 | `							iCurly--;` |
|       3 |  1238 | `							if( iCurly <= 0 ){` |
|       3 |  1239 | `								break;` |
|       - |  1240 | `							}` |
|     ! 0 |  1241 | `						}` |
|      14 |  1242 | `						zIn++;` |
|       2 |  1243 | `					}` |
|       6 |  1244 | `					if( zIn < zEnd ){` |
|       3 |  1245 | `						zIn++;` |
|       1 |  1246 | `					}` |
|       6 |  1247 | `					break;` |
|    1724 |  1248 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1249 | `					/* Member access operator '->' */` |
|      13 |  1250 | `					zIn += 2;` |
|    1718 |  1251 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1252 | `					/* Static member access operator '::' */` |
|     ! 0 |  1253 | `					zIn += 2;` |
|     ! 0 |  1254 | `				}else{` |
|     857 |  1255 | `					break;` |
|       - |  1256 | `				}` |
|       1 |  1257 | `			}` |
|       - |  1258 | `			/* Process the expression */` |
|    1844 |  1259 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1844 |  1260 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1261 | `				return SXERR_ABORT;` |
|       - |  1262 | `			}` |
|    1844 |  1263 | `			if( rc != SXERR_EMPTY ){` |
|    1842 |  1264 | `				++iCons;` |
|     920 |  1265 | `			}` |
|       - |  1266 | `		}` |
|       - |  1267 | `		/* Invalidate the previously used constant */` |
|    1966 |  1268 | `		pObj = 0;` |
|       2 |  1269 | `	}/*for(;;)*/` |
|   18656 |  1270 | `	if( iCons > 1 ){` |
|       - |  1271 | `		/* Concatenate all compiled constants */` |
|    1454 |  1272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     726 |  1273 | `	}` |
|       - |  1274 | `	/* Node successfully compiled */` |
|   18656 |  1275 | `	return SXRET_OK;` |
|    9462 |  1276 |  |
|       - |  1277 | `/*` |
|       - |  1278 | ` * Compile a double quoted string.` |
|       - |  1279 | ` *  See the block-comment above for more information.` |
|       - |  1280 | ` */` |
|   18860 |  1281 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1282 |  |
|       - |  1283 | `	sxi32 rc;` |
|   18862 |  1284 | `	rc = GenStateCompileString(&(*pGen));` |
|    9430 |  1285 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1286 | `	/* Compilation result */` |
|   18862 |  1287 | `	return rc;` |
|       2 |  1288 |  |
|       - |  1289 | `/*` |
|       - |  1290 | ` * Compile a Heredoc string.` |
|       - |  1291 | ` *  See the block-comment above for more information.` |
|       - |  1292 | ` */` |
|      64 |  1293 | `PH7_PRIVATE sxi32 PH7_CompileHereDoc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1294 |  |
|       - |  1295 | `	SyString sOrig, sStripped;` |
|       - |  1296 | `	sxi32 rc;` |
|      66 |  1297 | `	rc = GenStateStripHeredocIndent(&(*pGen), &sStripped);` |
|      66 |  1298 | `	if( rc != SXRET_OK ){` |
|       5 |  1299 | `		return rc;` |
|       - |  1300 | `	}` |
|       - |  1301 | `	/* Temporarily swap in the dedented body so GenStateCompileString` |
|       - |  1302 | `	 * (which reads pGen->pIn->sData directly) sees the stripped content.` |
|       - |  1303 | `	 * Restore before returning so downstream code that references pIn is` |
|       - |  1304 | `	 * unaffected, including on the error path. */` |
|      62 |  1305 | `	sOrig = pGen->pIn->sData;` |
|      62 |  1306 | `	pGen->pIn->sData = sStripped;` |
|      62 |  1307 | `	rc = GenStateCompileString(&(*pGen));` |
|      62 |  1308 | `	pGen->pIn->sData = sOrig;` |
|      30 |  1309 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|      62 |  1310 | `	return rc;` |
|      34 |  1311 |  |
|       - |  1312 | `/*` |
|       - |  1313 | ` * Compile an array entry whether it is a key or a value.` |
|       - |  1314 | ` *  Notes on array entries.` |
|       - |  1315 | ` *  According to the PHP language reference manual` |
|       - |  1316 | ` *  An array can be created by the array() language construct.` |
|       - |  1317 | ` *  It takes as parameters any number of comma-separated key => value pairs.` |
|       - |  1318 | ` *  array(  key =>  value` |
|       - |  1319 | ` *    , ...` |
|       - |  1320 | ` *    )` |
|       - |  1321 | ` *  A key may be either an integer or a string. If a key is the standard representation` |
|       - |  1322 | ` *  of an integer, it will be interpreted as such (i.e. "8" will be interpreted as 8, while` |
|       - |  1323 | ` *  "08" will be interpreted as "08"). Floats in key are truncated to integer.` |
|       - |  1324 | ` *  The indexed and associative array types are the same type in PHP, which can both` |
|       - |  1325 | ` *  contain integer and string indices.` |
|       - |  1326 | ` *  A value can be any PHP type.` |
|       - |  1327 | ` *  If a key is not specified for a value, the maximum of the integer indices is taken` |
|       - |  1328 | ` *  and the new key will be that value plus 1. If a key that already has an assigned value` |
|       - |  1329 | ` *  is specified, that value will be overwritten.` |
|       - |  1330 | ` */` |
|   17824 |  1331 | `static sxi32 GenStateCompileArrayEntry(` |
|       - |  1332 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  1333 | `	SyToken *pIn,        /* Token stream */` |
|       - |  1334 | `	SyToken *pEnd,       /* End of the token stream */` |
|       - |  1335 | `	sxi32 iFlags,        /* Compilation flags */` |
|       - |  1336 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *) /* Expression tree validator callback */` |
|       - |  1337 | `	)` |
|       2 |  1338 |  |
|       - |  1339 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  1340 | `	sxi32 rc;` |
|       - |  1341 | `	/* Swap token stream */` |
|   17826 |  1342 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1343 | `	/* Compile the expression*/` |
|   17826 |  1344 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1345 | `	/* Restore token stream */` |
|   17826 |  1346 | `	RE_SWAP_DELIMITER(pGen);` |
|   17826 |  1347 | `	return rc;` |
|       2 |  1348 |  |
|       - |  1349 | `/*` |
|       - |  1350 | ` * Expression tree validator callback for the 'array' language construct.` |
|       - |  1351 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1352 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1353 | ` * error message.` |
|       - |  1354 | ` * See the routine responible of compiling the array language construct` |
|       - |  1355 | ` * for more inforation.` |
|       - |  1356 | ` */` |
|      30 |  1357 | `static sxi32 GenStateArrayNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1358 |  |
|      32 |  1359 | `	sxi32 rc = SXRET_OK;` |
|      32 |  1360 | `	if( pRoot->pOp ){` |
|      19 |  1361 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ &&` |
|      12 |  1362 | `			pRoot->pOp->iOp != EXPR_OP_FUNC_CALL /* function() [Symisc extension: i.e: array(&foo())] */` |
|      14 |  1363 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  1364 | `			/* Unexpected expression */` |
|      11 |  1365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1366 | `				"array(): Expecting a variable/array member/function call after reference operator '&'");` |
|      11 |  1367 | `			if( rc != SXERR_ABORT ){` |
|      11 |  1368 | `				rc = SXERR_INVALID;` |
|       5 |  1369 | `			}` |
|       7 |  1370 | `		}` |
|      25 |  1371 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1372 | `		/* Unexpected expression */` |
|       3 |  1373 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1374 | `			"array(): Expecting a variable after reference operator '&'");` |
|       3 |  1375 | `		if( rc != SXERR_ABORT ){` |
|       3 |  1376 | `			rc = SXERR_INVALID;` |
|       1 |  1377 | `		}` |
|       1 |  1378 | `	}` |
|      32 |  1379 | `	return rc;` |
|       2 |  1380 |  |
|       - |  1381 | `/*` |
|       - |  1382 | ` * Compile the body of an array literal (shared by array() and short syntax []).` |
|       - |  1383 | ` * Assumes pGen->pIn points to the first content token and pGen->pEnd points` |
|       - |  1384 | ` * one past the last content token (i.e. the delimiters have been excluded).` |
|       - |  1385 | ` */` |
|   26296 |  1386 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1387 |  |
|       - |  1388 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1389 | `	SyToken *pKey,*pCur;` |
|   26298 |  1390 | `	sxi32 iEmitRef = 0;` |
|   26298 |  1391 | `	sxi32 iSpread = 0;` |
|   26298 |  1392 | `	sxi32 nPair = 0;` |
|       - |  1393 | `	sxi32 iNest;` |
|       - |  1394 | `	sxi32 rc;` |
|   26298 |  1395 | `	xValidator = 0;` |
|   21350 |  1396 | `	for(;;){` |
|       - |  1397 | `		/* Jump leading commas */` |
|   48226 |  1398 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5526 |  1399 | `			pGen->pIn++;` |
|       2 |  1400 | `		}` |
|   42702 |  1401 | `		pCur = pGen->pIn;` |
|   42702 |  1402 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1403 | `			/* No more entry to process */` |
|   26282 |  1404 | `			break;` |
|       - |  1405 | `		}` |
|   16422 |  1406 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1407 | `			continue;` |
|       - |  1408 | `		}` |
|       - |  1409 | `		/* Compile the key if available */` |
|   16422 |  1410 | `		pKey = pCur;` |
|   16422 |  1411 | `		iNest = 0;` |
|   45952 |  1412 | `		while( pCur < pGen->pIn ){` |
|   30838 |  1413 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1304 |  1414 | `				break;` |
|       - |  1415 | `			}` |
|       - |  1416 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1417 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1418 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1419 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1420 | `			 */` |
|   29536 |  1421 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      84 |  1422 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      84 |  1423 | `				SyToken *pFn = pCur;` |
|      82 |  1424 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 |  1425 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 |  1426 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1427 | `					pFn = &pCur[1];` |
|     ! 0 |  1428 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1429 | `				}` |
|      84 |  1430 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1431 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1432 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1433 | `						pCur++;` |
|     ! 0 |  1434 | `					}` |
|       5 |  1435 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1436 | `						pCur++;` |
|       5 |  1437 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1438 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1439 | `						if( pCur < pGen->pIn ){` |
|       5 |  1440 | `							pCur++;` |
|       2 |  1441 | `						}` |
|       2 |  1442 | `					}` |
|       5 |  1443 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1444 | `						pCur++;` |
|     ! 0 |  1445 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1446 | `							&& pCur->sData.nByte == 1` |
|     ! 0 |  1447 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1448 | `							pCur++;` |
|     ! 0 |  1449 | `						}` |
|     ! 0 |  1450 | `						if( pCur < pGen->pIn` |
|     ! 0 |  1451 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1452 | `							pCur++;` |
|     ! 0 |  1453 | `						}` |
|     ! 0 |  1454 | `					}` |
|       - |  1455 | `					/* The rest of the entry is the arrow function body — no` |
|       - |  1456 | `					 * outer key to extract. Stop the scan here. */` |
|       5 |  1457 | `					pCur = pGen->pIn;` |
|       5 |  1458 | `					break;` |
|       - |  1459 | `				}` |
|       - |  1460 | `				/* Match expression (PHP 8.0): 'match (subject) { ... }'.` |
|       - |  1461 | `				 * The '=>' inside match arms is not an array key/value separator —` |
|       - |  1462 | `				 * it introduces each arm's result expression. Skip past the full` |
|       - |  1463 | `				 * match span so the outer scan sees no false '=>'. */` |
|      80 |  1464 | `				if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1465 | `					pCur++; /* past 'match' */` |
|       3 |  1466 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1467 | `						pCur++;` |
|       3 |  1468 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1469 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1470 | `						if( pCur < pGen->pIn ){` |
|       3 |  1471 | `							pCur++;` |
|       1 |  1472 | `						}` |
|       1 |  1473 | `					}` |
|       3 |  1474 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1475 | `						pCur++;` |
|       3 |  1476 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1477 | `							PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1478 | `						if( pCur < pGen->pIn ){` |
|       3 |  1479 | `							pCur++;` |
|       1 |  1480 | `						}` |
|       1 |  1481 | `					}` |
|       3 |  1482 | `					continue;` |
|       - |  1483 | `				}` |
|      38 |  1484 | `			}` |
|   29530 |  1485 | `			if( pCur->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OSB/*'['*/\|PH7_TK_OCB/*'{'*/) ){` |
|     210 |  1486 | `				iNest++;` |
|   29426 |  1487 | `			}else if( pCur->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_CCB/*'}'*/) ){` |
|       - |  1488 | `				/* Don't worry about mismatched brackets here,the expression` |
|       - |  1489 | `				 * parser will shortly detect any syntax error.` |
|       - |  1490 | `				 */` |
|     210 |  1491 | `				iNest--;` |
|     104 |  1492 | `			}` |
|   29530 |  1493 | `			pCur++;` |
|       2 |  1494 | `		}` |
|   16422 |  1495 | `		rc = SXERR_EMPTY;` |
|   16422 |  1496 | `		if( pCur < pGen->pIn ){` |
|    1304 |  1497 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1498 | `				/* Missing value */` |
|      11 |  1499 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 |  1500 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1501 | `					return SXERR_ABORT;` |
|       - |  1502 | `				}` |
|      11 |  1503 | `				return SXRET_OK;` |
|       - |  1504 | `			}` |
|       - |  1505 | `			/* Compile the expression holding the key */` |
|    1294 |  1506 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1507 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1294 |  1508 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1509 | `				return SXERR_ABORT;` |
|       - |  1510 | `			}` |
|    1294 |  1511 | `			pCur++; /* Jump the '=>' operator */` |
|   15766 |  1512 | `		}else if( pKey == pCur ){` |
|       - |  1513 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1514 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1515 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1516 | `		}else{` |
|       - |  1517 | `			/* Reset back the cursor and point to the entry value */` |
|   15120 |  1518 | `			pCur = pKey;` |
|       - |  1519 | `		}` |
|   16412 |  1520 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1521 | `			/* No available key,load NULL */` |
|   15122 |  1522 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7560 |  1523 | `		}` |
|   16412 |  1524 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1525 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      36 |  1526 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      36 |  1527 | `			iEmitRef = 1;` |
|      36 |  1528 | `			pCur++; /* Jump the '&' token */` |
|      36 |  1529 | `			if( pCur >= pGen->pIn ){` |
|       - |  1530 | `				/* Missing value */` |
|       3 |  1531 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1532 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1533 | `					return SXERR_ABORT;` |
|       - |  1534 | `				}` |
|       3 |  1535 | `				return SXRET_OK;` |
|       - |  1536 | `			}` |
|      16 |  1537 | `		}` |
|       - |  1538 | `		/* Detect array unpack: '...$expr' as the entry value (PHP 7.4+, with` |
|       - |  1539 | `		 * string-key support since PHP 8.1). The parser strips the '...' inside` |
|       - |  1540 | `		 * ExprExtractNode; we only need to know it's there so we can emit` |
|       - |  1541 | `		 * PH7_OP_FLAG_SPREAD after the value, instructing LOAD_MAP to merge the` |
|       - |  1542 | `		 * resulting hashmap rather than insert it as a scalar entry. */` |
|   16410 |  1543 | `		iSpread = (pCur < pGen->pIn && (pCur->nType & PH7_TK_ELLIPSIS)) ? 1 : 0;` |
|   16410 |  1544 | `		if( iSpread && (rc != SXERR_EMPTY \|\| iEmitRef) ){` |
|       - |  1545 | `			/* '[k => ...$a]' and '[&...$a]' are syntax errors in PHP — the` |
|       - |  1546 | `			 * '...' token cannot follow either '=>' or '&' inside an array` |
|       - |  1547 | `			 * literal. Emit the same Parse-error wording PHP uses so the` |
|       - |  1548 | `			 * output is engine-portable. */` |
|       5 |  1549 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pCur->nLine,` |
|       - |  1550 | `				"syntax error, unexpected token \"...\"");` |
|       5 |  1551 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1552 | `				return SXERR_ABORT;` |
|       - |  1553 | `			}` |
|       5 |  1554 | `			return SXRET_OK;` |
|       - |  1555 | `		}` |
|       - |  1556 | `		/* Compile indice value */` |
|   16406 |  1557 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   16406 |  1558 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1559 | `			return SXERR_ABORT;` |
|       - |  1560 | `		}` |
|   16406 |  1561 | `		if( iSpread ){` |
|       - |  1562 | `			/* Mark the value on TOS as a spread source; LOAD_MAP merges it. */` |
|      50 |  1563 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_FLAG_SPREAD,0,0,0,0);` |
|   16382 |  1564 | `		}else if( iEmitRef ){` |
|       - |  1565 | `			/* Emit the load reference instruction */` |
|      32 |  1566 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1567 | `		}` |
|   16406 |  1568 | `		xValidator = 0;` |
|   16406 |  1569 | `		iEmitRef = 0;` |
|   16406 |  1570 | `		iSpread = 0;` |
|   16406 |  1571 | `		nPair++;` |
|       2 |  1572 | `	}` |
|       - |  1573 | `	/* Emit the load map instruction */` |
|   26282 |  1574 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1575 | `	/* Node successfully compiled */` |
|   26282 |  1576 | `	return SXRET_OK;` |
|   13150 |  1577 |  |
|       - |  1578 | `/*` |
|       - |  1579 | ` * Compile the 'array' language construct.` |
|       - |  1580 | ` *	 According to the PHP language reference manual` |
|       - |  1581 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1582 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1583 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1584 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1585 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1586 | ` */` |
|   25846 |  1587 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1588 |  |
|       - |  1589 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   25848 |  1590 | `	pGen->pIn += 2;` |
|   25848 |  1591 | `	pGen->pEnd--;` |
|   12923 |  1592 | `	SXUNUSED(iCompileFlag);` |
|   25848 |  1593 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1594 |  |
|       - |  1595 | `/*` |
|       - |  1596 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1597 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1598 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1599 | ` */` |
|     450 |  1600 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1601 |  |
|       - |  1602 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     452 |  1603 | `	pGen->pIn++;` |
|     452 |  1604 | `	pGen->pEnd--;` |
|     225 |  1605 | `	SXUNUSED(iCompileFlag);` |
|     452 |  1606 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1607 |  |
|       - |  1608 | `/*` |
|       - |  1609 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1610 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1611 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1612 | ` * error message.` |
|       - |  1613 | ` * See the routine responible of compiling the list language construct` |
|       - |  1614 | ` * for more inforation.` |
|       - |  1615 | ` */` |
|     128 |  1616 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1617 |  |
|     130 |  1618 | `	sxi32 rc = SXRET_OK;` |
|     130 |  1619 | `	if( pRoot->pOp ){` |
|     ! 0 |  1620 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 |  1621 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1622 | `				/* Unexpected expression */` |
|     ! 0 |  1623 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1624 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1625 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1626 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1627 | `				}` |
|     ! 0 |  1628 | `		}` |
|     130 |  1629 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1630 | `		/* Unexpected expression */` |
|       5 |  1631 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1632 | `			"list(): Expecting a variable not an expression");` |
|       5 |  1633 | `		if( rc != SXERR_ABORT ){` |
|       5 |  1634 | `			rc = SXERR_INVALID;` |
|       2 |  1635 | `		}` |
|       2 |  1636 | `	}` |
|     130 |  1637 | `	return rc;` |
|       2 |  1638 |  |
|       - |  1639 | `/*` |
|       - |  1640 | ` * Compile the 'list' language construct.` |
|       - |  1641 | ` *  According to the PHP language reference` |
|       - |  1642 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1643 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1644 | ` *  Description` |
|       - |  1645 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1646 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1647 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1648 | ` *  Parameters` |
|       - |  1649 | ` *   $varname: A variable.` |
|       - |  1650 | ` *  Return Values` |
|       - |  1651 | ` *   The assigned array.` |
|       - |  1652 | ` */` |
|       - |  1653 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1654 | `struct NestedListEntry {` |
|       - |  1655 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1656 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1657 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1658 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1659 | `};` |
|       - |  1660 | `/*` |
|       - |  1661 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1662 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1663 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1664 | ` */` |
|      74 |  1665 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 |  1666 |  |
|       - |  1667 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1668 | `	SyToken *pNext;` |
|       - |  1669 | `	sxi32 nExpr;` |
|       - |  1670 | `	sxi32 rc;` |
|      76 |  1671 | `	nExpr = 0;` |
|      76 |  1672 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 |  1673 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 |  1674 | `		if( pGen->pIn < pNext ){` |
|       - |  1675 | `			/* Check for nested list() */` |
|     144 |  1676 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1677 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1678 | `				/* Record this nested list for post-processing */` |
|       3 |  1679 | `				SyToken *pListEnd = 0;` |
|       3 |  1680 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1681 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1682 | `				}` |
|       3 |  1683 | `				if( pListEnd ){` |
|       - |  1684 | `					struct NestedListEntry sEntry;` |
|       3 |  1685 | `					sEntry.nIndex = nExpr;` |
|       3 |  1686 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1687 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1688 | `					sEntry.isShort = 0;` |
|       3 |  1689 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1690 | `				}` |
|       - |  1691 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1692 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 |  1693 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1694 | `				/* Nested short destructuring [...] */` |
|      13 |  1695 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1696 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1697 | `				if( pBracketEnd ){` |
|       - |  1698 | `					struct NestedListEntry sEntry;` |
|      13 |  1699 | `					sEntry.nIndex = nExpr;` |
|      13 |  1700 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1701 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1702 | `					sEntry.isShort = 1;` |
|      13 |  1703 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1704 | `				}` |
|       - |  1705 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1706 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1707 | `			}else{` |
|       - |  1708 | `				/* Compile the expression holding the variable */` |
|     130 |  1709 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 |  1710 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1711 | `					SySetRelease(&sNested);` |
|     ! 0 |  1712 | `					return SXRET_OK;` |
|       - |  1713 | `				}` |
|       - |  1714 | `			}` |
|      73 |  1715 | `		}else{` |
|       - |  1716 | `			/* Empty entry,load NULL */` |
|      13 |  1717 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1718 | `		}` |
|     156 |  1719 | `		nExpr++;` |
|       - |  1720 | `		/* Advance the stream cursor */` |
|     156 |  1721 | `		pGen->pIn = &pNext[1];` |
|       2 |  1722 | `	}` |
|       - |  1723 | `	/* Emit the LOAD_LIST instruction */` |
|      76 |  1724 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1725 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1726 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1727 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1728 | `	 */` |
|      76 |  1729 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1730 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1731 | `		sxu32 i;` |
|      27 |  1732 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1733 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1734 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1735 | `			ph7_value *pIdx;` |
|       - |  1736 | `			sxu32 nConstIdx;` |
|       - |  1737 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1738 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1739 | `			/* Push the integer index for this nested entry */` |
|      15 |  1740 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1741 | `			if( pIdx == 0 ){` |
|     ! 0 |  1742 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1743 | `				SySetRelease(&sNested);` |
|     ! 0 |  1744 | `				return SXERR_ABORT;` |
|       - |  1745 | `			}` |
|      15 |  1746 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1747 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1748 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1749 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1750 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1751 | `			 */` |
|      15 |  1752 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1753 | `			/* Recursively compile the inner list */` |
|      15 |  1754 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1755 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1756 | `			if( apNested[i].isShort ){` |
|      13 |  1757 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1758 | `			}else{` |
|       3 |  1759 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1760 | `			}` |
|      15 |  1761 | `			pGen->pIn = pSavedIn;` |
|      15 |  1762 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1763 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1764 | `				SySetRelease(&sNested);` |
|     ! 0 |  1765 | `				return SXERR_ABORT;` |
|       - |  1766 | `			}` |
|       - |  1767 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1768 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1769 | `		}` |
|       6 |  1770 | `	}` |
|      76 |  1771 | `	SySetRelease(&sNested);` |
|       - |  1772 | `	/* Node successfully compiled */` |
|      76 |  1773 | `	return SXRET_OK;` |
|      39 |  1774 |  |
|      32 |  1775 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1776 |  |
|       - |  1777 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 |  1778 | `	pGen->pIn += 2;` |
|      34 |  1779 | `	pGen->pEnd--;` |
|      16 |  1780 | `	SXUNUSED(iCompileFlag);` |
|      34 |  1781 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1782 |  |
|      42 |  1783 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1784 |  |
|       - |  1785 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 |  1786 | `	pGen->pIn++;` |
|      44 |  1787 | `	pGen->pEnd--;` |
|      21 |  1788 | `	SXUNUSED(iCompileFlag);` |
|      44 |  1789 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1790 |  |
|       - |  1791 | `/* Forward declarations */` |
|       - |  1792 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1793 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1794 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1795 | `/*` |
|       - |  1796 | ` * Compile an annoynmous function or a closure.` |
|       - |  1797 | ` * According to the PHP language reference` |
|       - |  1798 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1799 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1800 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1801 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1802 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1803 | ` *  Example Anonymous function variable assignment example` |
|       - |  1804 | ` * <?php` |
|       - |  1805 | ` * $greet = function($name)` |
|       - |  1806 | ` * {` |
|       - |  1807 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1808 | ` * };` |
|       - |  1809 | ` * $greet('World');` |
|       - |  1810 | ` * $greet('PHP');` |
|       - |  1811 | ` * ?>` |
|       - |  1812 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1813 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1814 | ` */` |
|     190 |  1815 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1816 |  |
|       - |  1817 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1818 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1819 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1820 | `							  * one thread is allowed to compile the script.` |
|       - |  1821 | `						      */` |
|       - |  1822 | `	ph7_value *pObj;` |
|       - |  1823 | `	SyString sName;` |
|       - |  1824 | `	sxu32 nIdx;` |
|       - |  1825 | `	sxu32 nLen;` |
|       - |  1826 | `	sxi32 rc;` |
|       - |  1827 |  |
|     192 |  1828 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     192 |  1829 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1830 | `		pGen->pIn++;` |
|     ! 0 |  1831 | `	}` |
|       - |  1832 | `	/* Reserve a constant for the lambda */` |
|     192 |  1833 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     192 |  1834 | `	if( pObj == 0 ){` |
|     ! 0 |  1835 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1836 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1837 | `		return SXERR_ABORT;` |
|       - |  1838 | `	}` |
|       - |  1839 | `	/* Generate a unique name */` |
|     192 |  1840 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1841 | `	/* Make sure the generated name is unique */` |
|     192 |  1842 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1843 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1844 | `	}` |
|     192 |  1845 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     192 |  1846 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1847 | `	/* Compile the lambda body */` |
|     192 |  1848 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     192 |  1849 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1850 | `		return SXERR_ABORT;` |
|       - |  1851 | `	}` |
|     192 |  1852 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1853 | `		/* Emit the load closure instruction */` |
|      18 |  1854 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|      10 |  1855 | `	}else{` |
|       - |  1856 | `		/* Emit the load constant instruction */` |
|     176 |  1857 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1858 | `	}` |
|       - |  1859 | `	/* Node successfully compiled */` |
|     192 |  1860 | `	return SXRET_OK;` |
|      97 |  1861 |  |
|       - |  1862 | `/*` |
|       - |  1863 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1864 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1865 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1866 | ` */` |
|     124 |  1867 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1868 | `	ph7_gen_state *pGen,` |
|       - |  1869 | `	ph7_vm_func *pFunc,` |
|       - |  1870 | `	const char *zName,` |
|       - |  1871 | `	sxu32 nByte,` |
|       - |  1872 | `	SyString *aShadow,` |
|       - |  1873 | `	sxu32 nShadow)` |
|       2 |  1874 |  |
|       - |  1875 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1876 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1877 | `	sxu32 n, nEnv;` |
|       - |  1878 | `	char *zDup;` |
|     126 |  1879 | `	if( nByte == 0 ){` |
|     ! 0 |  1880 | `		return SXRET_OK;` |
|       - |  1881 | `	}` |
|     124 |  1882 | `	if( nByte == sizeof("this")-1` |
|      68 |  1883 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1884 | `		return SXRET_OK;` |
|       - |  1885 | `	}` |
|     150 |  1886 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      96 |  1887 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      93 |  1888 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      72 |  1889 | `			return SXRET_OK;` |
|       - |  1890 | `		}` |
|      14 |  1891 | `	}` |
|      53 |  1892 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1893 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1894 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1895 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1896 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1897 | `			return SXRET_OK;` |
|       - |  1898 | `		}` |
|      15 |  1899 | `	}` |
|      53 |  1900 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1901 | `	if( zDup == 0 ){` |
|     ! 0 |  1902 | `		return SXERR_ABORT;` |
|       - |  1903 | `	}` |
|      53 |  1904 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1905 | `	sEnv.iFlags = 0;` |
|      53 |  1906 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1907 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1908 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1909 | `	return SXRET_OK;` |
|      64 |  1910 |  |
|       - |  1911 | `/*` |
|       - |  1912 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1913 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1914 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1915 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1916 | ` */` |
|      14 |  1917 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1918 | `	ph7_gen_state *pGen,` |
|       - |  1919 | `	ph7_vm_func *pFunc,` |
|       - |  1920 | `	const char *zIn,` |
|       - |  1921 | `	const char *zEnd,` |
|       - |  1922 | `	SyString *aShadow,` |
|       - |  1923 | `	sxu32 nShadow)` |
|       1 |  1924 |  |
|       - |  1925 | `	sxi32 rc;` |
|     159 |  1926 | `	while( zIn < zEnd ){` |
|     145 |  1927 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1928 | `			zIn++;` |
|     ! 0 |  1929 | `			if( zIn < zEnd ){` |
|     ! 0 |  1930 | `				zIn++;` |
|     ! 0 |  1931 | `			}` |
|     ! 0 |  1932 | `			continue;` |
|       - |  1933 | `		}` |
|     144 |  1934 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1935 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1936 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1937 | `			const char *zName;` |
|      13 |  1938 | `			zIn++; /* skip '$' */` |
|      13 |  1939 | `			zName = zIn;` |
|      39 |  1940 | `			while( zIn < zEnd ){` |
|      35 |  1941 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1942 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1943 | `					zIn++;` |
|     ! 0 |  1944 | `					while( zIn < zEnd` |
|     ! 0 |  1945 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1946 | `						zIn++;` |
|     ! 0 |  1947 | `					}` |
|     ! 0 |  1948 | `					continue;` |
|       - |  1949 | `				}` |
|      35 |  1950 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1951 | `					break;` |
|       - |  1952 | `				}` |
|      27 |  1953 | `				zIn++;` |
|       1 |  1954 | `			}` |
|      13 |  1955 | `			if( zIn > zName ){` |
|      19 |  1956 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1957 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1958 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1959 | `					return SXERR_ABORT;` |
|       - |  1960 | `				}` |
|       6 |  1961 | `			}` |
|      13 |  1962 | `			continue;` |
|       - |  1963 | `		}` |
|     133 |  1964 | `		zIn++;` |
|       1 |  1965 | `	}` |
|      15 |  1966 | `	return SXRET_OK;` |
|       8 |  1967 |  |
|       - |  1968 | `/*` |
|       - |  1969 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1970 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1971 | ` *   - plain $<id> pairs` |
|       - |  1972 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1973 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1974 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1975 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1976 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1977 | ` *     are never mistakenly captured.` |
|       - |  1978 | ` */` |
|     106 |  1979 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1980 | `	ph7_gen_state *pGen,` |
|       - |  1981 | `	ph7_vm_func *pFunc,` |
|       - |  1982 | `	SyToken *pStart,` |
|       - |  1983 | `	SyToken *pEnd,` |
|       - |  1984 | `	SyString *aShadow,` |
|       - |  1985 | `	sxu32 nShadow)` |
|       2 |  1986 |  |
|     108 |  1987 | `	SyToken *pScan = pStart;` |
|       - |  1988 | `	sxi32 rc;` |
|     398 |  1989 | `	while( pScan < pEnd ){` |
|     292 |  1990 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1991 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1992 | `				pScan->sData.zString,` |
|      14 |  1993 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1994 | `				aShadow,nShadow);` |
|      15 |  1995 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1996 | `				return SXERR_ABORT;` |
|       - |  1997 | `			}` |
|      15 |  1998 | `			pScan++;` |
|      15 |  1999 | `			continue;` |
|       - |  2000 | `		}` |
|     278 |  2001 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  2002 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  2003 | `			SyToken *pFnKw = pScan;` |
|      20 |  2004 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  2005 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  2006 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  2007 | `				pFnKw = &pScan[1];` |
|     ! 0 |  2008 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  2009 | `			}` |
|      21 |  2010 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  2011 | `				SyToken *pInnerSigStart;` |
|       - |  2012 | `				SyToken *pInnerSigEnd;` |
|       - |  2013 | `				SyToken *pInnerBodyEnd;` |
|       - |  2014 | `				SyString *aInnerShadow;` |
|       - |  2015 | `				sxu32 nInnerShadow;` |
|       - |  2016 | `				sxu32 nInnerParamMax;` |
|       - |  2017 | `				SyToken *p;` |
|       - |  2018 | `				int iNestInner;` |
|      19 |  2019 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  2020 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2021 | `					pScan++;` |
|     ! 0 |  2022 | `				}` |
|      19 |  2023 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2024 | `					pScan++;` |
|     ! 0 |  2025 | `					continue;` |
|       - |  2026 | `				}` |
|      19 |  2027 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2028 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2029 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2030 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2031 | `					pScan = pEnd;` |
|     ! 0 |  2032 | `					continue;` |
|       - |  2033 | `				}` |
|       - |  2034 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2035 | `				nInnerParamMax = 0;` |
|      57 |  2036 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2037 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2038 | `						nInnerParamMax++;` |
|       6 |  2039 | `					}` |
|      20 |  2040 | `				}` |
|      19 |  2041 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2042 | `					&pGen->pVm->sAllocator,` |
|      18 |  2043 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2044 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2045 | `					return SXERR_ABORT;` |
|       - |  2046 | `				}` |
|      19 |  2047 | `				nInnerShadow = 0;` |
|      25 |  2048 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2049 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2050 | `				}` |
|      57 |  2051 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2052 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2053 | `						continue;` |
|       - |  2054 | `					}` |
|      13 |  2055 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2056 | `						break;` |
|       - |  2057 | `					}` |
|      13 |  2058 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2059 | `						continue;` |
|       - |  2060 | `					}` |
|      13 |  2061 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2062 | `				}` |
|      19 |  2063 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2064 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2065 | `					pScan++;` |
|     ! 0 |  2066 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2067 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2068 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2069 | `						pScan++;` |
|     ! 0 |  2070 | `					}` |
|     ! 0 |  2071 | `					if( pScan < pEnd` |
|     ! 0 |  2072 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2073 | `						pScan++;` |
|     ! 0 |  2074 | `					}` |
|     ! 0 |  2075 | `				}` |
|      19 |  2076 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2077 | `					pScan++; /* past '=>' */` |
|       9 |  2078 | `				}` |
|      19 |  2079 | `				pInnerBodyEnd = pScan;` |
|      19 |  2080 | `				iNestInner = 0;` |
|     131 |  2081 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2082 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2083 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2084 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2085 | `						break;` |
|       - |  2086 | `					}` |
|     113 |  2087 | `					if( pInnerBodyEnd->nType &` |
|       - |  2088 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2089 | `						iNestInner++;` |
|     112 |  2090 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2091 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2092 | `						iNestInner--;` |
|       1 |  2093 | `					}` |
|     113 |  2094 | `					pInnerBodyEnd++;` |
|       1 |  2095 | `				}` |
|       - |  2096 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2097 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2098 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2099 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2100 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2101 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2102 | `				 *` |
|       - |  2103 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2104 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2105 | `				 * range after the '=' sign. */` |
|       - |  2106 | `				{` |
|      19 |  2107 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2108 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2109 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2110 | `						SyToken *pEq = 0;` |
|      13 |  2111 | `						int iNestArg = 0;` |
|      49 |  2112 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2113 | `							if( iNestArg == 0` |
|      39 |  2114 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2115 | `								break;` |
|       - |  2116 | `							}` |
|      37 |  2117 | `							if( pArgEnd->nType &` |
|       - |  2118 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2119 | `								iNestArg++;` |
|      37 |  2120 | `							}else if( pArgEnd->nType &` |
|       - |  2121 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2122 | `								iNestArg--;` |
|     ! 0 |  2123 | `							}` |
|      36 |  2124 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2125 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2126 | `								pEq = pArgEnd;` |
|       3 |  2127 | `							}` |
|      37 |  2128 | `							pArgEnd++;` |
|       1 |  2129 | `						}` |
|      13 |  2130 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2131 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2132 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2133 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2134 | `								return SXERR_ABORT;` |
|       - |  2135 | `							}` |
|       3 |  2136 | `						}` |
|      13 |  2137 | `						pArgStart = pArgEnd;` |
|      12 |  2138 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2139 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2140 | `							pArgStart++;` |
|       1 |  2141 | `						}` |
|       1 |  2142 | `					}` |
|       - |  2143 | `				}` |
|      28 |  2144 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2145 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2146 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2147 | `					return SXERR_ABORT;` |
|       - |  2148 | `				}` |
|      19 |  2149 | `				pScan = pInnerBodyEnd;` |
|      19 |  2150 | `				continue;` |
|       - |  2151 | `			}` |
|       1 |  2152 | `		}` |
|     260 |  2153 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     148 |  2154 | `			pScan++;` |
|     148 |  2155 | `			continue;` |
|       - |  2156 | `		}` |
|       - |  2157 | `		{` |
|       - |  2158 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     114 |  2159 | `			SyToken *pDollar = pScan;` |
|     168 |  2160 | `			while( &pDollar[1] < pEnd` |
|     114 |  2161 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2162 | `				pDollar++;` |
|     ! 0 |  2163 | `			}` |
|     114 |  2164 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2165 | `				break;` |
|       - |  2166 | `			}` |
|     114 |  2167 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2168 | `				pScan = pDollar + 1;` |
|     ! 0 |  2169 | `				continue;` |
|       - |  2170 | `			}` |
|     170 |  2171 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     112 |  2172 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      56 |  2173 | `				aShadow,nShadow);` |
|     114 |  2174 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2175 | `				return SXERR_ABORT;` |
|       - |  2176 | `			}` |
|     114 |  2177 | `			pScan = pDollar + 2;` |
|       - |  2178 | `		}` |
|       2 |  2179 | `	}` |
|     108 |  2180 | `	return SXRET_OK;` |
|      55 |  2181 |  |
|       - |  2182 | `/*` |
|       - |  2183 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2184 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2185 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2186 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2187 | ` * $this is also made available.` |
|       - |  2188 | ` */` |
|      88 |  2189 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2190 |  |
|       - |  2191 | `	ph7_vm_func *pFunc;` |
|       - |  2192 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2193 | `	GenBlock *pBlock;` |
|       - |  2194 | `	SySet *pInstrContainer;` |
|       - |  2195 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2196 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2197 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2198 | `	SyToken *pSavedEnd;` |
|       - |  2199 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2200 | `	char zName[512];` |
|       - |  2201 | `	static int iCnt = 1;` |
|       - |  2202 | `	char *zDup;` |
|       - |  2203 | `	sxu32 nLen;` |
|       - |  2204 | `	sxu32 nLine;` |
|      90 |  2205 | `	sxi32 iFlags = 0;` |
|      90 |  2206 | `	int bStatic = 0;` |
|       - |  2207 | `	sxi32 rc;` |
|       - |  2208 | `	sxu32 n;` |
|      44 |  2209 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2210 |  |
|      90 |  2211 | `	nLine = pGen->pIn->nLine;` |
|       - |  2212 | `	/* Optional 'static' prefix */` |
|      88 |  2213 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      90 |  2214 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2215 | `		bStatic = 1;` |
|       3 |  2216 | `		pGen->pIn++;` |
|       1 |  2217 | `	}` |
|       - |  2218 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      88 |  2219 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      90 |  2220 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2221 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2222 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2223 | `		return SXERR_SYNTAX;` |
|       - |  2224 | `	}` |
|      90 |  2225 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2226 | `	/* Optional '&' — return by reference */` |
|      90 |  2227 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2228 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2229 | `		pGen->pIn++;` |
|     ! 0 |  2230 | `	}` |
|       - |  2231 | `	/* Expect '(' */` |
|      90 |  2232 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2233 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2234 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2235 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2236 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2237 | `		}else{` |
|     ! 0 |  2238 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2239 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2240 | `		}` |
|       3 |  2241 | `		return SXERR_SYNTAX;` |
|       - |  2242 | `	}` |
|      88 |  2243 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2244 | `	/* Delimit the parameter list */` |
|      88 |  2245 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      88 |  2246 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2247 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2248 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2249 | `		return SXERR_SYNTAX;` |
|       - |  2250 | `	}` |
|       - |  2251 | `	/* Allocate the function state */` |
|      86 |  2252 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      86 |  2253 | `	if( pFunc == 0 ){` |
|     ! 0 |  2254 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2255 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2256 | `		return SXERR_ABORT;` |
|       - |  2257 | `	}` |
|       - |  2258 | `	/* Generate a unique lambda name */` |
|      86 |  2259 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     178 |  2260 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      94 |  2261 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  2262 | `	}` |
|      86 |  2263 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      86 |  2264 | `	if( zDup == 0 ){` |
|     ! 0 |  2265 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2266 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2267 | `		return SXERR_ABORT;` |
|       - |  2268 | `	}` |
|      86 |  2269 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2270 | `	/* Collect function arguments */` |
|      86 |  2271 | `	if( pGen->pIn < pSigEnd ){` |
|      56 |  2272 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      56 |  2273 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2274 | `			return SXERR_ABORT;` |
|       - |  2275 | `		}` |
|      27 |  2276 | `	}` |
|       - |  2277 | `	/* Point past ')' and parse optional return type */` |
|      86 |  2278 | `	pGen->pIn = &pSigEnd[1];` |
|      86 |  2279 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      86 |  2280 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2281 | `		return SXERR_ABORT;` |
|      86 |  2282 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2283 | `		return SXERR_SYNTAX;` |
|       - |  2284 | `	}` |
|       - |  2285 | `	/* Expect '=>' */` |
|      86 |  2286 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2287 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2288 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2289 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2290 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2291 | `		}else{` |
|     ! 0 |  2292 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2293 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2294 | `		}` |
|       3 |  2295 | `		return SXERR_SYNTAX;` |
|       - |  2296 | `	}` |
|      84 |  2297 | `	pGen->pIn++; /* Jump '=>' */` |
|      84 |  2298 | `	pBodyStart = pGen->pIn;` |
|      84 |  2299 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2300 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2301 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2302 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2303 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      84 |  2304 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2305 | `	{` |
|      84 |  2306 | `		SyString *aShadow = 0;` |
|      84 |  2307 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      84 |  2308 | `		if( nShadow > 0 ){` |
|      54 |  2309 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      52 |  2310 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      54 |  2311 | `			if( aShadow == 0 ){` |
|     ! 0 |  2312 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2313 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2314 | `				return SXERR_ABORT;` |
|       - |  2315 | `			}` |
|     112 |  2316 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      60 |  2317 | `				aShadow[n] = aArgs[n].sName;` |
|      31 |  2318 | `			}` |
|      26 |  2319 | `		}` |
|     125 |  2320 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      41 |  2321 | `			aShadow,nShadow);` |
|      84 |  2322 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2323 | `			return SXERR_ABORT;` |
|       - |  2324 | `		}` |
|       - |  2325 | `	}` |
|       - |  2326 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2327 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2328 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2329 | `	 * $this. */` |
|      84 |  2330 | `	if( !bStatic ){` |
|       - |  2331 | `		char *zThisDup;` |
|      82 |  2332 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      82 |  2333 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2334 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2335 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2336 | `			return SXERR_ABORT;` |
|       - |  2337 | `		}` |
|      82 |  2338 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      82 |  2339 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      82 |  2340 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      82 |  2341 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      82 |  2342 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      40 |  2343 | `	}` |
|       - |  2344 | `	/* Arrow functions are always closures */` |
|      84 |  2345 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2346 | `	/* Compile the body expression as an implicit return */` |
|     125 |  2347 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      41 |  2348 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      84 |  2349 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2350 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2351 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2352 | `		return SXERR_ABORT;` |
|       - |  2353 | `	}` |
|      84 |  2354 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      84 |  2355 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      84 |  2356 | `	pSavedEnd = pGen->pEnd;` |
|      84 |  2357 | `	pGen->pIn = pBodyStart;` |
|      84 |  2358 | `	pGen->pEnd = pBodyEnd;` |
|      84 |  2359 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      84 |  2360 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2361 | `		return SXERR_ABORT;` |
|       - |  2362 | `	}` |
|       - |  2363 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2364 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2365 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2366 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      84 |  2367 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      84 |  2368 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      84 |  2369 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      84 |  2370 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      84 |  2371 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2372 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      84 |  2373 | `	pGen->pIn = pBodyEnd;` |
|      84 |  2374 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2375 | `	/* Emit the load-closure instruction */` |
|      84 |  2376 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      84 |  2377 | `	return SXRET_OK;` |
|      46 |  2378 |  |
|       - |  2379 | `/*` |
|       - |  2380 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2381 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2382 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2383 | ` * expression's value.` |
|       - |  2384 | ` */` |
|     346 |  2385 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2386 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       2 |  2387 |  |
|       - |  2388 | `	SySet *pInstrContainer;` |
|       - |  2389 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2390 | `	GenBlock *pArmBlock;` |
|       - |  2391 | `	sxi32 rc;` |
|     348 |  2392 | `	pTmpIn  = pGen->pIn;` |
|     348 |  2393 | `	pTmpEnd = pGen->pEnd;` |
|     348 |  2394 | `	pGen->pIn  = pStart;` |
|     348 |  2395 | `	pGen->pEnd = pStop;` |
|     348 |  2396 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     348 |  2397 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2398 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2399 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2400 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2401 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2402 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     521 |  2403 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2404 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     348 |  2405 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2406 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2407 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2408 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2409 | `		return SXERR_ABORT;` |
|       - |  2410 | `	}` |
|     348 |  2411 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     348 |  2412 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     348 |  2413 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     348 |  2414 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     348 |  2415 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     348 |  2416 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     348 |  2417 | `	pGen->pIn  = pTmpIn;` |
|     348 |  2418 | `	pGen->pEnd = pTmpEnd;` |
|     348 |  2419 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2420 | `		return SXERR_ABORT;` |
|       - |  2421 | `	}` |
|     348 |  2422 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2423 | `		return SXERR_EMPTY;` |
|       - |  2424 | `	}` |
|     348 |  2425 | `	return SXRET_OK;` |
|     175 |  2426 |  |
|       - |  2427 | `/*` |
|       - |  2428 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2429 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2430 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2431 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2432 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2433 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2434 | ` */` |
|       - |  2435 | `/*` |
|       - |  2436 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2437 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2438 | ` * caller can bail out of the current expression.` |
|       - |  2439 | ` */` |
|       2 |  2440 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2441 |  |
|       - |  2442 | `	va_list ap;` |
|       - |  2443 | `	sxi32 rc;` |
|       - |  2444 | `	SyBlob sMsg;` |
|       3 |  2445 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2446 | `	va_start(ap,zFmt);` |
|       3 |  2447 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2448 | `	va_end(ap);` |
|       3 |  2449 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2450 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2451 | `	SyBlobRelease(&sMsg);` |
|       3 |  2452 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2453 | `		return SXERR_ABORT;` |
|       - |  2454 | `	}` |
|       3 |  2455 | `	return SXERR_SYNTAX;` |
|       2 |  2456 |  |
|       - |  2457 | `/*` |
|       - |  2458 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2459 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2460 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2461 | ` */` |
|     348 |  2462 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       2 |  2463 |  |
|     350 |  2464 | `	SyToken *pCur = pStart;` |
|     350 |  2465 | `	int iNest = 0;` |
|     812 |  2466 | `	while( pCur < pEnd ){` |
|     778 |  2467 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2468 | `			iNest++;` |
|     772 |  2469 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2470 | `			iNest--;` |
|     760 |  2471 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     316 |  2472 | `			return pCur;` |
|       - |  2473 | `		}` |
|     464 |  2474 | `		pCur++;` |
|       2 |  2475 | `	}` |
|      36 |  2476 | `	return pEnd;` |
|     176 |  2477 |  |
|      70 |  2478 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2479 |  |
|       - |  2480 | `	ph7_match *pMatch;` |
|       - |  2481 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      72 |  2482 | `	int bHasDefault = 0;` |
|       - |  2483 | `	sxu32 nLine;` |
|       - |  2484 | `	sxi32 rc;` |
|      35 |  2485 | `	SXUNUSED(iCompileFlag);` |
|      72 |  2486 | `	nLine = pGen->pIn->nLine;` |
|      72 |  2487 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2488 | `	/* Expect '(' */` |
|      72 |  2489 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2490 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2491 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2492 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2493 | `	}` |
|      72 |  2494 | `	pGen->pIn++; /* Jump '(' */` |
|      72 |  2495 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      72 |  2496 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2497 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2498 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2499 | `	}` |
|      72 |  2500 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2501 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2502 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2503 | `	}` |
|       - |  2504 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      72 |  2505 | `	pSavedEnd = pGen->pEnd;` |
|      72 |  2506 | `	pGen->pEnd = pSubjEnd;` |
|      72 |  2507 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      72 |  2508 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2509 | `		return SXERR_ABORT;` |
|       - |  2510 | `	}` |
|      72 |  2511 | `	pGen->pEnd = pSavedEnd;` |
|      72 |  2512 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2513 | `	/* Expect '{' */` |
|      72 |  2514 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2515 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2516 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2517 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2518 | `	}` |
|      72 |  2519 | `	pGen->pIn++; /* Jump '{' */` |
|      72 |  2520 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      72 |  2521 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2522 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2523 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2524 | `	}` |
|       - |  2525 | `	/* Allocate ph7_match container */` |
|      72 |  2526 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      72 |  2527 | `	if( pMatch == 0 ){` |
|     ! 0 |  2528 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2529 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2530 | `		return SXERR_ABORT;` |
|       - |  2531 | `	}` |
|      72 |  2532 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      72 |  2533 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2534 | `	/* Iterate arms */` |
|     250 |  2535 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2536 | `		ph7_match_arm sArm;` |
|       - |  2537 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     184 |  2538 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     184 |  2539 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     184 |  2540 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     184 |  2541 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2542 | `		/* 'default' arm? */` |
|     182 |  2543 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     103 |  2544 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2545 | `			if( bHasDefault ){` |
|       3 |  2546 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2547 | `					"Match expressions may only contain one default arm");` |
|       4 |  2548 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2549 | `			}` |
|      20 |  2550 | `			sArm.bDefault = 1;` |
|      20 |  2551 | `			bHasDefault = 1;` |
|      20 |  2552 | `			pGen->pIn++;` |
|      20 |  2553 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2554 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2555 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2556 | `			}` |
|      20 |  2557 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2558 | `		}else{` |
|       - |  2559 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     164 |  2560 | `			pCondStart = pGen->pIn;` |
|     164 |  2561 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2562 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     172 |  2563 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2564 | `				SySet sCondBc;` |
|       9 |  2565 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2566 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2567 | `						"syntax error, empty match condition expression");` |
|       - |  2568 | `				}` |
|       9 |  2569 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2570 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2571 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2572 | `					return SXERR_ABORT;` |
|       - |  2573 | `				}` |
|       9 |  2574 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2575 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2576 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2577 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2578 | `			}` |
|     164 |  2579 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2580 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2581 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2582 | `			}` |
|     162 |  2583 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2584 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2585 | `					"syntax error, empty match condition expression");` |
|       - |  2586 | `			}` |
|       - |  2587 | `			{` |
|       - |  2588 | `				SySet sCondBc;` |
|     162 |  2589 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     162 |  2590 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     162 |  2591 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2592 | `					return SXERR_ABORT;` |
|       - |  2593 | `				}` |
|     162 |  2594 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2595 | `			}` |
|     162 |  2596 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2597 | `		}` |
|       - |  2598 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     180 |  2599 | `		pResStart = pGen->pIn;` |
|     180 |  2600 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     180 |  2601 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2602 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2603 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2604 | `		}` |
|     180 |  2605 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     180 |  2606 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2607 | `			return SXERR_ABORT;` |
|       - |  2608 | `		}` |
|     180 |  2609 | `		pGen->pIn = pResEnd;` |
|     180 |  2610 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     148 |  2611 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2612 | `		}` |
|     180 |  2613 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       2 |  2614 | `	}` |
|      68 |  2615 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      68 |  2616 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      68 |  2617 | `	return SXRET_OK;` |
|      37 |  2618 |  |
|       - |  2619 | `/*` |
|       - |  2620 | ` * Compile a backtick quoted string.` |
|       - |  2621 | ` */` |
|       4 |  2622 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  2623 |  |
|       - |  2624 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2625 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2626 | `	 */` |
|       7 |  2627 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2628 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2629 | `		ph7_lib_version()` |
|       - |  2630 | `		);` |
|       - |  2631 | `	/* Load NULL */` |
|       5 |  2632 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2633 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2634 | `	/* Node successfully compiled */` |
|       5 |  2635 | `	return SXRET_OK;` |
|       1 |  2636 |  |
|       - |  2637 | `/*` |
|       - |  2638 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2639 | ` * construct.` |
|       - |  2640 | ` */` |
|      74 |  2641 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2642 |  |
|       - |  2643 | `	SyString *pName;` |
|       - |  2644 | `	sxu32 nKeyID;` |
|       - |  2645 | `	sxi32 rc;` |
|       - |  2646 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      76 |  2647 | `	pName = &pGen->pIn->sData;` |
|      76 |  2648 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  2649 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      76 |  2650 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2651 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2652 | `		/* Compile arguments one after one */` |
|       9 |  2653 | `		pTmp = pGen->pEnd;` |
|       - |  2654 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2655 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2656 | `		 *  mean that the following expression is valid:` |
|       - |  2657 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2658 | `		 */` |
|       9 |  2659 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2660 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2661 | `			if( pGen->pIn < pNext ){` |
|       9 |  2662 | `				pGen->pEnd = pNext;` |
|       9 |  2663 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2664 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2665 | `					return SXERR_ABORT;` |
|       - |  2666 | `				}` |
|       9 |  2667 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2668 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2669 | `					 * without the overhead of a function call.` |
|       - |  2670 | `					 * This is a very powerful optimization that improve` |
|       - |  2671 | `					 * performance greatly.` |
|       - |  2672 | `					 */` |
|       9 |  2673 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2674 | `				}` |
|       4 |  2675 | `			}` |
|       - |  2676 | `			/* Jump trailing commas */` |
|       9 |  2677 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2678 | `				pNext++;` |
|     ! 0 |  2679 | `			}` |
|       9 |  2680 | `			pGen->pIn = pNext;` |
|       1 |  2681 | `		}` |
|       - |  2682 | `		/* Restore token stream */` |
|       9 |  2683 | `		pGen->pEnd = pTmp;` |
|       5 |  2684 | `	}else{` |
|      68 |  2685 | `		sxi32 nArg = 0;` |
|      68 |  2686 | `		sxu32 nIdx = 0;` |
|      68 |  2687 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      68 |  2688 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2689 | `			return SXERR_ABORT;` |
|      68 |  2690 | `		}else if(rc != SXERR_EMPTY ){` |
|      68 |  2691 | `			nArg = 1;` |
|      33 |  2692 | `		}` |
|      68 |  2693 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2694 | `			ph7_value *pObj;` |
|       - |  2695 | `			/* Emit the call instruction */` |
|      20 |  2696 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2697 | `			if( pObj == 0 ){` |
|     ! 0 |  2698 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2699 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2700 | `				return SXERR_ABORT;` |
|       - |  2701 | `			}` |
|      20 |  2702 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2703 | `			/* Install in the literal table */` |
|      20 |  2704 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 |  2705 | `		}` |
|       - |  2706 | `		/* Emit the call instruction */` |
|      68 |  2707 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      68 |  2708 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2709 | `	}` |
|       - |  2710 | `	/* Node successfully compiled */` |
|      76 |  2711 | `	return SXRET_OK;` |
|      39 |  2712 |  |
|       - |  2713 | `/*` |
|       - |  2714 | ` * Compile a node holding a variable declaration.` |
|       - |  2715 | ` * According to the PHP language reference` |
|       - |  2716 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2717 | ` *  The variable name is case-sensitive.` |
|       - |  2718 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2719 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2720 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2721 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2722 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2723 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2724 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2725 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2726 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2727 | ` *  the chapter on Expressions.` |
|       - |  2728 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2729 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2730 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2731 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2732 | ` *  is being assigned (the source variable).` |
|       - |  2733 | ` */` |
|  941972 |  2734 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2735 |  |
|  941974 |  2736 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2737 | `	sxi32 iVv;` |
|       - |  2738 | `	sxi32 iP1;` |
|       - |  2739 | `	void *p3;` |
|       - |  2740 | `	sxi32 rc;` |
|  941974 |  2741 | `	iVv = -1; /* Variable variable counter */` |
| 1883958 |  2742 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  941986 |  2743 | `		pGen->pIn++;` |
|  941986 |  2744 | `		iVv++;` |
|       2 |  2745 | `	}` |
|  941974 |  2746 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2747 | `		/* Invalid variable name */` |
|     ! 0 |  2748 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2749 | `		if( rc == SXERR_ABORT ){` |
|       - |  2750 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2751 | `			return SXERR_ABORT;` |
|       - |  2752 | `		}` |
|     ! 0 |  2753 | `		return SXRET_OK;` |
|       - |  2754 | `	}` |
|  941974 |  2755 | `	p3  = 0;` |
|  941974 |  2756 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2757 | `		/* Dynamic variable creation */` |
|      18 |  2758 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 |  2759 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 |  2760 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2761 | `			/* Empty expression */` |
|       3 |  2762 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2763 | `			return SXRET_OK;` |
|       - |  2764 | `		}` |
|       - |  2765 | `		/* Compile the expression holding the variable name */` |
|      16 |  2766 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2767 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2768 | `			return SXERR_ABORT;` |
|      16 |  2769 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2770 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2771 | `			return SXRET_OK;` |
|       - |  2772 | `		}` |
|       7 |  2773 | `	}else{` |
|       - |  2774 | `		SyHashEntry *pEntry;` |
|       - |  2775 | `		SyString *pName;` |
|  941958 |  2776 | `		char *zName = 0;` |
|       - |  2777 | `		/* Extract variable name */` |
|  941958 |  2778 | `		pName = &pGen->pIn->sData;` |
|       - |  2779 | `		/* Advance the stream cursor */` |
|  941958 |  2780 | `		pGen->pIn++;` |
|  941958 |  2781 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  941958 |  2782 | `		if( pEntry == 0 ){` |
|       - |  2783 | `			/* Duplicate name */` |
|  126340 |  2784 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  126340 |  2785 | `			if( zName == 0 ){` |
|     ! 0 |  2786 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2787 | `				return SXERR_ABORT;` |
|       - |  2788 | `			}` |
|       - |  2789 | `			/* Install in the hashtable */` |
|  126340 |  2790 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   63171 |  2791 | `		}else{` |
|       - |  2792 | `			/* Name already available */` |
|  815620 |  2793 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2794 | `		}` |
|  941958 |  2795 | `		p3 = (void *)zName;` |
|       - |  2796 | `	}` |
|  941970 |  2797 | `	iP1 = 0;` |
|  941970 |  2798 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  343378 |  2799 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2800 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  336792 |  2801 | `			iP1 = 1;` |
|  168395 |  2802 | `		}` |
|  171688 |  2803 | `	}` |
|       - |  2804 | `	/* Emit the load instruction */` |
|  941970 |  2805 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  941982 |  2806 | `	while( iVv > 0 ){` |
|      13 |  2807 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2808 | `		iVv--;` |
|       1 |  2809 | `	}` |
|       - |  2810 | `	/* Node successfully compiled */` |
|  941970 |  2811 | `	return SXRET_OK;` |
|  470988 |  2812 |  |
|       - |  2813 | `/*` |
|       - |  2814 | ` * Load a literal.` |
|       - |  2815 | ` */` |
|  661942 |  2816 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2817 |  |
|  661944 |  2818 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2819 | `	ph7_value *pObj;` |
|       - |  2820 | `	SyString *pStr;` |
|       - |  2821 | `	sxu32 nIdx;` |
|       - |  2822 | `	/* Extract token value */` |
|  661944 |  2823 | `	pStr = &pToken->sData;` |
|       - |  2824 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  661944 |  2825 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  140268 |  2826 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2827 | `			/* NULL constant are always indexed at 0 */` |
|   51668 |  2828 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   51668 |  2829 | `			return SXRET_OK;` |
|   88602 |  2830 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2831 | `			/* TRUE constant are always indexed at 1 */` |
|     550 |  2832 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     550 |  2833 | `			return SXRET_OK;` |
|       2 |  2834 | `		}` |
|  618183 |  2835 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  104958 |  2836 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2837 | `			/* FALSE constant are always indexed at 2 */` |
|   39682 |  2838 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   39682 |  2839 | `			return SXRET_OK;` |
|  529073 |  2840 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   94150 |  2841 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2842 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    9022 |  2843 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    9022 |  2844 | `			if( pObj == 0 ){` |
|     ! 0 |  2845 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2846 | `				return SXERR_ABORT;` |
|       - |  2847 | `			}` |
|    9022 |  2848 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2849 | `			/* Emit the load constant instruction */` |
|    9022 |  2850 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    9022 |  2851 | `			return SXRET_OK;` |
|  488182 |  2852 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   30408 |  2853 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2854 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2855 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2856 | `			if( pObj == 0 ){` |
|     ! 0 |  2857 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2858 | `				return SXERR_ABORT;` |
|       - |  2859 | `			}` |
|       7 |  2860 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2861 | `				SyString sNs;` |
|       7 |  2862 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2863 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2864 | `			}else{` |
|     ! 0 |  2865 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2866 | `			}` |
|       7 |  2867 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2868 | `			return SXRET_OK;` |
|  487322 |  2869 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   12730 |  2870 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  480951 |  2871 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   15976 |  2872 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2873 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2874 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2875 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2876 | `				/* Point to the upper block */` |
|      11 |  2877 | `				pBlock = pBlock->pParent;` |
|       1 |  2878 | `			}` |
|      11 |  2879 | `			if( pBlock == 0 ){` |
|       - |  2880 | `				/* Called in the global scope,load NULL */` |
|       5 |  2881 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2882 | `			}else{` |
|       - |  2883 | `				/* Extract the target function/method */` |
|       7 |  2884 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2885 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2886 | `					/* Not a class method,Load null */` |
|       3 |  2887 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2888 | `				}else{` |
|       5 |  2889 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2890 | `					if( pObj == 0 ){` |
|     ! 0 |  2891 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2892 | `						return SXERR_ABORT;` |
|       - |  2893 | `					}` |
|       5 |  2894 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2895 | `					/* Emit the load constant instruction */` |
|       5 |  2896 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2897 | `				}` |
|       - |  2898 | `			}` |
|      11 |  2899 | `			return SXRET_OK;` |
|       - |  2900 | `	}` |
|       - |  2901 | `	/* Query literal table */` |
|  561014 |  2902 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2903 | `		ph7_value *pLitObj;` |
|       - |  2904 | `		/* Unknown literal,install it in the literal table */` |
|  232922 |  2905 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  232922 |  2906 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2907 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2908 | `			return SXERR_ABORT;` |
|       - |  2909 | `		}` |
|  232922 |  2910 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  232922 |  2911 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  116460 |  2912 | `	}` |
|       - |  2913 | `	/* Emit the load constant instruction */` |
|  561014 |  2914 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  561014 |  2915 | `	return SXRET_OK;` |
|  330973 |  2916 |  |
|       - |  2917 | `/*` |
|       - |  2918 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2919 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2920 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2921 | ` * Otherwise, load the simple literal directly.` |
|       - |  2922 | ` */` |
|  661976 |  2923 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2924 |  |
|       - |  2925 | `	sxi32 rc;` |
|  661978 |  2926 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2927 | `		return SXRET_OK;` |
|       - |  2928 | `	}` |
|       - |  2929 | `	/* Check if this is a multi-token namespace path */` |
|  661978 |  2930 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2931 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      36 |  2932 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      36 |  2933 | `		int isAbsolute = 0;` |
|      36 |  2934 | `		SyBlobReset(pWorker);` |
|       - |  2935 | `		/* Check for leading backslash (absolute path) */` |
|      36 |  2936 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      34 |  2937 | `			isAbsolute = 1;` |
|      34 |  2938 | `			pGen->pIn++; /* Skip leading backslash */` |
|      16 |  2939 | `		}` |
|       - |  2940 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      36 |  2941 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2942 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2943 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2944 | `		}` |
|       - |  2945 | `		/* Collect all path components */` |
|     132 |  2946 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     132 |  2947 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      50 |  2948 | `				SyBlobAppend(pWorker,"\\",1);` |
|      26 |  2949 | `			}else{` |
|      84 |  2950 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2951 | `			}` |
|     132 |  2952 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      36 |  2953 | `				pGen->pIn++;` |
|      36 |  2954 | `				break;` |
|       - |  2955 | `			}` |
|      98 |  2956 | `			pGen->pIn++;` |
|       2 |  2957 | `		}` |
|      36 |  2958 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2959 | `			ph7_value *pObj;` |
|       - |  2960 | `			SyString sPath;` |
|       - |  2961 | `			sxu32 nIdx;` |
|      36 |  2962 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2963 | `			/* Install in the literal table */` |
|      36 |  2964 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      18 |  2965 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 |  2966 | `				if( pObj == 0 ){` |
|     ! 0 |  2967 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2968 | `					return SXERR_ABORT;` |
|       - |  2969 | `				}` |
|      18 |  2970 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      18 |  2971 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  2972 | `			}` |
|       - |  2973 | `			/* Emit the load constant instruction.` |
|       - |  2974 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  2975 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      53 |  2976 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      17 |  2977 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      17 |  2978 | `				nIdx,0,0);` |
|      36 |  2979 | `			return SXRET_OK;` |
|       - |  2980 | `		}` |
|     ! 0 |  2981 | `	}` |
|       - |  2982 | `	/* Single-token literal: load directly */` |
|  661944 |  2983 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  661944 |  2984 | `	return rc;` |
|  330990 |  2985 |  |
|       - |  2986 | `/*` |
|       - |  2987 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2988 | ` */` |
|  661976 |  2989 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2990 |  |
|       - |  2991 | `	sxi32 rc;` |
|  661978 |  2992 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  661978 |  2993 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2994 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2995 | `		return rc;` |
|       - |  2996 | `	}` |
|       - |  2997 | `	/* Node successfully compiled */` |
|  661978 |  2998 | `	return SXRET_OK;` |
|  330990 |  2999 |  |
|       - |  3000 | `/*` |
|       - |  3001 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  3002 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  3003 | ` */` |
|       8 |  3004 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  3005 |  |
|       - |  3006 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  3007 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  3008 | `		pGen->pIn++;` |
|       1 |  3009 | `	}` |
|       9 |  3010 | `	return SXRET_OK;` |
|       1 |  3011 |  |
|       - |  3012 | `/*` |
|       - |  3013 | ` * Check if the given identifier name is reserved or not.` |
|       - |  3014 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  3015 | ` */` |
|      56 |  3016 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 |  3017 |  |
|      58 |  3018 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 |  3019 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  3020 | `			return TRUE;` |
|      24 |  3021 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  3022 | `			return TRUE;` |
|       2 |  3023 | `		}` |
|      43 |  3024 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3025 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3026 | `			return TRUE;` |
|       - |  3027 | `		}` |
|     ! 0 |  3028 | `	}` |
|       - |  3029 | `	/* Not a reserved constant */` |
|      50 |  3030 | `	return FALSE;` |
|      30 |  3031 |  |
|       - |  3032 | `/*` |
|       - |  3033 | ` * Compile the 'const' statement.` |
|       - |  3034 | ` * According to the PHP language reference` |
|       - |  3035 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3036 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3037 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3038 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3039 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3040 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3041 | ` *  Syntax` |
|       - |  3042 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3043 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3044 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3045 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3046 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3047 | ` *  to get a list of all defined constants.` |
|       - |  3048 | ` *` |
|       - |  3049 | ` * Symisc eXtension.` |
|       - |  3050 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3051 | ` *  would allow only simple scalar value.` |
|       - |  3052 | ` *  Example` |
|       - |  3053 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3054 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3055 | ` */` |
|      32 |  3056 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 |  3057 |  |
|       - |  3058 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 |  3059 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3060 | `	SyString *pName;` |
|       - |  3061 | `	sxi32 rc;` |
|      34 |  3062 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 |  3063 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3064 | `		/* Invalid constant name */` |
|       7 |  3065 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  3066 | `		if( rc == SXERR_ABORT ){` |
|       - |  3067 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3068 | `			return SXERR_ABORT;` |
|       - |  3069 | `		}` |
|       7 |  3070 | `		goto Synchronize;` |
|       - |  3071 | `	}` |
|       - |  3072 | `	/* Peek constant name */` |
|      28 |  3073 | `	pName = &pGen->pIn->sData;` |
|       - |  3074 | `	/* Make sure the constant name isn't reserved */` |
|      28 |  3075 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3076 | `		/* Reserved constant */` |
|       9 |  3077 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3078 | `		if( rc == SXERR_ABORT ){` |
|       - |  3079 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3080 | `			return SXERR_ABORT;` |
|       - |  3081 | `		}` |
|       9 |  3082 | `		goto Synchronize;` |
|       - |  3083 | `	}` |
|      20 |  3084 | `	pGen->pIn++;` |
|      20 |  3085 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3086 | `		/* Invalid statement*/` |
|       5 |  3087 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 |  3088 | `		if( rc == SXERR_ABORT ){` |
|       - |  3089 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3090 | `			return SXERR_ABORT;` |
|       - |  3091 | `		}` |
|       5 |  3092 | `		goto Synchronize;` |
|       - |  3093 | `	}` |
|      15 |  3094 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3095 | `	/* Allocate a new constant value container */` |
|      15 |  3096 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3097 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3098 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3099 | `		return SXERR_ABORT;` |
|       - |  3100 | `	}` |
|      15 |  3101 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3102 | `	/* Swap bytecode container */` |
|      15 |  3103 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3104 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3105 | `	/* Compile constant value */` |
|      15 |  3106 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3107 | `	/* Emit the done instruction */` |
|      15 |  3108 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3109 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3110 | `	if( rc == SXERR_ABORT ){` |
|       - |  3111 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3112 | `		return SXERR_ABORT;` |
|       - |  3113 | `	}` |
|      15 |  3114 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3115 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3116 | `	{` |
|       - |  3117 | `		SyBlob sFQN;` |
|       - |  3118 | `		SyString sFQNStr;` |
|      15 |  3119 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3120 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3121 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3122 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3123 | `		SyBlobRelease(&sFQN);` |
|       - |  3124 | `	}` |
|      15 |  3125 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3126 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3127 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3128 | `	}` |
|      15 |  3129 | `	return SXRET_OK;` |
|       9 |  3130 | `Synchronize:` |
|       - |  3131 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 |  3132 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 |  3133 | `		pGen->pIn++;` |
|       1 |  3134 | `	}` |
|      19 |  3135 | `	return SXRET_OK;` |
|      18 |  3136 |  |
|       - |  3137 | `/*` |
|       - |  3138 | ` * Compile the 'continue' statement.` |
|       - |  3139 | ` * According to the PHP language reference` |
|       - |  3140 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3141 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3142 | ` *  iteration.` |
|       - |  3143 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3144 | ` *  the purposes of continue.` |
|       - |  3145 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3146 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3147 | ` *  Note:` |
|       - |  3148 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3149 | ` */` |
|       - |  3150 | `/*` |
|       - |  3151 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3152 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3153 | ` * break/continue crosses a try boundary.` |
|       - |  3154 | ` *` |
|       - |  3155 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3156 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3157 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3158 | ` */` |
|    3142 |  3159 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3160 |  |
|    3144 |  3161 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   18382 |  3162 | `	while( pBlock && pBlock != pTarget ){` |
|   15240 |  3163 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3164 | `			if( pBlock->pUserData ){` |
|       - |  3165 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3166 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3167 | `			}else{` |
|       - |  3168 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3169 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3170 | `				 * exception context from a sub-execution.` |
|       - |  3171 | `				 */` |
|     ! 0 |  3172 | `				break;` |
|       - |  3173 | `			}` |
|       1 |  3174 | `		}` |
|   15240 |  3175 | `		pBlock = pBlock->pParent;` |
|       2 |  3176 | `	}` |
|    3144 |  3177 |  |
|    3050 |  3178 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3179 |  |
|       - |  3180 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3181 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3182 | `	sxu32 nLineLocal;` |
|       - |  3183 | `	sxi32 rc;` |
|    3052 |  3184 | `	nLineLocal = pGen->pIn->nLine;` |
|    3052 |  3185 | `	iLevel = 0;` |
|       - |  3186 | `	/* Jump the 'continue' keyword */` |
|    3052 |  3187 | `	pGen->pIn++;` |
|    3052 |  3188 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3189 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3190 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3191 | `		 */` |
|       - |  3192 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3193 | `		char *zAlloc = 0;` |
|       - |  3194 | `		SyString sNum;` |
|      16 |  3195 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3196 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3197 | `			return SXERR_ABORT;` |
|       - |  3198 | `		}` |
|      16 |  3199 | `		if( rc == SXRET_OK ){` |
|      20 |  3200 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3201 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3202 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3203 | `				return SXERR_ABORT;` |
|       - |  3204 | `			}` |
|      14 |  3205 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3206 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3207 | `		}` |
|      16 |  3208 | `		if( iLevel < 2 ){` |
|       3 |  3209 | `			iLevel = 0;` |
|       1 |  3210 | `		}` |
|      16 |  3211 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3212 | `	}` |
|       - |  3213 | `	/* Point to the target loop */` |
|    3052 |  3214 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    3052 |  3215 | `	if( pLoop == 0 ){` |
|       - |  3216 | `		/* Illegal continue */` |
|      11 |  3217 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3218 | `		if( rc == SXERR_ABORT ){` |
|       - |  3219 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3220 | `			return SXERR_ABORT;` |
|       - |  3221 | `		}` |
|       6 |  3222 | `	}else{` |
|    3042 |  3223 | `		sxu32 nInstrIdx = 0;` |
|       - |  3224 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    3042 |  3225 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    3042 |  3226 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3227 | `			/* According to the PHP language reference manual` |
|       - |  3228 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3229 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3230 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3231 | `			 */` |
|       5 |  3232 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3233 | `			if( rc == SXRET_OK ){` |
|       5 |  3234 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3235 | `			}` |
|       3 |  3236 | `		}else{` |
|       - |  3237 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    3038 |  3238 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    3038 |  3239 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3240 | `				JumpFixup sJumpFix;` |
|       - |  3241 | `				/* Post-continue */` |
|      14 |  3242 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3243 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3244 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3245 | `			}` |
|       - |  3246 | `		}` |
|       - |  3247 | `	}` |
|    3052 |  3248 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3249 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3250 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3251 | `	}` |
|       - |  3252 | `	/* Statement successfully compiled */` |
|    3052 |  3253 | `	return SXRET_OK;` |
|    1527 |  3254 |  |
|       - |  3255 | `/*` |
|       - |  3256 | ` * Compile the 'break' statement.` |
|       - |  3257 | ` * According to the PHP language reference` |
|       - |  3258 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3259 | ` *  structure.` |
|       - |  3260 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3261 | ` *  enclosing structures are to be broken out of.` |
|       - |  3262 | ` */` |
|     118 |  3263 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 |  3264 |  |
|       - |  3265 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3266 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3267 | `	sxi32 rc;` |
|     120 |  3268 | `	iLevel = 0;` |
|       - |  3269 | `	/* Jump the 'break' keyword */` |
|     120 |  3270 | `	pGen->pIn++;` |
|     120 |  3271 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3272 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3273 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3274 | `		 */` |
|       - |  3275 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3276 | `		char *zAlloc = 0;` |
|       - |  3277 | `		SyString sNum;` |
|      16 |  3278 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3279 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3280 | `			return SXERR_ABORT;` |
|       - |  3281 | `		}` |
|      16 |  3282 | `		if( rc == SXRET_OK ){` |
|      20 |  3283 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3284 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3285 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3286 | `				return SXERR_ABORT;` |
|       - |  3287 | `			}` |
|      14 |  3288 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3289 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3290 | `		}` |
|      16 |  3291 | `		if( iLevel < 2 ){` |
|       3 |  3292 | `			iLevel = 0;` |
|       1 |  3293 | `		}` |
|      16 |  3294 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3295 | `	}` |
|       - |  3296 | `	/* Extract the target loop */` |
|     120 |  3297 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     120 |  3298 | `	if( pLoop == 0 ){` |
|       - |  3299 | `		/* Illegal break */` |
|      17 |  3300 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 |  3301 | `		if( rc == SXERR_ABORT ){` |
|       - |  3302 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3303 | `			return SXERR_ABORT;` |
|       - |  3304 | `		}` |
|       9 |  3305 | `	}else{` |
|       - |  3306 | `		sxu32 nInstrIdx;` |
|       - |  3307 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|     104 |  3308 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|     104 |  3309 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|     104 |  3310 | `		if( rc == SXRET_OK ){` |
|       - |  3311 | `			/* Fix the jump later when the jump destination is resolved */` |
|     104 |  3312 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      51 |  3313 | `		}` |
|       - |  3314 | `	}` |
|     120 |  3315 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3316 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3317 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3318 | `	}` |
|       - |  3319 | `	/* Statement successfully compiled */` |
|     120 |  3320 | `	return SXRET_OK;` |
|      61 |  3321 |  |
|       - |  3322 | `/*` |
|       - |  3323 | ` * Compile or record a label.` |
|       - |  3324 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3325 | ` * Example` |
|       - |  3326 | ` *  goto LABEL;` |
|       - |  3327 | ` *   echo 'Foo';` |
|       - |  3328 | ` *  LABEL:` |
|       - |  3329 | ` *   echo 'Bar';` |
|       - |  3330 | ` */` |
|     112 |  3331 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 |  3332 |  |
|       - |  3333 | `	GenBlock *pBlock;` |
|       - |  3334 | `	Label sLabel;` |
|       - |  3335 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 |  3336 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 |  3337 | `	if( pBlock ){` |
|       - |  3338 | `		sxi32 rc;` |
|       7 |  3339 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3340 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 |  3341 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3342 | `			return SXERR_ABORT;` |
|       - |  3343 | `		}` |
|       3 |  3344 | `	}else{` |
|     110 |  3345 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3346 | `		char *zDup;` |
|       - |  3347 | `		/* Initialize label fields */` |
|     110 |  3348 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3349 | `		/* Duplicate label name */` |
|     110 |  3350 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 |  3351 | `		if( zDup == 0 ){` |
|     ! 0 |  3352 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3353 | `			return SXERR_ABORT;` |
|       - |  3354 | `		}` |
|     110 |  3355 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 |  3356 | `		sLabel.bRef  = FALSE;` |
|     110 |  3357 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 |  3358 | `		pBlock = pGen->pCurrent;` |
|     218 |  3359 | `		while( pBlock ){` |
|     130 |  3360 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3361 | `				break;` |
|       - |  3362 | `			}` |
|       - |  3363 | `			/* Point to the upper block */` |
|     110 |  3364 | `			pBlock = pBlock->pParent;` |
|       2 |  3365 | `		}` |
|     110 |  3366 | `		if( pBlock ){` |
|      22 |  3367 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3368 | `		}else{` |
|      90 |  3369 | `			sLabel.pFunc = 0;` |
|       - |  3370 | `		}` |
|       - |  3371 | `		/* Insert in label set */` |
|     110 |  3372 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3373 | `	}` |
|     114 |  3374 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 |  3375 | `	return SXRET_OK;` |
|      58 |  3376 |  |
|       - |  3377 | `/*` |
|       - |  3378 | ` * Compile the so hated 'goto' statement.` |
|       - |  3379 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3380 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3381 | ` * a compiler it has to do this.` |
|       - |  3382 | ` * According to the PHP language reference manual` |
|       - |  3383 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3384 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3385 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3386 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3387 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3388 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3389 | ` *   of a multi-level break` |
|       - |  3390 | ` */` |
|     152 |  3391 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 |  3392 |  |
|       - |  3393 | `	JumpFixup sJump;` |
|       - |  3394 | `	sxi32 rc;` |
|     154 |  3395 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 |  3396 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3397 | `		/* Missing label */` |
|     ! 0 |  3398 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3399 | `		if( rc == SXERR_ABORT ){` |
|       - |  3400 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3401 | `			return SXERR_ABORT;` |
|       - |  3402 | `		}` |
|     ! 0 |  3403 | `		return SXRET_OK;` |
|       - |  3404 | `	}` |
|     154 |  3405 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3406 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3407 | `		if( rc == SXERR_ABORT ){` |
|       - |  3408 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3409 | `			return SXERR_ABORT;` |
|       - |  3410 | `		}` |
|       3 |  3411 | `	}else{` |
|     150 |  3412 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3413 | `		GenBlock *pBlock;` |
|       - |  3414 | `		char *zDup;` |
|       - |  3415 | `		/* Prepare the jump destination */` |
|     150 |  3416 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 |  3417 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3418 | `		/* Duplicate label name */` |
|     150 |  3419 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 |  3420 | `		if( zDup == 0 ){` |
|     ! 0 |  3421 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3422 | `			return SXERR_ABORT;` |
|       - |  3423 | `		}` |
|     150 |  3424 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 |  3425 | `		pBlock = pGen->pCurrent;` |
|     312 |  3426 | `		while( pBlock ){` |
|     196 |  3427 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 |  3428 | `				break;` |
|       - |  3429 | `			}` |
|       - |  3430 | `			/* Point to the upper block */` |
|     164 |  3431 | `			pBlock = pBlock->pParent;` |
|       2 |  3432 | `		}` |
|     150 |  3433 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 |  3434 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 |  3435 | `			if( rc == SXERR_ABORT ){` |
|       - |  3436 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3437 | `				return SXERR_ABORT;` |
|       - |  3438 | `			}` |
|       3 |  3439 | `		}` |
|     150 |  3440 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 |  3441 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3442 | `		}else{` |
|     124 |  3443 | `			sJump.pFunc = 0;` |
|       - |  3444 | `		}` |
|       - |  3445 | `		/* Emit the unconditional jump */` |
|     150 |  3446 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 |  3447 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3448 | `		}` |
|       - |  3449 | `	}` |
|     154 |  3450 | `	pGen->pIn++; /* Jump the label name */` |
|     154 |  3451 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3452 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3453 | `	}` |
|       - |  3454 | `	/* Statement successfully compiled */` |
|     154 |  3455 | `	return SXRET_OK;` |
|      78 |  3456 |  |
|       - |  3457 | `/*` |
|       - |  3458 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3459 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3460 | ` * failure.` |
|       - |  3461 | ` */` |
|      20 |  3462 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3463 |  |
|       - |  3464 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3465 | `	sxu32 nRawObj;` |
|      10 |  3466 | `	sxu32 nObjIdx;` |
|       - |  3467 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3468 | `	 * a PHP block.` |
|       - |  3469 | `	 */` |
|      10 |  3470 | `Consume:` |
|      21 |  3471 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3472 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3473 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3474 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3475 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3476 | `			return SXERR_ABORT;` |
|       - |  3477 | `		}` |
|       - |  3478 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3479 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3480 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3481 | `		++nRawObj;` |
|     ! 0 |  3482 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3483 | `	}` |
|      21 |  3484 | `	if( nRawObj > 0 ){` |
|       - |  3485 | `		/* Emit the consume instruction */` |
|     ! 0 |  3486 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3487 | `	}` |
|      21 |  3488 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3489 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3490 | `		/* Reset the token set */` |
|     ! 0 |  3491 | `		SySetReset(pTokenSet);` |
|       - |  3492 | `		/* Tokenize input */` |
|     ! 0 |  3493 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3494 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3495 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3496 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3497 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3498 | `		/* Advance the stream cursor */` |
|     ! 0 |  3499 | `		pGen->pRawIn++;` |
|       - |  3500 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3501 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3502 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3503 | `			sxi32 rc;` |
|       - |  3504 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3505 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3506 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3507 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3508 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3509 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3510 | `				return SXERR_ABORT;` |
|     ! 0 |  3511 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3512 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3513 | `			}` |
|     ! 0 |  3514 | `			goto Consume;` |
|       - |  3515 | `		}` |
|     ! 0 |  3516 | `	}else{` |
|       - |  3517 | `		/* No more chunks to process */` |
|      21 |  3518 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3519 | `		return SXERR_EOF;` |
|       - |  3520 | `	}` |
|     ! 0 |  3521 | `	return SXRET_OK;` |
|      11 |  3522 |  |
|       - |  3523 | `/*` |
|       - |  3524 | ` * Compile a PHP block.` |
|       - |  3525 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3526 | ` * optionally delimited by braces {}.` |
|       - |  3527 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3528 | ` * and this function takes care of generating the appropriate error` |
|       - |  3529 | ` * message.` |
|       - |  3530 | ` */` |
|  364412 |  3531 | `static sxi32 PH7_CompileBlock(` |
|       - |  3532 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3533 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3534 | `	)` |
|       2 |  3535 |  |
|       - |  3536 | `	sxi32 rc;` |
|       - |  3537 | `	sxu32 nLine;` |
|  364414 |  3538 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  363000 |  3539 | `		nLine = pGen->pIn->nLine;` |
|  363000 |  3540 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  363000 |  3541 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3542 | `			return SXERR_ABORT;` |
|       - |  3543 | `		}` |
|  363000 |  3544 | `		pGen->pIn++;` |
|       - |  3545 | `		/* Compile until we hit the closing braces '}' */` |
|  495906 |  3546 | `		for(;;){` |
|  991814 |  3547 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3548 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3549 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3550 | `			 	   return SXERR_ABORT;` |
|       - |  3551 | `				}` |
|      21 |  3552 | `				if( rc == SXERR_EOF ){` |
|       - |  3553 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3554 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3555 | `					break;` |
|       - |  3556 | `				}` |
|     ! 0 |  3557 | `			}` |
|  991794 |  3558 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3559 | `				/* Closing braces found,break immediately*/` |
|  362980 |  3560 | `				pGen->pIn++;` |
|  362980 |  3561 | `				break;` |
|       - |  3562 | `			}` |
|       - |  3563 | `			/* Compile a single statement */` |
|  628816 |  3564 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  628816 |  3565 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3566 | `				return SXERR_ABORT;` |
|       - |  3567 | `			}` |
|       2 |  3568 | `		}` |
|  363000 |  3569 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  182915 |  3570 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3571 | `		pGen->pIn++;` |
|     ! 0 |  3572 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3573 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3574 | `			return SXERR_ABORT;` |
|       - |  3575 | `		}` |
|       - |  3576 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3577 | `		for(;;){` |
|     ! 0 |  3578 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3579 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3580 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3581 | `			 	   return SXERR_ABORT;` |
|       - |  3582 | `				}` |
|     ! 0 |  3583 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3584 | `					/* No more token to process */` |
|     ! 0 |  3585 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3586 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3587 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3588 | `					}` |
|     ! 0 |  3589 | `					break;` |
|       - |  3590 | `				}` |
|     ! 0 |  3591 | `			}` |
|     ! 0 |  3592 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3593 | `				sxi32 nKwrd;` |
|       - |  3594 | `				/* Keyword found */` |
|     ! 0 |  3595 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3596 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3597 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3598 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3599 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3600 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3601 | `						}` |
|     ! 0 |  3602 | `						break;` |
|       - |  3603 | `				}` |
|     ! 0 |  3604 | `			}` |
|       - |  3605 | `			/* Compile a single statement */` |
|     ! 0 |  3606 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3607 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3608 | `				return SXERR_ABORT;` |
|       - |  3609 | `			}` |
|     ! 0 |  3610 | `		}` |
|     ! 0 |  3611 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3612 | `	}else{` |
|       - |  3613 | `		/* Compile a single statement */` |
|    1416 |  3614 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1416 |  3615 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3616 | `			return SXERR_ABORT;` |
|       - |  3617 | `		}` |
|       - |  3618 | `	}` |
|       - |  3619 | `	/* Jump trailing semi-colons ';' */` |
|  364414 |  3620 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3621 | `		pGen->pIn++;` |
|     ! 0 |  3622 | `	}` |
|  364414 |  3623 | `	return SXRET_OK;` |
|  182208 |  3624 |  |
|       - |  3625 | `/*` |
|       - |  3626 | ` * Compile the gentle 'while' statement.` |
|       - |  3627 | ` * According to the PHP language reference` |
|       - |  3628 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3629 | ` *  The basic form of a while statement is:` |
|       - |  3630 | ` *  while (expr)` |
|       - |  3631 | ` *   statement` |
|       - |  3632 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3633 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3634 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3635 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3636 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3637 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3638 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3639 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3640 | ` *  while (expr):` |
|       - |  3641 | ` *    statement` |
|       - |  3642 | ` *   endwhile;` |
|       - |  3643 | ` */` |
|   12126 |  3644 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3645 |  |
|   12128 |  3646 | `	GenBlock *pWhileBlock = 0;` |
|   12128 |  3647 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3648 | `	sxu32 nFalseJump;` |
|       - |  3649 | `	sxu32 nLine;` |
|       - |  3650 | `	sxi32 rc;` |
|   12128 |  3651 | `	nLine = pGen->pIn->nLine;` |
|       - |  3652 | `	/* Jump the 'while' keyword */` |
|   12128 |  3653 | `	pGen->pIn++;` |
|   12128 |  3654 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3655 | `		/* Syntax error */` |
|     ! 0 |  3656 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3657 | `		if( rc == SXERR_ABORT ){` |
|       - |  3658 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3659 | `			return SXERR_ABORT;` |
|       - |  3660 | `		}` |
|     ! 0 |  3661 | `		goto Synchronize;` |
|       - |  3662 | `	}` |
|       - |  3663 | `	/* Jump the left parenthesis '(' */` |
|   12128 |  3664 | `	pGen->pIn++;` |
|       - |  3665 | `	/* Create the loop block */` |
|   12128 |  3666 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   12128 |  3667 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3668 | `		return SXERR_ABORT;` |
|       - |  3669 | `	}` |
|       - |  3670 | `	/* Delimit the condition */` |
|   12128 |  3671 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12128 |  3672 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3673 | `		/* Empty expression */` |
|       3 |  3674 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3675 | `		if( rc == SXERR_ABORT ){` |
|       - |  3676 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3677 | `			return SXERR_ABORT;` |
|       - |  3678 | `		}` |
|       1 |  3679 | `	}` |
|       - |  3680 | `	/* Swap token streams */` |
|   12128 |  3681 | `	pTmp = pGen->pEnd;` |
|   12128 |  3682 | `	pGen->pEnd = pEnd;` |
|       - |  3683 | `	/* Compile the expression */` |
|   12128 |  3684 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12128 |  3685 | `	if( rc == SXERR_ABORT ){` |
|       - |  3686 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3687 | `		return SXERR_ABORT;` |
|       - |  3688 | `	}` |
|       - |  3689 | `	/* Update token stream */` |
|   12128 |  3690 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3691 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3692 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3693 | `			return SXERR_ABORT;` |
|       - |  3694 | `		}` |
|     ! 0 |  3695 | `		pGen->pIn++;` |
|     ! 0 |  3696 | `	}` |
|       - |  3697 | `	/* Synchronize pointers */` |
|   12128 |  3698 | `	pGen->pIn  = &pEnd[1];` |
|   12128 |  3699 | `	pGen->pEnd = pTmp;` |
|       - |  3700 | `	/* Emit the false jump */` |
|   12128 |  3701 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3702 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12128 |  3703 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3704 | `	/* Compile the loop body */` |
|   12128 |  3705 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   12128 |  3706 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3707 | `		return SXERR_ABORT;` |
|       - |  3708 | `	}` |
|       - |  3709 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12128 |  3710 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3711 | `	/* Fix all jumps now the destination is resolved */` |
|   12128 |  3712 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3713 | `	/* Release the loop block */` |
|   12128 |  3714 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3715 | `	/* Statement successfully compiled */` |
|   12128 |  3716 | `	return SXRET_OK;` |
|     ! 0 |  3717 | `Synchronize:` |
|       - |  3718 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3719 | `	 * compiling this erroneous block.` |
|       - |  3720 | `	 */` |
|     ! 0 |  3721 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3722 | `		pGen->pIn++;` |
|     ! 0 |  3723 | `	}` |
|     ! 0 |  3724 | `	return SXRET_OK;` |
|    6065 |  3725 |  |
|       - |  3726 | `/*` |
|       - |  3727 | ` * Compile the ugly do..while() statement.` |
|       - |  3728 | ` * According to the PHP language reference` |
|       - |  3729 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3730 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3731 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3732 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3733 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3734 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3735 | ` *  would end immediately).` |
|       - |  3736 | ` *  There is just one syntax for do-while loops:` |
|       - |  3737 | ` *  <?php` |
|       - |  3738 | ` *  $i = 0;` |
|       - |  3739 | ` *  do {` |
|       - |  3740 | ` *   echo $i;` |
|       - |  3741 | ` *  } while ($i > 0);` |
|       - |  3742 | ` * ?>` |
|       - |  3743 | ` */` |
|       2 |  3744 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3745 |  |
|       3 |  3746 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3747 | `	GenBlock *pDoBlock = 0;` |
|       - |  3748 | `	sxu32 nLine;` |
|       - |  3749 | `	sxi32 rc;` |
|       3 |  3750 | `	nLine = pGen->pIn->nLine;` |
|       - |  3751 | `	/* Jump the 'do' keyword */` |
|       3 |  3752 | `	pGen->pIn++;` |
|       - |  3753 | `	/* Create the loop block */` |
|       3 |  3754 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3755 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3756 | `		return SXERR_ABORT;` |
|       - |  3757 | `	}` |
|       - |  3758 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3759 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3760 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3761 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3762 | `		return SXERR_ABORT;` |
|       - |  3763 | `	}` |
|       3 |  3764 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3765 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3766 | `	}` |
|       3 |  3767 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3768 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3769 | `			/* Missing 'while' statement */` |
|       3 |  3770 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3771 | `			if( rc == SXERR_ABORT ){` |
|       - |  3772 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3773 | `				return SXERR_ABORT;` |
|       - |  3774 | `			}` |
|       3 |  3775 | `			goto Synchronize;` |
|       - |  3776 | `	}` |
|       - |  3777 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3778 | `	pGen->pIn++;` |
|     ! 0 |  3779 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3780 | `		/* Syntax error */` |
|     ! 0 |  3781 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3782 | `		if( rc == SXERR_ABORT ){` |
|       - |  3783 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3784 | `			return SXERR_ABORT;` |
|       - |  3785 | `		}` |
|     ! 0 |  3786 | `		goto Synchronize;` |
|       - |  3787 | `	}` |
|       - |  3788 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3789 | `	pGen->pIn++;` |
|       - |  3790 | `	/* Delimit the condition */` |
|     ! 0 |  3791 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3792 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3793 | `		/* Empty expression */` |
|     ! 0 |  3794 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3795 | `		if( rc == SXERR_ABORT ){` |
|       - |  3796 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3797 | `			return SXERR_ABORT;` |
|       - |  3798 | `		}` |
|     ! 0 |  3799 | `		goto Synchronize;` |
|       - |  3800 | `	}` |
|       - |  3801 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3802 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3803 | `		JumpFixup *aPost;` |
|       - |  3804 | `		VmInstr *pInstr;` |
|       - |  3805 | `		sxu32 nJumpDest;` |
|       - |  3806 | `		sxu32 n;` |
|     ! 0 |  3807 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3808 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3809 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3810 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3811 | `			if( pInstr ){` |
|       - |  3812 | `				/* Fix */` |
|     ! 0 |  3813 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3814 | `			}` |
|     ! 0 |  3815 | `		}` |
|     ! 0 |  3816 | `	}` |
|       - |  3817 | `	/* Swap token streams */` |
|     ! 0 |  3818 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3819 | `	pGen->pEnd = pEnd;` |
|       - |  3820 | `	/* Compile the expression */` |
|     ! 0 |  3821 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3822 | `	if( rc == SXERR_ABORT ){` |
|       - |  3823 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3824 | `		return SXERR_ABORT;` |
|       - |  3825 | `	}` |
|       - |  3826 | `	/* Update token stream */` |
|     ! 0 |  3827 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3829 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3830 | `			return SXERR_ABORT;` |
|       - |  3831 | `		}` |
|     ! 0 |  3832 | `		pGen->pIn++;` |
|     ! 0 |  3833 | `	}` |
|     ! 0 |  3834 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3835 | `	pGen->pEnd = pTmp;` |
|       - |  3836 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3837 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3838 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3839 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3840 | `	/* Release the loop block */` |
|     ! 0 |  3841 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3842 | `	/* Statement successfully compiled */` |
|     ! 0 |  3843 | `	return SXRET_OK;` |
|       1 |  3844 | `Synchronize:` |
|       - |  3845 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3846 | `	 * compiling this erroneous block.` |
|       - |  3847 | `	 */` |
|       3 |  3848 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3849 | `		pGen->pIn++;` |
|     ! 0 |  3850 | `	}` |
|       3 |  3851 | `	return SXRET_OK;` |
|       2 |  3852 |  |
|       - |  3853 | `/*` |
|       - |  3854 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3855 | ` * According to the PHP language reference` |
|       - |  3856 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3857 | ` *  The syntax of a for loop is:` |
|       - |  3858 | ` *  for (expr1; expr2; expr3)` |
|       - |  3859 | ` *   statement` |
|       - |  3860 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3861 | ` *  the beginning of the loop.` |
|       - |  3862 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3863 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3864 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3865 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3866 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3867 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3868 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3869 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3870 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3871 | ` *  of using the for truth expression.` |
|       - |  3872 | ` */` |
|   12138 |  3873 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3874 |  |
|   12140 |  3875 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   12140 |  3876 | `	GenBlock *pForBlock = 0;` |
|       - |  3877 | `	sxu32 nFalseJump;` |
|       - |  3878 | `	sxu32 nLine;` |
|       - |  3879 | `	sxi32 rc;` |
|   12140 |  3880 | `	nLine = pGen->pIn->nLine;` |
|       - |  3881 | `	/* Jump the 'for' keyword */` |
|   12140 |  3882 | `	pGen->pIn++;` |
|   12140 |  3883 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3884 | `		/* Syntax error */` |
|     ! 0 |  3885 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3886 | `		if( rc == SXERR_ABORT ){` |
|       - |  3887 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3888 | `			return SXERR_ABORT;` |
|       - |  3889 | `		}` |
|     ! 0 |  3890 | `		return SXRET_OK;` |
|       - |  3891 | `	}` |
|       - |  3892 | `	/* Jump the left parenthesis '(' */` |
|   12140 |  3893 | `	pGen->pIn++;` |
|       - |  3894 | `	/* Delimit the init-expr;condition;post-expr */` |
|   12140 |  3895 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   12140 |  3896 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3897 | `		/* Empty expression */` |
|     ! 0 |  3898 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3899 | `		if( rc == SXERR_ABORT ){` |
|       - |  3900 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3901 | `			return SXERR_ABORT;` |
|       - |  3902 | `		}` |
|       - |  3903 | `		/* Synchronize */` |
|     ! 0 |  3904 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3905 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3906 | `			pGen->pIn++;` |
|     ! 0 |  3907 | `		}` |
|     ! 0 |  3908 | `		return SXRET_OK;` |
|       - |  3909 | `	}` |
|       - |  3910 | `	/* Swap token streams */` |
|   12140 |  3911 | `	pTmp = pGen->pEnd;` |
|   12140 |  3912 | `	pGen->pEnd = pEnd;` |
|       - |  3913 | `	/* Compile initialization expressions if available */` |
|   12140 |  3914 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3915 | `	/* Pop operand lvalues */` |
|   12140 |  3916 | `	if( rc == SXERR_ABORT ){` |
|       - |  3917 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3918 | `		return SXERR_ABORT;` |
|   12140 |  3919 | `	}else if( rc != SXERR_EMPTY ){` |
|   12138 |  3920 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6068 |  3921 | `	}` |
|   12140 |  3922 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3923 | `		/* Syntax error */` |
|     ! 0 |  3924 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3925 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3926 | `		if( rc == SXERR_ABORT ){` |
|       - |  3927 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3928 | `			return SXERR_ABORT;` |
|       - |  3929 | `		}` |
|     ! 0 |  3930 | `		return SXRET_OK;` |
|       - |  3931 | `	}` |
|       - |  3932 | `	/* Jump the trailing ';' */` |
|   12140 |  3933 | `	pGen->pIn++;` |
|       - |  3934 | `	/* Create the loop block */` |
|   12140 |  3935 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   12140 |  3936 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3937 | `		return SXERR_ABORT;` |
|       - |  3938 | `	}` |
|       - |  3939 | `	/* Deffer continue jumps */` |
|   12140 |  3940 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3941 | `	/* Compile the condition */` |
|   12140 |  3942 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12140 |  3943 | `	if( rc == SXERR_ABORT ){` |
|       - |  3944 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3945 | `		return SXERR_ABORT;` |
|   12140 |  3946 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3947 | `		/* Emit the false jump */` |
|   12138 |  3948 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3949 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   12138 |  3950 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    6068 |  3951 | `	}` |
|   12140 |  3952 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3953 | `		/* Syntax error */` |
|       5 |  3954 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3955 | `			"for: Expected ';' after conditionals expressions");` |
|       5 |  3956 | `		if( rc == SXERR_ABORT ){` |
|       - |  3957 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3958 | `			return SXERR_ABORT;` |
|       - |  3959 | `		}` |
|       5 |  3960 | `		return SXRET_OK;` |
|       - |  3961 | `	}` |
|       - |  3962 | `	/* Jump the trailing ';' */` |
|   12136 |  3963 | `	pGen->pIn++;` |
|       - |  3964 | `	/* Save the post condition stream */` |
|   12136 |  3965 | `	pPostStart = pGen->pIn;` |
|       - |  3966 | `	/* Compile the loop body */` |
|   12136 |  3967 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   12136 |  3968 | `	pGen->pEnd = pTmp;` |
|   12136 |  3969 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   12136 |  3970 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3971 | `		return SXERR_ABORT;` |
|       - |  3972 | `	}` |
|       - |  3973 | `	/* Fix post-continue jumps */` |
|   12136 |  3974 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3975 | `		JumpFixup *aPost;` |
|       - |  3976 | `		VmInstr *pInstr;` |
|       - |  3977 | `		sxu32 nJumpDest;` |
|       - |  3978 | `		sxu32 n;` |
|      14 |  3979 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3980 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3981 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3982 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3983 | `			if( pInstr ){` |
|       - |  3984 | `				/* Fix jump */` |
|      14 |  3985 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3986 | `			}` |
|       8 |  3987 | `		}` |
|       6 |  3988 | `	}` |
|       - |  3989 | `	/* compile the post-expressions if available */` |
|   12136 |  3990 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3991 | `		pPostStart++;` |
|     ! 0 |  3992 | `	}` |
|   12136 |  3993 | `	if( pPostStart < pEnd ){` |
|       - |  3994 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   12136 |  3995 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   12136 |  3996 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   12136 |  3997 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3998 | `			/* Syntax error */` |
|     ! 0 |  3999 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  4000 | `			if( rc == SXERR_ABORT ){` |
|       - |  4001 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4002 | `				return SXERR_ABORT;` |
|       - |  4003 | `			}` |
|     ! 0 |  4004 | `			return SXRET_OK;` |
|       - |  4005 | `		}` |
|   12136 |  4006 | `		RE_SWAP_DELIMITER(pGen);` |
|   12136 |  4007 | `		if( rc == SXERR_ABORT ){` |
|       - |  4008 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4009 | `			return SXERR_ABORT;` |
|   12136 |  4010 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  4011 | `			/* Pop operand lvalue */` |
|   12136 |  4012 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    6067 |  4013 | `		}` |
|    6067 |  4014 | `	}` |
|       - |  4015 | `	/* Emit the unconditional jump to the start of the loop */` |
|   12136 |  4016 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  4017 | `	/* Fix all jumps now the destination is resolved */` |
|   12136 |  4018 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4019 | `	/* Release the loop block */` |
|   12136 |  4020 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4021 | `	/* Statement successfully compiled */` |
|   12136 |  4022 | `	return SXRET_OK;` |
|    6071 |  4023 |  |
|       - |  4024 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4025 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4026 | ` * are allowed.` |
|       - |  4027 | ` */` |
|    6490 |  4028 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  4029 |  |
|    6492 |  4030 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6492 |  4031 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4032 | `		/* Unexpected expression */` |
|     ! 0 |  4033 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4034 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4035 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4036 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4037 | `		}` |
|     ! 0 |  4038 | `	}` |
|    6492 |  4039 | `	return rc;` |
|       2 |  4040 |  |
|       - |  4041 | `/*` |
|       - |  4042 | ` * Compile the 'foreach' statement.` |
|       - |  4043 | ` * According to the PHP language reference` |
|       - |  4044 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4045 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4046 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4047 | ` *  is a minor but useful extension of the first:` |
|       - |  4048 | ` *  foreach (array_expression as $value)` |
|       - |  4049 | ` *    statement` |
|       - |  4050 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4051 | ` *   statement` |
|       - |  4052 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4053 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4054 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4055 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4056 | ` *  to the variable $key on each loop.` |
|       - |  4057 | ` *  Note:` |
|       - |  4058 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4059 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4060 | ` *  Note:` |
|       - |  4061 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4062 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4063 | ` *  or after the foreach without resetting it.` |
|       - |  4064 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4065 | ` *  of copying the value.` |
|       - |  4066 | ` */` |
|    3306 |  4067 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  4068 |  |
|    3308 |  4069 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3308 |  4070 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3308 |  4071 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4072 | `	ph7_foreach_info *pInfo;` |
|       - |  4073 | `	sxu32 nFalseJump;` |
|       - |  4074 | `	VmInstr *pInstr;` |
|       - |  4075 | `	sxu32 nLine;` |
|       - |  4076 | `	sxi32 rc;` |
|    3308 |  4077 | `	nLine = pGen->pIn->nLine;` |
|       - |  4078 | `	/* Jump the 'foreach' keyword */` |
|    3308 |  4079 | `	pGen->pIn++;` |
|    3308 |  4080 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4081 | `		/* Syntax error */` |
|     ! 0 |  4082 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4083 | `		if( rc == SXERR_ABORT ){` |
|       - |  4084 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4085 | `			return SXERR_ABORT;` |
|       - |  4086 | `		}` |
|     ! 0 |  4087 | `		goto Synchronize;` |
|       - |  4088 | `	}` |
|       - |  4089 | `	/* Jump the left parenthesis '(' */` |
|    3308 |  4090 | `	pGen->pIn++;` |
|       - |  4091 | `	/* Create the loop block */` |
|    3308 |  4092 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3308 |  4093 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4094 | `		return SXERR_ABORT;` |
|       - |  4095 | `	}` |
|       - |  4096 | `	/* Delimit the expression */` |
|    3308 |  4097 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3308 |  4098 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4099 | `		/* Empty expression */` |
|     ! 0 |  4100 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4101 | `		if( rc == SXERR_ABORT ){` |
|       - |  4102 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4103 | `			return SXERR_ABORT;` |
|       - |  4104 | `		}` |
|       - |  4105 | `		/* Synchronize */` |
|     ! 0 |  4106 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4107 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4108 | `			pGen->pIn++;` |
|     ! 0 |  4109 | `		}` |
|     ! 0 |  4110 | `		return SXRET_OK;` |
|       - |  4111 | `	}` |
|       - |  4112 | `	/* Compile the array expression */` |
|    3308 |  4113 | `	pCur = pGen->pIn;` |
|   22140 |  4114 | `	while( pCur < pEnd ){` |
|   22140 |  4115 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3320 |  4116 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3320 |  4117 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4118 | `				/* Break with the first 'as' found */` |
|    3308 |  4119 | `				break;` |
|       - |  4120 | `			}` |
|       6 |  4121 | `		}` |
|       - |  4122 | `		/* Advance the stream cursor */` |
|   18834 |  4123 | `		pCur++;` |
|       2 |  4124 | `	}` |
|    3308 |  4125 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4126 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4127 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4128 | `		if( rc == SXERR_ABORT ){` |
|       - |  4129 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4130 | `			return SXERR_ABORT;` |
|       - |  4131 | `		}` |
|     ! 0 |  4132 | `		goto Synchronize;` |
|       - |  4133 | `	}` |
|       - |  4134 | `	/* Swap token streams */` |
|    3308 |  4135 | `	pTmp = pGen->pEnd;` |
|    3308 |  4136 | `	pGen->pEnd = pCur;` |
|    3308 |  4137 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3308 |  4138 | `	if( rc == SXERR_ABORT ){` |
|       - |  4139 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4140 | `		return SXERR_ABORT;` |
|       - |  4141 | `	}` |
|       - |  4142 | `	/* Update token stream */` |
|    3308 |  4143 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4144 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4145 | `		if( rc == SXERR_ABORT ){` |
|       - |  4146 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4147 | `			return SXERR_ABORT;` |
|       - |  4148 | `		}` |
|     ! 0 |  4149 | `		pGen->pIn++;` |
|     ! 0 |  4150 | `	}` |
|    3308 |  4151 | `	pCur++; /* Jump the 'as' keyword */` |
|    3308 |  4152 | `	pGen->pIn = pCur;` |
|    3308 |  4153 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4154 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4155 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4156 | `			return SXERR_ABORT;` |
|       - |  4157 | `		}` |
|     ! 0 |  4158 | `	}` |
|       - |  4159 | `	/* Create the foreach context */` |
|    3308 |  4160 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3308 |  4161 | `	if( pInfo == 0 ){` |
|     ! 0 |  4162 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4163 | `		return SXERR_ABORT;` |
|       - |  4164 | `	}` |
|       - |  4165 | `	/* Zero the structure */` |
|    3308 |  4166 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4167 | `	/* Initialize structure fields */` |
|    3308 |  4168 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4169 | `	/* Check if we have a key field */` |
|    9970 |  4170 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6664 |  4171 | `		pCur++;` |
|       2 |  4172 | `	}` |
|    3308 |  4173 | `	if( pCur < pEnd ){` |
|       - |  4174 | `		/* Compile the expression holding the key name */` |
|    3196 |  4175 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4176 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4177 | `			if( rc == SXERR_ABORT ){` |
|       - |  4178 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4179 | `				return SXERR_ABORT;` |
|       - |  4180 | `			}` |
|     ! 0 |  4181 | `		}else{` |
|    3196 |  4182 | `			pGen->pEnd = pCur;` |
|    3196 |  4183 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3196 |  4184 | `			if( rc == SXERR_ABORT ){` |
|       - |  4185 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4186 | `				return SXERR_ABORT;` |
|       - |  4187 | `			}` |
|    3196 |  4188 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3196 |  4189 | `			if( pInstr->p3 ){` |
|       - |  4190 | `				/* Record key name */` |
|    3196 |  4191 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1597 |  4192 | `			}` |
|    3196 |  4193 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4194 | `		}` |
|    3196 |  4195 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1597 |  4196 | `	}` |
|    3308 |  4197 | `	pGen->pEnd = pEnd;` |
|    3308 |  4198 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4199 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4200 | `		if( rc == SXERR_ABORT ){` |
|       - |  4201 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4202 | `			return SXERR_ABORT;` |
|       - |  4203 | `		}` |
|     ! 0 |  4204 | `		goto Synchronize;` |
|       - |  4205 | `	}` |
|    3308 |  4206 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4207 | `		pGen->pIn++;` |
|       - |  4208 | `		/* Pass by reference  */` |
|      11 |  4209 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4210 | `	}` |
|       - |  4211 | `	/* Check if the value target is list() */` |
|    3308 |  4212 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4213 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4214 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4215 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4216 | `		 */` |
|       - |  4217 | `		static int iForeachListCnt = 0;` |
|       - |  4218 | `		char zTmp[128];` |
|       - |  4219 | `		sxu32 nLen;` |
|       - |  4220 | `		char *zDup;` |
|      10 |  4221 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4222 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4223 | `		if( zDup == 0 ){` |
|     ! 0 |  4224 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4225 | `			return SXERR_ABORT;` |
|       - |  4226 | `		}` |
|      10 |  4227 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4228 | `		/* Save list() token boundaries */` |
|      10 |  4229 | `		pListStart = pGen->pIn;` |
|       - |  4230 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4231 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4232 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4233 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4234 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4236 | `				return SXERR_ABORT;` |
|       - |  4237 | `			}` |
|       3 |  4238 | `			goto Synchronize;` |
|       - |  4239 | `		}` |
|       7 |  4240 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4241 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4242 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4243 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4244 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4245 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4246 | `				return SXERR_ABORT;` |
|       - |  4247 | `			}` |
|     ! 0 |  4248 | `			goto Synchronize;` |
|       - |  4249 | `		}` |
|       7 |  4250 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4251 | `		pListEnd = pGen->pIn;` |
|       7 |  4252 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3303 |  4253 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4254 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4255 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4256 | `		 */` |
|       - |  4257 | `		static int iForeachShortListCnt = 0;` |
|       - |  4258 | `		char zTmp[128];` |
|       - |  4259 | `		sxu32 nLen;` |
|       - |  4260 | `		char *zDup;` |
|       3 |  4261 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4262 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4263 | `		if( zDup == 0 ){` |
|     ! 0 |  4264 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4265 | `			return SXERR_ABORT;` |
|       - |  4266 | `		}` |
|       3 |  4267 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4268 | `		/* Save [...] token boundaries */` |
|       3 |  4269 | `		pListStart = pGen->pIn;` |
|       - |  4270 | `		/* Advance past [...] */` |
|       3 |  4271 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4272 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4273 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4274 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4275 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4276 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4277 | `				return SXERR_ABORT;` |
|       - |  4278 | `			}` |
|     ! 0 |  4279 | `			goto Synchronize;` |
|       - |  4280 | `		}` |
|       3 |  4281 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4282 | `		pListEnd = pGen->pIn;` |
|       3 |  4283 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4284 | `	}else{` |
|       - |  4285 | `		/* Compile the expression holding the value name */` |
|    3298 |  4286 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3298 |  4287 | `		if( rc == SXERR_ABORT ){` |
|       - |  4288 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4289 | `			return SXERR_ABORT;` |
|       - |  4290 | `		}` |
|    3298 |  4291 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3298 |  4292 | `		if( pInstr->p3 ){` |
|       - |  4293 | `			/* Record value name */` |
|    3298 |  4294 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1648 |  4295 | `		}` |
|       - |  4296 | `	}` |
|       - |  4297 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3306 |  4298 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4299 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3306 |  4300 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4301 | `	/* Record the first instruction to execute */` |
|    3306 |  4302 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4303 | `	/* Emit the FOREACH_STEP instruction */` |
|    3306 |  4304 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4305 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3306 |  4306 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4307 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3306 |  4308 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4309 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4310 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4311 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4312 | `		 */` |
|       9 |  4313 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4314 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4315 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4316 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4317 | `		 */` |
|       9 |  4318 | `		pSavedIn = pGen->pIn;` |
|       9 |  4319 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4320 | `		pGen->pIn = pListStart;` |
|       9 |  4321 | `		pGen->pEnd = pListEnd;` |
|       9 |  4322 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4323 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4324 | `		}else{` |
|       7 |  4325 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4326 | `		}` |
|       9 |  4327 | `		pGen->pIn = pSavedIn;` |
|       9 |  4328 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4329 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4330 | `			return SXERR_ABORT;` |
|       - |  4331 | `		}` |
|       - |  4332 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4333 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4334 | `	}` |
|       - |  4335 | `	/* Compile the loop body */` |
|    3306 |  4336 | `	pGen->pIn = &pEnd[1];` |
|    3306 |  4337 | `	pGen->pEnd = pTmp;` |
|    3306 |  4338 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3306 |  4339 | `	if( rc == SXERR_ABORT ){` |
|       - |  4340 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4341 | `		return SXERR_ABORT;` |
|       - |  4342 | `	}` |
|       - |  4343 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3306 |  4344 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4345 | `	/* Fix all jumps now the destination is resolved */` |
|    3306 |  4346 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4347 | `	/* Release the loop block */` |
|    3306 |  4348 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4349 | `	/* Statement successfully compiled */` |
|    3306 |  4350 | `	return SXRET_OK;` |
|       1 |  4351 | `Synchronize:` |
|       - |  4352 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4353 | `	 * compiling this erroneous block.` |
|       - |  4354 | `	 */` |
|       3 |  4355 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4356 | `		pGen->pIn++;` |
|     ! 0 |  4357 | `	}` |
|       3 |  4358 | `	return SXRET_OK;` |
|    1655 |  4359 |  |
|       - |  4360 | `/*` |
|       - |  4361 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4362 | ` * According to the PHP language reference` |
|       - |  4363 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4364 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4365 | ` *  that is similar to that of C:` |
|       - |  4366 | ` *  if (expr)` |
|       - |  4367 | ` *   statement` |
|       - |  4368 | ` *  else construct:` |
|       - |  4369 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4370 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4371 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4372 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4373 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4374 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4375 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4376 | ` *  elseif` |
|       - |  4377 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4378 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4379 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4380 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4381 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4382 | ` *   <?php` |
|       - |  4383 | ` *    if ($a > $b) {` |
|       - |  4384 | ` *     echo "a is bigger than b";` |
|       - |  4385 | ` *    } elseif ($a == $b) {` |
|       - |  4386 | ` *     echo "a is equal to b";` |
|       - |  4387 | ` *    } else {` |
|       - |  4388 | ` *     echo "a is smaller than b";` |
|       - |  4389 | ` *    }` |
|       - |  4390 | ` *    ?>` |
|       - |  4391 | ` */` |
|  126350 |  4392 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4393 |  |
|  126352 |  4394 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  126352 |  4395 | `	GenBlock *pCondBlock = 0;` |
|       - |  4396 | `	sxu32 nJumpIdx;` |
|       - |  4397 | `	sxu32 nKeyID;` |
|       - |  4398 | `	sxi32 rc;` |
|       - |  4399 | `	/* Jump the 'if' keyword */` |
|  126352 |  4400 | `	pGen->pIn++;` |
|  126352 |  4401 | `	pToken = pGen->pIn;` |
|       - |  4402 | `	/* Create the conditional block */` |
|  126352 |  4403 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  126352 |  4404 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4405 | `		return SXERR_ABORT;` |
|       - |  4406 | `	}` |
|       - |  4407 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   69200 |  4408 | `	for(;;){` |
|  138402 |  4409 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4410 | `			/* Syntax error */` |
|     ! 0 |  4411 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4412 | `				pToken--;` |
|     ! 0 |  4413 | `			}` |
|     ! 0 |  4414 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4415 | `			if( rc == SXERR_ABORT ){` |
|       - |  4416 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4417 | `				return SXERR_ABORT;` |
|       - |  4418 | `			}` |
|     ! 0 |  4419 | `			goto Synchronize;` |
|       - |  4420 | `		}` |
|       - |  4421 | `		/* Jump the left parenthesis '(' */` |
|  138402 |  4422 | `		pToken++;` |
|       - |  4423 | `		/* Delimit the condition */` |
|  138402 |  4424 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  138402 |  4425 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4426 | `			/* Syntax error */` |
|     ! 0 |  4427 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4428 | `				pToken--;` |
|     ! 0 |  4429 | `			}` |
|     ! 0 |  4430 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4431 | `			if( rc == SXERR_ABORT ){` |
|       - |  4432 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4433 | `				return SXERR_ABORT;` |
|       - |  4434 | `			}` |
|     ! 0 |  4435 | `			goto Synchronize;` |
|       - |  4436 | `		}` |
|       - |  4437 | `		/* Swap token streams */` |
|  138402 |  4438 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4439 | `		/* Compile the condition */` |
|  138402 |  4440 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4441 | `		/* Update token stream */` |
|  138402 |  4442 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4443 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4444 | `			pGen->pIn++;` |
|     ! 0 |  4445 | `		}` |
|  138402 |  4446 | `		pGen->pIn  = &pEnd[1];` |
|  138402 |  4447 | `		pGen->pEnd = pTmp;` |
|  138402 |  4448 | `		if( rc == SXERR_ABORT ){` |
|       - |  4449 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4450 | `			return SXERR_ABORT;` |
|       - |  4451 | `		}` |
|       - |  4452 | `		/* Emit the false jump */` |
|  138402 |  4453 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4454 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  138402 |  4455 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4456 | `		/* Compile the body */` |
|  138402 |  4457 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  138402 |  4458 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4459 | `			return SXERR_ABORT;` |
|       - |  4460 | `		}` |
|  138402 |  4461 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   38617 |  4462 | `			break;` |
|       - |  4463 | `		}` |
|       - |  4464 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   61172 |  4465 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   61172 |  4466 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   39374 |  4467 | `			break;` |
|       - |  4468 | `		}` |
|       - |  4469 | `		/* Emit the unconditional jump */` |
|   21800 |  4470 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4471 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21800 |  4472 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   21800 |  4473 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   15762 |  4474 | `			pToken = &pGen->pIn[1];` |
|   15762 |  4475 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    6042 |  4476 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4876 |  4477 | `					break;` |
|       - |  4478 | `			}` |
|    6014 |  4479 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    3006 |  4480 | `		}` |
|   12052 |  4481 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4482 | `		/* Synchronize cursors */` |
|   12052 |  4483 | `		pToken = pGen->pIn;` |
|       - |  4484 | `		/* Fix the false jump */` |
|   12052 |  4485 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4486 | `	} /* For(;;) */` |
|       - |  4487 | `	/* Fix the false jump */` |
|  126352 |  4488 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  126352 |  4489 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   49120 |  4490 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4491 | `			/* Compile the else block */` |
|    9750 |  4492 | `			pGen->pIn++;` |
|    9750 |  4493 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9750 |  4494 | `			if( rc == SXERR_ABORT ){` |
|       - |  4495 |  |
|     ! 0 |  4496 | `				return SXERR_ABORT;` |
|       - |  4497 | `			}` |
|    4874 |  4498 | `	}` |
|  126352 |  4499 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4500 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  126352 |  4501 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4502 | `	/* Release the conditional block */` |
|  126352 |  4503 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4504 | `	/* Statement successfully compiled */` |
|  126352 |  4505 | `	return SXRET_OK;` |
|     ! 0 |  4506 | `Synchronize:` |
|       - |  4507 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4508 | `	 */` |
|     ! 0 |  4509 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4510 | `		pGen->pIn++;` |
|     ! 0 |  4511 | `	}` |
|     ! 0 |  4512 | `	return SXRET_OK;` |
|   63177 |  4513 |  |
|       - |  4514 | `/*` |
|       - |  4515 | ` * Compile the global construct.` |
|       - |  4516 | ` * According to the PHP language reference` |
|       - |  4517 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4518 | ` *  to be used in that function.` |
|       - |  4519 | ` *  Example #1 Using global` |
|       - |  4520 | ` *  <?php` |
|       - |  4521 | ` *   $a = 1;` |
|       - |  4522 | ` *   $b = 2;` |
|       - |  4523 | ` *   function Sum()` |
|       - |  4524 | ` *   {` |
|       - |  4525 | ` *    global $a, $b;` |
|       - |  4526 | ` *    $b = $a + $b;` |
|       - |  4527 | ` *   }` |
|       - |  4528 | ` *   Sum();` |
|       - |  4529 | ` *   echo $b;` |
|       - |  4530 | ` *  ?>` |
|       - |  4531 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4532 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4533 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4534 | ` */` |
|      32 |  4535 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4536 |  |
|      34 |  4537 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4538 | `	sxi32 nExpr;` |
|       - |  4539 | `	sxi32 rc;` |
|       - |  4540 | `	/* Jump the 'global' keyword */` |
|      34 |  4541 | `	pGen->pIn++;` |
|      34 |  4542 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4543 | `		/* Nothing to process */` |
|     ! 0 |  4544 | `		return SXRET_OK;` |
|       - |  4545 | `	}` |
|      34 |  4546 | `	pTmp = pGen->pEnd;` |
|      34 |  4547 | `	nExpr = 0;` |
|      68 |  4548 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      36 |  4549 | `		if( pGen->pIn < pNext ){` |
|      36 |  4550 | `			pGen->pEnd = pNext;` |
|      36 |  4551 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4552 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4553 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4554 | `					return SXERR_ABORT;` |
|       - |  4555 | `				}` |
|     ! 0 |  4556 | `			}else{` |
|      36 |  4557 | `				pGen->pIn++;` |
|      36 |  4558 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4559 | `					/* Emit a warning */` |
|     ! 0 |  4560 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4561 | `				}else{` |
|      36 |  4562 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      36 |  4563 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4564 | `						return SXERR_ABORT;` |
|      36 |  4565 | `					}else if(rc != SXERR_EMPTY ){` |
|      36 |  4566 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      36 |  4567 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4568 | `							/* Variable name, not a constant */` |
|      36 |  4569 | `							pLast->iP1 = 0;` |
|      17 |  4570 | `						}` |
|      36 |  4571 | `						nExpr++;` |
|      17 |  4572 | `					}` |
|       - |  4573 | `				}` |
|       - |  4574 | `			}` |
|      17 |  4575 | `		}` |
|       - |  4576 | `		/* Next expression in the stream */` |
|      36 |  4577 | `		pGen->pIn = pNext;` |
|       - |  4578 | `		/* Jump trailing commas */` |
|      38 |  4579 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4580 | `			pGen->pIn++;` |
|       1 |  4581 | `		}` |
|       2 |  4582 | `	}` |
|       - |  4583 | `	/* Restore token stream */` |
|      34 |  4584 | `	pGen->pEnd = pTmp;` |
|      34 |  4585 | `	if( nExpr > 0 ){` |
|       - |  4586 | `		/* Emit the uplink instruction */` |
|      34 |  4587 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      16 |  4588 | `	}` |
|      34 |  4589 | `	return SXRET_OK;` |
|      18 |  4590 |  |
|       - |  4591 | `/*` |
|       - |  4592 | ` * Compile the return statement.` |
|       - |  4593 | ` * According to the PHP language reference` |
|       - |  4594 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4595 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4596 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4597 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4598 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4599 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4600 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4601 | ` *  from within the main script file, then script execution end.` |
|       - |  4602 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4603 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4604 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4605 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4606 | ` */` |
|  199244 |  4607 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4608 |  |
|  199246 |  4609 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4610 | `	sxi32 rc;` |
|       - |  4611 | `	/* Jump the 'return' keyword */` |
|  199246 |  4612 | `	pGen->pIn++;` |
|  199246 |  4613 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4614 | `		/* Compile the expression */` |
|  199222 |  4615 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  199222 |  4616 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4617 | `			return SXERR_ABORT;` |
|  199222 |  4618 | `		}else if(rc != SXERR_EMPTY ){` |
|  199222 |  4619 | `			nRet = 1;` |
|   99610 |  4620 | `		}` |
|   99610 |  4621 | `	}` |
|       - |  4622 | `	/* Emit the done instruction */` |
|  199246 |  4623 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  199246 |  4624 | `	return SXRET_OK;` |
|   99624 |  4625 |  |
|       - |  4626 | `/*` |
|       - |  4627 | ` * Compile a yield expression.` |
|       - |  4628 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4629 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4630 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4631 | ` */` |
|      34 |  4632 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  4633 |  |
|       - |  4634 | `	SyToken *pTmp, *pSplit;` |
|      36 |  4635 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      36 |  4636 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4637 | `	sxi32 rc;` |
|      17 |  4638 | `	(void)iCompileFlag;` |
|       - |  4639 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      36 |  4640 | `	pGen->pIn++;` |
|       - |  4641 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4642 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      36 |  4643 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4644 | `		/* Bare yield — no value */` |
|     ! 0 |  4645 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4646 | `		return SXRET_OK;` |
|       - |  4647 | `	}` |
|       - |  4648 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      36 |  4649 | `	pSplit = 0;` |
|       - |  4650 | `	{` |
|      36 |  4651 | `		SyToken *pCur = pGen->pIn;` |
|      36 |  4652 | `		sxi32 nNest = 0;` |
|      84 |  4653 | `		while( pCur < pGen->pEnd ){` |
|      56 |  4654 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4655 | `				nNest++;` |
|      56 |  4656 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4657 | `				nNest--;` |
|      56 |  4658 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 |  4659 | `				pSplit = pCur;` |
|       7 |  4660 | `				break;` |
|       - |  4661 | `			}` |
|      50 |  4662 | `			pCur++;` |
|       2 |  4663 | `		}` |
|       - |  4664 | `	}` |
|      36 |  4665 | `	pTmp = pGen->pEnd;` |
|      36 |  4666 | `	if( pSplit ){` |
|       - |  4667 | `		/* yield $key => $value */` |
|       7 |  4668 | `		pGen->pEnd = pSplit;` |
|       7 |  4669 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4670 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4671 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 |  4672 | `		pGen->pEnd = pTmp;` |
|       7 |  4673 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4674 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4675 | `		iP1 = 1;` |
|       7 |  4676 | `		iP2 = 1;` |
|       4 |  4677 | `	}else{` |
|       - |  4678 | `		/* yield $value */` |
|      30 |  4679 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      30 |  4680 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      30 |  4681 | `		if( rc != SXERR_EMPTY ){` |
|      30 |  4682 | `			iP1 = 1;` |
|      14 |  4683 | `		}` |
|       - |  4684 | `	}` |
|      36 |  4685 | `	pGen->pEnd = pTmp;` |
|      36 |  4686 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      36 |  4687 | `	return SXRET_OK;` |
|      19 |  4688 |  |
|       - |  4689 | `/*` |
|       - |  4690 | ` * Compile the die/exit language construct.` |
|       - |  4691 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4692 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4693 | ` */` |
|      88 |  4694 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 |  4695 |  |
|      90 |  4696 | `	sxi32 nExpr = 0;` |
|       - |  4697 | `	sxi32 rc;` |
|       - |  4698 | `	/* Jump the die/exit keyword */` |
|      90 |  4699 | `	pGen->pIn++;` |
|      90 |  4700 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4701 | `		/* Compile the expression */` |
|      90 |  4702 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 |  4703 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4704 | `			return SXERR_ABORT;` |
|      90 |  4705 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 |  4706 | `			nExpr = 1;` |
|      44 |  4707 | `		}` |
|      44 |  4708 | `	}` |
|       - |  4709 | `	/* Emit the HALT instruction */` |
|      90 |  4710 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 |  4711 | `	return SXRET_OK;` |
|      46 |  4712 |  |
|       - |  4713 | `/*` |
|       - |  4714 | ` * Compile the 'echo' language construct.` |
|       - |  4715 | ` */` |
|   12540 |  4716 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4717 |  |
|   12542 |  4718 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4719 | `	sxi32 rc;` |
|       - |  4720 | `	/* Jump the 'echo' keyword */` |
|   12542 |  4721 | `	pGen->pIn++;` |
|       - |  4722 | `	/* Compile arguments one after one */` |
|   12542 |  4723 | `	pTmp = pGen->pEnd;` |
|   26596 |  4724 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   14056 |  4725 | `		if( pGen->pIn < pNext ){` |
|   14056 |  4726 | `			pGen->pEnd = pNext;` |
|   14056 |  4727 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   14056 |  4728 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4729 | `				return SXERR_ABORT;` |
|   14056 |  4730 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4731 | `				/* Emit the consume instruction */` |
|   14032 |  4732 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    7015 |  4733 | `			}` |
|    7027 |  4734 | `		}` |
|       - |  4735 | `		/* Jump trailing commas */` |
|   15570 |  4736 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    1516 |  4737 | `			pNext++;` |
|       2 |  4738 | `		}` |
|   14056 |  4739 | `		pGen->pIn = pNext;` |
|       2 |  4740 | `	}` |
|       - |  4741 | `	/* Restore token stream */` |
|   12542 |  4742 | `	pGen->pEnd = pTmp;` |
|   12542 |  4743 | `	return SXRET_OK;` |
|    6272 |  4744 |  |
|       - |  4745 | `/*` |
|       - |  4746 | ` * Compile the static statement.` |
|       - |  4747 | ` * According to the PHP language reference` |
|       - |  4748 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4749 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4750 | ` *  when program execution leaves this scope.` |
|       - |  4751 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4752 | ` * Symisc eXtension.` |
|       - |  4753 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4754 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4755 | ` *  Example` |
|       - |  4756 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4757 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4758 | ` */` |
|       2 |  4759 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 |  4760 |  |
|       - |  4761 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4762 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4763 | `	GenBlock *pBlock;` |
|       - |  4764 | `	SyString *pName;` |
|       - |  4765 | `	char *zDup;` |
|       - |  4766 | `	sxu32 nLine;` |
|       - |  4767 | `	sxi32 rc;` |
|       - |  4768 | `	/* Jump the static keyword */` |
|       3 |  4769 | `	nLine = pGen->pIn->nLine;` |
|       3 |  4770 | `	pGen->pIn++;` |
|       - |  4771 | `	/* Extract the enclosing function if any */` |
|       3 |  4772 | `	pBlock = pGen->pCurrent;` |
|       5 |  4773 | `	while( pBlock ){` |
|       5 |  4774 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 |  4775 | `			break;` |
|       - |  4776 | `		}` |
|       - |  4777 | `		/* Point to the upper block */` |
|       3 |  4778 | `		pBlock = pBlock->pParent;` |
|       1 |  4779 | `	}` |
|       3 |  4780 | `	if( pBlock == 0 ){` |
|       - |  4781 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4782 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4783 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4784 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4785 | `				return SXERR_ABORT;` |
|       - |  4786 | `			}` |
|     ! 0 |  4787 | `			goto Synchronize;` |
|       - |  4788 | `		}` |
|       - |  4789 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4790 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4791 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4792 | `			return SXERR_ABORT;` |
|     ! 0 |  4793 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4794 | `			/* Emit the POP instruction */` |
|     ! 0 |  4795 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4796 | `		}` |
|     ! 0 |  4797 | `		return SXRET_OK;` |
|       - |  4798 | `	}` |
|       3 |  4799 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4800 | `	/* Make sure we are dealing with a valid statement */` |
|       3 |  4801 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 |  4802 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4803 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4804 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4805 | `				return SXERR_ABORT;` |
|       - |  4806 | `			}` |
|       3 |  4807 | `			goto Synchronize;` |
|       - |  4808 | `	}` |
|     ! 0 |  4809 | `	pGen->pIn++;` |
|       - |  4810 | `	/* Extract variable name */` |
|     ! 0 |  4811 | `	pName = &pGen->pIn->sData;` |
|     ! 0 |  4812 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 |  4813 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4814 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4815 | `		goto Synchronize;` |
|       - |  4816 | `	}` |
|       - |  4817 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 |  4818 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 |  4819 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4820 | `	/* Duplicate variable name */` |
|     ! 0 |  4821 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 |  4822 | `	if( zDup == 0 ){` |
|     ! 0 |  4823 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4824 | `		return SXERR_ABORT;` |
|       - |  4825 | `	}` |
|     ! 0 |  4826 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4827 | `	/* Check if we have an expression to compile */` |
|     ! 0 |  4828 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4829 | `		SySet *pInstrContainer;` |
|       - |  4830 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4831 | `		 * Static variable can take any complex expression including function` |
|       - |  4832 | `		 * call as their initialization value.` |
|       - |  4833 | `		 * Example:` |
|       - |  4834 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4835 | `		 */` |
|     ! 0 |  4836 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4837 | `		/* Swap bytecode container */` |
|     ! 0 |  4838 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 |  4839 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4840 | `		/* Compile the expression */` |
|     ! 0 |  4841 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4842 | `		/* Emit the done instruction */` |
|     ! 0 |  4843 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4844 | `		/* Restore default bytecode container */` |
|     ! 0 |  4845 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  4846 | `	}` |
|       - |  4847 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 |  4848 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 |  4849 | `	return SXRET_OK;` |
|       1 |  4850 | `Synchronize:` |
|       - |  4851 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4852 | `	 * statement.` |
|       - |  4853 | `	 */` |
|       5 |  4854 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4855 | `		pGen->pIn++;` |
|       1 |  4856 | `	}` |
|       3 |  4857 | `	return SXRET_OK;` |
|       2 |  4858 |  |
|       - |  4859 | `/*` |
|       - |  4860 | ` * Compile the var statement.` |
|       - |  4861 | ` * Symisc Extension:` |
|       - |  4862 | ` *      var statement can be used outside of a class definition.` |
|       - |  4863 | ` */` |
|       4 |  4864 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4865 |  |
|       - |  4866 | `	sxu32 nLine;` |
|       - |  4867 | `	sxi32 rc;` |
|       5 |  4868 | `	nLine = pGen->pIn->nLine;` |
|       - |  4869 | `	/* Jump the 'var' keyword */` |
|       5 |  4870 | `	pGen->pIn++;` |
|       5 |  4871 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4872 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4873 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4874 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4875 | `			pGen->pIn++;` |
|     ! 0 |  4876 | `		}` |
|     ! 0 |  4877 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4878 | `			return SXERR_ABORT;` |
|       - |  4879 | `		}` |
|     ! 0 |  4880 | `	}else{` |
|       - |  4881 | `		/* Compile the expression */` |
|       5 |  4882 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4883 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4884 | `			return SXERR_ABORT;` |
|       5 |  4885 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4886 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4887 | `		}` |
|       - |  4888 | `	}` |
|       5 |  4889 | `	return SXRET_OK;` |
|       3 |  4890 |  |
|       - |  4891 | `/*` |
|       - |  4892 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4893 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4894 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4895 | ` */` |
|       - |  4896 | `/*` |
|       - |  4897 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4898 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4899 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4900 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4901 | ` *` |
|       - |  4902 | ` * Resolution order:` |
|       - |  4903 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4904 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4905 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4906 | ` *` |
|       - |  4907 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4908 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4909 | ` * Returns the (possibly new) literal index.` |
|       - |  4910 | ` */` |
|  371834 |  4911 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 |  4912 |  |
|       - |  4913 | `	ph7_value *pLit;` |
|       - |  4914 | `	const char *zLit;` |
|       - |  4915 | `	SyString sQualified;` |
|       - |  4916 | `	sxu32 nLit;` |
|       - |  4917 | `	sxu32 k;` |
|       - |  4918 | `	sxu32 nNewIdx;` |
|       - |  4919 | `	int hasNsSep;` |
|       - |  4920 | `	SyHashEntry *pImport;` |
|       - |  4921 | `	ph7_value *pNew;` |
|  371836 |  4922 | `	if( pFromImport ){` |
|  355752 |  4923 | `		*pFromImport = 0;` |
|  177875 |  4924 | `	}` |
|  371836 |  4925 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  371836 |  4926 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4927 | `		return nOrigIdx;` |
|       - |  4928 | `	}` |
|  371836 |  4929 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  371836 |  4930 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4931 | `	/* Skip if already qualified (contains backslash) */` |
|  371836 |  4932 | `	hasNsSep = 0;` |
| 4022920 |  4933 | `	for( k = 0; k < nLit; k++ ){` |
| 3651094 |  4934 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1825544 |  4935 | `	}` |
|  371836 |  4936 | `	if( hasNsSep ){` |
|       9 |  4937 | `		return nOrigIdx;` |
|       - |  4938 | `	}` |
|       - |  4939 | `	/* Check use imports first (works even outside namespaces) */` |
|  371828 |  4940 | `	SyBlobReset(&pGen->sWorker);` |
|  371828 |  4941 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  371828 |  4942 | `	if( pImport ){` |
|      38 |  4943 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4944 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4945 | `		if( pFromImport ){` |
|      18 |  4946 | `			*pFromImport = 1;` |
|       8 |  4947 | `		}` |
|      20 |  4948 | `	}else{` |
|  371792 |  4949 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  371702 |  4950 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4951 | `		}` |
|       - |  4952 | `		/* Prepend current namespace */` |
|      92 |  4953 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      92 |  4954 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      92 |  4955 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4956 | `	}` |
|       - |  4957 | `	/* Look up or create a new literal for the qualified name */` |
|     128 |  4958 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     128 |  4959 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 |  4960 | `		return nNewIdx; /* Already interned */` |
|       - |  4961 | `	}` |
|      76 |  4962 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      76 |  4963 | `	if( pNew == 0 ){` |
|     ! 0 |  4964 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4965 | `	}` |
|      76 |  4966 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      76 |  4967 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      76 |  4968 | `	return nNewIdx;` |
|  185919 |  4969 |  |
|       - |  4970 | `/*` |
|       - |  4971 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4972 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4973 | ` */` |
|   33596 |  4974 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4975 |  |
|       - |  4976 | `	SyHashEntry *pImport;` |
|       - |  4977 | `	/* Check use imports first */` |
|   33598 |  4978 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   33598 |  4979 | `	if( pImport ){` |
|      14 |  4980 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  4981 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  4982 | `		return;` |
|       - |  4983 | `	}` |
|       - |  4984 | `	/* Prepend current namespace if active */` |
|   33586 |  4985 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4986 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4987 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4988 | `	}` |
|   33586 |  4989 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   16800 |  4990 |  |
|       - |  4991 | `/*` |
|       - |  4992 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4993 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4994 | ` * The caller must release pOut when done.` |
|       - |  4995 | ` */` |
|   55042 |  4996 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4997 |  |
|   55044 |  4998 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      60 |  4999 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      60 |  5000 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  5001 | `	}` |
|   55044 |  5002 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   55044 |  5003 |  |
|       - |  5004 | `/*` |
|       - |  5005 | ` * Compile a namespace statement` |
|       - |  5006 | ` * According to the PHP language reference manual` |
|       - |  5007 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  5008 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  5009 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  5010 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  5011 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  5012 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  5013 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  5014 | ` *  programming world.` |
|       - |  5015 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  5016 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  5017 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  5018 | ` *  classes/functions/constants.` |
|       - |  5019 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  5020 | ` *  readability of source code.` |
|       - |  5021 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  5022 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5023 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5024 | ` *       class MyClass {}` |
|       - |  5025 | ` *       function myfunction() {}` |
|       - |  5026 | ` *       const MYCONST = 1;` |
|       - |  5027 | ` *       $a = new MyClass;` |
|       - |  5028 | ` *       $c = new \my\name\MyClass;` |
|       - |  5029 | ` *       $a = strlen('hi');` |
|       - |  5030 | ` *       $d = namespace\MYCONST;` |
|       - |  5031 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5032 | ` *       echo constant($d);` |
|       - |  5033 | ` * NOTE` |
|       - |  5034 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5035 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5036 | ` */` |
|       - |  5037 | `/*` |
|       - |  5038 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5039 | ` */` |
|      14 |  5040 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 |  5041 |  |
|      15 |  5042 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 |  5043 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 |  5044 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 |  5045 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 |  5046 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 |  5047 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5048 | `	return "token";` |
|       8 |  5049 |  |
|     104 |  5050 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 |  5051 |  |
|       - |  5052 | `	sxu32 nLine;` |
|       - |  5053 | `	sxi32 rc;` |
|     106 |  5054 | `	nLine = pGen->pIn->nLine;` |
|     106 |  5055 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5056 | `	/* Reset namespace and clear previous use imports */` |
|     106 |  5057 | `	SyBlobReset(&pGen->sNamespace);` |
|     106 |  5058 | `	SyHashRelease(&pGen->hUseImports);` |
|     106 |  5059 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5060 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     106 |  5061 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5062 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     106 |  5063 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5064 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5065 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5066 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5067 | `		return SXRET_OK;` |
|       - |  5068 | `	}` |
|     106 |  5069 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5070 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5071 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5072 | `		return SXRET_OK;` |
|       - |  5073 | `	}` |
|     106 |  5074 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5075 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5076 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5077 | `		return SXRET_OK;` |
|       - |  5078 | `	}` |
|       - |  5079 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     252 |  5080 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     148 |  5081 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5082 | `			/* Append backslash separator */` |
|      24 |  5083 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      24 |  5084 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5085 | `			}` |
|      13 |  5086 | `		}else{` |
|       - |  5087 | `			/* Append identifier */` |
|     126 |  5088 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5089 | `		}` |
|     148 |  5090 | `		pGen->pIn++;` |
|       2 |  5091 | `	}` |
|       - |  5092 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5093 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5094 | `	{` |
|     106 |  5095 | `		char *zNsDup = 0;` |
|     106 |  5096 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     155 |  5097 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     102 |  5098 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      51 |  5099 | `		}` |
|     106 |  5100 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5101 | `	}` |
|     106 |  5102 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 |  5103 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5104 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5105 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 |  5106 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5107 | `			return SXERR_ABORT;` |
|       - |  5108 | `		}` |
|       2 |  5109 | `	}` |
|     106 |  5110 | `	return SXRET_OK;` |
|      54 |  5111 |  |
|       - |  5112 | `/*` |
|       - |  5113 | ` * Compile the 'use' statement` |
|       - |  5114 | ` * According to the PHP language reference manual` |
|       - |  5115 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5116 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5117 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5118 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5119 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5120 | ` *  a function or constant is not supported.` |
|       - |  5121 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5122 | ` * NOTE` |
|       - |  5123 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5124 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5125 | ` */` |
|      68 |  5126 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 |  5127 |  |
|       - |  5128 | `	sxu32 nLine;` |
|       - |  5129 | `	sxi32 rc;` |
|       - |  5130 | `	SyBlob sPath;` |
|       - |  5131 | `	SyString sAlias;` |
|       - |  5132 | `	SyToken *pLast;` |
|       - |  5133 | `	char *zDup;` |
|       - |  5134 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5135 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5136 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      70 |  5137 | `	nLine = pGen->pIn->nLine;` |
|      70 |  5138 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5139 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      70 |  5140 | `	iUseType = 0;` |
|      70 |  5141 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5142 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5143 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5144 | `			iUseType = 1;` |
|      16 |  5145 | `			pGen->pIn++;` |
|      23 |  5146 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5147 | `			iUseType = 2;` |
|      16 |  5148 | `			pGen->pIn++;` |
|       7 |  5149 | `		}` |
|      14 |  5150 | `	}` |
|       - |  5151 | `	/* Select target hash tables based on import type */` |
|      70 |  5152 | `	switch( iUseType ){` |
|       7 |  5153 | `		case 1:` |
|      16 |  5154 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5155 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5156 | `			break;` |
|       7 |  5157 | `		case 2:` |
|      16 |  5158 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5159 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5160 | `			break;` |
|      20 |  5161 | `		default:` |
|      42 |  5162 | `			pGenHash = &pGen->hUseImports;` |
|      42 |  5163 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5164 | `			break;` |
|       - |  5165 | `	}` |
|      70 |  5166 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5167 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5168 | `	for(;;){` |
|      72 |  5169 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5170 | `			break;` |
|       - |  5171 | `		}` |
|      72 |  5172 | `		SyBlobReset(&sPath);` |
|      72 |  5173 | `		pLast = 0;` |
|       - |  5174 | `		/* Collect the full namespace path */` |
|     258 |  5175 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     188 |  5176 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     128 |  5177 | `				pLast = pGen->pIn;` |
|     128 |  5178 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 |  5179 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5180 | `				}` |
|     128 |  5181 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5182 | `			}` |
|     188 |  5183 | `			pGen->pIn++;` |
|       2 |  5184 | `		}` |
|      72 |  5185 | `		if( pLast == 0 ){` |
|       - |  5186 | `			/* Empty path */` |
|       5 |  5187 | `			break;` |
|       - |  5188 | `		}` |
|       - |  5189 | `		/* Default alias is the last component of the path */` |
|      68 |  5190 | `		sAlias = pLast->sData;` |
|       - |  5191 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5192 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      43 |  5193 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5194 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5195 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5196 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5197 | `				pGen->pIn++;` |
|       8 |  5198 | `			}` |
|       8 |  5199 | `		}` |
|       - |  5200 | `		/* Check for duplicate import alias (per-type) */` |
|      68 |  5201 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 |  5202 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5203 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5204 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 |  5205 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5206 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5207 | `				return SXERR_ABORT;` |
|       - |  5208 | `			}` |
|       2 |  5209 | `		}` |
|       - |  5210 | `		/* Register the import: alias -> FQN.` |
|       - |  5211 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5212 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5213 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     101 |  5214 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5215 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      68 |  5216 | `		if( zDup ){` |
|      68 |  5217 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      68 |  5218 | `			if( pVmHash ){` |
|       - |  5219 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5220 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      40 |  5221 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      40 |  5222 | `				if( zAliasDup ){` |
|      40 |  5223 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5224 | `				}` |
|      19 |  5225 | `			}` |
|      68 |  5226 | `			if( iUseType == 2 ){` |
|       - |  5227 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5228 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5229 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5230 | `				if( zAliasDup ){` |
|       - |  5231 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5232 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5233 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5234 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5235 | `					if( azPair ){` |
|      16 |  5236 | `						azPair[0] = zAliasDup;` |
|      16 |  5237 | `						azPair[1] = zDup;` |
|      16 |  5238 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5239 | `					}` |
|       7 |  5240 | `				}` |
|       7 |  5241 | `			}` |
|      33 |  5242 | `		}` |
|       - |  5243 | `		/* Check for comma (multiple use declarations) */` |
|      68 |  5244 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5245 | `			pGen->pIn++;` |
|       2 |  5246 | `		}else{` |
|      34 |  5247 | `			break;` |
|       - |  5248 | `		}` |
|       1 |  5249 | `	}` |
|      70 |  5250 | `	SyBlobRelease(&sPath);` |
|      70 |  5251 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5252 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5253 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5254 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5255 | `			return SXERR_ABORT;` |
|       - |  5256 | `		}` |
|       1 |  5257 | `	}` |
|      70 |  5258 | `	return SXRET_OK;` |
|      36 |  5259 |  |
|       - |  5260 | `/*` |
|       - |  5261 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5262 | ` *` |
|       - |  5263 | ` * According to the PHP language reference manual.` |
|       - |  5264 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5265 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5266 | ` *  declare (directive)` |
|       - |  5267 | ` *   statement` |
|       - |  5268 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5269 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5270 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5271 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5272 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5273 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5274 | ` * <?php` |
|       - |  5275 | ` * // these are the same:` |
|       - |  5276 | ` * // you can use this:` |
|       - |  5277 | ` * declare(ticks=1) {` |
|       - |  5278 | ` *   // entire script here` |
|       - |  5279 | ` * }` |
|       - |  5280 | ` * // or you can use this:` |
|       - |  5281 | ` * declare(ticks=1);` |
|       - |  5282 | ` * // entire script here` |
|       - |  5283 | ` * ?>` |
|       - |  5284 | ` *` |
|       - |  5285 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5286 | ` */` |
|       - |  5287 | `/*` |
|       - |  5288 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5289 | ` */` |
|      64 |  5290 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       2 |  5291 |  |
|      94 |  5292 | `	return SyStringLength(pName) == nWant` |
|      64 |  5293 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       2 |  5294 |  |
|       - |  5295 |  |
|      38 |  5296 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       2 |  5297 |  |
|      40 |  5298 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      40 |  5299 | `	SyToken *pBodyEnd = 0;` |
|       - |  5300 | `	SyToken *pBodyStart;` |
|       - |  5301 | `	SyToken *pCursor;` |
|       - |  5302 | `	int bHasStrictTypes;` |
|       - |  5303 | `	int bBlockForm;` |
|       - |  5304 | `	int bPlacementOk;` |
|       - |  5305 | `	sxi32 rc;` |
|      40 |  5306 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      40 |  5307 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5308 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5309 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5310 | `			return SXERR_ABORT;` |
|       - |  5311 | `		}` |
|       5 |  5312 | `		goto Synchro;` |
|       - |  5313 | `	}` |
|      36 |  5314 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      36 |  5315 | `	pBodyStart = pGen->pIn;` |
|       - |  5316 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      36 |  5317 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      36 |  5318 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5319 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5320 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5321 | `			return SXERR_ABORT;` |
|       - |  5322 | `		}` |
|     ! 0 |  5323 | `		return SXRET_OK;` |
|       - |  5324 | `	}` |
|       - |  5325 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5326 | `	 * now delimits the comma-separated directive list. */` |
|      36 |  5327 | `	pGen->pIn = &pBodyEnd[1];` |
|      36 |  5328 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5329 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5330 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5331 | `			return SXERR_ABORT;` |
|       - |  5332 | `		}` |
|     ! 0 |  5333 | `	}` |
|      36 |  5334 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      36 |  5335 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      36 |  5336 | `	bHasStrictTypes = 0;` |
|       - |  5337 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5338 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5339 | `	 * directive appears anywhere in the list, before validating values. */` |
|      36 |  5340 | `	pCursor = pBodyStart;` |
|      48 |  5341 | `	while( pCursor < pBodyEnd ){` |
|      44 |  5342 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      36 |  5343 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      32 |  5344 | `				bHasStrictTypes = 1;` |
|      32 |  5345 | `				break;` |
|       - |  5346 | `			}` |
|       2 |  5347 | `		}` |
|      13 |  5348 | `		pCursor++;` |
|       1 |  5349 | `	}` |
|      36 |  5350 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5351 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5352 | `			"strict_types declaration must not use block mode");` |
|       3 |  5353 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5354 | `		return SXRET_OK;` |
|       - |  5355 | `	}` |
|      34 |  5356 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       5 |  5357 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5358 | `			"strict_types declaration must be the very first statement in the script");` |
|       5 |  5359 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       5 |  5360 | `		return SXRET_OK;` |
|       - |  5361 | `	}` |
|       - |  5362 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      30 |  5363 | `	pCursor = pBodyStart;` |
|      58 |  5364 | `	while( pCursor < pBodyEnd ){` |
|       - |  5365 | `		SyToken *pNameTok;` |
|       - |  5366 | `		SyToken *pEqTok;` |
|       - |  5367 | `		SyToken *pValTok;` |
|       - |  5368 | `		SyString *pDirName;` |
|       - |  5369 | `		int bIsStrict;` |
|       - |  5370 | `		int iStrictValue;` |
|      32 |  5371 | `		pNameTok = pCursor;` |
|      32 |  5372 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5373 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5374 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5375 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5376 | `			return SXRET_OK;` |
|       - |  5377 | `		}` |
|      32 |  5378 | `		pEqTok = pNameTok + 1;` |
|      32 |  5379 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5380 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5381 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5382 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5383 | `			return SXRET_OK;` |
|       - |  5384 | `		}` |
|      32 |  5385 | `		pValTok = pEqTok + 1;` |
|      32 |  5386 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5387 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5388 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5389 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5390 | `			return SXRET_OK;` |
|       - |  5391 | `		}` |
|      32 |  5392 | `		pDirName = &pNameTok->sData;` |
|      32 |  5393 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      32 |  5394 | `		if( bIsStrict ){` |
|       - |  5395 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5396 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      28 |  5397 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5398 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5399 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5400 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5401 | `				return SXRET_OK;` |
|       - |  5402 | `			}` |
|      28 |  5403 | `			iStrictValue = -1;` |
|      28 |  5404 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      28 |  5405 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      28 |  5406 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      28 |  5407 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      26 |  5408 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      13 |  5409 | `			}` |
|      28 |  5410 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5411 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5412 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5413 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5414 | `				return SXRET_OK;` |
|       - |  5415 | `			}` |
|      26 |  5416 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      14 |  5417 | `		}else{` |
|       - |  5418 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5419 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5420 | `			 * behavior don't regress. */` |
|       7 |  5421 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5422 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5423 | `				ph7_lib_version()` |
|       - |  5424 | `				);` |
|       - |  5425 | `		}` |
|      30 |  5426 | `		pCursor = pValTok + 1;` |
|       - |  5427 | `		/* Consume separating comma (or end). */` |
|      30 |  5428 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5429 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5430 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5431 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5432 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5433 | `				return SXRET_OK;` |
|       - |  5434 | `			}` |
|       3 |  5435 | `			pCursor++;` |
|       1 |  5436 | `		}` |
|       2 |  5437 | `	}` |
|       - |  5438 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5439 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5440 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      28 |  5441 | `	return SXRET_OK;` |
|       2 |  5442 | `Synchro:` |
|       - |  5443 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5444 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5445 | `		pGen->pIn++;` |
|       1 |  5446 | `	}` |
|       5 |  5447 | `	return SXRET_OK;` |
|      21 |  5448 |  |
|       - |  5449 | `/*` |
|       - |  5450 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5451 | ` * as follows:` |
|       - |  5452 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5453 | ` * {` |
|       - |  5454 | ` *   return "Making a cup of $type.\n";` |
|       - |  5455 | ` * }` |
|       - |  5456 | ` * Symisc eXtension.` |
|       - |  5457 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5458 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5459 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5460 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5461 | ` *      {` |
|       - |  5462 | ` *       var_dump($a);` |
|       - |  5463 | ` *      }` |
|       - |  5464 | ` *     //call test without args` |
|       - |  5465 | ` *      test();` |
|       - |  5466 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5467 | ` *      Example:` |
|       - |  5468 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5469 | ` * 3 -) Function overloading!!` |
|       - |  5470 | ` *      Example:` |
|       - |  5471 | ` *      function foo($a) {` |
|       - |  5472 | ` *   	  return $a.PHP_EOL;` |
|       - |  5473 | ` *	    }` |
|       - |  5474 | ` *	    function foo($a, $b) {` |
|       - |  5475 | ` *   	  return $a + $b;` |
|       - |  5476 | ` *	    }` |
|       - |  5477 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5478 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5479 | ` *      // Same arg` |
|       - |  5480 | ` *	   function foo(string $a)` |
|       - |  5481 | ` *	   {` |
|       - |  5482 | ` *	     echo "a is a string\n";` |
|       - |  5483 | ` *	     var_dump($a);` |
|       - |  5484 | ` *	   }` |
|       - |  5485 | ` *	  function foo(int $a)` |
|       - |  5486 | ` *	  {` |
|       - |  5487 | ` *	    echo "a is integer\n";` |
|       - |  5488 | ` *	    var_dump($a);` |
|       - |  5489 | ` *	  }` |
|       - |  5490 | ` *	  function foo(array $a)` |
|       - |  5491 | ` *	  {` |
|       - |  5492 | ` * 	    echo "a is an array\n";` |
|       - |  5493 | ` * 	    var_dump($a);` |
|       - |  5494 | ` *	  }` |
|       - |  5495 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5496 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5497 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5498 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5499 | ` * introduced by the PH7 engine.` |
|       - |  5500 | ` */` |
|   57146 |  5501 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5502 |  |
|       - |  5503 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5504 | `	SySet *pInstrContainer;` |
|       - |  5505 | `	sxi32 rc;` |
|       - |  5506 | `	/* Swap token stream */` |
|   57148 |  5507 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   57148 |  5508 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   57148 |  5509 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5510 | `	/* Compile the expression holding the argument value */` |
|   57148 |  5511 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5512 | `	/* Emit the done instruction */` |
|   57148 |  5513 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   57148 |  5514 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   57148 |  5515 | `	RE_SWAP_DELIMITER(pGen);` |
|   57148 |  5516 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5517 | `		return SXERR_ABORT;` |
|       - |  5518 | `	}` |
|   57148 |  5519 | `	return SXRET_OK;` |
|   28575 |  5520 |  |
|       - |  5521 | `/*` |
|       - |  5522 | ` * Collect function arguments one after one.` |
|       - |  5523 | ` * According to the PHP language reference manual.` |
|       - |  5524 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5525 | ` * list of expressions.` |
|       - |  5526 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5527 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5528 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5529 | ` * for more information.` |
|       - |  5530 | ` * Example #1 Passing arrays to functions` |
|       - |  5531 | ` * <?php` |
|       - |  5532 | ` * function takes_array($input)` |
|       - |  5533 | ` * {` |
|       - |  5534 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5535 | ` * }` |
|       - |  5536 | ` * ?>` |
|       - |  5537 | ` * Making arguments be passed by reference` |
|       - |  5538 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5539 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5540 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5541 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5542 | ` * to the argument name in the function definition:` |
|       - |  5543 | ` * Example #2 Passing function parameters by reference` |
|       - |  5544 | ` * <?php` |
|       - |  5545 | ` * function add_some_extra(&$string)` |
|       - |  5546 | ` * {` |
|       - |  5547 | ` *   $string .= 'and something extra.';` |
|       - |  5548 | ` * }` |
|       - |  5549 | ` * $str = 'This is a string, ';` |
|       - |  5550 | ` * add_some_extra($str);` |
|       - |  5551 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5552 | ` * ?>` |
|       - |  5553 | ` *` |
|       - |  5554 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5555 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5556 | ` * on these extension.` |
|       - |  5557 | ` */` |
|   61030 |  5558 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       2 |  5559 |  |
|       - |  5560 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5561 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5562 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5563 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5564 | `	sxi32 rc;` |
|       - |  5565 |  |
|   61032 |  5566 | `	pIn = pGen->pIn;` |
|   61032 |  5567 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5568 | `	/* Process arguments one after one */` |
|   79375 |  5569 | `	for(;;){` |
|  158752 |  5570 | `		if( pIn >= pEnd ){` |
|       - |  5571 | `			/* No more arguments to process */` |
|   61020 |  5572 | `			break;` |
|       - |  5573 | `		}` |
|   97734 |  5574 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   97734 |  5575 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   97734 |  5576 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   97734 |  5577 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5578 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|   97734 |  5579 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   54262 |  5580 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   54262 |  5581 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      42 |  5582 | `				if( !bCtorCtx ){` |
|       5 |  5583 | `					if( bAbstractCtx ){` |
|       3 |  5584 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5585 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5586 | `					}else{` |
|       3 |  5587 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5588 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5589 | `					}` |
|       5 |  5590 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5591 | `						return SXERR_ABORT;` |
|       - |  5592 | `					}` |
|       5 |  5593 | `					return SXERR_SYNTAX;` |
|       - |  5594 | `				}` |
|      38 |  5595 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      38 |  5596 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5597 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      37 |  5598 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5599 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5600 | `				}else{` |
|      34 |  5601 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5602 | `				}` |
|      38 |  5603 | `				pIn++;` |
|      18 |  5604 | `			}` |
|   27128 |  5605 | `		}` |
|       - |  5606 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  129395 |  5607 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   82047 |  5608 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   64852 |  5609 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   63316 |  5610 | `			sxu32 nLineLocal = pIn->nLine;` |
|   63316 |  5611 | `			sxi32 iTFlags = 0;` |
|   63316 |  5612 | `			pGen->pIn = pIn;` |
|   63316 |  5613 | `			rc = GenStateParseUnionTypeDecl(` |
|   31657 |  5614 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   31657 |  5615 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5616 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5617 | `				/* bAllowVoid */ 0,` |
|   31657 |  5618 | `						nLineLocal);` |
|   63316 |  5619 | `			pIn = pGen->pIn;` |
|   63316 |  5620 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5621 | `				return SXERR_ABORT;` |
|   63316 |  5622 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5623 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5624 | `				return SXERR_SYNTAX;` |
|   63314 |  5625 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5626 | `				if( pIn < pEnd ){` |
|       7 |  5627 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5628 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5629 | `						&pIn->sData);` |
|       3 |  5630 | `				}else{` |
|     ! 0 |  5631 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5632 | `						"syntax error, unexpected end of file");` |
|       - |  5633 | `				}` |
|       5 |  5634 | `				return SXERR_SYNTAX;` |
|       - |  5635 | `			}` |
|   63310 |  5636 | `			sArg.iFlags \|= iTFlags;` |
|   31654 |  5637 | `		}` |
|   97724 |  5638 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5639 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5640 | `			return rc;` |
|       - |  5641 | `		}` |
|   97724 |  5642 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5643 | `			/* Pass by reference,record that */` |
|    3036 |  5644 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    3036 |  5645 | `			pIn++;` |
|    1517 |  5646 | `		}` |
|   97724 |  5647 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5648 | `			/* Variadic parameter: ...$args */` |
|      42 |  5649 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      42 |  5650 | `			pIn++;` |
|      20 |  5651 | `		}` |
|   97724 |  5652 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5653 | `			/* Invalid argument */` |
|     ! 0 |  5654 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5655 | `			return rc;` |
|       - |  5656 | `		}` |
|   97724 |  5657 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5658 | `		/* Copy argument name */` |
|   97724 |  5659 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   97724 |  5660 | `		if( zDup == 0 ){` |
|     ! 0 |  5661 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5662 | `			return SXERR_ABORT;` |
|       - |  5663 | `		}` |
|   97724 |  5664 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   97724 |  5665 | `		pIn++;` |
|   97724 |  5666 | `		if( pIn < pEnd ){` |
|   63788 |  5667 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5668 | `				SyToken *pDefend;` |
|   57150 |  5669 | `				sxi32 iNest = 0;` |
|   57150 |  5670 | `				pIn++; /* Jump the equal sign */` |
|   57150 |  5671 | `				pDefend = pIn;` |
|       - |  5672 | `				/* Process the default value associated with this argument */` |
|  120308 |  5673 | `				while( pDefend < pEnd ){` |
|   93228 |  5674 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   30070 |  5675 | `						break;` |
|       - |  5676 | `					}` |
|   63160 |  5677 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5678 | `						/* Increment nesting level */` |
|    3008 |  5679 | `						iNest++;` |
|   61657 |  5680 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5681 | `						/* Decrement nesting level */` |
|    3008 |  5682 | `						iNest--;` |
|    1503 |  5683 | `					}` |
|   63160 |  5684 | `					pDefend++;` |
|       2 |  5685 | `				}` |
|   57150 |  5686 | `				if( pIn >= pDefend ){` |
|       3 |  5687 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5688 | `					return rc;` |
|       - |  5689 | `				}` |
|       - |  5690 | `				/* Process default value */` |
|   57148 |  5691 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   57148 |  5692 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5693 | `					return rc;` |
|       - |  5694 | `				}` |
|       - |  5695 | `				/* Point beyond the default value */` |
|   57148 |  5696 | `				pIn = pDefend;` |
|   28573 |  5697 | `			}` |
|   63786 |  5698 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5699 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5700 | `				return rc;` |
|       - |  5701 | `			}` |
|   63786 |  5702 | `			pIn++; /* Jump the trailing comma */` |
|   31892 |  5703 | `		}` |
|       - |  5704 | `		/* Append argument signature */` |
|   97722 |  5705 | `		if( sArg.nType > 0 ){` |
|   63268 |  5706 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5707 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    9036 |  5708 | `				int marker = 'o';` |
|    9036 |  5709 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    9036 |  5710 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4519 |  5711 | `			}else{` |
|       - |  5712 | `				int c;` |
|   54234 |  5713 | `				c = 'n'; /* cc warning */` |
|       - |  5714 | `				/* Type leading character */` |
|   54234 |  5715 | `				switch(sArg.nType){` |
|     ! 0 |  5716 | `				case MEMOBJ_HASHMAP:` |
|       - |  5717 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5718 | `					c = 'h';` |
|     ! 0 |  5719 | `					break;` |
|    7549 |  5720 | `				case MEMOBJ_INT:` |
|       - |  5721 | `					/* Integer */` |
|   15100 |  5722 | `					c = 'i';` |
|   15100 |  5723 | `					break;` |
|       1 |  5724 | `				case MEMOBJ_BOOL:` |
|       - |  5725 | `					/* Bool */` |
|       3 |  5726 | `					c = 'b';` |
|       3 |  5727 | `					break;` |
|       1 |  5728 | `				case MEMOBJ_REAL:` |
|       - |  5729 | `					/* Float */` |
|       3 |  5730 | `					c = 'f';` |
|       3 |  5731 | `					break;` |
|   19558 |  5732 | `				case MEMOBJ_STRING:` |
|       - |  5733 | `					/* String */` |
|   39118 |  5734 | `					c = 's';` |
|   39118 |  5735 | `					break;` |
|       7 |  5736 | `				case MEMOBJ_OBJ:` |
|       - |  5737 | `					/* Object */` |
|      16 |  5738 | `					c = 'o';` |
|      14 |  5739 | `					break;` |
|     ! 0 |  5740 | `				default:` |
|     ! 0 |  5741 | `					break;` |
|       - |  5742 | `				}` |
|   54234 |  5743 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5744 | `			}` |
|   31635 |  5745 | `		}else{` |
|       - |  5746 | `			/* No type is associated with this parameter which mean` |
|       - |  5747 | `			 * that this function is not condidate for overloading.` |
|       - |  5748 | `			 */` |
|   34456 |  5749 | `			SyBlobRelease(&sSig);` |
|       - |  5750 | `		}` |
|       - |  5751 | `		/* Save in the argument set */` |
|   97722 |  5752 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5753 | `	}` |
|   61020 |  5754 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5755 | `		/* Save function signature */` |
|   39198 |  5756 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   19598 |  5757 | `	}` |
|   61020 |  5758 | `	return SXRET_OK;` |
|   30517 |  5759 |  |
|       - |  5760 | `/*` |
|       - |  5761 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5762 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5763 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5764 | ` */` |
|  187820 |  5765 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5766 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5767 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5768 | `	)` |
|       2 |  5769 |  |
|       - |  5770 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5771 | `	GenBlock *pBlock;` |
|       - |  5772 | `	sxu32 nGotoOfft;` |
|       - |  5773 | `	sxi32 rc;` |
|       - |  5774 | `	/* Attach the new function */` |
|  187822 |  5775 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  187822 |  5776 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5777 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5778 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5779 | `		return SXERR_ABORT;` |
|       - |  5780 | `	}` |
|  187822 |  5781 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5782 | `	/* Swap bytecode containers */` |
|  187822 |  5783 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  187822 |  5784 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5785 | `	/* Emit constructor property promotion prologue:` |
|       - |  5786 | `	 *   $this->NAME = $NAME;` |
|       - |  5787 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5788 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5789 | `	{` |
|  187822 |  5790 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5791 | `		sxu32 i;` |
|  282456 |  5792 | `		for( i = 0; i < nArg; i++ ){` |
|   94636 |  5793 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5794 | `			char *zSrc;` |
|       - |  5795 | `			sxu32 nSrc,nName;` |
|       - |  5796 | `			SySet sToken;` |
|       - |  5797 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5798 | `			sxi32 rcPromote;` |
|   94636 |  5799 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   94608 |  5800 | `				continue;` |
|       - |  5801 | `			}` |
|       - |  5802 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5803 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5804 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5805 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5806 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      30 |  5807 | `			nName = SyStringLength(&pArg->sName);` |
|      30 |  5808 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      30 |  5809 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      30 |  5810 | `			if( zSrc == 0 ){` |
|     ! 0 |  5811 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5812 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5813 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5814 | `				return SXERR_ABORT;` |
|       - |  5815 | `			}` |
|       - |  5816 | `			{` |
|      30 |  5817 | `				char *z = zSrc;` |
|      30 |  5818 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      30 |  5819 | `				z += sizeof("$this->")-1;` |
|      30 |  5820 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5821 | `				z += nName;` |
|      30 |  5822 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      30 |  5823 | `				z += sizeof(" = $")-1;` |
|      30 |  5824 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5825 | `				z += nName;` |
|      30 |  5826 | `				*z = 0;` |
|       - |  5827 | `			}` |
|      30 |  5828 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      30 |  5829 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      30 |  5830 | `			pTmpIn = pGen->pIn;` |
|      30 |  5831 | `			pTmpEnd = pGen->pEnd;` |
|      30 |  5832 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      30 |  5833 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      30 |  5834 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  5835 | `			pGen->pIn = pTmpIn;` |
|      30 |  5836 | `			pGen->pEnd = pTmpEnd;` |
|      30 |  5837 | `			SySetRelease(&sToken);` |
|      30 |  5838 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5839 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5840 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5841 | `				return SXERR_ABORT;` |
|       - |  5842 | `			}` |
|       - |  5843 | `			/* Discard the assignment result — this is a statement expression. */` |
|      30 |  5844 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      16 |  5845 | `		}` |
|       - |  5846 | `	}` |
|       - |  5847 | `	/* Compile the body */` |
|  187822 |  5848 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5849 | `	/* Fix exception jumps now the destination is resolved */` |
|  187822 |  5850 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5851 | `	/* Emit the final return if not yet done */` |
|  187822 |  5852 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5853 | `	/* Fix gotos jumps now the destination is resolved */` |
|  187822 |  5854 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5855 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5856 | `	}` |
|  187822 |  5857 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5858 | `	/* Restore the default container */` |
|  187822 |  5859 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5860 | `	/* Leave function block */` |
|  187822 |  5861 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  187822 |  5862 | `	if( rc == SXERR_ABORT ){` |
|       - |  5863 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5864 | `		return SXERR_ABORT;` |
|       - |  5865 | `	}` |
|       - |  5866 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5867 | `	{` |
|  187822 |  5868 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5869 | `		sxu32 i;` |
| 3672118 |  5870 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3484316 |  5871 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5872 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5873 | `				break;` |
|       - |  5874 | `			}` |
| 1742150 |  5875 | `		}` |
|       - |  5876 | `	}` |
|       - |  5877 | `	/* All done, function body compiled */` |
|  187822 |  5878 | `	return SXRET_OK;` |
|   93912 |  5879 |  |
|       - |  5880 | `/*` |
|       - |  5881 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5882 | ` * According to the PHP language reference manual.` |
|       - |  5883 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5884 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5885 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5886 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5887 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5888 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5889 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5890 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5891 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5892 | ` *` |
|       - |  5893 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5894 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5895 | ` * on these extension.` |
|       - |  5896 | ` */` |
|       - |  5897 | `/*` |
|       - |  5898 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5899 | ` */` |
|      92 |  5900 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5901 |  |
|       - |  5902 | `	sxu32 i;` |
|     286 |  5903 | `	for( i = 0; i < n; i++ ){` |
|     252 |  5904 | `		int a = zA[i], b = zB[i];` |
|     252 |  5905 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     252 |  5906 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     252 |  5907 | `		if( a != b ) return a - b;` |
|      98 |  5908 | `	}` |
|      36 |  5909 | `	return 0;` |
|      48 |  5910 |  |
|       - |  5911 | `/*` |
|       - |  5912 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5913 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5914 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5915 | ` */` |
|       - |  5916 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5917 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5918 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5919 |  |
|       - |  5920 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5921 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5922 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5923 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5924 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5925 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5926 |  |
|       - |  5927 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5928 | `struct PhlTypeAtom {` |
|       - |  5929 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5930 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5931 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5932 | `	sxu32 nCanon;` |
|       - |  5933 | `};` |
|       - |  5934 |  |
|       - |  5935 | `/*` |
|       - |  5936 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5937 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5938 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5939 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5940 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5941 | ` * already be consumed by the caller.` |
|       - |  5942 | ` */` |
|   63662 |  5943 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5944 |  |
|   63664 |  5945 | `	SyToken *pIn = pGen->pIn;` |
|   63664 |  5946 | `	SyZero(pOut, sizeof(*pOut));` |
|   63664 |  5947 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   63664 |  5948 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5949 | `		return SXERR_SYNTAX;` |
|       - |  5950 | `	}` |
|       - |  5951 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   63664 |  5952 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5953 | `		pIn++;` |
|       8 |  5954 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5955 | `			return SXERR_SYNTAX;` |
|       - |  5956 | `		}` |
|       3 |  5957 | `	}` |
|   63664 |  5958 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5959 | `		return SXERR_SYNTAX;` |
|       - |  5960 | `	}` |
|   63664 |  5961 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   54558 |  5962 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   54558 |  5963 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5964 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   54551 |  5965 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      12 |  5966 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   54539 |  5967 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   15254 |  5968 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   46908 |  5969 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   39228 |  5970 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   19669 |  5971 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      26 |  5972 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      44 |  5973 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5974 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5975 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5976 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5977 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5978 | `			pOut->sClass = pIn->sData;` |
|       4 |  5979 | `		}else{` |
|       3 |  5980 | `			return SXERR_SYNTAX;` |
|       - |  5981 | `		}` |
|   54556 |  5982 | `		pIn++;` |
|   27279 |  5983 | `	}else{` |
|       - |  5984 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5985 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    9108 |  5986 | `		SyString *pT = &pIn->sData;` |
|    9108 |  5987 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5988 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5989 | `			pIn++;` |
|    9103 |  5990 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      20 |  5991 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      20 |  5992 | `			pIn++;` |
|    9089 |  5993 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5994 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5995 | `			pIn++;` |
|       2 |  5996 | `		}else{` |
|       - |  5997 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    9078 |  5998 | `			SyToken *pFirst = pIn;` |
|    9078 |  5999 | `			SyToken *pLast = pIn;` |
|    9078 |  6000 | `			pOut->nType = SXU32_HIGH;` |
|    9078 |  6001 | `			pOut->sClass = pIn->sData;` |
|    9078 |  6002 | `			pIn++;` |
|   13617 |  6003 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    9081 |  6004 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  6005 | `				pLast = &pIn[1];` |
|       3 |  6006 | `				pIn += 2;` |
|       1 |  6007 | `			}` |
|    9078 |  6008 | `			if( pLast != pFirst ){` |
|       3 |  6009 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  6010 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  6011 | `				pOut->sClass.zString = zFirst;` |
|       3 |  6012 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  6013 | `			}` |
|       - |  6014 | `		}` |
|       - |  6015 | `	}` |
|   63662 |  6016 | `	pGen->pIn = pIn;` |
|   63662 |  6017 | `	return SXRET_OK;` |
|   31833 |  6018 |  |
|       - |  6019 |  |
|       - |  6020 | `/*` |
|       - |  6021 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  6022 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6023 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6024 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6025 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6026 | ` */` |
|   63564 |  6027 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  6028 |  |
|       - |  6029 | `	int i;` |
|   63566 |  6030 | `	int nNonNull = 0;` |
|  127212 |  6031 | `	for( i = 0; i < nAtoms; i++ ){` |
|   63648 |  6032 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   63638 |  6033 | `			nNonNull++;` |
|   31818 |  6034 | `		}` |
|   31825 |  6035 | `	}` |
|   63566 |  6036 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6037 | `		/* Shorthand: ?T */` |
|      56 |  6038 | `		for( i = 0; i < nAtoms; i++ ){` |
|      56 |  6039 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      56 |  6040 | `			SyBlobAppend(pBlob, "?", 1);` |
|      56 |  6041 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6042 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  6043 | `			}else{` |
|      46 |  6044 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6045 | `			}` |
|      56 |  6046 | `			return;` |
|     ! 0 |  6047 | `		}` |
|     ! 0 |  6048 | `	}` |
|       - |  6049 | `	{` |
|   63512 |  6050 | `		int bFirst = 1;` |
|       - |  6051 | `		/* 1) Classes in declaration order */` |
|  127098 |  6052 | `		for( i = 0; i < nAtoms; i++ ){` |
|   63588 |  6053 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    9072 |  6054 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    9072 |  6055 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    9072 |  6056 | `				bFirst = 0;` |
|    4535 |  6057 | `			}` |
|   31795 |  6058 | `		}` |
|       - |  6059 | `		/* 2) Built-ins in canonical order */` |
|       - |  6060 | `		{` |
|       - |  6061 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6062 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6063 | `			int k;` |
|  444572 |  6064 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  707990 |  6065 | `				for( i = 0; i < nAtoms; i++ ){` |
|  381426 |  6066 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   54498 |  6067 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   54498 |  6068 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   54498 |  6069 | `						bFirst = 0;` |
|   54498 |  6070 | `						break;` |
|       - |  6071 | `					}` |
|  163466 |  6072 | `				}` |
|  190532 |  6073 | `			}` |
|       - |  6074 | `		}` |
|       - |  6075 | `		/* 3) null suffix */` |
|   63512 |  6076 | `		if( bNullable ){` |
|       6 |  6077 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  6078 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  6079 | `		}` |
|       - |  6080 | `	}` |
|   31784 |  6081 |  |
|       - |  6082 |  |
|       - |  6083 | `/*` |
|       - |  6084 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6085 | ` *` |
|       - |  6086 | ` * Outputs:` |
|       - |  6087 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6088 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6089 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6090 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6091 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6092 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6093 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6094 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6095 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6096 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6097 | ` *` |
|       - |  6098 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6099 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6100 | ` */` |
|   63574 |  6101 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6102 | `	ph7_gen_state *pGen,` |
|       - |  6103 | `	sxu32 *pnType,` |
|       - |  6104 | `	SyString *pClass,` |
|       - |  6105 | `	SySet *pAlts,` |
|       - |  6106 | `	sxi32 *piTypeFlags,` |
|       - |  6107 | `	SyString *pTypeText,` |
|       - |  6108 | `	int iNullableFlag,` |
|       - |  6109 | `	int iUnionFlag,` |
|       - |  6110 | `	int bAllowVoid,` |
|       - |  6111 | `	sxu32 nLine` |
|       2 |  6112 | `){` |
|       - |  6113 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   63576 |  6114 | `	int nAtoms = 0;` |
|   63576 |  6115 | `	int bShortNullable = 0;` |
|   63576 |  6116 | `	int bExplicitNull = 0;` |
|       - |  6117 | `	sxi32 rc;` |
|   63576 |  6118 | `	*pnType = 0;` |
|   63576 |  6119 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   63576 |  6120 | `	*piTypeFlags = 0;` |
|   63576 |  6121 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6122 |  |
|   63576 |  6123 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6124 | `		return SXRET_OK;` |
|       - |  6125 | `	}` |
|       - |  6126 | ``	/* Optional `?` shorthand prefix */`` |
|   63574 |  6127 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      52 |  6128 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      52 |  6129 | `		bShortNullable = 1;` |
|      52 |  6130 | `		pGen->pIn++;` |
|      52 |  6131 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6132 | `			return SXERR_SYNTAX;` |
|       - |  6133 | `		}` |
|      25 |  6134 | `	}` |
|       - |  6135 | `	/* First atom is mandatory */` |
|   63576 |  6136 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   63576 |  6137 | `	if( rc != SXRET_OK ){` |
|       3 |  6138 | `		return rc;` |
|       - |  6139 | `	}` |
|   63574 |  6140 | `	nAtoms = 1;` |
|       - |  6141 | ``	/* Subsequent atoms separated by `\|` */`` |
|   95492 |  6142 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   63708 |  6143 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      92 |  6144 | `		if( bShortNullable ){` |
|       - |  6145 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6146 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6147 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6148 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6149 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6150 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6151 | `		}` |
|      90 |  6152 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6153 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6154 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6155 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6156 | `		}` |
|      90 |  6157 | ``		pGen->pIn++; /* skip `\|` */`` |
|      90 |  6158 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      90 |  6159 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6160 | `			return rc;` |
|       - |  6161 | `		}` |
|      90 |  6162 | `		nAtoms++;` |
|       2 |  6163 | `	}` |
|       - |  6164 | `	/* Validation pass.` |
|       - |  6165 | `	 *` |
|       - |  6166 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6167 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6168 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6169 | `	 */` |
|       - |  6170 | `	{` |
|       - |  6171 | `		int i, j;` |
|   63572 |  6172 | `		int bHasNonNull = 0;` |
|  127224 |  6173 | `		for( i = 0; i < nAtoms; i++ ){` |
|   63660 |  6174 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      20 |  6175 | `				if( nAtoms > 1 ){` |
|       3 |  6176 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6177 | `						"Void can only be used as a standalone type");` |
|       3 |  6178 | `					return SXERR_SYNTAX;` |
|       - |  6179 | `				}` |
|      18 |  6180 | `				if( !bAllowVoid ){` |
|     ! 0 |  6181 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6182 | `						"void cannot be used here");` |
|     ! 0 |  6183 | `					return SXERR_SYNTAX;` |
|       - |  6184 | `				}` |
|      18 |  6185 | `				if( bShortNullable ){` |
|     ! 0 |  6186 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6187 | `						"Void type cannot be nullable");` |
|     ! 0 |  6188 | `					return SXERR_SYNTAX;` |
|       - |  6189 | `				}` |
|       8 |  6190 | `			}` |
|   63658 |  6191 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6192 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6193 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6194 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6195 | `				 * does not return), and folding them would mislead any` |
|       - |  6196 | `				 * future return-enforcement work. */` |
|       3 |  6197 | `				if( nAtoms > 1 ){` |
|       3 |  6198 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6199 | `						"never can only be used as a standalone type");` |
|       3 |  6200 | `					return SXERR_SYNTAX;` |
|       - |  6201 | `				}` |
|     ! 0 |  6202 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6203 | `					"never type is not yet implemented");` |
|     ! 0 |  6204 | `				return SXERR_SYNTAX;` |
|       - |  6205 | `			}` |
|   63656 |  6206 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  6207 | `				bExplicitNull = 1;` |
|       7 |  6208 | `			}else{` |
|   63646 |  6209 | `				bHasNonNull = 1;` |
|       - |  6210 | `			}` |
|       - |  6211 | `			/* Duplicate detection */` |
|   63774 |  6212 | `			for( j = 0; j < i; j++ ){` |
|     122 |  6213 | `				int bDup = 0;` |
|     122 |  6214 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  6215 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6216 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6217 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6218 | `								aAtoms[j].sClass.zString,` |
|      12 |  6219 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6220 | `							bDup = 1;` |
|     ! 0 |  6221 | `						}` |
|       8 |  6222 | `					}else{` |
|       3 |  6223 | `						bDup = 1;` |
|       - |  6224 | `					}` |
|       7 |  6225 | `				}` |
|     122 |  6226 | `				if( bDup ){` |
|       - |  6227 | `					const char *zName;` |
|       - |  6228 | `					sxu32 nName;` |
|       3 |  6229 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6230 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6231 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6232 | `					}else{` |
|       3 |  6233 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6234 | `						nName = aAtoms[i].nCanon;` |
|       - |  6235 | `					}` |
|       4 |  6236 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6237 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6238 | `					return SXERR_SYNTAX;` |
|       - |  6239 | `				}` |
|      61 |  6240 | `			}` |
|   31828 |  6241 | `		}` |
|   63566 |  6242 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  6243 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6244 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  6245 | `			return SXERR_SYNTAX;` |
|       - |  6246 | `		}` |
|       - |  6247 | `	}` |
|       - |  6248 | `	/* Compute nullability flag */` |
|   63566 |  6249 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      60 |  6250 | `		*piTypeFlags \|= iNullableFlag;` |
|      29 |  6251 | `	}` |
|       - |  6252 | `	/* Build canonical type text */` |
|   63566 |  6253 | `	if( pTypeText ){` |
|       - |  6254 | `		SyBlob sBlob;` |
|   63566 |  6255 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   95324 |  6256 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   31782 |  6257 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   63566 |  6258 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   95324 |  6259 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   63548 |  6260 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   63550 |  6261 | `			if( zDup ){` |
|   63550 |  6262 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   31774 |  6263 | `			}` |
|   31774 |  6264 | `		}` |
|   63566 |  6265 | `		SyBlobRelease(&sBlob);` |
|   31782 |  6266 | `	}` |
|       - |  6267 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6268 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6269 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6270 | `	{` |
|   63566 |  6271 | `		int nNonNull = 0;` |
|   63566 |  6272 | `		int iNonNullIdx = -1;` |
|       - |  6273 | `		int i;` |
|  127212 |  6274 | `		for( i = 0; i < nAtoms; i++ ){` |
|   63648 |  6275 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   63638 |  6276 | `				nNonNull++;` |
|   63638 |  6277 | `				iNonNullIdx = i;` |
|   31818 |  6278 | `			}` |
|   31825 |  6279 | `		}` |
|   63566 |  6280 | `		if( nNonNull <= 1 ){` |
|       - |  6281 | `			/* Fast path: store as single type. */` |
|   63510 |  6282 | `			if( iNonNullIdx >= 0 ){` |
|   63510 |  6283 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   63510 |  6284 | `				if( pA->nType == SXU32_HIGH ){` |
|   13583 |  6285 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4527 |  6286 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    9056 |  6287 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    9056 |  6288 | `					*pnType = SXU32_HIGH;` |
|    9056 |  6289 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   58983 |  6290 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      18 |  6291 | `					*pnType = MEMOBJ_VOID;` |
|      10 |  6292 | `				}else{` |
|       - |  6293 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6294 | `					 * pass above rejects it as not-yet-implemented. */` |
|   54440 |  6295 | `					*pnType = pA->nType;` |
|       - |  6296 | `				}` |
|   31754 |  6297 | `			}` |
|   31756 |  6298 | `		}else{` |
|       - |  6299 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      58 |  6300 | `			*piTypeFlags \|= iUnionFlag;` |
|     190 |  6301 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6302 | `				ph7_type_alt sAlt;` |
|     134 |  6303 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     130 |  6304 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     130 |  6305 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      41 |  6306 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      13 |  6307 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      28 |  6308 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      28 |  6309 | `					sAlt.nType = SXU32_HIGH;` |
|      28 |  6310 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      15 |  6311 | `				}else{` |
|     104 |  6312 | `					sAlt.nType = aAtoms[i].nType;` |
|     104 |  6313 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6314 | `				}` |
|     130 |  6315 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      66 |  6316 | `			}` |
|       - |  6317 | `		}` |
|       - |  6318 | `	}` |
|   63566 |  6319 | `	return SXRET_OK;` |
|   31789 |  6320 |  |
|       - |  6321 |  |
|       - |  6322 | `/*` |
|       - |  6323 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6324 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6325 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6326 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6327 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6328 | `` *          and union types `: T\|U`.`` |
|       - |  6329 | ` */` |
|  236062 |  6330 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6331 |  |
|  236064 |  6332 | `	sxi32 iFlags = 0;` |
|       - |  6333 | `	sxi32 rc;` |
|       - |  6334 | `	sxu32 nLine;` |
|  236064 |  6335 | `	pFunc->nReturnType = 0;` |
|  236064 |  6336 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  236064 |  6337 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  236064 |  6338 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  235926 |  6339 | `		return SXRET_OK;` |
|       - |  6340 | `	}` |
|     140 |  6341 | `	pGen->pIn++; /* Skip ':' */` |
|     140 |  6342 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6343 | `		return SXRET_OK;` |
|       - |  6344 | `	}` |
|     140 |  6345 | `	nLine = pGen->pIn->nLine;` |
|     140 |  6346 | `	rc = GenStateParseUnionTypeDecl(` |
|      69 |  6347 | `		pGen,` |
|      69 |  6348 | `		&pFunc->nReturnType,` |
|      69 |  6349 | `		&pFunc->sReturnClass,` |
|      69 |  6350 | `		&pFunc->aReturnUnion,` |
|       - |  6351 | `		&iFlags,` |
|      69 |  6352 | `		&pFunc->sReturnTypeName,` |
|       - |  6353 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6354 | `		/* iUnionFlag */ 0,` |
|       - |  6355 | `		/* bAllowVoid */ 1,` |
|      69 |  6356 | `		nLine);` |
|      69 |  6357 | `	(void)iFlags;` |
|     140 |  6358 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6359 | `		return SXERR_ABORT;` |
|       - |  6360 | `	}` |
|     140 |  6361 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6362 | `		/* Error already reported */` |
|     ! 0 |  6363 | `		return SXERR_SYNTAX;` |
|       - |  6364 | `	}` |
|     140 |  6365 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6366 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6367 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6368 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6369 | `				&pGen->pIn->sData);` |
|       3 |  6370 | `		}else{` |
|     ! 0 |  6371 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6372 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6373 | `		}` |
|       5 |  6374 | `		return SXERR_SYNTAX;` |
|       - |  6375 | `	}` |
|     136 |  6376 | `	return SXRET_OK;` |
|  118033 |  6377 |  |
|       - |  6378 |  |
|   39976 |  6379 | `static sxi32 GenStateCompileFunc(` |
|       - |  6380 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6381 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6382 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6383 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6384 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6385 | `	)` |
|       2 |  6386 |  |
|       - |  6387 | `	ph7_vm_func *pFunc;` |
|       - |  6388 | `	SyToken *pEnd;` |
|       - |  6389 | `	sxu32 nLine;` |
|       - |  6390 | `	char *zName;` |
|       - |  6391 | `	sxi32 rc;` |
|       - |  6392 | `	/* Extract line number */` |
|   39978 |  6393 | `	nLine = pGen->pIn->nLine;` |
|       - |  6394 | `	/* Jump the left parenthesis '(' */` |
|   39978 |  6395 | `	pGen->pIn++;` |
|       - |  6396 | `	/* Delimit the function signature */` |
|   39978 |  6397 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   39978 |  6398 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6399 | `		/* Syntax error */` |
|       7 |  6400 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6401 | `		if( rc == SXERR_ABORT ){` |
|       - |  6402 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6403 | `			return SXERR_ABORT;` |
|       - |  6404 | `		}` |
|       7 |  6405 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6406 | `		return SXRET_OK;` |
|       - |  6407 | `	}` |
|       - |  6408 | `	/* Create the function state */` |
|   39972 |  6409 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   39972 |  6410 | `	if( pFunc == 0 ){` |
|     ! 0 |  6411 | `		goto OutOfMem;` |
|       - |  6412 | `	}` |
|       - |  6413 | `	/* Build the function name, prepending namespace if active */` |
|   39979 |  6414 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6415 | `		SyBlob sFQN;` |
|       - |  6416 | `		sxu32 nLen;` |
|      16 |  6417 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6418 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6419 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6420 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6421 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6422 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6423 | `		SyBlobRelease(&sFQN);` |
|      16 |  6424 | `		if( zName == 0 ){` |
|     ! 0 |  6425 | `			goto OutOfMem;` |
|       - |  6426 | `		}` |
|      16 |  6427 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6428 | `	}else{` |
|   39958 |  6429 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   39958 |  6430 | `		if( zName == 0 ){` |
|     ! 0 |  6431 | `			goto OutOfMem;` |
|       - |  6432 | `		}` |
|   39958 |  6433 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6434 | `	}` |
|   39972 |  6435 | `	if( pGen->pIn < pEnd ){` |
|       - |  6436 | `		/* Collect function arguments */` |
|   27734 |  6437 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   27734 |  6438 | `		if( rc == SXERR_ABORT ){` |
|       - |  6439 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6440 | `			return SXERR_ABORT;` |
|       - |  6441 | `		}` |
|   13866 |  6442 | `	}` |
|       - |  6443 | `	/* Point past ')' and parse optional return type ': type' */` |
|   39972 |  6444 | `	pGen->pIn = &pEnd[1];` |
|       - |  6445 | `	{` |
|   39972 |  6446 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   39972 |  6447 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6448 | `			return SXERR_ABORT;` |
|   39972 |  6449 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6450 | `			return SXERR_SYNTAX;` |
|       - |  6451 | `		}` |
|       - |  6452 | `	}` |
|   39968 |  6453 | `	if( bHandleClosure ){` |
|       - |  6454 | `		ph7_vm_func_closure_env sEnv;` |
|     192 |  6455 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     190 |  6456 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     105 |  6457 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      18 |  6458 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6459 | `				/* Closure,record environment variable */` |
|      18 |  6460 | `				pGen->pIn++;` |
|      18 |  6461 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6462 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6463 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6464 | `						return SXERR_ABORT;` |
|       - |  6465 | `					}` |
|     ! 0 |  6466 | `				}` |
|      18 |  6467 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6468 | `				/* Compile until we hit the first closing parenthesis */` |
|      38 |  6469 | `				while( pGen->pIn < pGen->pEnd ){` |
|      38 |  6470 | `					int iFlagsLocal = 0;` |
|      38 |  6471 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      18 |  6472 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      18 |  6473 | `						break;` |
|       - |  6474 | `					}` |
|      22 |  6475 | `					nLineLocal = pGen->pIn->nLine;` |
|      22 |  6476 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6477 | `						/* Pass by reference,record that */` |
|     ! 0 |  6478 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6479 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6480 | `							);` |
|     ! 0 |  6481 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6482 | `						pGen->pIn++;` |
|     ! 0 |  6483 | `					}` |
|      20 |  6484 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      22 |  6485 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6486 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6487 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6488 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6489 | `								return SXERR_ABORT;` |
|       - |  6490 | `							}` |
|       - |  6491 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6492 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6493 | `								pGen->pIn++;` |
|     ! 0 |  6494 | `							}` |
|     ! 0 |  6495 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6496 | `								pGen->pIn++;` |
|     ! 0 |  6497 | `							}` |
|     ! 0 |  6498 | `							break;` |
|       - |  6499 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6500 | `					}else{` |
|       - |  6501 | `						SyString *pNameLocal;` |
|       - |  6502 | `						char *zDup;` |
|       - |  6503 | `						/* Duplicate variable name */` |
|      22 |  6504 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      22 |  6505 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      22 |  6506 | `						if( zDup ){` |
|       - |  6507 | `							/* Zero the structure */` |
|      22 |  6508 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      22 |  6509 | `							sEnv.iFlags = iFlagsLocal;` |
|      22 |  6510 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      22 |  6511 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      22 |  6512 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6513 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6514 | `									got_this = 1;` |
|     ! 0 |  6515 | `							}` |
|       - |  6516 | `							/* Save imported variable */` |
|      22 |  6517 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      12 |  6518 | `						}else{` |
|     ! 0 |  6519 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6520 | `							 return SXERR_ABORT;` |
|       - |  6521 | `						}` |
|       - |  6522 | `					}` |
|      22 |  6523 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      28 |  6524 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6525 | `						/* Ignore trailing commas */` |
|       7 |  6526 | `						pGen->pIn++;` |
|       1 |  6527 | `					}` |
|       2 |  6528 | `				}` |
|      18 |  6529 | `				if( !got_this ){` |
|       - |  6530 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6531 | `					 * available to the closure environment.` |
|       - |  6532 | `					 */` |
|      18 |  6533 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      18 |  6534 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      18 |  6535 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      18 |  6536 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      18 |  6537 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       8 |  6538 | `				}` |
|      18 |  6539 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6540 | `					/* Mark as closure */` |
|      18 |  6541 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       8 |  6542 | `				}` |
|       8 |  6543 | `		}` |
|      95 |  6544 | `	}` |
|       - |  6545 | `	/* Compile the body */` |
|   39968 |  6546 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   39968 |  6547 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6548 | `		return SXERR_ABORT;` |
|       - |  6549 | `	}` |
|   39968 |  6550 | `	if( ppFunc ){` |
|     192 |  6551 | `		*ppFunc = pFunc;` |
|      95 |  6552 | `	}` |
|   39968 |  6553 | `	rc = SXRET_OK;` |
|   39968 |  6554 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6555 | `		/* Finally register the function */` |
|   39952 |  6556 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   19975 |  6557 | `	}` |
|   39968 |  6558 | `	if( rc == SXRET_OK ){` |
|   39968 |  6559 | `		return SXRET_OK;` |
|       - |  6560 | `	}` |
|       - |  6561 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6562 | `OutOfMem:` |
|       - |  6563 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6564 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6565 | `	 */` |
|     ! 0 |  6566 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6567 | `	return SXERR_ABORT;` |
|   19990 |  6568 |  |
|       - |  6569 | `/*` |
|       - |  6570 | ` * Compile a standard PHP function.` |
|       - |  6571 | ` *  Refer to the block-comment above for more information.` |
|       - |  6572 | ` */` |
|   39792 |  6573 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6574 |  |
|       - |  6575 | `	SyString *pName;` |
|       - |  6576 | `	sxi32 iFlags;` |
|       - |  6577 | `	sxu32 nLine;` |
|       - |  6578 | `	sxi32 rc;` |
|       - |  6579 |  |
|   39794 |  6580 | `	nLine = pGen->pIn->nLine;` |
|   39794 |  6581 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   39794 |  6582 | `	iFlags = 0;` |
|   39794 |  6583 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6584 | `		/* Return by reference,remember that */` |
|       7 |  6585 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6586 | `		/* Jump the '&' token */` |
|       7 |  6587 | `		pGen->pIn++;` |
|       3 |  6588 | `	}` |
|   39794 |  6589 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6590 | `		/* Invalid function name */` |
|       5 |  6591 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6592 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6593 | `			return SXERR_ABORT;` |
|       - |  6594 | `		}` |
|       - |  6595 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6596 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6597 | `			pGen->pIn++;` |
|       1 |  6598 | `		}` |
|       5 |  6599 | `		return SXRET_OK;` |
|       - |  6600 | `	}` |
|   39790 |  6601 | `	pName = &pGen->pIn->sData;` |
|   39790 |  6602 | `	nLine = pGen->pIn->nLine;` |
|       - |  6603 | `	/* Jump the function name */` |
|   39790 |  6604 | `	pGen->pIn++;` |
|   39790 |  6605 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6606 | `		/* Syntax error */` |
|       3 |  6607 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6608 | `		if( rc == SXERR_ABORT ){` |
|       - |  6609 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6610 | `			return SXERR_ABORT;` |
|       - |  6611 | `		}` |
|       - |  6612 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6613 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6614 | `			pGen->pIn++;` |
|     ! 0 |  6615 | `		}` |
|       3 |  6616 | `		return SXRET_OK;` |
|       - |  6617 | `	}` |
|       - |  6618 | `	/* Compile function body */` |
|   39788 |  6619 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   39788 |  6620 | `	return rc;` |
|   19898 |  6621 |  |
|       - |  6622 | `/*` |
|       - |  6623 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6624 | ` * According to the PHP language reference manual` |
|       - |  6625 | ` *  Visibility:` |
|       - |  6626 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6627 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6628 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6629 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6630 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6631 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6632 | ` */` |
|  253592 |  6633 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6634 |  |
|  253594 |  6635 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    9092 |  6636 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  244504 |  6637 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   39122 |  6638 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6639 | `	}` |
|       - |  6640 | `	/* Assume public by default */` |
|  205384 |  6641 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  126798 |  6642 |  |
|       - |  6643 | `/*` |
|       - |  6644 | ` * Compile a class constant.` |
|       - |  6645 | ` * According to the PHP language reference manual` |
|       - |  6646 | ` *  Class Constants` |
|       - |  6647 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6648 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6649 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6650 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6651 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6652 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6653 | ` * Symisc eXtension.` |
|       - |  6654 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6655 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6656 | ` *  Example:` |
|       - |  6657 | ` *   class Test{` |
|       - |  6658 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6659 | ` *   };` |
|       - |  6660 | ` *   var_dump(TEST::MyConst);` |
|       - |  6661 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6662 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6663 | ` */` |
|      30 |  6664 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6665 |  |
|      32 |  6666 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6667 | `	SySet *pInstrContainer;` |
|       - |  6668 | `	ph7_class_attr *pCons;` |
|       - |  6669 | `	SyString *pName;` |
|       - |  6670 | `	sxi32 rc;` |
|       - |  6671 | `	/* Extract visibility level */` |
|      32 |  6672 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6673 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6674 | `loop:` |
|       - |  6675 | `	/* Mark as constant */` |
|      32 |  6676 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6677 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6678 | `		/* Invalid constant name */` |
|     ! 0 |  6679 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6680 | `		if( rc == SXERR_ABORT ){` |
|       - |  6681 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6682 | `			return SXERR_ABORT;` |
|       - |  6683 | `		}` |
|     ! 0 |  6684 | `		goto Synchronize;` |
|       - |  6685 | `	}` |
|       - |  6686 | `	/* Peek constant name */` |
|      32 |  6687 | `	pName = &pGen->pIn->sData;` |
|       - |  6688 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6689 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6690 | `		/* Reserved constant name */` |
|     ! 0 |  6691 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6692 | `		if( rc == SXERR_ABORT ){` |
|       - |  6693 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6694 | `			return SXERR_ABORT;` |
|       - |  6695 | `		}` |
|     ! 0 |  6696 | `		goto Synchronize;` |
|       - |  6697 | `	}` |
|       - |  6698 | `	/* Advance the stream cursor */` |
|      32 |  6699 | `	pGen->pIn++;` |
|      32 |  6700 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6701 | `		/* Invalid declaration */` |
|     ! 0 |  6702 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6703 | `		if( rc == SXERR_ABORT ){` |
|       - |  6704 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6705 | `			return SXERR_ABORT;` |
|       - |  6706 | `		}` |
|     ! 0 |  6707 | `		goto Synchronize;` |
|       - |  6708 | `	}` |
|      32 |  6709 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6710 | `	/* Allocate a new class attribute */` |
|      32 |  6711 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6712 | `	if( pCons == 0 ){` |
|     ! 0 |  6713 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6714 | `		return SXERR_ABORT;` |
|       - |  6715 | `	}` |
|       - |  6716 | `	/* Swap bytecode container */` |
|      32 |  6717 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6718 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6719 | `	/* Compile constant value.` |
|       - |  6720 | `	 */` |
|      32 |  6721 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6722 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6723 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6724 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6725 | `			return SXERR_ABORT;` |
|       - |  6726 | `		}` |
|       1 |  6727 | `	}` |
|       - |  6728 | `	/* Emit the done instruction */` |
|      32 |  6729 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6730 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6731 | `	if( rc == SXERR_ABORT ){` |
|       - |  6732 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6733 | `		return SXERR_ABORT;` |
|       - |  6734 | `	}` |
|       - |  6735 | `	/* All done,install the constant */` |
|      32 |  6736 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6737 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6738 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6739 | `		return SXERR_ABORT;` |
|       - |  6740 | `	}` |
|      32 |  6741 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6742 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6743 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6744 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6745 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6746 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6747 | `				pTok--;` |
|     ! 0 |  6748 | `			}` |
|     ! 0 |  6749 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6750 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6751 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6752 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6753 | `				return SXERR_ABORT;` |
|       - |  6754 | `			}` |
|     ! 0 |  6755 | `		}else{` |
|     ! 0 |  6756 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6757 | `				goto loop;` |
|       - |  6758 | `			}` |
|       - |  6759 | `		}` |
|     ! 0 |  6760 | `	}` |
|      32 |  6761 | `	return SXRET_OK;` |
|     ! 0 |  6762 | `Synchronize:` |
|       - |  6763 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6764 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6765 | `		pGen->pIn++;` |
|     ! 0 |  6766 | `	}` |
|     ! 0 |  6767 | `	return SXERR_CORRUPT;` |
|      17 |  6768 |  |
|       - |  6769 | `/*` |
|       - |  6770 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6771 | ` * According to the PHP language reference manual` |
|       - |  6772 | ` *  Properties` |
|       - |  6773 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6774 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6775 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6776 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6777 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6778 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6779 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6780 | ` * Symisc eXtension.` |
|       - |  6781 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6782 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6783 | ` *  Example:` |
|       - |  6784 | ` *   class Test{` |
|       - |  6785 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6786 | ` *   };` |
|       - |  6787 | ` *   var_dump(TEST::myVar);` |
|       - |  6788 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6789 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6790 | ` */` |
|       - |  6791 | `/*` |
|       - |  6792 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6793 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6794 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6795 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6796 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6797 | ` */` |
|  147952 |  6798 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6799 |  |
|  147954 |  6800 | `	SyToken *p = pStart;` |
|  147954 |  6801 | `	if( p >= pEnd ) return 0;` |
|  147954 |  6802 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6803 | `		p++;` |
|      16 |  6804 | `		if( p >= pEnd ) return 0;` |
|       7 |  6805 | `	}` |
|  147954 |  6806 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6807 | `		p++;` |
|       3 |  6808 | `		if( p >= pEnd ) return 0;` |
|       1 |  6809 | `	}` |
|  147954 |  6810 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6811 | `		return 0;` |
|       - |  6812 | `	}` |
|       - |  6813 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6814 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6815 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6816 | `	 * dispatch site. */` |
|  147954 |  6817 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  147936 |  6818 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  147986 |  6819 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3163 |  6820 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  147832 |  6821 | `			return 0;` |
|       - |  6822 | `		}` |
|      52 |  6823 | `	}` |
|     124 |  6824 | `	p++;` |
|       - |  6825 | `	/* Consume optional namespace path */` |
|     126 |  6826 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6827 | `		p += 2;` |
|       1 |  6828 | `	}` |
|       - |  6829 | ``	/* Consume any `\| Type` union alternatives */`` |
|     201 |  6830 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      81 |  6831 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6832 | `		p++;` |
|      14 |  6833 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6834 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6835 | `		p++;` |
|      14 |  6836 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6837 | `			p += 2;` |
|     ! 0 |  6838 | `		}` |
|       2 |  6839 | `	}` |
|     124 |  6840 | `	if( p >= pEnd ) return 0;` |
|     124 |  6841 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   73978 |  6842 |  |
|       - |  6843 |  |
|       - |  6844 | `/*` |
|       - |  6845 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6846 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6847 | ` * if not). Recognized forms:` |
|       - |  6848 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6849 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6850 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6851 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6852 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6853 | ` * on unrecoverable error.` |
|       - |  6854 | ` *` |
|       - |  6855 | ` * When a type is parsed:` |
|       - |  6856 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6857 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6858 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6859 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6860 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6861 | ` */` |
|     122 |  6862 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6863 | `	ph7_gen_state *pGen,` |
|       - |  6864 | `	sxu32 *pnType,` |
|       - |  6865 | `	SyString *pClass,` |
|       - |  6866 | `	sxi32 *piTypeFlags,` |
|       - |  6867 | `	SyString *pTypeText,` |
|       - |  6868 | `	SySet *pAlts` |
|       2 |  6869 | `){` |
|     124 |  6870 | `	sxi32 iFlags = 0;` |
|       - |  6871 | `	sxi32 rc;` |
|     124 |  6872 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6873 | `		return SXRET_OK;` |
|       - |  6874 | `	}` |
|       - |  6875 | `	/* If the first token is '$', there's no type */` |
|     124 |  6876 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6877 | `		return SXRET_OK;` |
|       - |  6878 | `	}` |
|     124 |  6879 | `	rc = GenStateParseUnionTypeDecl(` |
|      61 |  6880 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6881 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6882 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6883 | `		/* bAllowVoid */ 0,` |
|     122 |  6884 | `		pGen->pIn->nLine);` |
|     124 |  6885 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6886 | `		return rc;` |
|       - |  6887 | `	}` |
|       - |  6888 | `	/* Verify next token is '$' (start of property name) */` |
|     124 |  6889 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6890 | `		return SXERR_SYNTAX;` |
|       - |  6891 | `	}` |
|     124 |  6892 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     124 |  6893 | `	return SXRET_OK;` |
|      63 |  6894 |  |
|       - |  6895 |  |
|       - |  6896 | `/*` |
|       - |  6897 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6898 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6899 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6900 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6901 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6902 | ` * by the type parser itself before reaching here.` |
|       - |  6903 | ` *` |
|       - |  6904 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6905 | ` * use in the error message.` |
|       - |  6906 | ` */` |
|     188 |  6907 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6908 | `	sxu32 nType,` |
|       - |  6909 | `	const SyString *pClass,` |
|       - |  6910 | `	const char **pzName,` |
|       - |  6911 | `	sxu32 *pnName)` |
|       2 |  6912 |  |
|       - |  6913 | `	const char *z;` |
|       - |  6914 | `	sxu32 n;` |
|     190 |  6915 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     160 |  6916 | `		return 0;` |
|       - |  6917 | `	}` |
|      32 |  6918 | `	z = pClass->zString;` |
|      32 |  6919 | `	n = pClass->nByte;` |
|      32 |  6920 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       5 |  6921 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6922 | `	}` |
|      28 |  6923 | `	if( n == 5 && SyMemcmpNoCase(z,"mixed",5) == 0 ){` |
|     ! 0 |  6924 | `		*pzName = "mixed"; *pnName = 5; return 1;` |
|       - |  6925 | `	}` |
|      28 |  6926 | `	if( n == 8 && SyMemcmpNoCase(z,"iterable",8) == 0 ){` |
|     ! 0 |  6927 | `		*pzName = "iterable"; *pnName = 8; return 1;` |
|       - |  6928 | `	}` |
|      28 |  6929 | `	return 0;` |
|      96 |  6930 |  |
|       - |  6931 |  |
|       - |  6932 | `/*` |
|       - |  6933 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6934 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6935 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6936 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6937 | `` * unions like `callable\|int`).`` |
|       - |  6938 | ` *` |
|       - |  6939 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6940 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6941 | ` */` |
|     160 |  6942 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6943 | `	ph7_gen_state *pGen,` |
|       - |  6944 | `	ph7_class *pClass,` |
|       - |  6945 | `	const SyString *pPropName,` |
|       - |  6946 | `	sxu32 nType,` |
|       - |  6947 | `	const SyString *pTypeClass,` |
|       - |  6948 | `	const SyString *pTypeText,` |
|       - |  6949 | `	SySet *pUnionAlts,` |
|       - |  6950 | `	sxu32 nLine)` |
|       2 |  6951 |  |
|     162 |  6952 | `	const char *zBad = 0;` |
|     162 |  6953 | `	sxu32 nBad = 0;` |
|       - |  6954 | `	SyString sFallback;` |
|       - |  6955 | `	const SyString *pBad;` |
|       - |  6956 | `	sxi32 rc;` |
|     162 |  6957 | `	int bDisallowed = 0;` |
|     162 |  6958 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6959 | `		bDisallowed = 1;` |
|     161 |  6960 | `	}else if( pUnionAlts ){` |
|       - |  6961 | `		sxu32 i;` |
|      42 |  6962 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      30 |  6963 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      30 |  6964 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6965 | `				bDisallowed = 1;` |
|       3 |  6966 | `				break;` |
|       - |  6967 | `			}` |
|      15 |  6968 | `		}` |
|       7 |  6969 | `	}` |
|     162 |  6970 | `	if( !bDisallowed ){` |
|     158 |  6971 | `		return SXRET_OK;` |
|       - |  6972 | `	}` |
|       - |  6973 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6974 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6975 | `	 * canonical spelling if the type text is unavailable. */` |
|       5 |  6976 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       5 |  6977 | `		pBad = pTypeText;` |
|       3 |  6978 | `	}else{` |
|     ! 0 |  6979 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6980 | `		pBad = &sFallback;` |
|       - |  6981 | `	}` |
|       7 |  6982 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6983 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6984 | `		&pClass->sName,pPropName,pBad);` |
|       5 |  6985 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6986 | `		return SXERR_ABORT;` |
|       - |  6987 | `	}` |
|       5 |  6988 | `	return SXERR_SYNTAX;` |
|      82 |  6989 |  |
|       - |  6990 |  |
|   57552 |  6991 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6992 |  |
|   57554 |  6993 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6994 | `	ph7_class_attr *pAttr;` |
|       - |  6995 | `	SyString *pName;` |
|       - |  6996 | `	sxi32 rc;` |
|   57554 |  6997 | `	sxu32 nType = 0;` |
|       - |  6998 | `	SyString sTypeClass;` |
|       - |  6999 | `	SyString sTypeText;` |
|       - |  7000 | `	SySet aUnionAlts;` |
|   57554 |  7001 | `	sxi32 iTypeFlags = 0;` |
|   57554 |  7002 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   57554 |  7003 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   57554 |  7004 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  7005 | `	/* Extract visibility level */` |
|   57554 |  7006 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  7007 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   57615 |  7008 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     124 |  7009 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     124 |  7010 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  7011 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  7012 | `			goto Synchronize;` |
|     124 |  7013 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  7014 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7015 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  7016 | `				&pGen->pIn->sData);` |
|     ! 0 |  7017 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7018 | `				return SXERR_ABORT;` |
|       - |  7019 | `			}` |
|     ! 0 |  7020 | `			goto Synchronize;` |
|     124 |  7021 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  7022 | `			return SXERR_ABORT;` |
|       - |  7023 | `		}` |
|      61 |  7024 | `	}` |
|     ! 0 |  7025 | `loop:` |
|   57558 |  7026 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7027 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7028 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7029 | `			return SXERR_ABORT;` |
|       - |  7030 | `		}` |
|     ! 0 |  7031 | `		goto Synchronize;` |
|       - |  7032 | `	}` |
|   57558 |  7033 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   57558 |  7034 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7035 | `		/* Invalid attribute name */` |
|     ! 0 |  7036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7037 | `		if( rc == SXERR_ABORT ){` |
|       - |  7038 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7039 | `			return SXERR_ABORT;` |
|       - |  7040 | `		}` |
|     ! 0 |  7041 | `		goto Synchronize;` |
|       - |  7042 | `	}` |
|       - |  7043 | `	/* Peek attribute name */` |
|   57558 |  7044 | `	pName = &pGen->pIn->sData;` |
|       - |  7045 | `	/* Advance the stream cursor */` |
|   57558 |  7046 | `	pGen->pIn++;` |
|   57558 |  7047 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7048 | `		/* Invalid declaration */` |
|       3 |  7049 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7050 | `		if( rc == SXERR_ABORT ){` |
|       - |  7051 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7052 | `			return SXERR_ABORT;` |
|       - |  7053 | `		}` |
|       3 |  7054 | `		goto Synchronize;` |
|       - |  7055 | `	}` |
|       - |  7056 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7057 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7058 | `	 * by the type parser. */` |
|   57556 |  7059 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     191 |  7060 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7061 | `			&sTypeText,` |
|     126 |  7062 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     128 |  7063 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7064 | `			return SXERR_ABORT;` |
|     128 |  7065 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7066 | `			goto Synchronize;` |
|       - |  7067 | `		}` |
|      63 |  7068 | `	}` |
|       - |  7069 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   57556 |  7070 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7071 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7072 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7073 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7074 | `			return SXERR_ABORT;` |
|       - |  7075 | `		}` |
|       3 |  7076 | `		goto Synchronize;` |
|       - |  7077 | `	}` |
|       - |  7078 | `	/* Allocate a new class attribute */` |
|   57554 |  7079 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   57554 |  7080 | `	if( pAttr == 0 ){` |
|     ! 0 |  7081 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7082 | `		return SXERR_ABORT;` |
|       - |  7083 | `	}` |
|   57554 |  7084 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     126 |  7085 | `		pAttr->nType = nType;` |
|     126 |  7086 | `		pAttr->sClass = sTypeClass;` |
|     126 |  7087 | `		pAttr->sTypeName = sTypeText;` |
|     126 |  7088 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7089 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  7090 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  7091 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  7092 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  7093 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  7094 | `			sxu32 i;` |
|      32 |  7095 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  7096 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  7097 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  7098 | `			}` |
|       5 |  7099 | `		}` |
|      62 |  7100 | `	}` |
|   57554 |  7101 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7102 | `		SySet *pInstrContainer;` |
|   18370 |  7103 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7104 | `		/* Swap bytecode container */` |
|   18370 |  7105 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   18370 |  7106 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7107 | `		/* Compile attribute value.` |
|       - |  7108 | `		 */` |
|   18370 |  7109 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   18370 |  7110 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7111 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7112 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7113 | `				return SXERR_ABORT;` |
|       - |  7114 | `			}` |
|     ! 0 |  7115 | `		}` |
|       - |  7116 | `		/* Emit the done instruction */` |
|   18370 |  7117 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   18370 |  7118 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    9184 |  7119 | `	}` |
|       - |  7120 | `	/* All done,install the attribute */` |
|   57554 |  7121 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   57554 |  7122 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7123 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7124 | `		return SXERR_ABORT;` |
|       - |  7125 | `	}` |
|   57554 |  7126 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7127 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7128 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7129 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7130 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7131 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7132 | `				pTok--;` |
|     ! 0 |  7133 | `			}` |
|     ! 0 |  7134 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7135 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7136 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7137 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7138 | `				return SXERR_ABORT;` |
|       - |  7139 | `			}` |
|     ! 0 |  7140 | `		}else{` |
|       5 |  7141 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7142 | `				goto loop;` |
|       - |  7143 | `			}` |
|       - |  7144 | `		}` |
|     ! 0 |  7145 | `	}` |
|   57550 |  7146 | `	SySetRelease(&aUnionAlts);` |
|   57550 |  7147 | `	return SXRET_OK;` |
|       2 |  7148 | `Synchronize:` |
|       - |  7149 | `	/* Synchronize with the first semi-colon */` |
|      11 |  7150 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7151 | `		pGen->pIn++;` |
|       1 |  7152 | `	}` |
|       5 |  7153 | `	SySetRelease(&aUnionAlts);` |
|       5 |  7154 | `	return SXERR_CORRUPT;` |
|   28778 |  7155 |  |
|       - |  7156 | `/*` |
|       - |  7157 | ` * Compile a class method.` |
|       - |  7158 | ` *` |
|       - |  7159 | ` * Refer to the official documentation for more information` |
|       - |  7160 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7161 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7162 | ` * overloading and many more.` |
|       - |  7163 | ` */` |
|  196010 |  7164 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7165 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7166 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7167 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7168 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7169 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7170 | `	)` |
|       2 |  7171 |  |
|  196012 |  7172 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7173 | `	ph7_class_method *pMeth;` |
|       - |  7174 | `	sxi32 iFuncFlags;` |
|       - |  7175 | `	SyString *pName;` |
|       - |  7176 | `	SyToken *pEnd;` |
|       - |  7177 | `	sxi32 rc;` |
|       - |  7178 | `	/* Extract visibility level */` |
|  196012 |  7179 | `	iProtection = GetProtectionLevel(iProtection);` |
|  196012 |  7180 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  196012 |  7181 | `	iFuncFlags = 0;` |
|  196012 |  7182 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7183 | `		/* Invalid method name */` |
|     ! 0 |  7184 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7185 | `		if( rc == SXERR_ABORT ){` |
|       - |  7186 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7187 | `			return SXERR_ABORT;` |
|       - |  7188 | `		}` |
|     ! 0 |  7189 | `		goto Synchronize;` |
|       - |  7190 | `	}` |
|  196012 |  7191 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7192 | `		/* Return by reference,remember that */` |
|     ! 0 |  7193 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7194 | `		/* Jump the '&' token */` |
|     ! 0 |  7195 | `		pGen->pIn++;` |
|     ! 0 |  7196 | `	}` |
|  196012 |  7197 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7198 | `		/* Invalid method name */` |
|     ! 0 |  7199 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7200 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7201 | `			return SXERR_ABORT;` |
|       - |  7202 | `		}` |
|     ! 0 |  7203 | `		goto Synchronize;` |
|       - |  7204 | `	}` |
|       - |  7205 | `	/* Peek method name */` |
|  196012 |  7206 | `	pName = &pGen->pIn->sData;` |
|  196012 |  7207 | `	nLine = pGen->pIn->nLine;` |
|       - |  7208 | `	/* Jump the method name */` |
|  196012 |  7209 | `	pGen->pIn++;` |
|  196012 |  7210 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7211 | `		/* Abstract method */` |
|   48148 |  7212 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7213 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7214 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7215 | `				&pClass->sName,pName);` |
|     ! 0 |  7216 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7217 | `				return SXERR_ABORT;` |
|       - |  7218 | `			}` |
|     ! 0 |  7219 | `		}` |
|       - |  7220 | `		/* Assemble method signature only */` |
|   48148 |  7221 | `		doBody = FALSE;` |
|   24073 |  7222 | `	}` |
|  196012 |  7223 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7224 | `		/* Syntax error */` |
|     ! 0 |  7225 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7226 | `		if( rc == SXERR_ABORT ){` |
|       - |  7227 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7228 | `			return SXERR_ABORT;` |
|       - |  7229 | `		}` |
|     ! 0 |  7230 | `		goto Synchronize;` |
|       - |  7231 | `	}` |
|       - |  7232 | `	/* Allocate a new class_method instance */` |
|  196012 |  7233 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  196012 |  7234 | `	if( pMeth == 0 ){` |
|     ! 0 |  7235 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7236 | `		return SXERR_ABORT;` |
|       - |  7237 | `	}` |
|       - |  7238 | `	/* Jump the left parenthesis '(' */` |
|  196012 |  7239 | `	pGen->pIn++;` |
|  196012 |  7240 | `	pEnd = 0; /* cc warning */` |
|       - |  7241 | `	/* Delimit the method signature */` |
|  196012 |  7242 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  196012 |  7243 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7244 | `		/* Syntax error */` |
|       3 |  7245 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7246 | `		if( rc == SXERR_ABORT ){` |
|       - |  7247 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7248 | `			return SXERR_ABORT;` |
|       - |  7249 | `		}` |
|       3 |  7250 | `		goto Synchronize;` |
|       - |  7251 | `	}` |
|       - |  7252 | `	{` |
|  196010 |  7253 | `		int bIsCtor = 0;` |
|  196010 |  7254 | `		int bAbstractCtor = 0;` |
|  284947 |  7255 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  117603 |  7256 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  186945 |  7257 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   18132 |  7258 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7259 | `				bAbstractCtor = 1;` |
|       2 |  7260 | `			}else{` |
|   18130 |  7261 | `				bIsCtor = 1;` |
|       - |  7262 | `			}` |
|    9065 |  7263 | `		}` |
|  196010 |  7264 | `		if( pGen->pIn < pEnd ){` |
|       - |  7265 | `			/* Collect method arguments */` |
|   33246 |  7266 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   33246 |  7267 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7268 | `				return SXERR_ABORT;` |
|       - |  7269 | `			}` |
|   16622 |  7270 | `		}` |
|       - |  7271 | `	}` |
|       - |  7272 | `	/* Point past ')' and parse optional return type ': type' */` |
|  196010 |  7273 | `	pGen->pIn = &pEnd[1];` |
|       - |  7274 | `	{` |
|  196010 |  7275 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  196010 |  7276 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7277 | `			return SXERR_ABORT;` |
|  196010 |  7278 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7279 | `			goto Synchronize;` |
|       - |  7280 | `		}` |
|       - |  7281 | `	}` |
|       - |  7282 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7283 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7284 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7285 | `	{` |
|  196010 |  7286 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7287 | `		sxu32 i;` |
|  256358 |  7288 | `		for( i = 0; i < nArg; i++ ){` |
|   60358 |  7289 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7290 | `			ph7_class_attr *pAttr;` |
|   60358 |  7291 | `			sxi32 iAttrFlags = 0;` |
|   60358 |  7292 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   60322 |  7293 | `				continue;` |
|       - |  7294 | `			}` |
|      38 |  7295 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7296 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7297 | `					"Cannot declare variadic promoted property");` |
|       3 |  7298 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7299 | `					return SXERR_ABORT;` |
|       - |  7300 | `				}` |
|       3 |  7301 | `				goto Synchronize;` |
|       - |  7302 | `			}` |
|       - |  7303 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7304 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7305 | `			 * appear as an alternative of a union type. */` |
|      34 |  7306 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       6 |  7307 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      53 |  7308 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7309 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7310 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7311 | `					nLine);` |
|      36 |  7312 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7313 | `					return SXERR_ABORT;` |
|      36 |  7314 | `				}else if( rc != SXRET_OK ){` |
|       5 |  7315 | `					goto Synchronize;` |
|       - |  7316 | `				}` |
|      15 |  7317 | `			}` |
|       - |  7318 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      32 |  7319 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7320 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7321 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7322 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7323 | `					return SXERR_ABORT;` |
|       - |  7324 | `				}` |
|       3 |  7325 | `				goto Synchronize;` |
|       - |  7326 | `			}` |
|      30 |  7327 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      28 |  7328 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7329 | `			}` |
|      30 |  7330 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7331 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7332 | `			}` |
|      30 |  7333 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7334 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7335 | `			}` |
|      30 |  7336 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      30 |  7337 | `			if( pAttr == 0 ){` |
|     ! 0 |  7338 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7339 | `				return SXERR_ABORT;` |
|       - |  7340 | `			}` |
|      30 |  7341 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      28 |  7342 | `				pAttr->nType = pArg->nType;` |
|      28 |  7343 | `				pAttr->sClass = pArg->sClass;` |
|      28 |  7344 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      28 |  7345 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7346 | `					sxu32 k;` |
|     ! 0 |  7347 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7348 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7349 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7350 | `					}` |
|     ! 0 |  7351 | `				}` |
|      13 |  7352 | `			}` |
|      30 |  7353 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      30 |  7354 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7355 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7356 | `				return SXERR_ABORT;` |
|       - |  7357 | `			}` |
|      16 |  7358 | `		}` |
|       - |  7359 | `	}` |
|  196002 |  7360 | `	if( doBody ){` |
|       - |  7361 | `		/* Compile method body */` |
|  147856 |  7362 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  147856 |  7363 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7364 | `			return SXERR_ABORT;` |
|       - |  7365 | `		}` |
|   73929 |  7366 | `	}else{` |
|       - |  7367 | `		/* Only method signature is allowed */` |
|   48148 |  7368 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7369 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7370 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7371 | `				if( rc == SXERR_ABORT ){` |
|       - |  7372 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7373 | `					return SXERR_ABORT;` |
|       - |  7374 | `				}` |
|     ! 0 |  7375 | `				return SXERR_CORRUPT;` |
|       - |  7376 | `			}` |
|       - |  7377 | `	}` |
|       - |  7378 | `	/* All done,install the method */` |
|  196002 |  7379 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  196002 |  7380 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7381 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7382 | `		return SXERR_ABORT;` |
|       - |  7383 | `	}` |
|  196002 |  7384 | `	return SXRET_OK;` |
|       5 |  7385 | `Synchronize:` |
|       - |  7386 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7387 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      21 |  7388 | `		pGen->pIn++;` |
|       1 |  7389 | `	}` |
|      11 |  7390 | `	return SXERR_CORRUPT;` |
|   98007 |  7391 |  |
|       - |  7392 | `/*` |
|       - |  7393 | ` * Compile an object interface.` |
|       - |  7394 | ` *  According to the PHP language reference manual` |
|       - |  7395 | ` *   Object Interfaces:` |
|       - |  7396 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7397 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7398 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7399 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7400 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7401 | ` */` |
|   12066 |  7402 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  7403 |  |
|   12068 |  7404 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7405 | `	ph7_class *pClass,*pBase;` |
|       - |  7406 | `	SyToken *pEnd,*pTmp;` |
|       - |  7407 | `	SyString *pName;` |
|       - |  7408 | `	sxi32 nKwrd;` |
|       - |  7409 | `	sxi32 rc;` |
|       - |  7410 | `	/* Jump the 'interface' keyword */` |
|   12068 |  7411 | `	pGen->pIn++;` |
|       - |  7412 | `	/* Extract interface name */` |
|   12068 |  7413 | `	pName = &pGen->pIn->sData;` |
|       - |  7414 | `	/* Advance the stream cursor */` |
|   12068 |  7415 | `	pGen->pIn++;` |
|       - |  7416 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7417 | `		SyBlob sFQN;` |
|       - |  7418 | `		SyString sFQNStr;` |
|   12068 |  7419 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   12068 |  7420 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   12068 |  7421 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   12068 |  7422 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   12068 |  7423 | `		SyBlobRelease(&sFQN);` |
|       - |  7424 | `	}` |
|   12068 |  7425 | `	if( pClass == 0 ){` |
|     ! 0 |  7426 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7427 | `		return SXERR_ABORT;` |
|       - |  7428 | `	}` |
|       - |  7429 | `	/* Mark as an interface */` |
|   12068 |  7430 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7431 | `	/* Assume no base class is given */` |
|   12068 |  7432 | `	pBase = 0;` |
|   12068 |  7433 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 |  7434 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 |  7435 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7436 | `			SyBlob sResolved;` |
|       - |  7437 | `			SyString sBaseName;` |
|       - |  7438 | `			sxu32 nRefLine;` |
|       - |  7439 | `			/* Extract base interface */` |
|       8 |  7440 | `			pGen->pIn++;` |
|       8 |  7441 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       8 |  7442 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       8 |  7443 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7444 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7445 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7446 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7447 | `					pName);` |
|     ! 0 |  7448 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7449 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7450 | `					return SXERR_ABORT;` |
|       - |  7451 | `				}` |
|     ! 0 |  7452 | `				return SXRET_OK;` |
|       - |  7453 | `			}` |
|      11 |  7454 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|       6 |  7455 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       8 |  7456 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7457 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7458 | `			/* Only interfaces is allowed */` |
|       8 |  7459 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7460 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7461 | `			}` |
|       8 |  7462 | `			if( pBase == 0 ){` |
|     ! 0 |  7463 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7464 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7465 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7466 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7467 | `					return SXERR_ABORT;` |
|       - |  7468 | `				}` |
|     ! 0 |  7469 | `			}` |
|       8 |  7470 | `			SyBlobRelease(&sResolved);` |
|       3 |  7471 | `		}` |
|       3 |  7472 | `	}` |
|   12068 |  7473 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7474 | `		/* Syntax error */` |
|     ! 0 |  7475 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7476 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7477 | `		if( rc == SXERR_ABORT ){` |
|       - |  7478 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7479 | `			return SXERR_ABORT;` |
|       - |  7480 | `		}` |
|     ! 0 |  7481 | `		return SXRET_OK;` |
|       - |  7482 | `	}` |
|   12068 |  7483 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   12068 |  7484 | `	pEnd = 0; /* cc warning */` |
|       - |  7485 | `	/* Delimit the interface body */` |
|   12068 |  7486 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   12068 |  7487 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7488 | `		/* Syntax error */` |
|     ! 0 |  7489 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7490 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7491 | `		if( rc == SXERR_ABORT ){` |
|       - |  7492 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7493 | `			return SXERR_ABORT;` |
|       - |  7494 | `		}` |
|     ! 0 |  7495 | `		return SXRET_OK;` |
|       - |  7496 | `	}` |
|       - |  7497 | `	/* Swap token stream */` |
|   12068 |  7498 | `	pTmp = pGen->pEnd;` |
|   12068 |  7499 | `	pGen->pEnd = pEnd;` |
|       - |  7500 | `	/* Start the parse process` |
|       - |  7501 | `	 * Note (According to the PHP reference manual):` |
|       - |  7502 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7503 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7504 | `	 */` |
|   30100 |  7505 | `	for(;;){` |
|       - |  7506 | `		/* Jump leading/trailing semi-colons */` |
|  108336 |  7507 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   48136 |  7508 | `			pGen->pIn++;` |
|       2 |  7509 | `		}` |
|   60202 |  7510 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7511 | `			/* End of interface body */` |
|   12066 |  7512 | `			break;` |
|       - |  7513 | `		}` |
|   48138 |  7514 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7515 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7516 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7517 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7518 | `			if( rc == SXERR_ABORT ){` |
|       - |  7519 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7520 | `				return SXERR_ABORT;` |
|       - |  7521 | `			}` |
|     ! 0 |  7522 | `			goto done;` |
|       - |  7523 | `		}` |
|       - |  7524 | `		/* Extract the current keyword */` |
|   48138 |  7525 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   48138 |  7526 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7527 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7528 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7529 | `			const char *zKind = "member";` |
|       3 |  7530 | `			SyString *pMemberName = 0;` |
|       3 |  7531 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7532 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7533 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7534 | `					zKind = "constant";` |
|       3 |  7535 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7536 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7537 | `					}` |
|       1 |  7538 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7539 | `					zKind = "method";` |
|     ! 0 |  7540 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7541 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7542 | `					}` |
|     ! 0 |  7543 | `				}` |
|       1 |  7544 | `			}` |
|       3 |  7545 | `			if( pMemberName ){` |
|       4 |  7546 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7547 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7548 | `			}else{` |
|     ! 0 |  7549 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7550 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7551 | `			}` |
|       3 |  7552 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7553 | `				return SXERR_ABORT;` |
|       - |  7554 | `			}` |
|       3 |  7555 | `			goto done;` |
|       - |  7556 | `		}` |
|   48136 |  7557 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7558 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7559 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7560 | `			if( rc == SXERR_ABORT ){` |
|       - |  7561 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7562 | `				return SXERR_ABORT;` |
|       - |  7563 | `			}` |
|     ! 0 |  7564 | `			goto done;` |
|       - |  7565 | `		}` |
|   48136 |  7566 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7567 | `			/* Advance the stream cursor */` |
|   48132 |  7568 | `			pGen->pIn++;` |
|   48132 |  7569 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7570 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7571 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7572 | `				if( rc == SXERR_ABORT ){` |
|       - |  7573 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7574 | `					return SXERR_ABORT;` |
|       - |  7575 | `				}` |
|     ! 0 |  7576 | `				goto done;` |
|       - |  7577 | `			}` |
|   48132 |  7578 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   48132 |  7579 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7580 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7581 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7582 | `				if( rc == SXERR_ABORT ){` |
|       - |  7583 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7584 | `					return SXERR_ABORT;` |
|       - |  7585 | `				}` |
|     ! 0 |  7586 | `				goto done;` |
|       - |  7587 | `			}` |
|   24065 |  7588 | `		}` |
|   48136 |  7589 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7590 | `			/* Parse constant */` |
|       3 |  7591 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7592 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7593 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7594 | `					return SXERR_ABORT;` |
|       - |  7595 | `				}` |
|     ! 0 |  7596 | `				goto done;` |
|       - |  7597 | `			}` |
|       2 |  7598 | `		}else{` |
|   48134 |  7599 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   48134 |  7600 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7601 | `				/* Static method,record that */` |
|     ! 0 |  7602 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7603 | `				/* Advance the stream cursor */` |
|     ! 0 |  7604 | `				pGen->pIn++;` |
|     ! 0 |  7605 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  7606 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7607 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7608 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7609 | `						if( rc == SXERR_ABORT ){` |
|       - |  7610 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7611 | `							return SXERR_ABORT;` |
|       - |  7612 | `						}` |
|     ! 0 |  7613 | `						goto done;` |
|       - |  7614 | `				}` |
|     ! 0 |  7615 | `			}` |
|       - |  7616 | `			/* Process method signature (no body for interface methods) */` |
|   48134 |  7617 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   48134 |  7618 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7619 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7620 | `					return SXERR_ABORT;` |
|       - |  7621 | `				}` |
|     ! 0 |  7622 | `				goto done;` |
|       - |  7623 | `			}` |
|       - |  7624 | `		}` |
|       2 |  7625 | `	}` |
|       - |  7626 | `	/* Install the interface */` |
|   12066 |  7627 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   12066 |  7628 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7629 | `		/* Inherit from the base interface */` |
|       8 |  7630 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       3 |  7631 | `	}` |
|   12066 |  7632 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7633 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7634 | `		return SXERR_ABORT;` |
|       - |  7635 | `	}` |
|    6032 |  7636 | `done:` |
|       - |  7637 | `	/* Point beyond the interface body */` |
|   12068 |  7638 | `	pGen->pIn  = &pEnd[1];` |
|   12068 |  7639 | `	pGen->pEnd = pTmp;` |
|   12068 |  7640 | `	return PH7_OK;` |
|    6035 |  7641 |  |
|       - |  7642 | `/*` |
|       - |  7643 | ` * Compile a user-defined class.` |
|       - |  7644 | ` * According to the PHP language reference manual` |
|       - |  7645 | ` *  class` |
|       - |  7646 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7647 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7648 | ` *  of the properties and methods belonging to the class.` |
|       - |  7649 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7650 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7651 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7652 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7653 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7654 | ` *  (called "methods").` |
|       - |  7655 | ` */` |
|       - |  7656 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7657 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7658 | `struct TraitUseEntry {` |
|       - |  7659 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7660 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7661 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7662 | `};` |
|       - |  7663 | `/*` |
|       - |  7664 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7665 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7666 | ` */` |
|   42892 |  7667 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7668 |  |
|       - |  7669 | `	ph7_class **apIface;` |
|       - |  7670 | `	sxu32 nIface,i;` |
|       - |  7671 | `	sxi32 rc;` |
|   42894 |  7672 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7673 | `		return SXRET_OK;` |
|       - |  7674 | `	}` |
|   42894 |  7675 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   42894 |  7676 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   51954 |  7677 | `	for(i = 0; i < nIface; i++){` |
|    9062 |  7678 | `		ph7_class *pIface = apIface[i];` |
|       - |  7679 | `		SyHashEntry *pEntry;` |
|    9062 |  7680 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   72298 |  7681 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   63238 |  7682 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7683 | `			ph7_class_method *pImplMeth;` |
|   63238 |  7684 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7685 | `			/* Find the implementing method in the class */` |
|   63238 |  7686 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   63238 |  7687 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7688 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7689 | `			}` |
|       - |  7690 | `			/* Check visibility: interface methods must be implemented as public */` |
|   63224 |  7691 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7692 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7693 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7694 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7695 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7696 | `					return SXERR_ABORT;` |
|       - |  7697 | `				}` |
|       1 |  7698 | `			}` |
|       - |  7699 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7700 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7701 | `			 */` |
|       - |  7702 | `			{` |
|   63224 |  7703 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   63224 |  7704 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   63224 |  7705 | `				int sigError = 0;` |
|   63224 |  7706 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7707 | `					sigError = 1;` |
|   63223 |  7708 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7709 | `					/* Extra parameters must all have default values */` |
|       5 |  7710 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7711 | `					sxu32 k;` |
|       7 |  7712 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7713 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7714 | `							sigError = 1;` |
|       3 |  7715 | `							break;` |
|       - |  7716 | `						}` |
|       2 |  7717 | `					}` |
|       2 |  7718 | `				}` |
|   63224 |  7719 | `				if( sigError ){` |
|       - |  7720 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7721 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7722 | `					sxu32 j;` |
|       5 |  7723 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7724 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7725 | `					/* Build implementing method signature */` |
|       5 |  7726 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7727 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7728 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7729 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7730 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7731 | `					}` |
|       - |  7732 | `					/* Build interface method signature */` |
|       5 |  7733 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7734 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7735 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7736 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7737 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7738 | `					}` |
|       7 |  7739 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7740 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7741 | `						&pClass->sName,pMName,` |
|       4 |  7742 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7743 | `						&pIface->sName,pMName,` |
|       4 |  7744 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7745 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7746 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7747 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7748 | `						return SXERR_ABORT;` |
|       - |  7749 | `					}` |
|       2 |  7750 | `				}` |
|       - |  7751 | `			}` |
|       2 |  7752 | `		}` |
|    4532 |  7753 | `	}` |
|   42894 |  7754 | `	return SXRET_OK;` |
|   21448 |  7755 |  |
|       - |  7756 | `/*` |
|       - |  7757 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7758 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7759 | ` */` |
|   42892 |  7760 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7761 |  |
|       - |  7762 | `	ph7_class_method *pMeth;` |
|       - |  7763 | `	SyHashEntry *pEntry;` |
|       - |  7764 | `	sxu32 nAbstract;` |
|       - |  7765 | `	SyBlob sMsg;` |
|       - |  7766 | `	sxi32 rc;` |
|       - |  7767 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   42894 |  7768 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      22 |  7769 | `		return SXRET_OK;` |
|       - |  7770 | `	}` |
|       - |  7771 | `	/* Count abstract methods */` |
|   42874 |  7772 | `	nAbstract = 0;` |
|   42874 |  7773 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  404728 |  7774 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  361856 |  7775 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  361856 |  7776 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7777 | `			nAbstract++;` |
|       8 |  7778 | `		}` |
|       2 |  7779 | `	}` |
|   42874 |  7780 | `	if( nAbstract == 0 ){` |
|   42860 |  7781 | `		return SXRET_OK;` |
|       - |  7782 | `	}` |
|       - |  7783 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7784 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7785 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7786 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7787 | `		&pClass->sName,nAbstract,` |
|       7 |  7788 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7789 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7790 | `	/* Second pass: list methods with origins */` |
|       - |  7791 | `	{` |
|      15 |  7792 | `		sxu32 nListed = 0;` |
|      15 |  7793 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7794 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7795 | `			ph7_class *pOrigin = 0;` |
|       - |  7796 | `			SyString *pMName;` |
|      19 |  7797 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7798 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7799 | `				continue;` |
|       - |  7800 | `			}` |
|      17 |  7801 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7802 | `			if( nListed > 0 ){` |
|       3 |  7803 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7804 | `			}` |
|       - |  7805 | `			/* Find the origin of this abstract method.` |
|       - |  7806 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7807 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7808 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7809 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7810 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7811 | `			 * class's namespace.` |
|       - |  7812 | `			 */` |
|       - |  7813 | `			{` |
|       - |  7814 | `				ph7_class **apIface;` |
|       - |  7815 | `				ph7_class **apTrait;` |
|       - |  7816 | `				ph7_class *pWalk;` |
|       - |  7817 | `				sxu32 i;` |
|       - |  7818 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7819 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7820 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7821 | `				 */` |
|      17 |  7822 | `				if( pClass->pBase ){` |
|       9 |  7823 | `					pWalk = pClass->pBase;` |
|      17 |  7824 | `					while( pWalk ){` |
|       - |  7825 | `						ph7_class_method *pParentMeth;` |
|      11 |  7826 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7827 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7828 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7829 | `							 * in this class's ancestor chain.` |
|       - |  7830 | `							 */` |
|      11 |  7831 | `							int fromIface = 0;` |
|      11 |  7832 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7833 | `							while( pAnc ){` |
|       - |  7834 | `								ph7_class **apPI;` |
|       - |  7835 | `								sxu32 j;` |
|      13 |  7836 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7837 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7838 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7839 | `										fromIface = 1;` |
|       9 |  7840 | `										break;` |
|       - |  7841 | `									}` |
|     ! 0 |  7842 | `								}` |
|      13 |  7843 | `								if( fromIface ) break;` |
|       5 |  7844 | `								pAnc = pAnc->pBase;` |
|       1 |  7845 | `							}` |
|      11 |  7846 | `							if( !fromIface ){` |
|       3 |  7847 | `								pOrigin = pWalk;` |
|       3 |  7848 | `								break;` |
|       - |  7849 | `							}` |
|       4 |  7850 | `						}` |
|       9 |  7851 | `						pWalk = pWalk->pBase;` |
|       1 |  7852 | `					}` |
|       4 |  7853 | `				}` |
|       - |  7854 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7855 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7856 | `				 */` |
|      17 |  7857 | `				if( !pOrigin ){` |
|      15 |  7858 | `					pWalk = pClass;` |
|      37 |  7859 | `					while( pWalk && !pOrigin ){` |
|      23 |  7860 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7861 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7862 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7863 | `							ph7_class *pDeepest = 0;` |
|      25 |  7864 | `							while( pIface ){` |
|      13 |  7865 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7866 | `									pDeepest = pIface;` |
|       6 |  7867 | `								}` |
|      13 |  7868 | `								pIface = pIface->pBase;` |
|       1 |  7869 | `							}` |
|      13 |  7870 | `							if( pDeepest ){` |
|      13 |  7871 | `								pOrigin = pDeepest;` |
|      13 |  7872 | `								break;` |
|       - |  7873 | `							}` |
|     ! 0 |  7874 | `						}` |
|      23 |  7875 | `						pWalk = pWalk->pBase;` |
|       1 |  7876 | `					}` |
|       7 |  7877 | `				}` |
|       - |  7878 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7879 | `				if( !pOrigin ){` |
|       3 |  7880 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7881 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7882 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7883 | `							pOrigin = pClass;` |
|       3 |  7884 | `							break;` |
|       - |  7885 | `						}` |
|     ! 0 |  7886 | `					}` |
|       1 |  7887 | `				}` |
|       - |  7888 | `			}` |
|      17 |  7889 | `			if( pOrigin ){` |
|      17 |  7890 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7891 | `			}else{` |
|       - |  7892 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7893 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7894 | `			}` |
|      17 |  7895 | `			nListed++;` |
|       1 |  7896 | `		}` |
|       - |  7897 | `	}` |
|      15 |  7898 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7899 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7900 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7901 | `	SyBlobRelease(&sMsg);` |
|      15 |  7902 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7903 | `		return SXERR_ABORT;` |
|       - |  7904 | `	}` |
|      15 |  7905 | `	return SXRET_OK;` |
|   21448 |  7906 |  |
|       - |  7907 | `/*` |
|       - |  7908 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  7909 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  7910 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  7911 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  7912 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  7913 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  7914 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  7915 | ` */` |
|   33574 |  7916 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       2 |  7917 |  |
|   33576 |  7918 | `	int isAbsolute = 0;` |
|   33576 |  7919 | `	SyToken *pStart = pGen->pIn;` |
|       - |  7920 | `	SyBlob sName;` |
|   33576 |  7921 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      30 |  7922 | `		isAbsolute = 1;` |
|      30 |  7923 | `		pGen->pIn++;` |
|      14 |  7924 | `	}` |
|   33576 |  7925 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       7 |  7926 | `		pGen->pIn = pStart;` |
|       7 |  7927 | `		return SXERR_INVALID;` |
|       - |  7928 | `	}` |
|   33570 |  7929 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   33570 |  7930 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   33570 |  7931 | `	pGen->pIn++;` |
|   50370 |  7932 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   16804 |  7933 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  7934 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  7935 | `		pGen->pIn++;` |
|      13 |  7936 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  7937 | `		pGen->pIn++;` |
|       1 |  7938 | `	}` |
|   33570 |  7939 | `	if( isAbsolute ){` |
|      28 |  7940 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      15 |  7941 | `	}else{` |
|       - |  7942 | `		SyString sRaw;` |
|   33544 |  7943 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   33544 |  7944 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  7945 | `	}` |
|   33570 |  7946 | `	SyBlobRelease(&sName);` |
|   33570 |  7947 | `	return SXRET_OK;` |
|   16789 |  7948 |  |
|       - |  7949 | `/*` |
|       - |  7950 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  7951 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  7952 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  7953 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  7954 | ` * either direction cannot run unbounded.` |
|       - |  7955 | ` */` |
|       - |  7956 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    9066 |  7957 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       2 |  7958 |  |
|       - |  7959 | `	ph7_class **apParent;` |
|       - |  7960 | `	sxu32 n;` |
|   12118 |  7961 | `	while( pInterface ){` |
|    9074 |  7962 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  7963 | `			return FALSE;` |
|       - |  7964 | `		}` |
|   12088 |  7965 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    6028 |  7966 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    6024 |  7967 | `			return TRUE;` |
|       - |  7968 | `		}` |
|    3052 |  7969 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    3052 |  7970 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  7971 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  7972 | `				return TRUE;` |
|       - |  7973 | `			}` |
|     ! 0 |  7974 | `		}` |
|    3052 |  7975 | `		pInterface = pInterface->pBase;` |
|    3052 |  7976 | `		iDepth++;` |
|       2 |  7977 | `	}` |
|    3046 |  7978 | `	return FALSE;` |
|    4535 |  7979 |  |
|    9066 |  7980 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       2 |  7981 |  |
|    9068 |  7982 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       2 |  7983 |  |
|       - |  7984 | `/*` |
|       - |  7985 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  7986 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  7987 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  7988 | ` */` |
|    6022 |  7989 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       2 |  7990 |  |
|    6028 |  7991 | `	while( pBase ){` |
|      10 |  7992 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  7993 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  7994 | `			return TRUE;` |
|       - |  7995 | `		}` |
|      10 |  7996 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  7997 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  7998 | `			return TRUE;` |
|       - |  7999 | `		}` |
|       5 |  8000 | `		pBase = pBase->pBase;` |
|       1 |  8001 | `	}` |
|    6020 |  8002 | `	return FALSE;` |
|    3013 |  8003 |  |
|   42908 |  8004 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  8005 |  |
|   42910 |  8006 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8007 | `	ph7_class *pClass,*pBase;` |
|       - |  8008 | `	SyToken *pEnd,*pTmp;` |
|       - |  8009 | `	sxi32 iProtection;` |
|       - |  8010 | `	SySet aInterfaces;` |
|       - |  8011 | `	SySet aUseEntries;` |
|       - |  8012 | `	sxi32 iAttrflags;` |
|       - |  8013 | `	SyString *pName;` |
|       - |  8014 | `	sxi32 nKwrd;` |
|       - |  8015 | `	sxi32 rc;` |
|       - |  8016 | `	/* Jump the 'class' keyword */` |
|   42910 |  8017 | `	pGen->pIn++;` |
|   42910 |  8018 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  8019 | `		/* Syntax error */` |
|     ! 0 |  8020 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  8021 | `		if( rc == SXERR_ABORT ){` |
|       - |  8022 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8023 | `			return SXERR_ABORT;` |
|       - |  8024 | `		}` |
|       - |  8025 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8026 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8027 | `			pGen->pIn++;` |
|     ! 0 |  8028 | `		}` |
|     ! 0 |  8029 | `		return SXRET_OK;` |
|       - |  8030 | `	}` |
|       - |  8031 | `	/* Extract class name */` |
|   42910 |  8032 | `	pName = &pGen->pIn->sData;` |
|       - |  8033 | `	/* Advance the stream cursor */` |
|   42910 |  8034 | `	pGen->pIn++;` |
|       - |  8035 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8036 | `		SyBlob sFQN;` |
|       - |  8037 | `		SyString sFQNStr;` |
|   42910 |  8038 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   42910 |  8039 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   42910 |  8040 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   42910 |  8041 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   42910 |  8042 | `		SyBlobRelease(&sFQN);` |
|       - |  8043 | `	}` |
|   42910 |  8044 | `	if( pClass == 0 ){` |
|     ! 0 |  8045 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8046 | `		return SXERR_ABORT;` |
|       - |  8047 | `	}` |
|       - |  8048 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   42910 |  8049 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   42910 |  8050 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8051 | `	/* Assume a standalone class */` |
|   42910 |  8052 | `	pBase = 0;` |
|   42910 |  8053 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   33250 |  8054 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   33250 |  8055 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8056 | `			SyBlob sResolved;` |
|       - |  8057 | `			SyString sBaseName;` |
|       - |  8058 | `			sxu32 nRefLine;` |
|   24190 |  8059 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   24190 |  8060 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   24190 |  8061 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   24190 |  8062 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8063 | `				SyBlobRelease(&sResolved);` |
|       4 |  8064 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8065 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8066 | `					pName);` |
|       3 |  8067 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8068 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8069 | `					return SXERR_ABORT;` |
|       - |  8070 | `				}` |
|       3 |  8071 | `				return SXRET_OK;` |
|       - |  8072 | `			}` |
|   36281 |  8073 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   24186 |  8074 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   24188 |  8075 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8076 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8077 | `			/* Interfaces are not allowed */` |
|   24188 |  8078 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8079 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8080 | `			}` |
|   24188 |  8081 | `			if( pBase == 0 ){` |
|     ! 0 |  8082 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8083 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8084 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8085 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8086 | `					return SXERR_ABORT;` |
|       - |  8087 | `				}` |
|     ! 0 |  8088 | `			}else{` |
|   24188 |  8089 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8090 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8091 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8092 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8093 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8094 | `						return SXERR_ABORT;` |
|       - |  8095 | `					}` |
|     ! 0 |  8096 | `				}` |
|       - |  8097 | `			}` |
|   24188 |  8098 | `			SyBlobRelease(&sResolved);` |
|   12093 |  8099 | `		}` |
|   33248 |  8100 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8101 | `			ph7_class *pInterface;` |
|       - |  8102 | `			/* Interface implementation */` |
|    9068 |  8103 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4533 |  8104 | `			for(;;){` |
|       - |  8105 | `				SyBlob sResolved;` |
|       - |  8106 | `				SyString sIntName;` |
|       - |  8107 | `				sxu32 nRefLine;` |
|    9068 |  8108 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    9068 |  8109 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    9068 |  8110 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8111 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8112 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8113 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8114 | `						pName);` |
|     ! 0 |  8115 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8116 | `						return SXERR_ABORT;` |
|       - |  8117 | `					}` |
|     ! 0 |  8118 | `					break;` |
|       - |  8119 | `				}` |
|   18134 |  8120 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    9066 |  8121 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    9068 |  8122 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8123 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8124 | `				/* Only interfaces are allowed */` |
|    9068 |  8125 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8126 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8127 | `				}` |
|    9068 |  8128 | `				if( pInterface == 0 ){` |
|     ! 0 |  8129 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8130 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8131 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8132 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8133 | `						return SXERR_ABORT;` |
|       - |  8134 | `					}` |
|     ! 0 |  8135 | `				}else{` |
|       - |  8136 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8137 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8138 | `					 * unless they already extend Exception or Error.` |
|       - |  8139 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8140 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8141 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    9068 |  8142 | `					SyString *pFqn = &pClass->sName;` |
|    9068 |  8143 | `					int bIsExceptionOrError =` |
|    7539 |  8144 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   15102 |  8145 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    7566 |  8146 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    3012 |  8147 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   15086 |  8148 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    9033 |  8149 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    3009 |  8150 | `						!bIsExceptionOrError ){` |
|      10 |  8151 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8152 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8153 | `							&pClass->sName);` |
|       7 |  8154 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8155 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8156 | `							return SXERR_ABORT;` |
|       - |  8157 | `						}` |
|       - |  8158 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8159 | `						 * check does not produce a duplicate fatal. */` |
|       4 |  8160 | `					}else{` |
|    9062 |  8161 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8162 | `					}` |
|       - |  8163 | `				}` |
|    9068 |  8164 | `				SyBlobRelease(&sResolved);` |
|    9068 |  8165 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4535 |  8166 | `					break;` |
|       - |  8167 | `				}` |
|     ! 0 |  8168 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8169 | `			}` |
|    4533 |  8170 | `		}` |
|   16623 |  8171 | `	}` |
|   42908 |  8172 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8173 | `		/* Syntax error */` |
|     ! 0 |  8174 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8175 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8176 | `		if( rc == SXERR_ABORT ){` |
|       - |  8177 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8178 | `			return SXERR_ABORT;` |
|       - |  8179 | `		}` |
|     ! 0 |  8180 | `		return SXRET_OK;` |
|       - |  8181 | `	}` |
|   42908 |  8182 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   42908 |  8183 | `	pEnd = 0; /* cc warning */` |
|       - |  8184 | `	/* Delimit the class body */` |
|   42908 |  8185 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   42908 |  8186 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8187 | `		/* Syntax error */` |
|     ! 0 |  8188 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8189 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8190 | `		if( rc == SXERR_ABORT ){` |
|       - |  8191 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8192 | `			return SXERR_ABORT;` |
|       - |  8193 | `		}` |
|     ! 0 |  8194 | `		return SXRET_OK;` |
|       - |  8195 | `	}` |
|       - |  8196 | `	/* Swap token stream */` |
|   42908 |  8197 | `	pTmp = pGen->pEnd;` |
|   42908 |  8198 | `	pGen->pEnd = pEnd;` |
|       - |  8199 | `	/* Set the inherited flags */` |
|   42908 |  8200 | `	pClass->iFlags = iFlags;` |
|       - |  8201 | `	/* Start the parse process */` |
|   95377 |  8202 | `	for(;;){` |
|       - |  8203 | `		/* Jump leading/trailing semi-colons */` |
|  305928 |  8204 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   57606 |  8205 | `			pGen->pIn++;` |
|       2 |  8206 | `		}` |
|  248324 |  8207 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8208 | `			/* End of class body */` |
|   42894 |  8209 | `			break;` |
|       - |  8210 | `		}` |
|  205432 |  8211 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8212 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8213 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8214 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8215 | `			if( rc == SXERR_ABORT ){` |
|       - |  8216 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8217 | `				return SXERR_ABORT;` |
|       - |  8218 | `			}` |
|     ! 0 |  8219 | `			goto done;` |
|       - |  8220 | `		}` |
|       - |  8221 | `		/* Assume public visibility */` |
|  205432 |  8222 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  205432 |  8223 | `		iAttrflags = 0;` |
|  205432 |  8224 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8225 | `			/* Extract the current keyword */` |
|  205432 |  8226 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  205432 |  8227 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8228 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8229 | `				TraitUseEntry sUse;` |
|      44 |  8230 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  8231 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  8232 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  8233 | `				for(;;){` |
|       - |  8234 | `					ph7_class *pTrait;` |
|       - |  8235 | `					SyString *pTraitName;` |
|      52 |  8236 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8237 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8238 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8239 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8240 | `							return SXERR_ABORT;` |
|       - |  8241 | `						}` |
|     ! 0 |  8242 | `						break;` |
|       - |  8243 | `					}` |
|      52 |  8244 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8245 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8246 | `						SyBlob sResolved;` |
|      52 |  8247 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  8248 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  8249 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  8250 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  8251 | `						SyBlobRelease(&sResolved);` |
|       - |  8252 | `					}` |
|       - |  8253 | `					/* Only traits are allowed */` |
|      52 |  8254 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8255 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8256 | `					}` |
|      52 |  8257 | `					if( pTrait == 0 ){` |
|     ! 0 |  8258 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8259 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8260 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8261 | `							return SXERR_ABORT;` |
|       - |  8262 | `						}` |
|     ! 0 |  8263 | `					}else{` |
|      52 |  8264 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8265 | `					}` |
|      52 |  8266 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  8267 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  8268 | `						break;` |
|       - |  8269 | `					}` |
|       9 |  8270 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  8271 | `				}` |
|       - |  8272 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  8273 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8274 | `					SyToken *pBlock;` |
|       9 |  8275 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  8276 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  8277 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  8278 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  8279 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  8280 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  8281 | `					}else{` |
|     ! 0 |  8282 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8283 | `					}` |
|       4 |  8284 | `				}` |
|      44 |  8285 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8286 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  8287 | `				continue;` |
|       - |  8288 | `			}` |
|  205390 |  8289 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  202264 |  8290 | `				iProtection = nKwrd;` |
|  202264 |  8291 | `				pGen->pIn++; /* Jump the visibility token */` |
|  202262 |  8292 | `				if( pGen->pIn >= pGen->pEnd` |
|  202264 |  8293 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8294 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8295 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8296 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8297 | `					if( rc == SXERR_ABORT ){` |
|       - |  8298 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8299 | `						return SXERR_ABORT;` |
|       - |  8300 | `					}` |
|     ! 0 |  8301 | `					goto done;` |
|       - |  8302 | `				}` |
|  202264 |  8303 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8304 | `					/* Attribute declaration (untyped) */` |
|   57410 |  8305 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   57410 |  8306 | `					if( rc != SXRET_OK ){` |
|       3 |  8307 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8308 | `							return SXERR_ABORT;` |
|       - |  8309 | `						}` |
|       3 |  8310 | `						goto done;` |
|       - |  8311 | `					}` |
|   57408 |  8312 | `					continue;` |
|       - |  8313 | `				}` |
|  144856 |  8314 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8315 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     112 |  8316 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     112 |  8317 | `					if( rc != SXRET_OK ){` |
|       3 |  8318 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8319 | `							return SXERR_ABORT;` |
|       - |  8320 | `						}` |
|       3 |  8321 | `						goto done;` |
|       - |  8322 | `					}` |
|     110 |  8323 | `					continue;` |
|       - |  8324 | `				}` |
|       - |  8325 | `				/* Extract the keyword */` |
|  144746 |  8326 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   72372 |  8327 | `			}` |
|  147872 |  8328 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8329 | `				/* Process constant declaration */` |
|      30 |  8330 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  8331 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8332 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8333 | `						return SXERR_ABORT;` |
|       - |  8334 | `					}` |
|     ! 0 |  8335 | `					goto done;` |
|       - |  8336 | `				}` |
|      16 |  8337 | `			}else{` |
|  147844 |  8338 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8339 | `					/* Static method or attribute,record that */` |
|    3046 |  8340 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    3046 |  8341 | `					pGen->pIn++; /* Jump the static keyword */` |
|    3046 |  8342 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8343 | `						/* Extract the keyword */` |
|    3040 |  8344 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    3040 |  8345 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8346 | `							iProtection = nKwrd;` |
|     ! 0 |  8347 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8348 | `						}` |
|    1519 |  8349 | `					}` |
|    3044 |  8350 | `					if( pGen->pIn >= pGen->pEnd` |
|    3046 |  8351 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8352 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8353 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8354 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8355 | `						if( rc == SXERR_ABORT ){` |
|       - |  8356 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8357 | `							return SXERR_ABORT;` |
|       - |  8358 | `						}` |
|     ! 0 |  8359 | `						goto done;` |
|       - |  8360 | `					}` |
|    3046 |  8361 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8362 | `						/* Attribute declaration */` |
|       5 |  8363 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8364 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8365 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8366 | `								return SXERR_ABORT;` |
|       - |  8367 | `							}` |
|     ! 0 |  8368 | `							goto done;` |
|       - |  8369 | `						}` |
|       5 |  8370 | `						continue;` |
|       - |  8371 | `					}` |
|    3042 |  8372 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8373 | `						/* Typed static attribute declaration */` |
|      10 |  8374 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 |  8375 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8376 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8377 | `								return SXERR_ABORT;` |
|       - |  8378 | `							}` |
|     ! 0 |  8379 | `							goto done;` |
|       - |  8380 | `						}` |
|      10 |  8381 | `						continue;` |
|       - |  8382 | `					}` |
|       - |  8383 | `					/* Extract the keyword */` |
|    3034 |  8384 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  146316 |  8385 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8386 | `					/* Abstract method,record that */` |
|      12 |  8387 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8388 | `					/* Mark the whole class as abstract */` |
|      12 |  8389 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8390 | `					/* Advance the stream cursor */` |
|      12 |  8391 | `					pGen->pIn++;` |
|      12 |  8392 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8393 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8394 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8395 | `							iProtection = nKwrd;` |
|      10 |  8396 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8397 | `						}` |
|       5 |  8398 | `					}` |
|      12 |  8399 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8400 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8401 | `							/* Static method */` |
|     ! 0 |  8402 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8403 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8404 | `					}` |
|      12 |  8405 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8406 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8407 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8408 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8409 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8410 | `							if( rc == SXERR_ABORT ){` |
|       - |  8411 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8412 | `								return SXERR_ABORT;` |
|       - |  8413 | `							}` |
|     ! 0 |  8414 | `							goto done;` |
|       - |  8415 | `					}` |
|      12 |  8416 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  144795 |  8417 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8418 | `					/* final method ,record that */` |
|       5 |  8419 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  8420 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  8421 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8422 | `						/* Extract the keyword */` |
|       5 |  8423 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8424 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8425 | `							iProtection = nKwrd;` |
|       5 |  8426 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8427 | `						}` |
|       2 |  8428 | `					}` |
|       5 |  8429 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8430 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8431 | `							/* Static method */` |
|     ! 0 |  8432 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8433 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8434 | `					}` |
|       5 |  8435 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8436 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8437 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8438 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8439 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8440 | `							if( rc == SXERR_ABORT ){` |
|       - |  8441 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8442 | `								return SXERR_ABORT;` |
|       - |  8443 | `							}` |
|     ! 0 |  8444 | `							goto done;` |
|       - |  8445 | `					}` |
|       5 |  8446 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8447 | `				}` |
|  147832 |  8448 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8449 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8450 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8451 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8452 | `						if( rc == SXERR_ABORT ){` |
|       - |  8453 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8454 | `							return SXERR_ABORT;` |
|       - |  8455 | `						}` |
|     ! 0 |  8456 | `						goto done;` |
|       - |  8457 | `				}` |
|  147832 |  8458 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8459 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8460 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8461 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8462 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8463 | `						if( rc == SXERR_ABORT ){` |
|       - |  8464 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8465 | `							return SXERR_ABORT;` |
|       - |  8466 | `						}` |
|     ! 0 |  8467 | `						goto done;` |
|       - |  8468 | `					}` |
|       - |  8469 | `					/* Attribute declaration */` |
|       7 |  8470 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8471 | `				}else{` |
|       - |  8472 | `					/* Process method declaration */` |
|  147826 |  8473 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8474 | `				}` |
|  147832 |  8475 | `				if( rc != SXRET_OK ){` |
|      11 |  8476 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8477 | `						return SXERR_ABORT;` |
|       - |  8478 | `					}` |
|      11 |  8479 | `					goto done;` |
|       - |  8480 | `				}` |
|       - |  8481 | `			}` |
|   73926 |  8482 | `		}else{` |
|       - |  8483 | `			/* Attribute declaration */` |
|     ! 0 |  8484 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8485 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8486 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8487 | `					return SXERR_ABORT;` |
|       - |  8488 | `				}` |
|     ! 0 |  8489 | `				goto done;` |
|       - |  8490 | `			}` |
|       - |  8491 | `		}` |
|       2 |  8492 | `	}` |
|       - |  8493 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8494 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8495 | `	 */` |
|       - |  8496 | `	{` |
|       - |  8497 | `		TraitUseEntry *apUse;` |
|       - |  8498 | `		sxu32 nU;` |
|   42894 |  8499 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   42936 |  8500 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  8501 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  8502 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  8503 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  8504 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8505 | `			sxu32 nT;` |
|      44 |  8506 | `			if( !hasResolution ){` |
|       - |  8507 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  8508 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  8509 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  8510 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8511 | `						break;` |
|       - |  8512 | `					}` |
|      22 |  8513 | `				}` |
|      19 |  8514 | `			}else{` |
|       - |  8515 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8516 | `				 * then use the block to resolve method conflicts.` |
|       - |  8517 | `				 */` |
|       - |  8518 | `				SyToken *pR;` |
|      19 |  8519 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  8520 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8521 | `					ph7_class_attr *pAR;` |
|       - |  8522 | `					SyHashEntry *pER;` |
|       - |  8523 | `					SyString *pNR;` |
|      11 |  8524 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  8525 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8526 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8527 | `						pNR = &pAR->sName;` |
|     ! 0 |  8528 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8529 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8530 | `						}` |
|     ! 0 |  8531 | `					}` |
|      11 |  8532 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  8533 | `				}` |
|       - |  8534 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  8535 | `				pR = pUse->pResolvStart;` |
|      21 |  8536 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8537 | `					SyString sTrait,sMethod;` |
|       - |  8538 | `					ph7_class *pSrcTrait;` |
|       - |  8539 | `					ph7_class_method *pMeth;` |
|       - |  8540 | `					sxi32 nRKwrd;` |
|      33 |  8541 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8542 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8543 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8544 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8545 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8546 | `					sMethod = pR->sData;` |
|      13 |  8547 | `					pR++;` |
|      13 |  8548 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8549 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8550 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8551 | `							sTrait = sMethod;` |
|       7 |  8552 | `							pR++;` |
|       7 |  8553 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8554 | `							sMethod = pR->sData;` |
|       7 |  8555 | `							pR++;` |
|       3 |  8556 | `						}` |
|       3 |  8557 | `					}` |
|      13 |  8558 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8559 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8560 | `						continue;` |
|       - |  8561 | `					}` |
|      13 |  8562 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8563 | `					pR++;` |
|      13 |  8564 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8565 | `						pSrcTrait = 0;` |
|       7 |  8566 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8567 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8568 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8569 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8570 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8571 | `								break;` |
|       - |  8572 | `							}` |
|       2 |  8573 | `						}` |
|       5 |  8574 | `						if( pSrcTrait ){` |
|       5 |  8575 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8576 | `							if( pMeth ){` |
|       5 |  8577 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8578 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8579 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8580 | `								}` |
|       2 |  8581 | `							}` |
|       2 |  8582 | `						}` |
|       2 |  8583 | `					}` |
|      29 |  8584 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8585 | `				}` |
|       - |  8586 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8587 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8588 | `					ph7_class_method *pMR;` |
|       - |  8589 | `					SyHashEntry *pER;` |
|       - |  8590 | `					SyString *pNR;` |
|      11 |  8591 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8592 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8593 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8594 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8595 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8596 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8597 | `						}` |
|       1 |  8598 | `					}` |
|       6 |  8599 | `				}` |
|       - |  8600 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8601 | `				pR = pUse->pResolvStart;` |
|      21 |  8602 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8603 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8604 | `					ph7_class *pSrcTrait;` |
|       - |  8605 | `					ph7_class_method *pMeth;` |
|      21 |  8606 | `					int hasQual = 0;` |
|       - |  8607 | `					sxi32 nRKwrd;` |
|      33 |  8608 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8609 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8610 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8611 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8612 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8613 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8614 | `					sMethod = pR->sData;` |
|      13 |  8615 | `					pR++;` |
|      13 |  8616 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8617 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8618 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8619 | `							sTrait = sMethod;` |
|       7 |  8620 | `							hasQual = 1;` |
|       7 |  8621 | `							pR++;` |
|       7 |  8622 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8623 | `							sMethod = pR->sData;` |
|       7 |  8624 | `							pR++;` |
|       3 |  8625 | `						}` |
|       3 |  8626 | `					}` |
|      13 |  8627 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8628 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8629 | `						continue;` |
|       - |  8630 | `					}` |
|      13 |  8631 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8632 | `					pR++;` |
|      13 |  8633 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8634 | `						sxi32 iNewVis = -1;` |
|       9 |  8635 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8636 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8637 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8638 | `								iNewVis = nAK;` |
|       7 |  8639 | `								pR++;` |
|       3 |  8640 | `							}` |
|       3 |  8641 | `						}` |
|       9 |  8642 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8643 | `							sAlias = pR->sData;` |
|       7 |  8644 | `							pR++;` |
|       3 |  8645 | `						}` |
|       9 |  8646 | `						pMeth = 0;` |
|       9 |  8647 | `						if( hasQual ){` |
|       3 |  8648 | `							pSrcTrait = 0;` |
|       5 |  8649 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8650 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8651 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8652 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8653 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8654 | `									break;` |
|       - |  8655 | `								}` |
|       2 |  8656 | `							}` |
|       3 |  8657 | `							if( pSrcTrait ){` |
|       3 |  8658 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8659 | `							}` |
|       2 |  8660 | `						}else{` |
|       7 |  8661 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8662 | `						}` |
|       9 |  8663 | `						if( pMeth ){` |
|       9 |  8664 | `							if( sAlias.nByte > 0 ){` |
|       - |  8665 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8666 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8667 | `								 */` |
|       - |  8668 | `								ph7_class_method *pAlias;` |
|       - |  8669 | `								char *zAliasDup;` |
|       7 |  8670 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8671 | `								if( pAlias ){` |
|       7 |  8672 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8673 | `									if( iNewVis >= 0 ){` |
|       5 |  8674 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8675 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8676 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8677 | `									}` |
|       7 |  8678 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8679 | `									if( zAliasDup ){` |
|       7 |  8680 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8681 | `									}` |
|       4 |  8682 | `								}` |
|       6 |  8683 | `							}else if( iNewVis >= 0 ){` |
|       - |  8684 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8685 | `								ph7_class_method *pCopy;` |
|       3 |  8686 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8687 | `								if( pCopy ){` |
|       3 |  8688 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8689 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8690 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8691 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8692 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8693 | `									/* Replace the method in the class hash */` |
|       3 |  8694 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8695 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8696 | `								}` |
|       1 |  8697 | `							}` |
|       4 |  8698 | `						}` |
|       4 |  8699 | `						SXUNUSED(hasQual);` |
|       4 |  8700 | `					}` |
|      17 |  8701 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8702 | `				}` |
|       - |  8703 | `			}` |
|      44 |  8704 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  8705 | `		}` |
|       - |  8706 | `	}` |
|       - |  8707 | `	/* Install the class */` |
|   42894 |  8708 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   42894 |  8709 | `	if( rc == SXRET_OK ){` |
|       - |  8710 | `		ph7_class **apInterface;` |
|       - |  8711 | `		sxu32 n;` |
|   42894 |  8712 | `		if( pBase ){` |
|       - |  8713 | `			/* Inherit from base class and mark as a subclass */` |
|   24188 |  8714 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   12093 |  8715 | `		}` |
|   42894 |  8716 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   51954 |  8717 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8718 | `			/* Implements one or more interface */` |
|    9062 |  8719 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    9062 |  8720 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8721 | `				break;` |
|       - |  8722 | `			}` |
|    4532 |  8723 | `		}` |
|       - |  8724 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   42894 |  8725 | `		if( rc == SXRET_OK ){` |
|   42894 |  8726 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   42894 |  8727 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8728 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8729 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8730 | `				return SXERR_ABORT;` |
|       - |  8731 | `			}` |
|   21446 |  8732 | `		}` |
|       - |  8733 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   42894 |  8734 | `		if( rc == SXRET_OK ){` |
|   42894 |  8735 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   42894 |  8736 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8737 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8738 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8739 | `				return SXERR_ABORT;` |
|       - |  8740 | `			}` |
|   21446 |  8741 | `		}` |
|   21446 |  8742 | `	}` |
|   42894 |  8743 | `	SySetRelease(&aUseEntries);` |
|   42894 |  8744 | `	SySetRelease(&aInterfaces);` |
|   42894 |  8745 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8746 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8747 | `		return SXERR_ABORT;` |
|       - |  8748 | `	}` |
|   21446 |  8749 | `done:` |
|       - |  8750 | `	/* Point beyond the class body */` |
|   42908 |  8751 | `	pGen->pIn = &pEnd[1];` |
|   42908 |  8752 | `	pGen->pEnd = pTmp;` |
|   42908 |  8753 | `	return PH7_OK;` |
|   21456 |  8754 |  |
|       - |  8755 | `/*` |
|       - |  8756 | ` * Compile a user-defined abstract class.` |
|       - |  8757 | ` *  According to the PHP language reference manual` |
|       - |  8758 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8759 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8760 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8761 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8762 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8763 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8764 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8765 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8766 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8767 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8768 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8769 | ` *   could differ.` |
|       - |  8770 | ` */` |
|      18 |  8771 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8772 |  |
|       - |  8773 | `	sxi32 rc;` |
|      20 |  8774 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      20 |  8775 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      20 |  8776 | `	return rc;` |
|       2 |  8777 |  |
|       - |  8778 | `/*` |
|       - |  8779 | ` * Compile a user-defined final class.` |
|       - |  8780 | ` *  According to the PHP language reference manual` |
|       - |  8781 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8782 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8783 | ` *    final then it cannot be extended.` |
|       - |  8784 | ` */` |
|       2 |  8785 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8786 |  |
|       - |  8787 | `	sxi32 rc;` |
|       3 |  8788 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8789 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8790 | `	return rc;` |
|       1 |  8791 |  |
|       - |  8792 | `/*` |
|       - |  8793 | ` * Compile a user-defined trait.` |
|       - |  8794 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8795 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8796 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8797 | ` */` |
|      54 |  8798 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8799 |  |
|      56 |  8800 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8801 | `	ph7_class *pClass;` |
|       - |  8802 | `	SyToken *pEnd,*pTmp;` |
|       - |  8803 | `	sxi32 iProtection;` |
|       - |  8804 | `	sxi32 iAttrflags;` |
|       - |  8805 | `	SyString *pName;` |
|       - |  8806 | `	sxi32 nKwrd;` |
|       - |  8807 | `	sxi32 rc;` |
|       - |  8808 | `	/* Jump the 'trait' keyword */` |
|      56 |  8809 | `	pGen->pIn++;` |
|      56 |  8810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8811 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8812 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8813 | `			return SXERR_ABORT;` |
|       - |  8814 | `		}` |
|     ! 0 |  8815 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8816 | `			pGen->pIn++;` |
|     ! 0 |  8817 | `		}` |
|     ! 0 |  8818 | `		return SXRET_OK;` |
|       - |  8819 | `	}` |
|       - |  8820 | `	/* Extract trait name */` |
|      56 |  8821 | `	pName = &pGen->pIn->sData;` |
|      56 |  8822 | `	pGen->pIn++;` |
|       - |  8823 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8824 | `		SyBlob sFQN;` |
|       - |  8825 | `		SyString sFQNStr;` |
|      56 |  8826 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8827 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8828 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8829 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8830 | `		SyBlobRelease(&sFQN);` |
|       - |  8831 | `	}` |
|      56 |  8832 | `	if( pClass == 0 ){` |
|     ! 0 |  8833 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8834 | `		return SXERR_ABORT;` |
|       - |  8835 | `	}` |
|       - |  8836 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8837 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8838 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8839 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8840 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8841 | `			return SXERR_ABORT;` |
|       - |  8842 | `		}` |
|     ! 0 |  8843 | `		return SXRET_OK;` |
|       - |  8844 | `	}` |
|      56 |  8845 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8846 | `	pEnd = 0;` |
|      56 |  8847 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8848 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8849 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8850 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8851 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8852 | `			return SXERR_ABORT;` |
|       - |  8853 | `		}` |
|     ! 0 |  8854 | `		return SXRET_OK;` |
|       - |  8855 | `	}` |
|       - |  8856 | `	/* Swap token stream */` |
|      56 |  8857 | `	pTmp = pGen->pEnd;` |
|      56 |  8858 | `	pGen->pEnd = pEnd;` |
|       - |  8859 | `	/* Mark as trait */` |
|      56 |  8860 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8861 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8862 | `	for(;;){` |
|     154 |  8863 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8864 | `			pGen->pIn++;` |
|       2 |  8865 | `		}` |
|     130 |  8866 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8867 | `			break;` |
|       - |  8868 | `		}` |
|      76 |  8869 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8870 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8871 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8872 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8873 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8874 | `				return SXERR_ABORT;` |
|       - |  8875 | `			}` |
|     ! 0 |  8876 | `			goto done;` |
|       - |  8877 | `		}` |
|      76 |  8878 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8879 | `		iAttrflags = 0;` |
|      76 |  8880 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8881 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8882 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8883 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8884 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8885 | `				for(;;){` |
|       - |  8886 | `					ph7_class *pUsedTrait;` |
|       - |  8887 | `					SyString *pUsedName;` |
|       5 |  8888 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8889 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8890 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8891 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8892 | `							return SXERR_ABORT;` |
|       - |  8893 | `						}` |
|     ! 0 |  8894 | `						break;` |
|       - |  8895 | `					}` |
|       5 |  8896 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8897 | `					{` |
|       - |  8898 | `						SyBlob sResolved;` |
|       5 |  8899 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8900 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8901 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8902 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8903 | `						SyBlobRelease(&sResolved);` |
|       - |  8904 | `					}` |
|       5 |  8905 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8906 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8907 | `					}` |
|       5 |  8908 | `					if( pUsedTrait == 0 ){` |
|       4 |  8909 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8910 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8911 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8912 | `							return SXERR_ABORT;` |
|       - |  8913 | `						}` |
|       2 |  8914 | `					}else{` |
|       3 |  8915 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8916 | `					}` |
|       5 |  8917 | `					pGen->pIn++;` |
|       5 |  8918 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8919 | `						break;` |
|       - |  8920 | `					}` |
|     ! 0 |  8921 | `					pGen->pIn++;` |
|     ! 0 |  8922 | `				}` |
|       5 |  8923 | `				continue;` |
|       - |  8924 | `			}` |
|      72 |  8925 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8926 | `				iProtection = nKwrd;` |
|      68 |  8927 | `				pGen->pIn++;` |
|      66 |  8928 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8929 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8930 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8931 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8932 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8933 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8934 | `						return SXERR_ABORT;` |
|       - |  8935 | `					}` |
|     ! 0 |  8936 | `					goto done;` |
|       - |  8937 | `				}` |
|      68 |  8938 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8939 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8940 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8941 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8942 | `							return SXERR_ABORT;` |
|       - |  8943 | `						}` |
|     ! 0 |  8944 | `						goto done;` |
|       - |  8945 | `					}` |
|      11 |  8946 | `					continue;` |
|       - |  8947 | `				}` |
|      58 |  8948 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8949 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8950 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8951 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8952 | `							return SXERR_ABORT;` |
|       - |  8953 | `						}` |
|     ! 0 |  8954 | `						goto done;` |
|       - |  8955 | `					}` |
|       5 |  8956 | `					continue;` |
|       - |  8957 | `				}` |
|      53 |  8958 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8959 | `			}` |
|      57 |  8960 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8961 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8962 | `					"Traits cannot have constants");` |
|     ! 0 |  8963 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8964 | `					return SXERR_ABORT;` |
|       - |  8965 | `				}` |
|     ! 0 |  8966 | `				goto done;` |
|     ! 0 |  8967 | `			}else{` |
|      57 |  8968 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8969 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8970 | `					pGen->pIn++;` |
|       5 |  8971 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8972 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8973 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8974 | `							iProtection = nKwrd;` |
|     ! 0 |  8975 | `							pGen->pIn++;` |
|     ! 0 |  8976 | `						}` |
|       1 |  8977 | `					}` |
|       4 |  8978 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8979 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8980 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8981 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8982 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8983 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8984 | `							return SXERR_ABORT;` |
|       - |  8985 | `						}` |
|     ! 0 |  8986 | `						goto done;` |
|       - |  8987 | `					}` |
|       5 |  8988 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8989 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8990 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8991 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8992 | `								return SXERR_ABORT;` |
|       - |  8993 | `							}` |
|     ! 0 |  8994 | `							goto done;` |
|       - |  8995 | `						}` |
|       3 |  8996 | `						continue;` |
|       - |  8997 | `					}` |
|       3 |  8998 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8999 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9000 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  9001 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  9002 | `								return SXERR_ABORT;` |
|       - |  9003 | `							}` |
|     ! 0 |  9004 | `							goto done;` |
|       - |  9005 | `						}` |
|     ! 0 |  9006 | `						continue;` |
|       - |  9007 | `					}` |
|       3 |  9008 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  9009 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  9010 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  9011 | `					pGen->pIn++;` |
|       5 |  9012 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  9013 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  9014 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  9015 | `							iProtection = nKwrd;` |
|       5 |  9016 | `							pGen->pIn++;` |
|       2 |  9017 | `						}` |
|       2 |  9018 | `					}` |
|       5 |  9019 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  9020 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  9021 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9022 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9023 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9024 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9025 | `							return SXERR_ABORT;` |
|       - |  9026 | `						}` |
|     ! 0 |  9027 | `						goto done;` |
|       - |  9028 | `					}` |
|       5 |  9029 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9030 | `				}` |
|      55 |  9031 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9032 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9033 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9034 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9035 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9036 | `						return SXERR_ABORT;` |
|       - |  9037 | `					}` |
|     ! 0 |  9038 | `					goto done;` |
|       - |  9039 | `				}` |
|      55 |  9040 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9041 | `					pGen->pIn++;` |
|     ! 0 |  9042 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9043 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9044 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9045 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9046 | `							return SXERR_ABORT;` |
|       - |  9047 | `						}` |
|     ! 0 |  9048 | `						goto done;` |
|       - |  9049 | `					}` |
|     ! 0 |  9050 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9051 | `				}else{` |
|      55 |  9052 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9053 | `				}` |
|      55 |  9054 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9055 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9056 | `						return SXERR_ABORT;` |
|       - |  9057 | `					}` |
|     ! 0 |  9058 | `					goto done;` |
|       - |  9059 | `				}` |
|       - |  9060 | `			}` |
|      28 |  9061 | `		}else{` |
|     ! 0 |  9062 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9063 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9064 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9065 | `					return SXERR_ABORT;` |
|       - |  9066 | `				}` |
|     ! 0 |  9067 | `				goto done;` |
|       - |  9068 | `			}` |
|       - |  9069 | `		}` |
|       1 |  9070 | `	}` |
|       - |  9071 | `	/* Install the trait */` |
|      56 |  9072 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  9073 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9074 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9075 | `		return SXERR_ABORT;` |
|       - |  9076 | `	}` |
|      27 |  9077 | `done:` |
|       - |  9078 | `	/* Point beyond the trait body */` |
|      56 |  9079 | `	pGen->pIn = &pEnd[1];` |
|      56 |  9080 | `	pGen->pEnd = pTmp;` |
|      56 |  9081 | `	return PH7_OK;` |
|      29 |  9082 |  |
|       - |  9083 | `/*` |
|       - |  9084 | ` * Compile a user-defined class.` |
|       - |  9085 | ` *  According to the PHP language reference manual` |
|       - |  9086 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9087 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9088 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9089 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9090 | ` *   and functions (called "methods").` |
|       - |  9091 | ` */` |
|   42888 |  9092 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  9093 |  |
|       - |  9094 | `	sxi32 rc;` |
|   42890 |  9095 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   42890 |  9096 | `	return rc;` |
|       2 |  9097 |  |
|       - |  9098 | `/*` |
|       - |  9099 | ` * Exception handling.` |
|       - |  9100 | ` *  According to the PHP language reference manual` |
|       - |  9101 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9102 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9103 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9104 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9105 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9106 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9107 | ` *    (or re-thrown) within a catch block.` |
|       - |  9108 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9109 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9110 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9111 | ` *    been defined with set_exception_handler().` |
|       - |  9112 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9113 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9114 | ` */` |
|       - |  9115 | `/*` |
|       - |  9116 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9117 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9118 | ` * indicates failure.` |
|       - |  9119 | ` */` |
|    9136 |  9120 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  9121 |  |
|    9138 |  9122 | `	sxi32 rc = SXRET_OK;` |
|    9138 |  9123 | `	if( pRoot->pOp ){` |
|    9130 |  9124 | `		switch( pRoot->pOp->iOp ){` |
|    4564 |  9125 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9126 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9127 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9128 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9129 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9130 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    9130 |  9131 | `			break;` |
|     ! 0 |  9132 | `		default:` |
|       - |  9133 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9134 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9135 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9136 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9137 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9138 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9139 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9140 | `			}` |
|     ! 0 |  9141 | `			break;` |
|       - |  9142 | `		}` |
|    4574 |  9143 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9144 | `		/* Unexpected expression */` |
|     ! 0 |  9145 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9146 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9147 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9148 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9149 | `		}` |
|     ! 0 |  9150 | `	}` |
|    9138 |  9151 | `	return rc;` |
|       2 |  9152 |  |
|       - |  9153 | `/*` |
|       - |  9154 | ` * Compile a 'throw' statement.` |
|       - |  9155 | ` * throw: This is how you trigger an exception.` |
|       - |  9156 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9157 | ` */` |
|    9100 |  9158 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  9159 |  |
|    9102 |  9160 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9161 | `	GenBlock *pBlock;` |
|       - |  9162 | `	sxu32 nIdx;` |
|       - |  9163 | `	sxi32 rc;` |
|    9102 |  9164 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9165 | `	/* Compile the expression */` |
|    9102 |  9166 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    9102 |  9167 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9168 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9169 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9170 | `			return SXERR_ABORT;` |
|       - |  9171 | `		}` |
|     ! 0 |  9172 | `		return SXRET_OK;` |
|       - |  9173 | `	}` |
|    9102 |  9174 | `	pBlock = pGen->pCurrent;` |
|       - |  9175 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   42250 |  9176 | `	while(pBlock->pParent){` |
|   42246 |  9177 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    9098 |  9178 | `			break;` |
|       - |  9179 | `		}` |
|       - |  9180 | `		/* Point to the parent block */` |
|   33150 |  9181 | `		pBlock = pBlock->pParent;` |
|       2 |  9182 | `	}` |
|       - |  9183 | `	/* Emit the throw instruction */` |
|    9102 |  9184 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9185 | `	/* Emit the jump */` |
|    9102 |  9186 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    9102 |  9187 | `	return SXRET_OK;` |
|    4552 |  9188 |  |
|       - |  9189 | `/*` |
|       - |  9190 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9191 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9192 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9193 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9194 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9195 | ` */` |
|      36 |  9196 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9197 |  |
|      38 |  9198 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9199 | `	GenBlock *pBlock;` |
|       - |  9200 | `	sxu32 nIdx;` |
|       - |  9201 | `	sxi32 rc;` |
|      18 |  9202 | `	(void)iCompileFlag;` |
|      38 |  9203 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9204 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9205 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9206 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9207 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9208 | `			return SXERR_ABORT;` |
|       - |  9209 | `		}` |
|     ! 0 |  9210 | `		return SXRET_OK;` |
|       - |  9211 | `	}` |
|      38 |  9212 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9213 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9214 | `		return SXERR_ABORT;` |
|       - |  9215 | `	}` |
|      38 |  9216 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9217 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9218 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9219 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9220 | `			return SXERR_ABORT;` |
|       - |  9221 | `		}` |
|     ! 0 |  9222 | `		return SXRET_OK;` |
|       - |  9223 | `	}` |
|       - |  9224 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9225 | `	pBlock = pGen->pCurrent;` |
|      60 |  9226 | `	while( pBlock->pParent ){` |
|      49 |  9227 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9228 | `			break;` |
|       - |  9229 | `		}` |
|      23 |  9230 | `		pBlock = pBlock->pParent;` |
|       1 |  9231 | `	}` |
|      38 |  9232 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9233 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9234 | `	return SXRET_OK;` |
|      20 |  9235 |  |
|       - |  9236 | `/*` |
|       - |  9237 | ` * Compile a 'catch' block.` |
|       - |  9238 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9239 | ` * an object containing the exception information.` |
|       - |  9240 | ` */` |
|     288 |  9241 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  9242 |  |
|     290 |  9243 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9244 | `	ph7_exception_block sCatch;` |
|       - |  9245 | `	SySet *pInstrContainer;` |
|       - |  9246 | `	SyString sClassName;` |
|       - |  9247 | `	GenBlock *pCatch;` |
|       - |  9248 | `	SyToken *pToken;` |
|       - |  9249 | `	SyString *pName;` |
|       - |  9250 | `	char *zDup;` |
|       - |  9251 | `	sxi32 rc;` |
|     290 |  9252 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9253 | `	/* Zero the structure */` |
|     290 |  9254 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9255 | `	/* Initialize fields */` |
|     290 |  9256 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     290 |  9257 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     290 |  9258 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9259 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9260 | `			pToken = pGen->pIn;` |
|     ! 0 |  9261 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9262 | `				pToken--;` |
|     ! 0 |  9263 | `			}` |
|     ! 0 |  9264 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9265 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9266 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9267 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9268 | `				return SXERR_ABORT;` |
|       - |  9269 | `			}` |
|     ! 0 |  9270 | `			return SXERR_INVALID;` |
|       - |  9271 | `	}` |
|       - |  9272 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     290 |  9273 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     157 |  9274 | `	for(;;){` |
|       - |  9275 | `		SyBlob sResolved;` |
|     316 |  9276 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     316 |  9277 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       5 |  9278 | `			SyBlobRelease(&sResolved);` |
|       5 |  9279 | `			pToken = pGen->pIn;` |
|       5 |  9280 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9281 | `				pToken--;` |
|     ! 0 |  9282 | `			}` |
|       7 |  9283 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9284 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9285 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  9286 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9287 | `				return SXERR_ABORT;` |
|       - |  9288 | `			}` |
|       5 |  9289 | `			return SXERR_INVALID;` |
|       - |  9290 | `		}` |
|       - |  9291 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9292 | `		 * transient SyBlob allocation. */` |
|     467 |  9293 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     310 |  9294 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     312 |  9295 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     312 |  9296 | `		SyBlobRelease(&sResolved);` |
|     312 |  9297 | `		if( zDup == 0 ){` |
|     ! 0 |  9298 | `			goto Mem;` |
|       - |  9299 | `		}` |
|     312 |  9300 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     312 |  9301 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9302 | `			goto Mem;` |
|       - |  9303 | `		}` |
|       - |  9304 | `		/* Check for '\|' (multi-catch separator) */` |
|     323 |  9305 | `		if( pGen->pIn < pGen->pEnd &&` |
|     310 |  9306 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      28 |  9307 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9308 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9309 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9310 | `			continue;` |
|       - |  9311 | `		}` |
|     286 |  9312 | `		break;` |
|     ! 0 |  9313 | `	}` |
|     426 |  9314 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     286 |  9315 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9316 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9317 | `			pToken = pGen->pIn;` |
|     ! 0 |  9318 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9319 | `				pToken--;` |
|     ! 0 |  9320 | `			}` |
|     ! 0 |  9321 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9322 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9323 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9324 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9325 | `				return SXERR_ABORT;` |
|       - |  9326 | `			}` |
|     ! 0 |  9327 | `			return SXERR_INVALID;` |
|       - |  9328 | `	}` |
|     286 |  9329 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9330 | `	/* Duplicate instance name */` |
|     286 |  9331 | `	pName = &pGen->pIn->sData;` |
|     286 |  9332 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     286 |  9333 | `	if( zDup == 0 ){` |
|     ! 0 |  9334 | `		goto Mem;` |
|       - |  9335 | `	}` |
|     286 |  9336 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     286 |  9337 | `	pGen->pIn++;` |
|     286 |  9338 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9339 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9340 | `		pToken = pGen->pIn;` |
|     ! 0 |  9341 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9342 | `			pToken--;` |
|     ! 0 |  9343 | `		}` |
|     ! 0 |  9344 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9345 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9346 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9347 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9348 | `			return SXERR_ABORT;` |
|       - |  9349 | `		}` |
|     ! 0 |  9350 | `		return SXERR_INVALID;` |
|       - |  9351 | `	}` |
|       - |  9352 | `	/* Compile the block */` |
|     286 |  9353 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9354 | `	/* Create the catch block */` |
|     286 |  9355 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     286 |  9356 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9357 | `		return SXERR_ABORT;` |
|       - |  9358 | `	}` |
|       - |  9359 | `	/* Swap bytecode container */` |
|     286 |  9360 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     286 |  9361 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9362 | `	/* Compile the block */` |
|     286 |  9363 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9364 | `	/* Fix forward jumps now the destination is resolved  */` |
|     286 |  9365 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9366 | `	/* Emit the DONE instruction */` |
|     286 |  9367 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9368 | `	/* Leave the block */` |
|     286 |  9369 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9370 | `	/* Restore the default container */` |
|     286 |  9371 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9372 | `	/* Install the catch block */` |
|     286 |  9373 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     286 |  9374 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9375 | `		goto Mem;` |
|       - |  9376 | `	}` |
|     286 |  9377 | `	return SXRET_OK;` |
|     ! 0 |  9378 | `Mem:` |
|     ! 0 |  9379 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9380 | `	return SXERR_ABORT;` |
|     146 |  9381 |  |
|       - |  9382 | `/*` |
|       - |  9383 | ` * Compile a 'try' block.` |
|       - |  9384 | ` * A function using an exception should be in a "try" block.` |
|       - |  9385 | ` * If the exception does not trigger, the code will continue` |
|       - |  9386 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9387 | ` * is "thrown".` |
|       - |  9388 | ` */` |
|     292 |  9389 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  9390 |  |
|       - |  9391 | `	ph7_exception *pException;` |
|     294 |  9392 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9393 | `	GenBlock *pTry;` |
|       - |  9394 | `	sxu32 nJmpIdx;` |
|       - |  9395 | `	sxi32 rc;` |
|       - |  9396 | `	/* Create the exception container */` |
|     294 |  9397 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     294 |  9398 | `	if( pException == 0 ){` |
|     ! 0 |  9399 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9400 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9401 | `		return SXERR_ABORT;` |
|       - |  9402 | `	}` |
|       - |  9403 | `	/* Zero the structure */` |
|     294 |  9404 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9405 | `	/* Initialize fields */` |
|     294 |  9406 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     294 |  9407 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     294 |  9408 | `	pException->iHasFinally = 0;` |
|     294 |  9409 | `	pException->iFinallyDone = 0;` |
|     294 |  9410 | `	pException->pVm = pGen->pVm;` |
|       - |  9411 | `	/* Create the try block */` |
|     294 |  9412 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     294 |  9413 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9414 | `		return SXERR_ABORT;` |
|       - |  9415 | `	}` |
|       - |  9416 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     294 |  9417 | `	pTry->pUserData = pException;` |
|       - |  9418 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     294 |  9419 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9420 | `	/* Fix the jump later when the destination is resolved */` |
|     294 |  9421 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     294 |  9422 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9423 | `	/* Compile the block */` |
|     294 |  9424 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     294 |  9425 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9426 | `		return SXERR_ABORT;` |
|       - |  9427 | `	}` |
|       - |  9428 | `	/* Fix forward jumps now the destination is resolved */` |
|     294 |  9429 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9430 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     294 |  9431 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9432 | `	/* Leave the block */` |
|     294 |  9433 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9434 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     294 |  9435 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     290 |  9436 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9437 | `		/* Compile one or more catch blocks */` |
|     284 |  9438 | `		for(;;){` |
|     568 |  9439 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     441 |  9440 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     142 |  9441 | `					break;` |
|       - |  9442 | `			}` |
|     290 |  9443 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     290 |  9444 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9445 | `				return SXERR_ABORT;` |
|       - |  9446 | `			}` |
|       2 |  9447 | `		}` |
|     140 |  9448 | `	}` |
|       - |  9449 | `	/* Compile optional finally block */` |
|     294 |  9450 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     136 |  9451 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9452 | `		SySet *pInstrContainer;` |
|       - |  9453 | `		GenBlock *pFinBlock;` |
|      32 |  9454 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9455 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  9456 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  9457 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9458 | `			return SXERR_ABORT;` |
|       - |  9459 | `		}` |
|       - |  9460 | `		/* Swap bytecode container */` |
|      32 |  9461 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  9462 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9463 | `		/* Compile the finally body */` |
|      32 |  9464 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  9465 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9466 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9467 | `			return SXERR_ABORT;` |
|       - |  9468 | `		}` |
|       - |  9469 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  9470 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9471 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  9472 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9473 | `		/* Leave the block */` |
|      32 |  9474 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9475 | `		/* Restore the default container */` |
|      32 |  9476 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  9477 | `		pException->iHasFinally = 1;` |
|      15 |  9478 | `	}` |
|       - |  9479 | `	/* Must have at least one catch or finally */` |
|     294 |  9480 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  9481 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9482 | `			"Cannot use try without catch or finally");` |
|       7 |  9483 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9484 | `			return SXERR_ABORT;` |
|       - |  9485 | `		}` |
|       3 |  9486 | `	}` |
|     294 |  9487 | `	return SXRET_OK;` |
|     148 |  9488 |  |
|       - |  9489 | `/*` |
|       - |  9490 | ` * Compile a switch block.` |
|       - |  9491 | ` *  (See block-comment below for more information)` |
|       - |  9492 | ` */` |
|     108 |  9493 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  9494 |  |
|     110 |  9495 | `	sxi32 rc = SXRET_OK;` |
|     110 |  9496 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9497 | `		/* Unexpected token */` |
|     ! 0 |  9498 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9499 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9500 | `			return SXERR_ABORT;` |
|       - |  9501 | `		}` |
|     ! 0 |  9502 | `		pGen->pIn++;` |
|     ! 0 |  9503 | `	}` |
|     110 |  9504 | `	pGen->pIn++;` |
|       - |  9505 | `	/* First instruction to execute in this block. */` |
|     110 |  9506 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9507 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9508 | `	 * or the '}' token */` |
|     182 |  9509 | `	for(;;){` |
|     366 |  9510 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9511 | `			/* No more input to process */` |
|     ! 0 |  9512 | `			break;` |
|       - |  9513 | `		}` |
|     366 |  9514 | `		rc = SXRET_OK;` |
|     366 |  9515 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  9516 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  9517 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9518 | `					/* Unexpected token */` |
|     ! 0 |  9519 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9520 | `						&pGen->pIn->sData);` |
|     ! 0 |  9521 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9522 | `						return SXERR_ABORT;` |
|       - |  9523 | `					}` |
|       - |  9524 | `					/* FALL THROUGH */` |
|     ! 0 |  9525 | `				}` |
|      28 |  9526 | `				rc = SXERR_EOF;` |
|      28 |  9527 | `				break;` |
|       - |  9528 | `			}` |
|      23 |  9529 | `		}else{` |
|       - |  9530 | `			sxi32 nKwrd;` |
|       - |  9531 | `			/* Extract the keyword */` |
|     298 |  9532 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  9533 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  9534 | `				break;` |
|       - |  9535 | `			}` |
|     218 |  9536 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9537 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9538 | `					/* Unexpected token */` |
|     ! 0 |  9539 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9540 | `						&pGen->pIn->sData);` |
|     ! 0 |  9541 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9542 | `						return SXERR_ABORT;` |
|       - |  9543 | `					}` |
|       - |  9544 | `					/* FALL THROUGH */` |
|     ! 0 |  9545 | `				}` |
|       - |  9546 | `				/* Block compiled */` |
|       3 |  9547 | `				break;` |
|       - |  9548 | `			}` |
|       - |  9549 | `		}` |
|       - |  9550 | `		/* Compile block */` |
|     258 |  9551 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  9552 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9553 | `			return SXERR_ABORT;` |
|       - |  9554 | `		}` |
|       2 |  9555 | `	}` |
|     110 |  9556 | `	return rc;` |
|      56 |  9557 |  |
|       - |  9558 | `/*` |
|       - |  9559 | ` * Compile a case eXpression.` |
|       - |  9560 | ` *  (See block-comment below for more information)` |
|       - |  9561 | ` */` |
|      88 |  9562 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  9563 |  |
|       - |  9564 | `	SySet *pInstrContainer;` |
|       - |  9565 | `	SyToken *pEnd,*pTmp;` |
|      90 |  9566 | `	sxi32 iNest = 0;` |
|       - |  9567 | `	sxi32 rc;` |
|       - |  9568 | `	/* Delimit the expression */` |
|      90 |  9569 | `	pEnd = pGen->pIn;` |
|     186 |  9570 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  9571 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9572 | `			/* Increment nesting level */` |
|       3 |  9573 | `			iNest++;` |
|     185 |  9574 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9575 | `			/* Decrement nesting level */` |
|       3 |  9576 | `			iNest--;` |
|     183 |  9577 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  9578 | `			break;` |
|       - |  9579 | `		}` |
|      98 |  9580 | `		pEnd++;` |
|       2 |  9581 | `	}` |
|      90 |  9582 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9583 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9584 | `		if( rc == SXERR_ABORT ){` |
|       - |  9585 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9586 | `			return SXERR_ABORT;` |
|       - |  9587 | `		}` |
|     ! 0 |  9588 | `	}` |
|       - |  9589 | `	/* Swap token stream */` |
|      90 |  9590 | `	pTmp = pGen->pEnd;` |
|      90 |  9591 | `	pGen->pEnd = pEnd;` |
|      90 |  9592 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  9593 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9594 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9595 | `	/* Emit the done instruction */` |
|      90 |  9596 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9597 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9598 | `	/* Update token stream */` |
|      90 |  9599 | `	pGen->pIn  = pEnd;` |
|      90 |  9600 | `	pGen->pEnd = pTmp;` |
|      90 |  9601 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9602 | `		return SXERR_ABORT;` |
|       - |  9603 | `	}` |
|      90 |  9604 | `	return SXRET_OK;` |
|      46 |  9605 |  |
|       - |  9606 | `/*` |
|       - |  9607 | ` * Compile the smart switch statement.` |
|       - |  9608 | ` * According to the PHP language reference manual` |
|       - |  9609 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9610 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9611 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9612 | ` *  This is exactly what the switch statement is for.` |
|       - |  9613 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9614 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9615 | ` *  of the outer loop, use continue 2.` |
|       - |  9616 | ` *  Note that switch/case does loose comparision.` |
|       - |  9617 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9618 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9619 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9620 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9621 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9622 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9623 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9624 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9625 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9626 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9627 | ` *  list for the next case.` |
|       - |  9628 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9629 | ` *  or floating-point numbers and strings.` |
|       - |  9630 | ` */` |
|      28 |  9631 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9632 |  |
|       - |  9633 | `	GenBlock *pSwitchBlock;` |
|       - |  9634 | `	SyToken *pTmp,*pEnd;` |
|       - |  9635 | `	ph7_switch *pSwitch;` |
|       - |  9636 | `	sxu32 nToken;` |
|       - |  9637 | `	sxu32 nLine;` |
|       - |  9638 | `	sxi32 rc;` |
|      30 |  9639 | `	nLine = pGen->pIn->nLine;` |
|       - |  9640 | `	/* Jump the 'switch' keyword */` |
|      30 |  9641 | `	pGen->pIn++;` |
|      30 |  9642 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9643 | `		/* Syntax error */` |
|     ! 0 |  9644 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9645 | `		if( rc == SXERR_ABORT ){` |
|       - |  9646 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9647 | `			return SXERR_ABORT;` |
|       - |  9648 | `		}` |
|     ! 0 |  9649 | `		goto Synchronize;` |
|       - |  9650 | `	}` |
|       - |  9651 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9652 | `	pGen->pIn++;` |
|      30 |  9653 | `	pEnd = 0; /* cc warning */` |
|       - |  9654 | `	/* Create the loop block */` |
|      44 |  9655 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9656 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9657 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9658 | `		return SXERR_ABORT;` |
|       - |  9659 | `	}` |
|       - |  9660 | `	/* Delimit the condition */` |
|      30 |  9661 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9662 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9663 | `		/* Empty expression */` |
|     ! 0 |  9664 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9665 | `		if( rc == SXERR_ABORT ){` |
|       - |  9666 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9667 | `			return SXERR_ABORT;` |
|       - |  9668 | `		}` |
|     ! 0 |  9669 | `	}` |
|       - |  9670 | `	/* Swap token streams */` |
|      30 |  9671 | `	pTmp = pGen->pEnd;` |
|      30 |  9672 | `	pGen->pEnd = pEnd;` |
|       - |  9673 | `	/* Compile the expression */` |
|      30 |  9674 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9675 | `	if( rc == SXERR_ABORT ){` |
|       - |  9676 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9677 | `		return SXERR_ABORT;` |
|       - |  9678 | `	}` |
|       - |  9679 | `	/* Update token stream */` |
|      30 |  9680 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9681 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9682 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9683 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9684 | `			return SXERR_ABORT;` |
|       - |  9685 | `		}` |
|     ! 0 |  9686 | `		pGen->pIn++;` |
|     ! 0 |  9687 | `	}` |
|      30 |  9688 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9689 | `	pGen->pEnd = pTmp;` |
|      30 |  9690 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9691 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9692 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9693 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9694 | `				pTmp--;` |
|     ! 0 |  9695 | `			}` |
|       - |  9696 | `			/* Unexpected token */` |
|     ! 0 |  9697 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9698 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9699 | `				return SXERR_ABORT;` |
|       - |  9700 | `			}` |
|     ! 0 |  9701 | `			goto Synchronize;` |
|       - |  9702 | `	}` |
|       - |  9703 | `	/* Set the delimiter token */` |
|      30 |  9704 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9705 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9706 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9707 | `	}else{` |
|      28 |  9708 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9709 | `	}` |
|      30 |  9710 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9711 | `	/* Create the switch blocks container */` |
|      30 |  9712 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9713 | `	if( pSwitch == 0 ){` |
|       - |  9714 | `		/* Abort compilation */` |
|     ! 0 |  9715 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9716 | `		return SXERR_ABORT;` |
|       - |  9717 | `	}` |
|       - |  9718 | `	/* Zero the structure */` |
|      30 |  9719 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9720 | `	/* Initialize fields */` |
|      30 |  9721 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9722 | `	/* Emit the switch instruction */` |
|      30 |  9723 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9724 | `	/* Compile case blocks */` |
|      96 |  9725 | `	for(;;){` |
|       - |  9726 | `		sxu32 nKwrd;` |
|     112 |  9727 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9728 | `			/* No more input to process */` |
|     ! 0 |  9729 | `			break;` |
|       - |  9730 | `		}` |
|     112 |  9731 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9732 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9733 | `				/* Unexpected token */` |
|     ! 0 |  9734 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9735 | `					&pGen->pIn->sData);` |
|     ! 0 |  9736 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9737 | `					return SXERR_ABORT;` |
|       - |  9738 | `				}` |
|       - |  9739 | `				/* FALL THROUGH */` |
|     ! 0 |  9740 | `			}` |
|       - |  9741 | `			/* Block compiled */` |
|     ! 0 |  9742 | `			break;` |
|       - |  9743 | `		}` |
|       - |  9744 | `		/* Extract the keyword */` |
|     112 |  9745 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9746 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9747 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9748 | `				/* Unexpected token */` |
|     ! 0 |  9749 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9750 | `					&pGen->pIn->sData);` |
|     ! 0 |  9751 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9752 | `					return SXERR_ABORT;` |
|       - |  9753 | `				}` |
|       - |  9754 | `				/* FALL THROUGH */` |
|     ! 0 |  9755 | `			}` |
|       - |  9756 | `			/* Block compiled */` |
|       3 |  9757 | `			break;` |
|       - |  9758 | `		}` |
|     110 |  9759 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9760 | `			/*` |
|       - |  9761 | `			 * Accroding to the PHP language reference manual` |
|       - |  9762 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9763 | `			 *  that wasn't matched by the other cases.` |
|       - |  9764 | `			 */` |
|      22 |  9765 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9766 | `				/* Default case already compiled */` |
|     ! 0 |  9767 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9768 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9769 | `					return SXERR_ABORT;` |
|       - |  9770 | `				}` |
|     ! 0 |  9771 | `			}` |
|      22 |  9772 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9773 | `			/* Compile the default block */` |
|      22 |  9774 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9775 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9776 | `				return SXERR_ABORT;` |
|      22 |  9777 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9778 | `				break;` |
|       1 |  9779 | `			}` |
|      91 |  9780 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9781 | `			ph7_case_expr sCase;` |
|       - |  9782 | `			/* Standard case block */` |
|      90 |  9783 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9784 | `			/* initialize the structure */` |
|      90 |  9785 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9786 | `			/* Compile the case expression */` |
|      90 |  9787 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9788 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9789 | `				return SXERR_ABORT;` |
|       - |  9790 | `			}` |
|       - |  9791 | `			/* Compile the case block */` |
|      90 |  9792 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9793 | `			/* Insert in the switch container */` |
|      90 |  9794 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9795 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9796 | `				return SXERR_ABORT;` |
|      90 |  9797 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9798 | `				break;` |
|       - |  9799 | `			}` |
|      42 |  9800 | `		}else{` |
|       - |  9801 | `			/* Unexpected token */` |
|     ! 0 |  9802 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9803 | `				&pGen->pIn->sData);` |
|     ! 0 |  9804 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9805 | `				return SXERR_ABORT;` |
|       - |  9806 | `			}` |
|     ! 0 |  9807 | `			break;` |
|       - |  9808 | `		}` |
|       2 |  9809 | `	}` |
|       - |  9810 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9811 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9812 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9813 | `	/* Release the loop block */` |
|      30 |  9814 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9815 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9816 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9817 | `		pGen->pIn++;` |
|      14 |  9818 | `	}` |
|       - |  9819 | `	/* Statement successfully compiled */` |
|      30 |  9820 | `	return SXRET_OK;` |
|     ! 0 |  9821 | `Synchronize:` |
|       - |  9822 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9823 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9824 | `		pGen->pIn++;` |
|     ! 0 |  9825 | `	}` |
|     ! 0 |  9826 | `	return SXRET_OK;` |
|      16 |  9827 |  |
|       - |  9828 | `/*` |
|       - |  9829 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9830 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9831 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9832 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9833 | ` */` |
|       - |  9834 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9835 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9836 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9837 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9838 |  |
|       - |  9839 | `/*` |
|       - |  9840 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9841 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9842 | ` * patched entries from the pending set.` |
|       - |  9843 | ` */` |
| 2195822 |  9844 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       2 |  9845 |  |
| 2195824 |  9846 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9847 | `	sxu32 nTarget;` |
|       - |  9848 | `	sxu32 *aIdx;` |
|       - |  9849 | `	sxu32 i;` |
| 2195824 |  9850 | `	if( nCur <= nBaseline ){` |
| 2195734 |  9851 | `		return;` |
|       - |  9852 | `	}` |
|      92 |  9853 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      92 |  9854 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     190 |  9855 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     100 |  9856 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     100 |  9857 | `		if( pInstr ){` |
|     100 |  9858 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9859 | `		}` |
|      51 |  9860 | `	}` |
|      92 |  9861 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1097913 |  9862 |  |
|       - |  9863 |  |
|       - |  9864 | `/*` |
|       - |  9865 | ` * Generate bytecode for a given expression tree.` |
|       - |  9866 | ` * If something goes wrong while generating bytecode` |
|       - |  9867 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9868 | ` * this function takes care of generating the appropriate` |
|       - |  9869 | ` * error message.` |
|       - |  9870 | ` */` |
| 2958192 |  9871 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9872 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9873 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9874 | `	sxi32 iFlags /* Control flags */` |
|       - |  9875 | `	)` |
|       2 |  9876 |  |
|       - |  9877 | `	VmInstr *pInstr;` |
|       - |  9878 | `	sxu32 nJmpIdx;` |
| 2958194 |  9879 | `	sxi32 iP1 = 0;` |
| 2958194 |  9880 | `	sxu32 iP2 = 0;` |
| 2958194 |  9881 | `	void *p3  = 0;` |
|       - |  9882 | `	sxi32 iVmOp;` |
|       - |  9883 | `	sxi32 rc;` |
| 2958194 |  9884 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 2958194 |  9885 | `	sxu32 nRhsNsBase = 0;` |
| 2958194 |  9886 | `	if( pNode->xCode ){` |
|       - |  9887 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9888 | `		/* Compile node */` |
| 1832326 |  9889 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1832326 |  9890 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1832326 |  9891 | `		RE_SWAP_DELIMITER(pGen);` |
| 1832326 |  9892 | `		return rc;` |
|       - |  9893 | `	}` |
| 1125870 |  9894 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9895 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9896 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9897 | `		return SXERR_ABORT;` |
|       - |  9898 | `	}` |
| 1125870 |  9899 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1125870 |  9900 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9901 | `		sxu32 nJmp = 0;` |
|       - |  9902 | `		sxu32 nNcNsBase;` |
|       - |  9903 | `		VmInstr *pInstrFix;` |
|       - |  9904 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9905 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9906 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9907 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9908 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9909 | `		if( pNode->pRight ){` |
|      47 |  9910 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9911 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9912 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9913 | `				return rc;` |
|       - |  9914 | `			}` |
|      47 |  9915 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - |  9916 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9917 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9918 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9919 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9920 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9921 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9922 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9923 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9924 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9925 | `				pInstrFix->iP2 = 3;` |
|       9 |  9926 | `			}` |
|      23 |  9927 | `		}` |
|       - |  9928 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9929 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9930 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9931 | `		if( pNode->pLeft ){` |
|      47 |  9932 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9933 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9934 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9935 | `				return rc;` |
|       - |  9936 | `			}` |
|      47 |  9937 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      23 |  9938 | `		}` |
|       - |  9939 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9940 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9941 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9942 | `		if( nJmp > 0 ){` |
|      47 |  9943 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9944 | `			if( pInstrFix ){` |
|      47 |  9945 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9946 | `			}` |
|      23 |  9947 | `		}` |
|      47 |  9948 | `		return SXRET_OK;` |
|       - |  9949 | `	}` |
| 1125824 |  9950 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9951 | `		sxu32 nJz,nJmp;` |
|       - |  9952 | `		sxu32 nTernaryNsBase;` |
|       - |  9953 | `		/* Ternary operator require special handling */` |
|       - |  9954 | `		/* Phase#1: Compile the condition */` |
|    2292 |  9955 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2292 |  9956 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2292 |  9957 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9958 | `			return rc;` |
|       - |  9959 | `		}` |
|       - |  9960 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - |  9961 | `		 * compiling the condition must short-circuit to the end of the` |
|       - |  9962 | `		 * condition expression, not leak past the ternary. */` |
|    2292 |  9963 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2292 |  9964 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2292 |  9965 | `		if( pNode->pLeft ){` |
|       - |  9966 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9967 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    2224 |  9968 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9969 | `			/* Phase#3: Compile the 'then' expression  */` |
|    2224 |  9970 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2224 |  9971 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    2224 |  9972 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9973 | `				return rc;` |
|       - |  9974 | `			}` |
|    2224 |  9975 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1113 |  9976 | `		}else{` |
|       - |  9977 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9978 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9979 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9980 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9981 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9982 | `		}` |
|       - |  9983 | `		/* Phase#4: Emit the unconditional jump */` |
|    2292 |  9984 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9985 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2292 |  9986 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2292 |  9987 | `		if( pInstr ){` |
|    2292 |  9988 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1145 |  9989 | `		}` |
|    2292 |  9990 | `		if( !pNode->pLeft ){` |
|       - |  9991 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9992 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9993 | `		}` |
|       - |  9994 | `		/* Phase#6: Compile the 'else' expression */` |
|    2292 |  9995 | `		if( pNode->pRight ){` |
|    2292 |  9996 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2292 |  9997 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2292 |  9998 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9999 | `				return rc;` |
|       - | 10000 | `			}` |
|    2292 | 10001 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1145 | 10002 | `		}` |
|    2292 | 10003 | `		if( nJmp > 0 ){` |
|       - | 10004 | `			/* Phase#7: Fix the unconditional jump */` |
|    2292 | 10005 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2292 | 10006 | `			if( pInstr ){` |
|    2292 | 10007 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1145 | 10008 | `			}` |
|    1145 | 10009 | `		}` |
|       - | 10010 | `		/* All done */` |
|    2292 | 10011 | `		return SXRET_OK;` |
|       - | 10012 | `	}` |
| 1123534 | 10013 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - | 10014 | `	/* Generate code for the left tree */` |
| 1123534 | 10015 | `	if( pNode->pLeft ){` |
| 1123496 | 10016 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1123496 | 10017 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - | 10018 | `			ph7_expr_node **apNode;` |
|  356770 | 10019 | `			int hasSpread = 0;` |
|  356770 | 10020 | `			int hasNamed = 0;` |
|       - | 10021 | `			sxi32 nArgs;` |
|       - | 10022 | `			sxi32 n;` |
|       - | 10023 | `			/* Recurse and generate bytecodes for function arguments */` |
|  356770 | 10024 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  356770 | 10025 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10026 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10027 | `			{` |
|  356770 | 10028 | `				int seenNamed = 0;` |
|  706658 | 10029 | `				for( n = 0; n < nArgs; ++n ){` |
|  349892 | 10030 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     186 | 10031 | `						seenNamed = 1;` |
|     186 | 10032 | `						hasNamed = 1;` |
|  349800 | 10033 | `					}else if( seenNamed && !(apNode[n]->iFlags & EXPR_NODE_SPREAD) ){` |
|       3 | 10034 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10035 | `							"Cannot use positional argument after named argument");` |
|       3 | 10036 | `						return SXERR_SYNTAX;` |
|       - | 10037 | `					}` |
|  174946 | 10038 | `				}` |
|       - | 10039 | `			}` |
|       - | 10040 | `			/* Read-only load */` |
|  356768 | 10041 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  706654 | 10042 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  349888 | 10043 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  349888 | 10044 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  349888 | 10045 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10046 | `					return rc;` |
|       - | 10047 | `				}` |
|       - | 10048 | `				/* Each argument is an independent nullsafe scope. */` |
|  349888 | 10049 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  349888 | 10050 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10051 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 | 10052 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 | 10053 | `					hasSpread = 1;` |
|       9 | 10054 | `				}` |
|  174945 | 10055 | `			}` |
|       - | 10056 | `			/* Total number of given arguments */` |
|  356768 | 10057 | `			iP1 = nArgs;` |
|  356768 | 10058 | `			iP2 = hasSpread;` |
|       - | 10059 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10060 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  356768 | 10061 | `			if( hasNamed ){` |
|     100 | 10062 | `				sxu32 nStrBytes = 0;` |
|       - | 10063 | `				char *zBuf;` |
|     296 | 10064 | `				for( n = 0; n < nArgs; ++n ){` |
|     198 | 10065 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     184 | 10066 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      91 | 10067 | `					}` |
|     100 | 10068 | `				}` |
|       - | 10069 | `				{` |
|     100 | 10070 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|     100 | 10071 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      98 | 10072 | `					&pGen->pVm->sAllocator, mapSize);` |
|     100 | 10073 | `				if( pMap ){` |
|     100 | 10074 | `					SyZero(pMap, mapSize);` |
|     100 | 10075 | `					pMap->bHasNamed = 1;` |
|     100 | 10076 | `					pMap->nTotal = (sxu32)nArgs;` |
|     100 | 10077 | `					pMap->aNames = (SyString *)&pMap[1];` |
|     100 | 10078 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     296 | 10079 | `					for( n = 0; n < nArgs; ++n ){` |
|     198 | 10080 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     184 | 10081 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     184 | 10082 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     184 | 10083 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     184 | 10084 | `							zBuf += nb;` |
|      91 | 10085 | `						}` |
|       - | 10086 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|     100 | 10087 | `					}` |
|     100 | 10088 | `					p3 = (void *)pMap;` |
|      49 | 10089 | `				}` |
|       - | 10090 | `				}` |
|      49 | 10091 | `			}` |
|       - | 10092 | `			/* Remove stale flags now */` |
|  356768 | 10093 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  178383 | 10094 | `		}` |
| 1123494 | 10095 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1123494 | 10096 | `		if( rc != SXRET_OK ){` |
|      31 | 10097 | `			return rc;` |
|       - | 10098 | `		}` |
| 1123464 | 10099 | `		if( !bIsChainOp ){` |
|       - | 10100 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10101 | `			 * target the end of that LHS chain, which is right here. */` |
|  525338 | 10102 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  262668 | 10103 | `		}` |
| 1123464 | 10104 | `		if( iVmOp == PH7_OP_CALL ){` |
|  356768 | 10105 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  356768 | 10106 | `			if( pInstr ){` |
|  356768 | 10107 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  355770 | 10108 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10109 | `					sxu32 nQual;` |
|  355770 | 10110 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10111 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10112 | `					 * so the later NEW handler (if any) can see it. */` |
|  355770 | 10113 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10114 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10115 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10116 | `					 * imports — class imports must NOT affect function` |
|       - | 10117 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10118 | `					 * before NEW; we store the original literal index in the` |
|       - | 10119 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10120 | `					 * the unqualified name and re-qualify with class imports. */` |
|  355770 | 10121 | `					if( bAbsolute ){` |
|      20 | 10122 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      11 | 10123 | `					}else{` |
|  355752 | 10124 | `						int fromImport = 0;` |
|  355752 | 10125 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  355752 | 10126 | `						pInstr->iP2 = (sxi32)nQual;` |
|  355752 | 10127 | `						if( nQual != nOrig ){` |
|       - | 10128 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10129 | `							 * NEW handler can recover the unqualified name. */` |
|      74 | 10130 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      74 | 10131 | `							if( !fromImport ){` |
|       - | 10132 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      64 | 10133 | `								if( p3 == 0 ){` |
|      64 | 10134 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10135 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      64 | 10136 | `									if( pMap ){` |
|      64 | 10137 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      64 | 10138 | `										p3 = (void *)pMap;` |
|      31 | 10139 | `									}` |
|      31 | 10140 | `								}` |
|      64 | 10141 | `								if( p3 ){` |
|      64 | 10142 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10143 | `								}` |
|      31 | 10144 | `							}` |
|      36 | 10145 | `						}` |
|       2 | 10146 | `					}` |
|  178884 | 10147 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10148 | `					/* Method call,flag that */` |
|     800 | 10149 | `					pInstr->iP2 = 1;` |
|     399 | 10150 | `				}` |
|  178385 | 10151 | `			}` |
|  945081 | 10152 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10153 | `			ph7_expr_node **apNode;` |
|       - | 10154 | `			sxi32 n;` |
|       - | 10155 | `			/* Recurse and generate bytecodes for array index */` |
|   77144 | 10156 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  139186 | 10157 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   62044 | 10158 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   62044 | 10159 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   62044 | 10160 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10161 | `					return rc;` |
|       - | 10162 | `				}` |
|       - | 10163 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   62044 | 10164 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   31023 | 10165 | `			}` |
|   77144 | 10166 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   62044 | 10167 | `				iP1 = 1; /* Node have an index associated with it */` |
|   31021 | 10168 | `			}` |
|   77144 | 10169 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10170 | `				/* Create an empty entry when the desired index is not found */` |
|   30472 | 10171 | `				iP2 = 1;` |
|   15237 | 10172 | `			}` |
|  728127 | 10173 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10174 | `			/* POP the left node */` |
|      32 | 10175 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10176 | `		}` |
|  561731 | 10177 | `	}` |
| 1123502 | 10178 | `	rc = SXRET_OK;` |
| 1123502 | 10179 | `	nJmpIdx = 0;` |
|       - | 10180 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10181 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10182 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1123502 | 10183 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     270 | 10184 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     270 | 10185 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     270 | 10186 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     270 | 10187 | `			int isSpecial = 0;` |
|     270 | 10188 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     182 | 10189 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     182 | 10190 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     195 | 10191 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     160 | 10192 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      83 | 10193 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      90 | 10194 | `					isSpecial = 1;` |
|      44 | 10195 | `				}` |
|     112 | 10196 | `			}` |
|     314 | 10197 | `			pInstr->iP1 = 0;` |
|     314 | 10198 | `			if( !isSpecial ){` |
|     138 | 10199 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10200 | `			}` |
|       - | 10201 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10202 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     226 | 10203 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     138 | 10204 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     138 | 10205 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10206 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 10207 | `					return SXRET_OK;` |
|       - | 10208 | `				}` |
|      47 | 10209 | `			}` |
|      91 | 10210 | `		}` |
|     167 | 10211 | `	}` |
|       - | 10212 | `	/* Generate code for the right tree */` |
| 1123424 | 10213 | `	if( pNode->pRight ){` |
|  620772 | 10214 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10215 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9454 | 10216 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  616046 | 10217 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10218 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3168 | 10219 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  609737 | 10220 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10221 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      84 | 10222 | `			iVmOp = 0; /* No binary operator to emit */` |
|      84 | 10223 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  608162 | 10224 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10225 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10226 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10227 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10228 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10229 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10230 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     100 | 10231 | `			sxu32 nNsJmp = 0;` |
|     100 | 10232 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     100 | 10233 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  608023 | 10234 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  252044 | 10235 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  126021 | 10236 | `		}` |
|  620772 | 10237 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  620772 | 10238 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  620772 | 10239 | `		if( !bIsChainOp ){` |
|       - | 10240 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10241 | `			 * operator instruction is emitted. */` |
|  456596 | 10242 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  228297 | 10243 | `		}` |
|  620772 | 10244 | `		if( iVmOp == PH7_OP_STORE ){` |
|  248832 | 10245 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  248806 | 10246 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10247 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10248 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10249 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10250 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10251 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10252 | `				 */` |
|      54 | 10253 | `				iVmOp = 0;` |
|  248806 | 10254 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  248780 | 10255 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10256 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   69440 | 10257 | `					iP2 = 1;` |
|   34721 | 10258 | `				}else{` |
|  179342 | 10259 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10260 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   30406 | 10261 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   30406 | 10262 | `						iP1 = pInstr->iP1;` |
|   15204 | 10263 | `					}else{` |
|  148938 | 10264 | `						p3 = pInstr->p3;` |
|       - | 10265 | `					}` |
|       - | 10266 | `					/* POP the last dynamic load instruction */` |
|  179342 | 10267 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10268 | `				}` |
|  124391 | 10269 | `			}` |
|  496357 | 10270 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 10271 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 10272 | `			if( pInstr ){` |
|      48 | 10273 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10274 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10275 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10276 | `					 */` |
|      15 | 10277 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10278 | `					iP1 = pInstr->iP1;` |
|      15 | 10279 | `					iP2 = pInstr->iP2;` |
|      15 | 10280 | `					p3  = pInstr->p3;` |
|       8 | 10281 | `				}else{` |
|      34 | 10282 | `					p3 = pInstr->p3;` |
|       - | 10283 | `				}` |
|      23 | 10284 | `			}` |
|      23 | 10285 | `		}` |
|  310385 | 10286 | `	}` |
| 1123424 | 10287 | `	if( iVmOp > 0 ){` |
| 1123260 | 10288 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   12354 | 10289 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10290 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    9032 | 10291 | `				iP1 = 1;` |
|    4517 | 10292 | `			}` |
| 1117084 | 10293 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10294 | `			/* Namespace-qualify the class name for NEW */ {` |
|   15902 | 10295 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   15902 | 10296 | `				VmInstr *pCallInstr = 0;` |
|   15902 | 10297 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   15884 | 10298 | `					pCallInstr = pPeek;` |
|   15884 | 10299 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7941 | 10300 | `				}` |
|   15902 | 10301 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   15900 | 10302 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10303 | `					sxu32 nLitForClass;` |
|       - | 10304 | `					/* If the CALL handler already qualified the name using` |
|       - | 10305 | `					 * function imports, recover the original unqualified` |
|       - | 10306 | `					 * literal so we can re-qualify with class imports. */` |
|   15900 | 10307 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 | 10308 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 | 10309 | `					}else{` |
|   15868 | 10310 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10311 | `					}` |
|   15900 | 10312 | `					pPeek->iP1 = 0;` |
|   15900 | 10313 | `					if( !bAbsolute ){` |
|   15884 | 10314 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7943 | 10315 | `					}else{` |
|      18 | 10316 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10317 | `					}` |
|    7949 | 10318 | `				}` |
|       - | 10319 | `			}` |
|   15902 | 10320 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   15902 | 10321 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10322 | `				VmInstr *pPrev;` |
|   15884 | 10323 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   15884 | 10324 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10325 | `					/* Pop the call instruction, preserve named-arg map */` |
|   15884 | 10326 | `					iP1 = pInstr->iP1;` |
|   15884 | 10327 | `					if( pInstr->p3 ){` |
|      38 | 10328 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      18 | 10329 | `					}` |
|   15884 | 10330 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7941 | 10331 | `				}` |
|    7943 | 10332 | `			}` |
| 1102958 | 10333 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10334 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10335 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|      88 | 10336 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      88 | 10337 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      88 | 10338 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      88 | 10339 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|      88 | 10340 | `				int isSpecialIs = 0;` |
|      88 | 10341 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      84 | 10342 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      84 | 10343 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      87 | 10344 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      79 | 10345 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      42 | 10346 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 10347 | `						isSpecialIs = 1;` |
|       5 | 10348 | `					}` |
|      42 | 10349 | `				}` |
|      90 | 10350 | `				pInstr->iP1 = 0;` |
|      90 | 10351 | `				if( !isSpecialIs && !bAbsolute ){` |
|      68 | 10352 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      33 | 10353 | `				}` |
|      44 | 10354 | `			}` |
| 1094968 | 10355 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10356 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10357 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10358 | `			 * should not trigger constant lookup. */` |
|  164178 | 10359 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  164178 | 10360 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  164140 | 10361 | `				pInstr->iP1 = 0;` |
|   82069 | 10362 | `			}` |
|  164178 | 10363 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10364 | `				/* Static member access,remember that */` |
|     192 | 10365 | `				iP1 = 1;` |
|     192 | 10366 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     192 | 10367 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      32 | 10368 | `					p3 = pInstr->p3;` |
|      32 | 10369 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      15 | 10370 | `				}` |
|      95 | 10371 | `			}` |
|   82088 | 10372 | `		}` |
|       - | 10373 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 10374 | `		 * This is the primary emit path for user-visible calls. */` |
| 1123258 | 10375 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  372668 | 10376 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  186333 | 10377 | `		}` |
|       - | 10378 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1123258 | 10379 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  561628 | 10380 | `	}` |
| 1123422 | 10381 | `	if( nJmpIdx > 0 ){` |
|       - | 10382 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   12702 | 10383 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   12702 | 10384 | `		if( pInstr ){` |
|   12702 | 10385 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6350 | 10386 | `		}` |
|    6350 | 10387 | `	}` |
| 1123422 | 10388 | `	return rc;` |
| 1479079 | 10389 |  |
|       - | 10390 | `/*` |
|       - | 10391 | ` * Compile a PHP expression.` |
|       - | 10392 | ` * According to the PHP language reference manual:` |
|       - | 10393 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10394 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10395 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10396 | ` *  is "anything that has a value".` |
|       - | 10397 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10398 | ` * function takes care of generating the appropriate error` |
|       - | 10399 | ` * message.` |
|       - | 10400 | ` */` |
|  795262 | 10401 | `static sxi32 PH7_CompileExpr(` |
|       - | 10402 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10403 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10404 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10405 | `	)` |
|       2 | 10406 |  |
|       - | 10407 | `	ph7_expr_node *pRoot;` |
|       - | 10408 | `	SySet sExprNode;` |
|       - | 10409 | `	SyToken *pEnd;` |
|       - | 10410 | `	sxi32 nExpr;` |
|       - | 10411 | `	sxi32 iNest;` |
|       - | 10412 | `	sxi32 rc;` |
|       - | 10413 | `	sxu32 nNullsafeBase;` |
|       - | 10414 | `	/* Initialize worker variables */` |
|  795264 | 10415 | `	nExpr = 0;` |
|  795264 | 10416 | `	pRoot = 0;` |
|       - | 10417 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10418 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  795264 | 10419 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  795264 | 10420 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  795264 | 10421 | `	SySetAlloc(&sExprNode,0x10);` |
|  795264 | 10422 | `	rc = SXRET_OK;` |
|       - | 10423 | `	/* Delimit the expression */` |
|  795264 | 10424 | `	pEnd = pGen->pIn;` |
|  795264 | 10425 | `	iNest = 0;` |
| 5322342 | 10426 | `	while( pEnd < pGen->pEnd ){` |
| 5051456 | 10427 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10428 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     352 | 10429 | `			iNest++;` |
| 5051281 | 10430 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     360 | 10431 | `			iNest--;` |
| 5050927 | 10432 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  524616 | 10433 | `			if( iNest <= 0 ){` |
|  524378 | 10434 | `				break;` |
|       - | 10435 | `			}` |
|     119 | 10436 | `		}` |
| 4527080 | 10437 | `		pEnd++;` |
|       2 | 10438 | `	}` |
|  795264 | 10439 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   18400 | 10440 | `		SyToken *pEnd2 = pGen->pIn;` |
|   18400 | 10441 | `		iNest = 0;` |
|       - | 10442 | `		/* Stop at the first comma */` |
|   36848 | 10443 | `		while( pEnd2 < pEnd ){` |
|   18454 | 10444 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      16 | 10445 | `				iNest++;` |
|   18447 | 10446 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      16 | 10447 | `				iNest--;` |
|   18433 | 10448 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      13 | 10449 | `				if( iNest <= 0 ){` |
|       5 | 10450 | `					break;` |
|       - | 10451 | `				}` |
|       4 | 10452 | `			}` |
|   18450 | 10453 | `			pEnd2++;` |
|       2 | 10454 | `		}` |
|   18400 | 10455 | `		if( pEnd2 <pEnd ){` |
|       5 | 10456 | `			pEnd = pEnd2;` |
|       2 | 10457 | `		}` |
|    9199 | 10458 | `	}` |
|  795264 | 10459 | `	if( pEnd > pGen->pIn ){` |
|  795254 | 10460 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10461 | `		/* Swap delimiter */` |
|  795254 | 10462 | `		pGen->pEnd = pEnd;` |
|       - | 10463 | `		/* Try to get an expression tree */` |
|  795254 | 10464 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  795254 | 10465 | `		if( rc == SXRET_OK && pRoot ){` |
|  795072 | 10466 | `			rc = SXRET_OK;` |
|  795072 | 10467 | `			if( xTreeValidator ){` |
|       - | 10468 | `				/* Call the upper layer validator callback */` |
|   22368 | 10469 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   11183 | 10470 | `			}` |
|  795072 | 10471 | `			if( rc != SXERR_ABORT ){` |
|       - | 10472 | `				/* Generate code for the given tree */` |
|  795072 | 10473 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10474 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10475 | `				 * expression so they short-circuit to its end. */` |
|  795072 | 10476 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  397535 | 10477 | `			}` |
|  795072 | 10478 | `			nExpr = 1;` |
|  397535 | 10479 | `		}` |
|       - | 10480 | `		/* Release the whole tree */` |
|  795254 | 10481 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10482 | `		/* Synchronize token stream */` |
|  795254 | 10483 | `		pGen->pEnd = pTmp;` |
|  795254 | 10484 | `		pGen->pIn  = pEnd;` |
|  795254 | 10485 | `		if( rc == SXERR_ABORT ){` |
|      11 | 10486 | `			SySetRelease(&sExprNode);` |
|      11 | 10487 | `			return SXERR_ABORT;` |
|       - | 10488 | `		}` |
|  397621 | 10489 | `	}` |
|  795254 | 10490 | `	SySetRelease(&sExprNode);` |
|  795254 | 10491 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  397633 | 10492 |  |
|       - | 10493 | `/*` |
|       - | 10494 | ` * Return a pointer to the node construct handler associated` |
|       - | 10495 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10496 | ` */` |
|  201634 | 10497 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 10498 |  |
|  201636 | 10499 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10500 | `		/* Numeric literal: Either real or integer */` |
|  106222 | 10501 | `		return PH7_CompileNumLiteral;` |
|   95416 | 10502 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10503 | `		/* Double quoted string */` |
|   18868 | 10504 | `		return PH7_CompileString;` |
|   76550 | 10505 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10506 | `		/* Single quoted string */` |
|   76438 | 10507 | `		return PH7_CompileSimpleString;` |
|     114 | 10508 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10509 | `		/* Heredoc */` |
|      66 | 10510 | `		return PH7_CompileHereDoc;` |
|      50 | 10511 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10512 | `		/* Nowdoc */` |
|      44 | 10513 | `		return PH7_CompileNowDoc;` |
|       7 | 10514 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10515 | `		/* Backtick quoted string */` |
|       5 | 10516 | `		return PH7_CompileBacktic;` |
|       - | 10517 | `	}` |
|       3 | 10518 | `	return 0;` |
|  100819 | 10519 |  |
|       - | 10520 | `/*` |
|       - | 10521 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10522 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10523 | ` * in write context" parse error.` |
|       - | 10524 | ` */` |
|    6582 | 10525 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       2 | 10526 |  |
|       - | 10527 | `	sxi32 rc;` |
|    6584 | 10528 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6582 | 10529 | `		return SXRET_OK;` |
|       - | 10530 | `	}` |
|       5 | 10531 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10532 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10533 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10534 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3293 | 10535 |  |
|       - | 10536 | `/*` |
|       - | 10537 | ` * Compile an unset() statement.` |
|       - | 10538 | ` * unset($var, $arr[$key], ...);` |
|       - | 10539 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10540 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10541 | ` * parent array before extracting the element to unset.` |
|       - | 10542 | ` */` |
|    2842 | 10543 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 10544 |  |
|    2844 | 10545 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2844 | 10546 | `	sxu32 nIdx = 0;` |
|       - | 10547 | `	SyString sName;` |
|       - | 10548 | `	sxi32 rc;` |
|       - | 10549 | `	/* Jump the 'unset' keyword */` |
|    2844 | 10550 | `	pGen->pIn++;` |
|       - | 10551 | `	/* Save delimiter */` |
|    2844 | 10552 | `	pTmp = pGen->pEnd;` |
|       - | 10553 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2844 | 10554 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2844 | 10555 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10556 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10557 | `		SyToken *pClose;` |
|    2844 | 10558 | `		pGen->pIn++;   /* Skip '(' */` |
|    2844 | 10559 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2844 | 10560 | `		pEnd = pClose; /* Stop at ')' */` |
|    1421 | 10561 | `	}` |
|    2844 | 10562 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10563 | `	/* Resolve the 'unset' builtin name once */` |
|    2844 | 10564 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     340 | 10565 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     340 | 10566 | `		if( pObj == 0 ){` |
|     ! 0 | 10567 | `			return SXERR_ABORT;` |
|       - | 10568 | `		}` |
|     340 | 10569 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     340 | 10570 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     169 | 10571 | `	}` |
|       - | 10572 | `	/* Compile each comma-separated argument */` |
|    9428 | 10573 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6586 | 10574 | `		if( pGen->pIn < pNext ){` |
|    6586 | 10575 | `			pGen->pEnd = pNext;` |
|    6586 | 10576 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10577 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,` |
|       - | 10578 | `				GenStateUnsetValidator);` |
|    6586 | 10579 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10580 | `				return SXERR_ABORT;` |
|       - | 10581 | `			}` |
|    6586 | 10582 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10583 | `				/* Emit call for this single argument */` |
|    6584 | 10584 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6584 | 10585 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6584 | 10586 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3291 | 10587 | `			}` |
|    3292 | 10588 | `		}` |
|       - | 10589 | `		/* Jump trailing commas */` |
|   10330 | 10590 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3746 | 10591 | `			pNext++;` |
|       2 | 10592 | `		}` |
|    6586 | 10593 | `		pGen->pIn = pNext;` |
|       2 | 10594 | `	}` |
|       - | 10595 | `	/* Skip past the closing ')' if present */` |
|    2844 | 10596 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2844 | 10597 | `		pGen->pIn++;` |
|    1421 | 10598 | `	}` |
|       - | 10599 | `	/* Restore token stream */` |
|    2844 | 10600 | `	pGen->pEnd = pTmp;` |
|    2844 | 10601 | `	return SXRET_OK;` |
|    1423 | 10602 |  |
|       - | 10603 | `/*` |
|       - | 10604 | ` * PHP Language construct table.` |
|       - | 10605 | ` */` |
|       - | 10606 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10607 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10608 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10609 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10610 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10611 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10612 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10613 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10614 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10615 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10616 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10617 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10618 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10619 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10620 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10621 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10622 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10623 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10624 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10625 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10626 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10627 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10628 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10629 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10630 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10631 | `};` |
|       - | 10632 | `/*` |
|       - | 10633 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10634 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10635 | ` */` |
|  476616 | 10636 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10637 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10638 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10639 | `	)` |
|       2 | 10640 |  |
|  476618 | 10641 | `	sxu32 n = 0;` |
| 2018849 | 10642 | `	for(;;){` |
| 4037700 | 10643 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   55170 | 10644 | `			break;` |
|       - | 10645 | `		}` |
| 3982532 | 10646 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  421450 | 10647 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10648 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10649 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10650 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10651 | `					return 0;` |
|       - | 10652 | `				}` |
|     ! 0 | 10653 | `			}` |
|  421448 | 10654 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 10655 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 10656 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10657 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10658 | `				return 0;` |
|       - | 10659 | `			}` |
|       - | 10660 | `			/* Return a pointer to the handler.` |
|       - | 10661 | `			*/` |
|  421450 | 10662 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10663 | `		}` |
| 3561084 | 10664 | `		n++;` |
|       2 | 10665 | `	}` |
|   55170 | 10666 | `	if( pLookahed ){` |
|   55170 | 10667 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   12068 | 10668 | `			return PH7_CompileClassInterface;` |
|   43104 | 10669 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   42890 | 10670 | `			return PH7_CompileClass;` |
|     216 | 10671 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 | 10672 | `			return PH7_CompileTrait;` |
|     160 | 10673 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      21 | 10674 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      20 | 10675 | `				return PH7_CompileAbstractClass;` |
|     142 | 10676 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 10677 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10678 | `				return PH7_CompileFinalClass;` |
|       - | 10679 | `		}` |
|      70 | 10680 | `	}` |
|       - | 10681 | `	/* Not a language construct */` |
|     142 | 10682 | `	return 0;` |
|  238310 | 10683 |  |
|       - | 10684 | `/*` |
|       - | 10685 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10686 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10687 | ` */` |
|     140 | 10688 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 10689 |  |
|       - | 10690 | `	int rc;` |
|     142 | 10691 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     142 | 10692 | `	if( rc == FALSE ){` |
|      44 | 10693 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 | 10694 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10695 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10696 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10697 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10698 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10699 | `			*/` |
|       - | 10700 | `			){` |
|      38 | 10701 | `				rc = TRUE;` |
|      18 | 10702 | `		}` |
|      22 | 10703 | `	}` |
|     142 | 10704 | `	return rc;` |
|       2 | 10705 |  |
|       - | 10706 | `/*` |
|       - | 10707 | ` * Compile a PHP chunk.` |
|       - | 10708 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10709 | ` * takes care of generating the appropriate error message.` |
|       - | 10710 | ` */` |
|  643068 | 10711 | `static sxi32 GenStateCompileChunk(` |
|       - | 10712 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10713 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10714 | `	)` |
|       2 | 10715 |  |
|       - | 10716 | `	ProcLangConstruct xCons;` |
|       - | 10717 | `	sxi32 rc;` |
|  643070 | 10718 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  441200 | 10719 | `	for(;;){` |
|  762736 | 10720 | `		int bStmtIsDeclare = 0;` |
|  762736 | 10721 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10722 | `			/* No more input to process */` |
|   12832 | 10723 | `			break;` |
|       - | 10724 | `		}` |
|       - | 10725 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 10726 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  749906 | 10727 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  476618 | 10728 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  476618 | 10729 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      40 | 10730 | `				bStmtIsDeclare = 1;` |
|      19 | 10731 | `			}` |
|  238308 | 10732 | `		}` |
|  749906 | 10733 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 10734 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 10735 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  119640 | 10736 | `			pGen->bStrictTypesLocked = 1;` |
|   59819 | 10737 | `		}` |
|  749906 | 10738 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10739 | `			/* Compile block */` |
|      18 | 10740 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      18 | 10741 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10742 | `				break;` |
|       - | 10743 | `			}` |
|      10 | 10744 | `		}else{` |
|  749890 | 10745 | `			xCons = 0;` |
|  749890 | 10746 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  476618 | 10747 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10748 | `				/* Try to extract a language construct handler */` |
|  476618 | 10749 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  476618 | 10750 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10751 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10752 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10753 | `						&pGen->pIn->sData);` |
|       9 | 10754 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10755 | `						break;` |
|       - | 10756 | `					}` |
|       - | 10757 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10758 | `					 * this erroneous statement.` |
|       - | 10759 | `					 */` |
|       9 | 10760 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10761 | `				}` |
|  511582 | 10762 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   44774 | 10763 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10764 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10765 | `				xCons = PH7_CompileLabel;` |
|      56 | 10766 | `			}` |
|  749890 | 10767 | `			if( xCons == 0 ){` |
|       - | 10768 | `				/* Assume an expression an try to compile it */` |
|  273294 | 10769 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  273294 | 10770 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10771 | `					/* Pop l-value */` |
|  273144 | 10772 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  136571 | 10773 | `				}` |
|  136648 | 10774 | `			}else{` |
|       - | 10775 | `				/* Go compile the sucker */` |
|  476598 | 10776 | `				rc = xCons(&(*pGen));` |
|       - | 10777 | `			}` |
|  749890 | 10778 | `			if( rc == SXERR_ABORT ){` |
|       - | 10779 | `				/* Request to abort compilation */` |
|      11 | 10780 | `				break;` |
|       - | 10781 | `			}` |
|       - | 10782 | `		}` |
|       - | 10783 | `		/* Ignore trailing semi-colons ';' */` |
| 1250560 | 10784 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  500666 | 10785 | `			pGen->pIn++;` |
|       2 | 10786 | `		}` |
|  749896 | 10787 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10788 | `			/* Compile a single statement and return */` |
|  630230 | 10789 | `			break;` |
|       - | 10790 | `		}` |
|       - | 10791 | `		/* LOOP ONE */` |
|       - | 10792 | `		/* LOOP TWO */` |
|       - | 10793 | `		/* LOOP THREE */` |
|       - | 10794 | `		/* LOOP FOUR */` |
|       2 | 10795 | `	}` |
|       - | 10796 | `	/* Return compilation status */` |
|  643070 | 10797 | `	return rc;` |
|       2 | 10798 |  |
|       - | 10799 | `/*` |
|       - | 10800 | ` * Compile a Raw PHP chunk.` |
|       - | 10801 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10802 | ` * takes care of generating the appropriate error message.` |
|       - | 10803 | ` */` |
|   12842 | 10804 | `static sxi32 PH7_CompilePHP(` |
|       - | 10805 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10806 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10807 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10808 | `	)` |
|       2 | 10809 |  |
|   12844 | 10810 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10811 | `	sxi32 rc;` |
|       - | 10812 | `	/* Reset the token set */` |
|   12844 | 10813 | `	SySetReset(&(*pTokenSet));` |
|       - | 10814 | `	/* Mark as the default token set */` |
|   12844 | 10815 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10816 | `	/* Advance the stream cursor */` |
|   12844 | 10817 | `	pGen->pRawIn++;` |
|       - | 10818 | `	/* Tokenize the PHP chunk first */` |
|   12844 | 10819 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10820 | `	/* Point to the head and tail of the token stream. */` |
|   12844 | 10821 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   12844 | 10822 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   12844 | 10823 | `	if( is_expr ){` |
|     ! 0 | 10824 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10825 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10826 | `			/* A simple expression,compile it */` |
|     ! 0 | 10827 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10828 | `		}` |
|       - | 10829 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10830 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10831 | `		return SXRET_OK;` |
|       - | 10832 | `	}` |
|   12844 | 10833 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10834 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10835 | `		/*` |
|       - | 10836 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10837 | `		 * According to the PHP reference manual:` |
|       - | 10838 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10839 | `		 *  immediately follow` |
|       - | 10840 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10841 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10842 | `		 * Symisc extension:` |
|       - | 10843 | `		 *   This short syntax works with all PHP opening` |
|       - | 10844 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10845 | `		 *   only short tag.` |
|       - | 10846 | `		 */` |
|       - | 10847 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10848 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10849 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 10850 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 10851 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 10852 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 10853 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 10854 | `		}` |
|       3 | 10855 | `		return SXRET_OK;` |
|       - | 10856 | `	}` |
|       - | 10857 | `	/* Compile the PHP chunk */` |
|   12842 | 10858 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 10859 | `	/* Fix exceptions jumps */` |
|   12842 | 10860 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10861 | `	/* Fix gotos now, the jump destination is resolved */` |
|   12842 | 10862 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 10863 | `		rc = SXERR_ABORT;` |
|       1 | 10864 | `	}` |
|       - | 10865 | `	/* Reset container */` |
|   12842 | 10866 | `	SySetReset(&pGen->aGoto);` |
|   12842 | 10867 | `	SySetReset(&pGen->aLabel);` |
|   12842 | 10868 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 10869 | `	/* Compilation result */` |
|   12842 | 10870 | `	return rc;` |
|    6423 | 10871 |  |
|       - | 10872 | `/*` |
|       - | 10873 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 10874 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 10875 | ` * This is the only compile interface exported from this file.` |
|       - | 10876 | ` */` |
|   15344 | 10877 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 10878 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 10879 | `	SyString *pScript,  /* Script to compile */` |
|       - | 10880 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 10881 | `	)` |
|       2 | 10882 |  |
|       - | 10883 | `	SySet aPhpToken,aRawToken;` |
|       - | 10884 | `	ph7_gen_state *pCodeGen;` |
|       - | 10885 | `	ph7_value *pRawObj;` |
|       - | 10886 | `	sxu32 nObjIdx;` |
|       - | 10887 | `	sxi32 nRawObj;` |
|       - | 10888 | `	int is_expr;` |
|       - | 10889 | `	sxi8 bSavedStrict;` |
|       - | 10890 | `	sxi8 bSavedStrictLocked;` |
|       - | 10891 | `	sxi32 rc;` |
|   15346 | 10892 | `	if( pScript->nByte < 1 ){` |
|       - | 10893 | `		/* Nothing to compile */` |
|     ! 0 | 10894 | `		return PH7_OK;` |
|       - | 10895 | `	}` |
|       - | 10896 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 10897 | `	 * file's flags so include/require restore them on return. */` |
|   15346 | 10898 | `	pCodeGen = &pVm->sCodeGen;` |
|   15346 | 10899 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   15346 | 10900 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   15346 | 10901 | `	pCodeGen->bStrictTypes = 0;` |
|   15346 | 10902 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 10903 | `	/* Initialize the tokens containers */` |
|   15346 | 10904 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   15346 | 10905 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   15346 | 10906 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   15346 | 10907 | `	is_expr = 0;` |
|   15346 | 10908 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 10909 | `		SyToken sTmp;` |
|       - | 10910 | `		/* PHP only: -*/` |
|    3036 | 10911 | `		sTmp.nLine = 1;` |
|    3036 | 10912 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    3036 | 10913 | `		sTmp.pUserData = 0;` |
|    3036 | 10914 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    3036 | 10915 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    3036 | 10916 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 10917 | `			/* A simple PHP expression */` |
|     ! 0 | 10918 | `			is_expr = 1;` |
|     ! 0 | 10919 | `		}` |
|    1519 | 10920 | `	}else{` |
|       - | 10921 | `		/* Tokenize raw text */` |
|   12312 | 10922 | `		SySetAlloc(&aRawToken,32);` |
|   12312 | 10923 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 10924 | `	}` |
|       - | 10925 | `	/* Process high-level tokens */` |
|   15346 | 10926 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   15346 | 10927 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   15346 | 10928 | `	rc = PH7_OK;` |
|   15346 | 10929 | `	if( is_expr ){` |
|       - | 10930 | `		/* Compile the expression */` |
|     ! 0 | 10931 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 10932 | `		goto cleanup;` |
|       - | 10933 | `	}` |
|   15346 | 10934 | `	nObjIdx = 0;` |
|       - | 10935 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 10936 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 10937 | `	 * preventing namespace bleeding across include()d files. */` |
|   15346 | 10938 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 10939 | `	/* Start the compilation process */` |
|   13831 | 10940 | `	for(;;){` |
|   40494 | 10941 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   15334 | 10942 | `			break; /* No more tokens to process */` |
|       - | 10943 | `		}` |
|   25162 | 10944 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 10945 | `			/* Compile the PHP chunk */` |
|   12844 | 10946 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   12844 | 10947 | `			if( rc == SXERR_ABORT ){` |
|      13 | 10948 | `				break;` |
|       - | 10949 | `			}` |
|   12832 | 10950 | `			continue;` |
|       - | 10951 | `		}` |
|       - | 10952 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   12320 | 10953 | `		nRawObj = 0;` |
|   24638 | 10954 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 10955 | `			/* Consume the raw chunk without any processing */` |
|   12320 | 10956 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   12320 | 10957 | `			if( pRawObj == 0 ){` |
|     ! 0 | 10958 | `				rc = SXERR_MEM;` |
|     ! 0 | 10959 | `				break;` |
|       - | 10960 | `			}` |
|       - | 10961 | `			/* Mark as constant and emit the load constant instruction */` |
|   12320 | 10962 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   12320 | 10963 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   12320 | 10964 | `			++nRawObj;` |
|   12320 | 10965 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 10966 | `		}` |
|   12320 | 10967 | `		if( nRawObj > 0 ){` |
|       - | 10968 | `			/* Emit the consume instruction */` |
|   12320 | 10969 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    6159 | 10970 | `		}` |
|    7674 | 10971 | `	}` |
|    7672 | 10972 | `cleanup:` |
|   15346 | 10973 | `	SySetRelease(&aRawToken);` |
|   15346 | 10974 | `	SySetRelease(&aPhpToken);` |
|       - | 10975 | `	/* Restore outer file's strict_types scope */` |
|   15346 | 10976 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   15346 | 10977 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   15346 | 10978 | `	return rc;` |
|    7674 | 10979 |  |
|       - | 10980 | `/*` |
|       - | 10981 | ` * Utility routines.Initialize the code generator.` |
|       - | 10982 | ` */` |
|    3006 | 10983 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 10984 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10985 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10986 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10987 | `	)` |
|       2 | 10988 |  |
|    3008 | 10989 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10990 | `	/* Zero the structure */` |
|    3008 | 10991 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 10992 | `	/* Initial state */` |
|    3008 | 10993 | `	pGen->pVm  = &(*pVm);` |
|    3008 | 10994 | `	pGen->xErr = xErr;` |
|    3008 | 10995 | `	pGen->pErrData = pErrData;` |
|    3008 | 10996 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    3008 | 10997 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    3008 | 10998 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    3008 | 10999 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    3008 | 11000 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 11001 | `	/* Error log buffer */` |
|    3008 | 11002 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 11003 | `	/* General purpose working buffer */` |
|    3008 | 11004 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 11005 | `	/* Namespace state */` |
|    3008 | 11006 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    3008 | 11007 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    3008 | 11008 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    3008 | 11009 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11010 | `	/* Create the global scope */` |
|    3008 | 11011 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 11012 | `	/* Point to the global scope */` |
|    3008 | 11013 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    3008 | 11014 | `	return SXRET_OK;` |
|       2 | 11015 |  |
|       - | 11016 | `/*` |
|       - | 11017 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 11018 | ` */` |
|   18036 | 11019 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 11020 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 11021 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 11022 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11023 | `	)` |
|       2 | 11024 |  |
|   18038 | 11025 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11026 | `	GenBlock *pBlock,*pParent;` |
|       - | 11027 | `	/* Reset state */` |
|   18038 | 11028 | `	SySetReset(&pGen->aLabel);` |
|   18038 | 11029 | `	SySetReset(&pGen->aGoto);` |
|   18038 | 11030 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   18038 | 11031 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   18038 | 11032 | `	SyBlobRelease(&pGen->sWorker);` |
|   18038 | 11033 | `	SyBlobRelease(&pGen->sNamespace);` |
|   18038 | 11034 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   18038 | 11035 | `	SyHashRelease(&pGen->hUseImports);` |
|   18038 | 11036 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   18038 | 11037 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   18038 | 11038 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   18038 | 11039 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   18038 | 11040 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11041 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11042 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11043 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11044 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11045 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11046 | `	 * number of unique names, which is acceptable. */` |
|       - | 11047 | `	/* Point to the global scope */` |
|   18038 | 11048 | `	pBlock = pGen->pCurrent;` |
|   18038 | 11049 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11050 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11051 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11052 | `		pBlock = pParent;` |
|     ! 0 | 11053 | `	}` |
|   18038 | 11054 | `	pGen->xErr = xErr;` |
|   18038 | 11055 | `	pGen->pErrData = pErrData;` |
|   18038 | 11056 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   18038 | 11057 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   18038 | 11058 | `	pGen->pIn = pGen->pEnd = 0;` |
|   18038 | 11059 | `	pGen->nErr = 0;` |
|   18038 | 11060 | `	return SXRET_OK;` |
|       2 | 11061 |  |
|       - | 11062 | `/*` |
|       - | 11063 | ` * Generate a compile-time error message.` |
|       - | 11064 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11065 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11066 | ` * abort compilation immediately.` |
|       - | 11067 | ` */` |
|     574 | 11068 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 11069 |  |
|     576 | 11070 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     576 | 11071 | `	const char *zErr = "Error";` |
|       - | 11072 | `	SyString *pFile;` |
|       - | 11073 | `	va_list ap;` |
|       - | 11074 | `	sxi32 rc;` |
|       - | 11075 | `	/* Reset the working buffer */` |
|     576 | 11076 | `	SyBlobReset(pWorker);` |
|       - | 11077 | `	/* Peek the processed file path if available */` |
|     576 | 11078 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     576 | 11079 | `	if( nErrType == E_ERROR ){` |
|       - | 11080 | `		/* Increment the error counter */` |
|     470 | 11081 | `		pGen->nErr++;` |
|     470 | 11082 | `		if( pGen->nErr > 15 ){` |
|       - | 11083 | `			/* Error count limit reached */` |
|       5 | 11084 | `			if( pGen->xErr ){` |
|       5 | 11085 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11086 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11087 | `				if( pFile ){` |
|       5 | 11088 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11089 | `				}` |
|       5 | 11090 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11091 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11092 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11093 | `				}` |
|       2 | 11094 | `			}` |
|       - | 11095 | `			/* Abort immediately */` |
|       5 | 11096 | `			return SXERR_ABORT;` |
|       - | 11097 | `		}` |
|     232 | 11098 | `	}` |
|     572 | 11099 | `	if( pGen->xErr == 0 ){` |
|       - | 11100 | `		/* No available error consumer,return immediately */` |
|       3 | 11101 | `		return SXRET_OK;` |
|       - | 11102 | `	}` |
|     569 | 11103 | `	switch(nErrType){` |
|     463 | 11104 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 11105 | `	case E_WARNING: zErr = "Warning";     break;` |
|      73 | 11106 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 11107 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11108 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11109 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11110 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11111 | `	default:` |
|     ! 0 | 11112 | `		break;` |
|       - | 11113 | `	}` |
|     569 | 11114 | `	rc = SXRET_OK;` |
|       - | 11115 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     569 | 11116 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     569 | 11117 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     569 | 11118 | `	va_start(ap,zFormat);` |
|     569 | 11119 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     569 | 11120 | `	va_end(ap);` |
|     569 | 11121 | `	if( pFile ){` |
|     569 | 11122 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     284 | 11123 | `	}` |
|       - | 11124 | `	/* Append a new line */` |
|     569 | 11125 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     569 | 11126 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11127 | `		/* Consume the generated error message */` |
|     569 | 11128 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     284 | 11129 | `	}` |
|     569 | 11130 | `	return rc;` |
|     289 | 11131 |  |
|       - | 11132 |  |
