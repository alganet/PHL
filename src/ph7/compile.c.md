# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4980/6310 lines (78.92%)

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
|    3168 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3170 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    8939 |   131 | `	for(;;){` |
|   17880 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    3062 |   133 | `			iCount--; /* Decrement nesting level */` |
|    3062 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    3036 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   14846 |   140 | `		pBlock = pBlock->pParent;` |
|   14846 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1586 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  620056 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  620058 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  620058 |   162 | `	pBlock->pUserData   = pUserData;` |
|  620058 |   163 | `	pBlock->pGen        = pGen;` |
|  620058 |   164 | `	pBlock->iFlags      = iType;` |
|  620058 |   165 | `	pBlock->pParent     = 0;` |
|  620058 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  620058 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  620058 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  617154 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  617156 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  617156 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  617156 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  617156 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  617156 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  617156 |   200 | `	pGen->pCurrent = pBlock;` |
|  617156 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  298844 |   203 | `		*ppBlock = pBlock;` |
|  149421 |   204 | `	}` |
|  617156 |   205 | `	return SXRET_OK;` |
|  308579 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  617146 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  617148 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  617148 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  617148 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  617146 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  617148 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  617148 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  617148 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  617148 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  617146 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  617148 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  617148 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  617148 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  617148 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  617148 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  617148 |   244 | `	return SXRET_OK;` |
|  308575 |   245 |  |
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
|  187790 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  187792 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  187792 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  187792 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  187792 |   265 | `	return rc;` |
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
|  439066 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  439068 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  809142 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  370076 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  146670 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  223408 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   35620 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  187790 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  187790 |   298 | `		if( pInstr ){` |
|  187790 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  187790 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  187790 |   302 | `			aFix[n].nJumpType = -1;` |
|   93894 |   303 | `		}` |
|   93896 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  439068 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  167518 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  167520 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  167666 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  167518 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  167650 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  167518 |   358 | `	return SXRET_OK;` |
|   83761 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  545290 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  545292 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  545292 |   367 | `	if( pEntry == 0 ){` |
|  269068 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  276226 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  276226 |   371 | `	return SXRET_OK;` |
|  272647 |   372 |  |
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
|  269066 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  269068 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  269068 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  134533 |   387 | `	}` |
|  269068 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|   95758 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|   95760 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|   95760 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   95760 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|   95760 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|   95760 |   408 | `	return pObj;` |
|   47881 |   409 |  |
|       - |   410 | `/*` |
|       - |   411 | ` * Implementation of the PHP language constructs.` |
|       - |   412 | ` */` |
|       - |   413 | `/* Forward declaration */` |
|       - |   414 | `static sxi32 GenStateCompileChunk(ph7_gen_state *pGen,sxi32 iFlags);` |
|       - |   415 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx);` |
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
|   96282 |   470 | `static int GenStateFindBadNumericSeparator(` |
|       - |   471 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   472 |  |
|   96284 |   473 | `	const char *z = pRaw->zString;` |
|   96284 |   474 | `	sxu32 n = pRaw->nByte;` |
|   96284 |   475 | `	int base = 10;` |
|       - |   476 | `	sxu32 i, start;` |
|   96284 |   477 | `	if( n < 2 ) return 0;` |
|    8612 |   478 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   479 | `		base = 16;` |
|    8577 |   480 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   481 | `		base = 2;` |
|     139 |   482 | `	}` |
|   31790 |   483 | `	for( i = 0; i < n; ++i ){` |
|   23194 |   484 | `		if( z[i] != '_' ) continue;` |
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
|    8598 |   501 | `	return 0;` |
|   48143 |   502 |  |
|       - |   503 | `/*` |
|       - |   504 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   505 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   506 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   507 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   508 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   509 | ` * so callers can bail from the current construct).` |
|       - |   510 | ` */` |
|   96282 |   511 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   512 |  |
|   96284 |   513 | `	const char *zBad = 0;` |
|   96284 |   514 | `	sxu32 nBad = 0;` |
|       - |   515 | `	SyString sBad;` |
|       - |   516 | `	sxi32 rc;` |
|   96284 |   517 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   96270 |   518 | `		return SXRET_OK;` |
|       - |   519 | `	}` |
|      15 |   520 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   521 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   522 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   523 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   524 | `		return SXERR_ABORT;` |
|       - |   525 | `	}` |
|      15 |   526 | `	return SXERR_SYNTAX;` |
|   48143 |   527 |  |
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
|   96268 |   544 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   545 | `	SyMemBackend *pAlloc,` |
|       - |   546 | `	const SyString *pToken,` |
|       - |   547 | `	char *zScratch, sxu32 nScratch,` |
|       - |   548 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   549 |  |
|       - |   550 | `	sxu32 i, j;` |
|   96270 |   551 | `	int hasUnderscore = 0;` |
|       - |   552 | `	char *zBuf;` |
|   96270 |   553 | `	*pzAlloc = 0;` |
|  205054 |   554 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  109038 |   555 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   54394 |   556 | `	}` |
|   96270 |   557 | `	if( !hasUnderscore ){` |
|   96018 |   558 | `		SyStringDupPtr(pOut, pToken);` |
|   96018 |   559 | `		return SXRET_OK;` |
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
|   48136 |   576 |  |
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
|   96254 |   593 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   594 |  |
|   96256 |   595 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   96256 |   596 | `	sxu32 nIdx = 0;` |
|       - |   597 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   96256 |   598 | `	char *zAlloc = 0;` |
|       - |   599 | `	SyString sNum;` |
|       - |   600 | `	sxi32 rc;` |
|   48127 |   601 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   96256 |   602 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   96256 |   603 | `	if( rc != SXRET_OK ){` |
|      11 |   604 | `		return rc;` |
|       - |   605 | `	}` |
|  144368 |   606 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   48122 |   607 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   96246 |   608 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   609 | `		return SXERR_ABORT;` |
|       - |   610 | `	}` |
|   96246 |   611 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   612 | `		ph7_value *pObj;` |
|       - |   613 | `		sxi64 iValue;` |
|   95760 |   614 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   95760 |   615 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   95760 |   616 | `		if( pObj == 0 ){` |
|     ! 0 |   617 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   618 | `			return SXERR_ABORT;` |
|       - |   619 | `		}` |
|   95760 |   620 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   47881 |   621 | `	}else{` |
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
|   96246 |   634 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   635 | `	/* Emit the load constant instruction */` |
|   96246 |   636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   637 | `	/* Node successfully compiled */` |
|   96246 |   638 | `	return SXRET_OK;` |
|   48129 |   639 |  |
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
|   62106 |   651 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   652 |  |
|   62108 |   653 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   654 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   655 | `	ph7_value *pObj;` |
|       - |   656 | `	sxu32 nIdx;` |
|   62108 |   657 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   658 | `	/* Delimit the string */` |
|   62108 |   659 | `	zIn  = pStr->zString;` |
|   62108 |   660 | `	zEnd = &zIn[pStr->nByte];` |
|   62108 |   661 | `	if( zIn >= zEnd ){` |
|       - |   662 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   663 | `		 * rather than reserving a new object each time. */` |
|     144 |   664 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     144 |   665 | `		return SXRET_OK;` |
|       - |   666 | `	}` |
|   61966 |   667 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   668 | `		/* Already processed,emit the load constant instruction` |
|       - |   669 | `		 * and return.` |
|       - |   670 | `		 */` |
|   18164 |   671 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18164 |   672 | `		return SXRET_OK;` |
|       - |   673 | `	}` |
|       - |   674 | `	/* Reserve a new constant */` |
|   43804 |   675 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   43804 |   676 | `	if( pObj == 0 ){` |
|     ! 0 |   677 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   678 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   679 | `		return SXERR_ABORT;` |
|       - |   680 | `	}` |
|   43804 |   681 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   682 | `	/* Compile the node */` |
|   43844 |   683 | `	for(;;){` |
|   87690 |   684 | `		if( zIn >= zEnd ){` |
|       - |   685 | `			/* End of input */` |
|   43804 |   686 | `			break;` |
|       - |   687 | `		}` |
|   43888 |   688 | `		zCur = zIn;` |
|  697074 |   689 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  653188 |   690 | `			zIn++;` |
|       2 |   691 | `		}` |
|   43888 |   692 | `		if( zIn > zCur ){` |
|       - |   693 | `			/* Append raw contents*/` |
|   43868 |   694 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   21933 |   695 | `		}` |
|   43888 |   696 | `		zIn++;` |
|   43888 |   697 | `		if( zIn < zEnd ){` |
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
|   43888 |   712 | `		zIn++;` |
|       2 |   713 | `	}` |
|       - |   714 | `	/* Emit the load constant instruction */` |
|   43804 |   715 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   43804 |   716 | `	if( pStr->nByte < 1024 ){` |
|       - |   717 | `		/* Install in the literal table */` |
|   43804 |   718 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   21901 |   719 | `	}` |
|       - |   720 | `	/* Node successfully compiled */` |
|   43804 |   721 | `	return SXRET_OK;` |
|   31055 |   722 |  |
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
|    1940 |   888 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1942 |   899 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   900 | `	/* Preallocate some slots */` |
|    1942 |   901 | `	SySetAlloc(&sToken,0x08);` |
|       - |   902 | `	/* Tokenize the text */` |
|    1942 |   903 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   904 | `	/* Swap delimiter */` |
|    1942 |   905 | `	pTmpIn  = pGen->pIn;` |
|    1942 |   906 | `	pTmpEnd = pGen->pEnd;` |
|    1942 |   907 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1942 |   908 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   909 | `	/* Compile the expression */` |
|    1942 |   910 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   911 | `	/* Restore token stream */` |
|    1942 |   912 | `	pGen->pIn  = pTmpIn;` |
|    1942 |   913 | `	pGen->pEnd = pTmpEnd;` |
|       - |   914 | `	/* Release the token set */` |
|    1942 |   915 | `	SySetRelease(&sToken);` |
|       - |   916 | `	/* Compilation result */` |
|    1942 |   917 | `	return rc;` |
|       2 |   918 |  |
|       - |   919 | `/*` |
|       - |   920 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   921 | ` */` |
|   18394 |   922 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   923 |  |
|       - |   924 | `	ph7_value *pConstObj;` |
|   18396 |   925 | `	sxu32 nIdx = 0;` |
|       - |   926 | `	/* Reserve a new constant */` |
|   18396 |   927 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   18396 |   928 | `	if( pConstObj == 0 ){` |
|     ! 0 |   929 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   930 | `		return 0;` |
|       - |   931 | `	}` |
|   18396 |   932 | `	(*pCount)++;` |
|   18396 |   933 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   934 | `	/* Emit the load constant instruction */` |
|   18396 |   935 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18396 |   936 | `	return pConstObj;` |
|    9199 |   937 |  |
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
|   17004 |   976 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |   977 |  |
|   17006 |   978 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |   979 | `	const char *zIn,*zCur,*zEnd;` |
|   17006 |   980 | `	ph7_value *pObj = 0;` |
|       - |   981 | `	sxi32 iCons;` |
|       - |   982 | `	sxi32 rc;` |
|       - |   983 | `	/* Delimit the string */` |
|   17006 |   984 | `	zIn  = pStr->zString;` |
|   17006 |   985 | `	zEnd = &zIn[pStr->nByte];` |
|   17006 |   986 | `	if( zIn >= zEnd ){` |
|       - |   987 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |   988 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |   989 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |   990 | `		 */` |
|     234 |   991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     234 |   992 | `		return SXRET_OK;` |
|       - |   993 | `	}` |
|   16774 |   994 | `	zCur = 0;` |
|       - |   995 | `	/* Compile the node */` |
|   16774 |   996 | `	iCons = 0;` |
|    9356 |   997 | `	for(;;){` |
|   28294 |   998 | `		zCur = zIn;` |
|  144520 |   999 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  118168 |  1000 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      59 |  1001 | `				break;` |
|  118054 |  1002 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1828 |  1003 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     914 |  1004 | `					break;` |
|       - |  1005 | `			}` |
|  116228 |  1006 | `			zIn++;` |
|       2 |  1007 | `		}` |
|   28294 |  1008 | `		if( zIn > zCur ){` |
|   12818 |  1009 | `			if( pObj == 0 ){` |
|   12542 |  1010 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12542 |  1011 | `				if( pObj == 0 ){` |
|     ! 0 |  1012 | `					return SXERR_ABORT;` |
|       - |  1013 | `				}` |
|    6270 |  1014 | `			}` |
|   12818 |  1015 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6408 |  1016 | `		}` |
|   28294 |  1017 | `		if( zIn >= zEnd ){` |
|   16774 |  1018 | `			break;` |
|       - |  1019 | `		}` |
|   11522 |  1020 | `		if( zIn[0] == '\\' ){` |
|    9582 |  1021 | `			const char *zPtr = 0;` |
|       - |  1022 | `			sxu32 n;` |
|    9582 |  1023 | `			zIn++;` |
|    9582 |  1024 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1025 | `				break;` |
|       - |  1026 | `			}` |
|    9582 |  1027 | `			if( pObj == 0 ){` |
|    5856 |  1028 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5856 |  1029 | `				if( pObj == 0 ){` |
|     ! 0 |  1030 | `					return SXERR_ABORT;` |
|       - |  1031 | `				}` |
|    2927 |  1032 | `			}` |
|    9582 |  1033 | `			n = sizeof(char); /* size of conversion */` |
|    9582 |  1034 | `			switch( zIn[0] ){` |
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
|    4425 |  1055 | `			case 'n':` |
|       - |  1056 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8852 |  1057 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8852 |  1058 | `				break;` |
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
|    9582 |  1126 | `			zIn += n;` |
|    9582 |  1127 | `			continue;` |
|       - |  1128 | `		}` |
|    1942 |  1129 | `		if( zIn[0] == '{' ){` |
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
|    1826 |  1163 | `			const char *zExpr = zIn;` |
|       - |  1164 | `			/* Assemble variable name */` |
|     918 |  1165 | `			for(;;){` |
|       - |  1166 | `				/* Jump leading dollars */` |
|    3662 |  1167 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1826 |  1168 | `					zIn++;` |
|       2 |  1169 | `				}` |
|     918 |  1170 | `				for(;;){` |
|   10770 |  1171 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8016 |  1172 | `						zIn++;` |
|       2 |  1173 | `					}` |
|    1838 |  1174 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1175 | `						/* UTF-8 stream */` |
|     ! 0 |  1176 | `						zIn++;` |
|     ! 0 |  1177 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1178 | `							zIn++;` |
|     ! 0 |  1179 | `						}` |
|     ! 0 |  1180 | `						continue;` |
|       - |  1181 | `					}` |
|    1838 |  1182 | `					break;` |
|     ! 0 |  1183 | `				}` |
|    1838 |  1184 | `				if( zIn >= zEnd ){` |
|     112 |  1185 | `					break;` |
|       - |  1186 | `				}` |
|    1728 |  1187 | `				if( zIn[0] == '[' ){` |
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
|    1720 |  1205 | `				}else if(zIn[0] == '{' ){` |
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
|    1716 |  1223 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1224 | `					/* Member access operator '->' */` |
|      13 |  1225 | `					zIn += 2;` |
|    1710 |  1226 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1227 | `					/* Static member access operator '::' */` |
|     ! 0 |  1228 | `					zIn += 2;` |
|     ! 0 |  1229 | `				}else{` |
|     853 |  1230 | `					break;` |
|       - |  1231 | `				}` |
|       1 |  1232 | `			}` |
|       - |  1233 | `			/* Process the expression */` |
|    1826 |  1234 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1826 |  1235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1236 | `				return SXERR_ABORT;` |
|       - |  1237 | `			}` |
|    1826 |  1238 | `			if( rc != SXERR_EMPTY ){` |
|    1824 |  1239 | `				++iCons;` |
|     911 |  1240 | `			}` |
|       - |  1241 | `		}` |
|       - |  1242 | `		/* Invalidate the previously used constant */` |
|    1942 |  1243 | `		pObj = 0;` |
|       2 |  1244 | `	}/*for(;;)*/` |
|   16774 |  1245 | `	if( iCons > 1 ){` |
|       - |  1246 | `		/* Concatenate all compiled constants */` |
|    1438 |  1247 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     718 |  1248 | `	}` |
|       - |  1249 | `	/* Node successfully compiled */` |
|   16774 |  1250 | `	return SXRET_OK;` |
|    8504 |  1251 |  |
|       - |  1252 | `/*` |
|       - |  1253 | ` * Compile a double quoted string.` |
|       - |  1254 | ` *  See the block-comment above for more information.` |
|       - |  1255 | ` */` |
|   16944 |  1256 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1257 |  |
|       - |  1258 | `	sxi32 rc;` |
|   16946 |  1259 | `	rc = GenStateCompileString(&(*pGen));` |
|    8472 |  1260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1261 | `	/* Compilation result */` |
|   16946 |  1262 | `	return rc;` |
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
|   17018 |  1306 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   17020 |  1317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1318 | `	/* Compile the expression*/` |
|   17020 |  1319 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1320 | `	/* Restore token stream */` |
|   17020 |  1321 | `	RE_SWAP_DELIMITER(pGen);` |
|   17020 |  1322 | `	return rc;` |
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
|   25278 |  1361 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1362 |  |
|       - |  1363 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1364 | `	SyToken *pKey,*pCur;` |
|   25280 |  1365 | `	sxi32 iEmitRef = 0;` |
|   25280 |  1366 | `	sxi32 nPair = 0;` |
|       - |  1367 | `	sxi32 iNest;` |
|       - |  1368 | `	sxi32 rc;` |
|   25280 |  1369 | `	xValidator = 0;` |
|   20481 |  1370 | `	for(;;){` |
|       - |  1371 | `		/* Jump leading commas */` |
|   46234 |  1372 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5272 |  1373 | `			pGen->pIn++;` |
|       2 |  1374 | `		}` |
|   40964 |  1375 | `		pCur = pGen->pIn;` |
|   40964 |  1376 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1377 | `			/* No more entry to process */` |
|   25268 |  1378 | `			break;` |
|       - |  1379 | `		}` |
|   15698 |  1380 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1381 | `			continue;` |
|       - |  1382 | `		}` |
|       - |  1383 | `		/* Compile the key if available */` |
|   15698 |  1384 | `		pKey = pCur;` |
|   15698 |  1385 | `		iNest = 0;` |
|   43732 |  1386 | `		while( pCur < pGen->pIn ){` |
|   29256 |  1387 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 |  1388 | `				break;` |
|       - |  1389 | `			}` |
|       - |  1390 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1391 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1392 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1393 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1394 | `			 */` |
|   28040 |  1395 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   28034 |  1459 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      86 |  1460 | `				iNest++;` |
|   27992 |  1461 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - |  1462 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - |  1463 | `				 * parser will shortly detect any syntax error.` |
|       - |  1464 | `				 */` |
|      86 |  1465 | `				iNest--;` |
|      42 |  1466 | `			}` |
|   28034 |  1467 | `			pCur++;` |
|       2 |  1468 | `		}` |
|   15698 |  1469 | `		rc = SXERR_EMPTY;` |
|   15698 |  1470 | `		if( pCur < pGen->pIn ){` |
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
|   15085 |  1486 | `		}else if( pKey == pCur ){` |
|       - |  1487 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1488 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1489 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1490 | `		}else{` |
|       - |  1491 | `			/* Reset back the cursor and point to the entry value */` |
|   14482 |  1492 | `			pCur = pKey;` |
|       - |  1493 | `		}` |
|   15688 |  1494 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1495 | `			/* No available key,load NULL */` |
|   14484 |  1496 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7241 |  1497 | `		}` |
|   15688 |  1498 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   15686 |  1513 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15686 |  1514 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1515 | `			return SXERR_ABORT;` |
|       - |  1516 | `		}` |
|   15686 |  1517 | `		if( iEmitRef ){` |
|       - |  1518 | `			/* Emit the load reference instruction */` |
|      32 |  1519 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1520 | `		}` |
|   15686 |  1521 | `		xValidator = 0;` |
|   15686 |  1522 | `		iEmitRef = 0;` |
|   15686 |  1523 | `		nPair++;` |
|       2 |  1524 | `	}` |
|       - |  1525 | `	/* Emit the load map instruction */` |
|   25268 |  1526 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1527 | `	/* Node successfully compiled */` |
|   25268 |  1528 | `	return SXRET_OK;` |
|   12641 |  1529 |  |
|       - |  1530 | `/*` |
|       - |  1531 | ` * Compile the 'array' language construct.` |
|       - |  1532 | ` *	 According to the PHP language reference manual` |
|       - |  1533 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1534 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1535 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1536 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1537 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1538 | ` */` |
|   24980 |  1539 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1540 |  |
|       - |  1541 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   24982 |  1542 | `	pGen->pIn += 2;` |
|   24982 |  1543 | `	pGen->pEnd--;` |
|   12490 |  1544 | `	SXUNUSED(iCompileFlag);` |
|   24982 |  1545 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1546 |  |
|       - |  1547 | `/*` |
|       - |  1548 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1549 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1550 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1551 | ` */` |
|     298 |  1552 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1553 |  |
|       - |  1554 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     300 |  1555 | `	pGen->pIn++;` |
|     300 |  1556 | `	pGen->pEnd--;` |
|     149 |  1557 | `	SXUNUSED(iCompileFlag);` |
|     300 |  1558 | `	return GenStateCompileArrayBody(pGen);` |
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
|     122 |  1819 | `static sxi32 GenStateArrowAddCapture(` |
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
|     123 |  1831 | `	if( nByte == 0 ){` |
|     ! 0 |  1832 | `		return SXRET_OK;` |
|       - |  1833 | `	}` |
|     122 |  1834 | `	if( nByte == sizeof("this")-1` |
|      66 |  1835 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1836 | `		return SXRET_OK;` |
|       - |  1837 | `	}` |
|     147 |  1838 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      94 |  1839 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      90 |  1840 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      69 |  1841 | `			return SXRET_OK;` |
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
|      62 |  1862 |  |
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
|     104 |  1931 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1932 | `	ph7_gen_state *pGen,` |
|       - |  1933 | `	ph7_vm_func *pFunc,` |
|       - |  1934 | `	SyToken *pStart,` |
|       - |  1935 | `	SyToken *pEnd,` |
|       - |  1936 | `	SyString *aShadow,` |
|       - |  1937 | `	sxu32 nShadow)` |
|       1 |  1938 |  |
|     105 |  1939 | `	SyToken *pScan = pStart;` |
|       - |  1940 | `	sxi32 rc;` |
|     389 |  1941 | `	while( pScan < pEnd ){` |
|     285 |  1942 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|     271 |  1953 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  1954 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  1955 | `			SyToken *pFnKw = pScan;` |
|      20 |  1956 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  1957 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  1958 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1959 | `				pFnKw = &pScan[1];` |
|     ! 0 |  1960 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1961 | `			}` |
|      21 |  1962 | `			if( nKw == PH7_TKWRD_FN ){` |
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
|       1 |  2104 | `		}` |
|     253 |  2105 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     143 |  2106 | `			pScan++;` |
|     143 |  2107 | `			continue;` |
|       - |  2108 | `		}` |
|       - |  2109 | `		{` |
|       - |  2110 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     111 |  2111 | `			SyToken *pDollar = pScan;` |
|     165 |  2112 | `			while( &pDollar[1] < pEnd` |
|     111 |  2113 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2114 | `				pDollar++;` |
|     ! 0 |  2115 | `			}` |
|     111 |  2116 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2117 | `				break;` |
|       - |  2118 | `			}` |
|     111 |  2119 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2120 | `				pScan = pDollar + 1;` |
|     ! 0 |  2121 | `				continue;` |
|       - |  2122 | `			}` |
|     166 |  2123 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     110 |  2124 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      55 |  2125 | `				aShadow,nShadow);` |
|     111 |  2126 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2127 | `				return SXERR_ABORT;` |
|       - |  2128 | `			}` |
|     111 |  2129 | `			pScan = pDollar + 2;` |
|       - |  2130 | `		}` |
|       1 |  2131 | `	}` |
|     105 |  2132 | `	return SXRET_OK;` |
|      53 |  2133 |  |
|       - |  2134 | `/*` |
|       - |  2135 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2136 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2137 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2138 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2139 | ` * $this is also made available.` |
|       - |  2140 | ` */` |
|      86 |  2141 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|      88 |  2157 | `	sxi32 iFlags = 0;` |
|      88 |  2158 | `	int bStatic = 0;` |
|       - |  2159 | `	sxi32 rc;` |
|       - |  2160 | `	sxu32 n;` |
|      43 |  2161 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2162 |  |
|      88 |  2163 | `	nLine = pGen->pIn->nLine;` |
|       - |  2164 | `	/* Optional 'static' prefix */` |
|      86 |  2165 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      88 |  2166 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2167 | `		bStatic = 1;` |
|       3 |  2168 | `		pGen->pIn++;` |
|       1 |  2169 | `	}` |
|       - |  2170 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      86 |  2171 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      88 |  2172 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2173 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2174 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2175 | `		return SXERR_SYNTAX;` |
|       - |  2176 | `	}` |
|      88 |  2177 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2178 | `	/* Optional '&' — return by reference */` |
|      88 |  2179 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2180 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2181 | `		pGen->pIn++;` |
|     ! 0 |  2182 | `	}` |
|       - |  2183 | `	/* Expect '(' */` |
|      88 |  2184 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|      86 |  2195 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2196 | `	/* Delimit the parameter list */` |
|      86 |  2197 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      86 |  2198 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2199 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2200 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2201 | `		return SXERR_SYNTAX;` |
|       - |  2202 | `	}` |
|       - |  2203 | `	/* Allocate the function state */` |
|      84 |  2204 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      84 |  2205 | `	if( pFunc == 0 ){` |
|     ! 0 |  2206 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2207 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2208 | `		return SXERR_ABORT;` |
|       - |  2209 | `	}` |
|       - |  2210 | `	/* Generate a unique lambda name */` |
|      84 |  2211 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     168 |  2212 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      85 |  2213 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 |  2214 | `	}` |
|      84 |  2215 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      84 |  2216 | `	if( zDup == 0 ){` |
|     ! 0 |  2217 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2218 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2219 | `		return SXERR_ABORT;` |
|       - |  2220 | `	}` |
|      84 |  2221 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2222 | `	/* Collect function arguments */` |
|      84 |  2223 | `	if( pGen->pIn < pSigEnd ){` |
|      54 |  2224 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      54 |  2225 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2226 | `			return SXERR_ABORT;` |
|       - |  2227 | `		}` |
|      26 |  2228 | `	}` |
|       - |  2229 | `	/* Point past ')' and parse optional return type */` |
|      84 |  2230 | `	pGen->pIn = &pSigEnd[1];` |
|      84 |  2231 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      84 |  2232 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2233 | `		return SXERR_ABORT;` |
|      84 |  2234 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2235 | `		return SXERR_SYNTAX;` |
|       - |  2236 | `	}` |
|       - |  2237 | `	/* Expect '=>' */` |
|      84 |  2238 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|      81 |  2249 | `	pGen->pIn++; /* Jump '=>' */` |
|      81 |  2250 | `	pBodyStart = pGen->pIn;` |
|      81 |  2251 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2252 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2253 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2254 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2255 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      81 |  2256 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2257 | `	{` |
|      81 |  2258 | `		SyString *aShadow = 0;` |
|      81 |  2259 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      81 |  2260 | `		if( nShadow > 0 ){` |
|      51 |  2261 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      50 |  2262 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      51 |  2263 | `			if( aShadow == 0 ){` |
|     ! 0 |  2264 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2265 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2266 | `				return SXERR_ABORT;` |
|       - |  2267 | `			}` |
|     107 |  2268 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      57 |  2269 | `				aShadow[n] = aArgs[n].sName;` |
|      29 |  2270 | `			}` |
|      25 |  2271 | `		}` |
|     121 |  2272 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      40 |  2273 | `			aShadow,nShadow);` |
|      81 |  2274 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2275 | `			return SXERR_ABORT;` |
|       - |  2276 | `		}` |
|       - |  2277 | `	}` |
|       - |  2278 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2279 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2280 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2281 | `	 * $this. */` |
|      81 |  2282 | `	if( !bStatic ){` |
|       - |  2283 | `		char *zThisDup;` |
|      79 |  2284 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      79 |  2285 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2286 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2287 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2288 | `			return SXERR_ABORT;` |
|       - |  2289 | `		}` |
|      79 |  2290 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      79 |  2291 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      79 |  2292 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      79 |  2293 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      79 |  2294 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      39 |  2295 | `	}` |
|       - |  2296 | `	/* Arrow functions are always closures */` |
|      81 |  2297 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2298 | `	/* Compile the body expression as an implicit return */` |
|     121 |  2299 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      40 |  2300 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      81 |  2301 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2302 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2303 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2304 | `		return SXERR_ABORT;` |
|       - |  2305 | `	}` |
|      81 |  2306 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      81 |  2307 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      81 |  2308 | `	pSavedEnd = pGen->pEnd;` |
|      81 |  2309 | `	pGen->pIn = pBodyStart;` |
|      81 |  2310 | `	pGen->pEnd = pBodyEnd;` |
|      81 |  2311 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      81 |  2312 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2313 | `		return SXERR_ABORT;` |
|       - |  2314 | `	}` |
|       - |  2315 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2316 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2317 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2318 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      81 |  2319 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      81 |  2320 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      81 |  2321 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      81 |  2322 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      81 |  2323 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2324 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      81 |  2325 | `	pGen->pIn = pBodyEnd;` |
|      81 |  2326 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2327 | `	/* Emit the load-closure instruction */` |
|      81 |  2328 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      81 |  2329 | `	return SXRET_OK;` |
|      45 |  2330 |  |
|       - |  2331 | `/*` |
|       - |  2332 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2333 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2334 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2335 | ` * expression's value.` |
|       - |  2336 | ` */` |
|     346 |  2337 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2338 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       2 |  2339 |  |
|       - |  2340 | `	SySet *pInstrContainer;` |
|       - |  2341 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2342 | `	GenBlock *pArmBlock;` |
|       - |  2343 | `	sxi32 rc;` |
|     348 |  2344 | `	pTmpIn  = pGen->pIn;` |
|     348 |  2345 | `	pTmpEnd = pGen->pEnd;` |
|     348 |  2346 | `	pGen->pIn  = pStart;` |
|     348 |  2347 | `	pGen->pEnd = pStop;` |
|     348 |  2348 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     348 |  2349 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2350 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2351 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2352 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2353 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2354 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     521 |  2355 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2356 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     348 |  2357 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2358 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2359 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2360 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2361 | `		return SXERR_ABORT;` |
|       - |  2362 | `	}` |
|     348 |  2363 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     348 |  2364 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     348 |  2365 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     348 |  2366 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     348 |  2367 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     348 |  2368 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     348 |  2369 | `	pGen->pIn  = pTmpIn;` |
|     348 |  2370 | `	pGen->pEnd = pTmpEnd;` |
|     348 |  2371 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2372 | `		return SXERR_ABORT;` |
|       - |  2373 | `	}` |
|     348 |  2374 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2375 | `		return SXERR_EMPTY;` |
|       - |  2376 | `	}` |
|     348 |  2377 | `	return SXRET_OK;` |
|     175 |  2378 |  |
|       - |  2379 | `/*` |
|       - |  2380 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2381 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2382 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2383 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2384 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2385 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2386 | ` */` |
|       - |  2387 | `/*` |
|       - |  2388 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2389 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2390 | ` * caller can bail out of the current expression.` |
|       - |  2391 | ` */` |
|       2 |  2392 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2393 |  |
|       - |  2394 | `	va_list ap;` |
|       - |  2395 | `	sxi32 rc;` |
|       - |  2396 | `	SyBlob sMsg;` |
|       3 |  2397 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2398 | `	va_start(ap,zFmt);` |
|       3 |  2399 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2400 | `	va_end(ap);` |
|       3 |  2401 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2402 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2403 | `	SyBlobRelease(&sMsg);` |
|       3 |  2404 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2405 | `		return SXERR_ABORT;` |
|       - |  2406 | `	}` |
|       3 |  2407 | `	return SXERR_SYNTAX;` |
|       2 |  2408 |  |
|       - |  2409 | `/*` |
|       - |  2410 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2411 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2412 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2413 | ` */` |
|     348 |  2414 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       2 |  2415 |  |
|     350 |  2416 | `	SyToken *pCur = pStart;` |
|     350 |  2417 | `	int iNest = 0;` |
|     812 |  2418 | `	while( pCur < pEnd ){` |
|     778 |  2419 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2420 | `			iNest++;` |
|     772 |  2421 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2422 | `			iNest--;` |
|     760 |  2423 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     316 |  2424 | `			return pCur;` |
|       - |  2425 | `		}` |
|     464 |  2426 | `		pCur++;` |
|       2 |  2427 | `	}` |
|      36 |  2428 | `	return pEnd;` |
|     176 |  2429 |  |
|      70 |  2430 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2431 |  |
|       - |  2432 | `	ph7_match *pMatch;` |
|       - |  2433 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      72 |  2434 | `	int bHasDefault = 0;` |
|       - |  2435 | `	sxu32 nLine;` |
|       - |  2436 | `	sxi32 rc;` |
|      35 |  2437 | `	SXUNUSED(iCompileFlag);` |
|      72 |  2438 | `	nLine = pGen->pIn->nLine;` |
|      72 |  2439 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2440 | `	/* Expect '(' */` |
|      72 |  2441 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2442 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2443 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2444 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2445 | `	}` |
|      72 |  2446 | `	pGen->pIn++; /* Jump '(' */` |
|      72 |  2447 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      72 |  2448 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2449 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2450 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2451 | `	}` |
|      72 |  2452 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2453 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2454 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2455 | `	}` |
|       - |  2456 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      72 |  2457 | `	pSavedEnd = pGen->pEnd;` |
|      72 |  2458 | `	pGen->pEnd = pSubjEnd;` |
|      72 |  2459 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      72 |  2460 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2461 | `		return SXERR_ABORT;` |
|       - |  2462 | `	}` |
|      72 |  2463 | `	pGen->pEnd = pSavedEnd;` |
|      72 |  2464 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2465 | `	/* Expect '{' */` |
|      72 |  2466 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2467 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2468 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2469 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2470 | `	}` |
|      72 |  2471 | `	pGen->pIn++; /* Jump '{' */` |
|      72 |  2472 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      72 |  2473 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2474 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2475 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2476 | `	}` |
|       - |  2477 | `	/* Allocate ph7_match container */` |
|      72 |  2478 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      72 |  2479 | `	if( pMatch == 0 ){` |
|     ! 0 |  2480 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2481 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2482 | `		return SXERR_ABORT;` |
|       - |  2483 | `	}` |
|      72 |  2484 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      72 |  2485 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2486 | `	/* Iterate arms */` |
|     250 |  2487 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2488 | `		ph7_match_arm sArm;` |
|       - |  2489 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     184 |  2490 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     184 |  2491 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     184 |  2492 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     184 |  2493 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2494 | `		/* 'default' arm? */` |
|     182 |  2495 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     103 |  2496 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2497 | `			if( bHasDefault ){` |
|       3 |  2498 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2499 | `					"Match expressions may only contain one default arm");` |
|       4 |  2500 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2501 | `			}` |
|      20 |  2502 | `			sArm.bDefault = 1;` |
|      20 |  2503 | `			bHasDefault = 1;` |
|      20 |  2504 | `			pGen->pIn++;` |
|      20 |  2505 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2506 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2507 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2508 | `			}` |
|      20 |  2509 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2510 | `		}else{` |
|       - |  2511 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     164 |  2512 | `			pCondStart = pGen->pIn;` |
|     164 |  2513 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2514 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     172 |  2515 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2516 | `				SySet sCondBc;` |
|       9 |  2517 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2518 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2519 | `						"syntax error, empty match condition expression");` |
|       - |  2520 | `				}` |
|       9 |  2521 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2522 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2523 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2524 | `					return SXERR_ABORT;` |
|       - |  2525 | `				}` |
|       9 |  2526 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2527 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2528 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2529 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2530 | `			}` |
|     164 |  2531 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2532 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2533 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2534 | `			}` |
|     162 |  2535 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2536 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2537 | `					"syntax error, empty match condition expression");` |
|       - |  2538 | `			}` |
|       - |  2539 | `			{` |
|       - |  2540 | `				SySet sCondBc;` |
|     162 |  2541 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     162 |  2542 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     162 |  2543 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2544 | `					return SXERR_ABORT;` |
|       - |  2545 | `				}` |
|     162 |  2546 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2547 | `			}` |
|     162 |  2548 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2549 | `		}` |
|       - |  2550 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     180 |  2551 | `		pResStart = pGen->pIn;` |
|     180 |  2552 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     180 |  2553 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2554 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2555 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2556 | `		}` |
|     180 |  2557 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     180 |  2558 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2559 | `			return SXERR_ABORT;` |
|       - |  2560 | `		}` |
|     180 |  2561 | `		pGen->pIn = pResEnd;` |
|     180 |  2562 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     148 |  2563 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2564 | `		}` |
|     180 |  2565 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       2 |  2566 | `	}` |
|      68 |  2567 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      68 |  2568 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      68 |  2569 | `	return SXRET_OK;` |
|      37 |  2570 |  |
|       - |  2571 | `/*` |
|       - |  2572 | ` * Compile a backtick quoted string.` |
|       - |  2573 | ` */` |
|       4 |  2574 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  2575 |  |
|       - |  2576 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2577 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2578 | `	 */` |
|       7 |  2579 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2580 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2581 | `		ph7_lib_version()` |
|       - |  2582 | `		);` |
|       - |  2583 | `	/* Load NULL */` |
|       5 |  2584 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2585 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2586 | `	/* Node successfully compiled */` |
|       5 |  2587 | `	return SXRET_OK;` |
|       1 |  2588 |  |
|       - |  2589 | `/*` |
|       - |  2590 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2591 | ` * construct.` |
|       - |  2592 | ` */` |
|      72 |  2593 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2594 |  |
|       - |  2595 | `	SyString *pName;` |
|       - |  2596 | `	sxu32 nKeyID;` |
|       - |  2597 | `	sxi32 rc;` |
|       - |  2598 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 |  2599 | `	pName = &pGen->pIn->sData;` |
|      74 |  2600 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 |  2601 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 |  2602 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2603 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2604 | `		/* Compile arguments one after one */` |
|       9 |  2605 | `		pTmp = pGen->pEnd;` |
|       - |  2606 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2607 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2608 | `		 *  mean that the following expression is valid:` |
|       - |  2609 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2610 | `		 */` |
|       9 |  2611 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2612 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2613 | `			if( pGen->pIn < pNext ){` |
|       9 |  2614 | `				pGen->pEnd = pNext;` |
|       9 |  2615 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2616 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2617 | `					return SXERR_ABORT;` |
|       - |  2618 | `				}` |
|       9 |  2619 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2620 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2621 | `					 * without the overhead of a function call.` |
|       - |  2622 | `					 * This is a very powerful optimization that improve` |
|       - |  2623 | `					 * performance greatly.` |
|       - |  2624 | `					 */` |
|       9 |  2625 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2626 | `				}` |
|       4 |  2627 | `			}` |
|       - |  2628 | `			/* Jump trailing commas */` |
|       9 |  2629 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2630 | `				pNext++;` |
|     ! 0 |  2631 | `			}` |
|       9 |  2632 | `			pGen->pIn = pNext;` |
|       1 |  2633 | `		}` |
|       - |  2634 | `		/* Restore token stream */` |
|       9 |  2635 | `		pGen->pEnd = pTmp;` |
|       5 |  2636 | `	}else{` |
|      66 |  2637 | `		sxi32 nArg = 0;` |
|      66 |  2638 | `		sxu32 nIdx = 0;` |
|      66 |  2639 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 |  2640 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2641 | `			return SXERR_ABORT;` |
|      66 |  2642 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 |  2643 | `			nArg = 1;` |
|      32 |  2644 | `		}` |
|      66 |  2645 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2646 | `			ph7_value *pObj;` |
|       - |  2647 | `			/* Emit the call instruction */` |
|      20 |  2648 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2649 | `			if( pObj == 0 ){` |
|     ! 0 |  2650 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2651 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2652 | `				return SXERR_ABORT;` |
|       - |  2653 | `			}` |
|      20 |  2654 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2655 | `			/* Install in the literal table */` |
|      20 |  2656 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 |  2657 | `		}` |
|       - |  2658 | `		/* Emit the call instruction */` |
|      66 |  2659 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 |  2660 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - |  2661 | `	}` |
|       - |  2662 | `	/* Node successfully compiled */` |
|      74 |  2663 | `	return SXRET_OK;` |
|      38 |  2664 |  |
|       - |  2665 | `/*` |
|       - |  2666 | ` * Compile a node holding a variable declaration.` |
|       - |  2667 | ` * According to the PHP language reference` |
|       - |  2668 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2669 | ` *  The variable name is case-sensitive.` |
|       - |  2670 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2671 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2672 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2673 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2674 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2675 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2676 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2677 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2678 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2679 | ` *  the chapter on Expressions.` |
|       - |  2680 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2681 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2682 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2683 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2684 | ` *  is being assigned (the source variable).` |
|       - |  2685 | ` */` |
|  848192 |  2686 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2687 |  |
|  848194 |  2688 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2689 | `	sxi32 iVv;` |
|       - |  2690 | `	sxi32 iP1;` |
|       - |  2691 | `	void *p3;` |
|       - |  2692 | `	sxi32 rc;` |
|  848194 |  2693 | `	iVv = -1; /* Variable variable counter */` |
| 1696398 |  2694 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  848206 |  2695 | `		pGen->pIn++;` |
|  848206 |  2696 | `		iVv++;` |
|       2 |  2697 | `	}` |
|  848194 |  2698 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2699 | `		/* Invalid variable name */` |
|     ! 0 |  2700 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2701 | `		if( rc == SXERR_ABORT ){` |
|       - |  2702 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2703 | `			return SXERR_ABORT;` |
|       - |  2704 | `		}` |
|     ! 0 |  2705 | `		return SXRET_OK;` |
|       - |  2706 | `	}` |
|  848194 |  2707 | `	p3  = 0;` |
|  848194 |  2708 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2709 | `		/* Dynamic variable creation */` |
|      18 |  2710 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 |  2711 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 |  2712 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2713 | `			/* Empty expression */` |
|       3 |  2714 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2715 | `			return SXRET_OK;` |
|       - |  2716 | `		}` |
|       - |  2717 | `		/* Compile the expression holding the variable name */` |
|      16 |  2718 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2719 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2720 | `			return SXERR_ABORT;` |
|      16 |  2721 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2722 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2723 | `			return SXRET_OK;` |
|       - |  2724 | `		}` |
|       7 |  2725 | `	}else{` |
|       - |  2726 | `		SyHashEntry *pEntry;` |
|       - |  2727 | `		SyString *pName;` |
|  848178 |  2728 | `		char *zName = 0;` |
|       - |  2729 | `		/* Extract variable name */` |
|  848178 |  2730 | `		pName = &pGen->pIn->sData;` |
|       - |  2731 | `		/* Advance the stream cursor */` |
|  848178 |  2732 | `		pGen->pIn++;` |
|  848178 |  2733 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  848178 |  2734 | `		if( pEntry == 0 ){` |
|       - |  2735 | `			/* Duplicate name */` |
|  122018 |  2736 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  122018 |  2737 | `			if( zName == 0 ){` |
|     ! 0 |  2738 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2739 | `				return SXERR_ABORT;` |
|       - |  2740 | `			}` |
|       - |  2741 | `			/* Install in the hashtable */` |
|  122018 |  2742 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   61010 |  2743 | `		}else{` |
|       - |  2744 | `			/* Name already available */` |
|  726162 |  2745 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2746 | `		}` |
|  848178 |  2747 | `		p3 = (void *)zName;` |
|       - |  2748 | `	}` |
|  848190 |  2749 | `	iP1 = 0;` |
|  848190 |  2750 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  325682 |  2751 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2752 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  319224 |  2753 | `			iP1 = 1;` |
|  159611 |  2754 | `		}` |
|  162840 |  2755 | `	}` |
|       - |  2756 | `	/* Emit the load instruction */` |
|  848190 |  2757 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  848202 |  2758 | `	while( iVv > 0 ){` |
|      13 |  2759 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2760 | `		iVv--;` |
|       1 |  2761 | `	}` |
|       - |  2762 | `	/* Node successfully compiled */` |
|  848190 |  2763 | `	return SXRET_OK;` |
|  424098 |  2764 |  |
|       - |  2765 | `/*` |
|       - |  2766 | ` * Load a literal.` |
|       - |  2767 | ` */` |
|  569032 |  2768 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2769 |  |
|  569034 |  2770 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2771 | `	ph7_value *pObj;` |
|       - |  2772 | `	SyString *pStr;` |
|       - |  2773 | `	sxu32 nIdx;` |
|       - |  2774 | `	/* Extract token value */` |
|  569034 |  2775 | `	pStr = &pToken->sData;` |
|       - |  2776 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  569034 |  2777 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  103506 |  2778 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2779 | `			/* NULL constant are always indexed at 0 */` |
|   44072 |  2780 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   44072 |  2781 | `			return SXRET_OK;` |
|   59436 |  2782 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2783 | `			/* TRUE constant are always indexed at 1 */` |
|     520 |  2784 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     520 |  2785 | `			return SXRET_OK;` |
|       2 |  2786 | `		}` |
|  539854 |  2787 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   89732 |  2788 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2789 | `			/* FALSE constant are always indexed at 2 */` |
|   38312 |  2790 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   38312 |  2791 | `			return SXRET_OK;` |
|  466866 |  2792 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   79292 |  2793 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2794 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5808 |  2795 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5808 |  2796 | `			if( pObj == 0 ){` |
|     ! 0 |  2797 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2798 | `				return SXERR_ABORT;` |
|       - |  2799 | `			}` |
|    5808 |  2800 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2801 | `			/* Emit the load constant instruction */` |
|    5808 |  2802 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5808 |  2803 | `			return SXRET_OK;` |
|  436053 |  2804 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   29278 |  2805 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2806 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2807 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2808 | `			if( pObj == 0 ){` |
|     ! 0 |  2809 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2810 | `				return SXERR_ABORT;` |
|       - |  2811 | `			}` |
|       7 |  2812 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2813 | `				SyString sNs;` |
|       7 |  2814 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2815 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2816 | `			}else{` |
|     ! 0 |  2817 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2818 | `			}` |
|       7 |  2819 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2820 | `			return SXRET_OK;` |
|  435173 |  2821 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   12212 |  2822 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  429061 |  2823 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   15324 |  2824 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2825 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2826 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2827 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2828 | `				/* Point to the upper block */` |
|      11 |  2829 | `				pBlock = pBlock->pParent;` |
|       1 |  2830 | `			}` |
|      11 |  2831 | `			if( pBlock == 0 ){` |
|       - |  2832 | `				/* Called in the global scope,load NULL */` |
|       5 |  2833 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2834 | `			}else{` |
|       - |  2835 | `				/* Extract the target function/method */` |
|       7 |  2836 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2837 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2838 | `					/* Not a class method,Load null */` |
|       3 |  2839 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2840 | `				}else{` |
|       5 |  2841 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2842 | `					if( pObj == 0 ){` |
|     ! 0 |  2843 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2844 | `						return SXERR_ABORT;` |
|       - |  2845 | `					}` |
|       5 |  2846 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2847 | `					/* Emit the load constant instruction */` |
|       5 |  2848 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2849 | `				}` |
|       - |  2850 | `			}` |
|      11 |  2851 | `			return SXRET_OK;` |
|       - |  2852 | `	}` |
|       - |  2853 | `	/* Query literal table */` |
|  480314 |  2854 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2855 | `		ph7_value *pLitObj;` |
|       - |  2856 | `		/* Unknown literal,install it in the literal table */` |
|  224828 |  2857 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  224828 |  2858 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2859 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2860 | `			return SXERR_ABORT;` |
|       - |  2861 | `		}` |
|  224828 |  2862 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  224828 |  2863 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  112413 |  2864 | `	}` |
|       - |  2865 | `	/* Emit the load constant instruction */` |
|  480314 |  2866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  480314 |  2867 | `	return SXRET_OK;` |
|  284518 |  2868 |  |
|       - |  2869 | `/*` |
|       - |  2870 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2871 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2872 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2873 | ` * Otherwise, load the simple literal directly.` |
|       - |  2874 | ` */` |
|  569060 |  2875 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2876 |  |
|       - |  2877 | `	sxi32 rc;` |
|  569062 |  2878 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2879 | `		return SXRET_OK;` |
|       - |  2880 | `	}` |
|       - |  2881 | `	/* Check if this is a multi-token namespace path */` |
|  569062 |  2882 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2883 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      30 |  2884 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      30 |  2885 | `		int isAbsolute = 0;` |
|      30 |  2886 | `		SyBlobReset(pWorker);` |
|       - |  2887 | `		/* Check for leading backslash (absolute path) */` |
|      30 |  2888 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      28 |  2889 | `			isAbsolute = 1;` |
|      28 |  2890 | `			pGen->pIn++; /* Skip leading backslash */` |
|      13 |  2891 | `		}` |
|       - |  2892 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      30 |  2893 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2894 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2895 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2896 | `		}` |
|       - |  2897 | `		/* Collect all path components */` |
|     110 |  2898 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     110 |  2899 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      42 |  2900 | `				SyBlobAppend(pWorker,"\\",1);` |
|      22 |  2901 | `			}else{` |
|      70 |  2902 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2903 | `			}` |
|     110 |  2904 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      30 |  2905 | `				pGen->pIn++;` |
|      30 |  2906 | `				break;` |
|       - |  2907 | `			}` |
|      82 |  2908 | `			pGen->pIn++;` |
|       2 |  2909 | `		}` |
|      30 |  2910 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2911 | `			ph7_value *pObj;` |
|       - |  2912 | `			SyString sPath;` |
|       - |  2913 | `			sxu32 nIdx;` |
|      30 |  2914 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2915 | `			/* Install in the literal table */` |
|      30 |  2916 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      16 |  2917 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      16 |  2918 | `				if( pObj == 0 ){` |
|     ! 0 |  2919 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2920 | `					return SXERR_ABORT;` |
|       - |  2921 | `				}` |
|      16 |  2922 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      16 |  2923 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       7 |  2924 | `			}` |
|       - |  2925 | `			/* Emit the load constant instruction.` |
|       - |  2926 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      30 |  2927 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      30 |  2928 | `			return SXRET_OK;` |
|       - |  2929 | `		}` |
|     ! 0 |  2930 | `	}` |
|       - |  2931 | `	/* Single-token literal: load directly */` |
|  569034 |  2932 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  569034 |  2933 | `	return rc;` |
|  284532 |  2934 |  |
|       - |  2935 | `/*` |
|       - |  2936 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2937 | ` */` |
|  569060 |  2938 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2939 |  |
|       - |  2940 | `	sxi32 rc;` |
|  569062 |  2941 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  569062 |  2942 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2943 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2944 | `		return rc;` |
|       - |  2945 | `	}` |
|       - |  2946 | `	/* Node successfully compiled */` |
|  569062 |  2947 | `	return SXRET_OK;` |
|  284532 |  2948 |  |
|       - |  2949 | `/*` |
|       - |  2950 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  2951 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  2952 | ` */` |
|       8 |  2953 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  2954 |  |
|       - |  2955 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  2956 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  2957 | `		pGen->pIn++;` |
|       1 |  2958 | `	}` |
|       9 |  2959 | `	return SXRET_OK;` |
|       1 |  2960 |  |
|       - |  2961 | `/*` |
|       - |  2962 | ` * Check if the given identifier name is reserved or not.` |
|       - |  2963 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  2964 | ` */` |
|      56 |  2965 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 |  2966 |  |
|      58 |  2967 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 |  2968 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  2969 | `			return TRUE;` |
|      24 |  2970 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  2971 | `			return TRUE;` |
|       2 |  2972 | `		}` |
|      43 |  2973 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  2974 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  2975 | `			return TRUE;` |
|       - |  2976 | `		}` |
|     ! 0 |  2977 | `	}` |
|       - |  2978 | `	/* Not a reserved constant */` |
|      50 |  2979 | `	return FALSE;` |
|      30 |  2980 |  |
|       - |  2981 | `/*` |
|       - |  2982 | ` * Compile the 'const' statement.` |
|       - |  2983 | ` * According to the PHP language reference` |
|       - |  2984 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  2985 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  2986 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  2987 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  2988 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2989 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  2990 | ` *  Syntax` |
|       - |  2991 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  2992 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  2993 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  2994 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  2995 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  2996 | ` *  to get a list of all defined constants.` |
|       - |  2997 | ` *` |
|       - |  2998 | ` * Symisc eXtension.` |
|       - |  2999 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3000 | ` *  would allow only simple scalar value.` |
|       - |  3001 | ` *  Example` |
|       - |  3002 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3003 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3004 | ` */` |
|      32 |  3005 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 |  3006 |  |
|       - |  3007 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 |  3008 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3009 | `	SyString *pName;` |
|       - |  3010 | `	sxi32 rc;` |
|      34 |  3011 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 |  3012 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3013 | `		/* Invalid constant name */` |
|       7 |  3014 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  3015 | `		if( rc == SXERR_ABORT ){` |
|       - |  3016 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3017 | `			return SXERR_ABORT;` |
|       - |  3018 | `		}` |
|       7 |  3019 | `		goto Synchronize;` |
|       - |  3020 | `	}` |
|       - |  3021 | `	/* Peek constant name */` |
|      28 |  3022 | `	pName = &pGen->pIn->sData;` |
|       - |  3023 | `	/* Make sure the constant name isn't reserved */` |
|      28 |  3024 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3025 | `		/* Reserved constant */` |
|       9 |  3026 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3027 | `		if( rc == SXERR_ABORT ){` |
|       - |  3028 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3029 | `			return SXERR_ABORT;` |
|       - |  3030 | `		}` |
|       9 |  3031 | `		goto Synchronize;` |
|       - |  3032 | `	}` |
|      20 |  3033 | `	pGen->pIn++;` |
|      20 |  3034 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3035 | `		/* Invalid statement*/` |
|       5 |  3036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 |  3037 | `		if( rc == SXERR_ABORT ){` |
|       - |  3038 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3039 | `			return SXERR_ABORT;` |
|       - |  3040 | `		}` |
|       5 |  3041 | `		goto Synchronize;` |
|       - |  3042 | `	}` |
|      15 |  3043 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3044 | `	/* Allocate a new constant value container */` |
|      15 |  3045 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3046 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3047 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3048 | `		return SXERR_ABORT;` |
|       - |  3049 | `	}` |
|      15 |  3050 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3051 | `	/* Swap bytecode container */` |
|      15 |  3052 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3053 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3054 | `	/* Compile constant value */` |
|      15 |  3055 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3056 | `	/* Emit the done instruction */` |
|      15 |  3057 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3058 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3059 | `	if( rc == SXERR_ABORT ){` |
|       - |  3060 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3061 | `		return SXERR_ABORT;` |
|       - |  3062 | `	}` |
|      15 |  3063 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3064 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3065 | `	{` |
|       - |  3066 | `		SyBlob sFQN;` |
|       - |  3067 | `		SyString sFQNStr;` |
|      15 |  3068 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3069 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3070 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3071 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3072 | `		SyBlobRelease(&sFQN);` |
|       - |  3073 | `	}` |
|      15 |  3074 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3075 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3076 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3077 | `	}` |
|      15 |  3078 | `	return SXRET_OK;` |
|       9 |  3079 | `Synchronize:` |
|       - |  3080 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 |  3081 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 |  3082 | `		pGen->pIn++;` |
|       1 |  3083 | `	}` |
|      19 |  3084 | `	return SXRET_OK;` |
|      18 |  3085 |  |
|       - |  3086 | `/*` |
|       - |  3087 | ` * Compile the 'continue' statement.` |
|       - |  3088 | ` * According to the PHP language reference` |
|       - |  3089 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3090 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3091 | ` *  iteration.` |
|       - |  3092 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3093 | ` *  the purposes of continue.` |
|       - |  3094 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3095 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3096 | ` *  Note:` |
|       - |  3097 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3098 | ` */` |
|       - |  3099 | `/*` |
|       - |  3100 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3101 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3102 | ` * break/continue crosses a try boundary.` |
|       - |  3103 | ` *` |
|       - |  3104 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3105 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3106 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3107 | ` */` |
|    3030 |  3108 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3109 |  |
|    3032 |  3110 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17726 |  3111 | `	while( pBlock && pBlock != pTarget ){` |
|   14696 |  3112 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3113 | `			if( pBlock->pUserData ){` |
|       - |  3114 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3115 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3116 | `			}else{` |
|       - |  3117 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3118 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3119 | `				 * exception context from a sub-execution.` |
|       - |  3120 | `				 */` |
|     ! 0 |  3121 | `				break;` |
|       - |  3122 | `			}` |
|       1 |  3123 | `		}` |
|   14696 |  3124 | `		pBlock = pBlock->pParent;` |
|       2 |  3125 | `	}` |
|    3032 |  3126 |  |
|    2946 |  3127 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3128 |  |
|       - |  3129 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3130 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3131 | `	sxu32 nLineLocal;` |
|       - |  3132 | `	sxi32 rc;` |
|    2948 |  3133 | `	nLineLocal = pGen->pIn->nLine;` |
|    2948 |  3134 | `	iLevel = 0;` |
|       - |  3135 | `	/* Jump the 'continue' keyword */` |
|    2948 |  3136 | `	pGen->pIn++;` |
|    2948 |  3137 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3138 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3139 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3140 | `		 */` |
|       - |  3141 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3142 | `		char *zAlloc = 0;` |
|       - |  3143 | `		SyString sNum;` |
|      16 |  3144 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3145 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3146 | `			return SXERR_ABORT;` |
|       - |  3147 | `		}` |
|      16 |  3148 | `		if( rc == SXRET_OK ){` |
|      20 |  3149 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3150 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3151 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3152 | `				return SXERR_ABORT;` |
|       - |  3153 | `			}` |
|      14 |  3154 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3155 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3156 | `		}` |
|      16 |  3157 | `		if( iLevel < 2 ){` |
|       3 |  3158 | `			iLevel = 0;` |
|       1 |  3159 | `		}` |
|      16 |  3160 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3161 | `	}` |
|       - |  3162 | `	/* Point to the target loop */` |
|    2948 |  3163 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2948 |  3164 | `	if( pLoop == 0 ){` |
|       - |  3165 | `		/* Illegal continue */` |
|      11 |  3166 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3167 | `		if( rc == SXERR_ABORT ){` |
|       - |  3168 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3169 | `			return SXERR_ABORT;` |
|       - |  3170 | `		}` |
|       6 |  3171 | `	}else{` |
|    2938 |  3172 | `		sxu32 nInstrIdx = 0;` |
|       - |  3173 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2938 |  3174 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2938 |  3175 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3176 | `			/* According to the PHP language reference manual` |
|       - |  3177 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3178 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3179 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3180 | `			 */` |
|       5 |  3181 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3182 | `			if( rc == SXRET_OK ){` |
|       5 |  3183 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3184 | `			}` |
|       3 |  3185 | `		}else{` |
|       - |  3186 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2934 |  3187 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2934 |  3188 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3189 | `				JumpFixup sJumpFix;` |
|       - |  3190 | `				/* Post-continue */` |
|      14 |  3191 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3192 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3193 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3194 | `			}` |
|       - |  3195 | `		}` |
|       - |  3196 | `	}` |
|    2948 |  3197 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3198 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3199 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3200 | `	}` |
|       - |  3201 | `	/* Statement successfully compiled */` |
|    2948 |  3202 | `	return SXRET_OK;` |
|    1475 |  3203 |  |
|       - |  3204 | `/*` |
|       - |  3205 | ` * Compile the 'break' statement.` |
|       - |  3206 | ` * According to the PHP language reference` |
|       - |  3207 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3208 | ` *  structure.` |
|       - |  3209 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3210 | ` *  enclosing structures are to be broken out of.` |
|       - |  3211 | ` */` |
|     110 |  3212 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 |  3213 |  |
|       - |  3214 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3215 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3216 | `	sxi32 rc;` |
|     112 |  3217 | `	iLevel = 0;` |
|       - |  3218 | `	/* Jump the 'break' keyword */` |
|     112 |  3219 | `	pGen->pIn++;` |
|     112 |  3220 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3221 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3222 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3223 | `		 */` |
|       - |  3224 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3225 | `		char *zAlloc = 0;` |
|       - |  3226 | `		SyString sNum;` |
|      16 |  3227 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3228 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3229 | `			return SXERR_ABORT;` |
|       - |  3230 | `		}` |
|      16 |  3231 | `		if( rc == SXRET_OK ){` |
|      20 |  3232 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3233 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3234 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3235 | `				return SXERR_ABORT;` |
|       - |  3236 | `			}` |
|      14 |  3237 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3238 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3239 | `		}` |
|      16 |  3240 | `		if( iLevel < 2 ){` |
|       3 |  3241 | `			iLevel = 0;` |
|       1 |  3242 | `		}` |
|      16 |  3243 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3244 | `	}` |
|       - |  3245 | `	/* Extract the target loop */` |
|     112 |  3246 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 |  3247 | `	if( pLoop == 0 ){` |
|       - |  3248 | `		/* Illegal break */` |
|      17 |  3249 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 |  3250 | `		if( rc == SXERR_ABORT ){` |
|       - |  3251 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3252 | `			return SXERR_ABORT;` |
|       - |  3253 | `		}` |
|       9 |  3254 | `	}else{` |
|       - |  3255 | `		sxu32 nInstrIdx;` |
|       - |  3256 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 |  3257 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 |  3258 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 |  3259 | `		if( rc == SXRET_OK ){` |
|       - |  3260 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 |  3261 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 |  3262 | `		}` |
|       - |  3263 | `	}` |
|     112 |  3264 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3265 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3266 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3267 | `	}` |
|       - |  3268 | `	/* Statement successfully compiled */` |
|     112 |  3269 | `	return SXRET_OK;` |
|      57 |  3270 |  |
|       - |  3271 | `/*` |
|       - |  3272 | ` * Compile or record a label.` |
|       - |  3273 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3274 | ` * Example` |
|       - |  3275 | ` *  goto LABEL;` |
|       - |  3276 | ` *   echo 'Foo';` |
|       - |  3277 | ` *  LABEL:` |
|       - |  3278 | ` *   echo 'Bar';` |
|       - |  3279 | ` */` |
|     112 |  3280 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 |  3281 |  |
|       - |  3282 | `	GenBlock *pBlock;` |
|       - |  3283 | `	Label sLabel;` |
|       - |  3284 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 |  3285 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 |  3286 | `	if( pBlock ){` |
|       - |  3287 | `		sxi32 rc;` |
|       7 |  3288 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3289 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 |  3290 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3291 | `			return SXERR_ABORT;` |
|       - |  3292 | `		}` |
|       3 |  3293 | `	}else{` |
|     110 |  3294 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3295 | `		char *zDup;` |
|       - |  3296 | `		/* Initialize label fields */` |
|     110 |  3297 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3298 | `		/* Duplicate label name */` |
|     110 |  3299 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 |  3300 | `		if( zDup == 0 ){` |
|     ! 0 |  3301 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3302 | `			return SXERR_ABORT;` |
|       - |  3303 | `		}` |
|     110 |  3304 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 |  3305 | `		sLabel.bRef  = FALSE;` |
|     110 |  3306 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 |  3307 | `		pBlock = pGen->pCurrent;` |
|     218 |  3308 | `		while( pBlock ){` |
|     130 |  3309 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3310 | `				break;` |
|       - |  3311 | `			}` |
|       - |  3312 | `			/* Point to the upper block */` |
|     110 |  3313 | `			pBlock = pBlock->pParent;` |
|       2 |  3314 | `		}` |
|     110 |  3315 | `		if( pBlock ){` |
|      22 |  3316 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3317 | `		}else{` |
|      90 |  3318 | `			sLabel.pFunc = 0;` |
|       - |  3319 | `		}` |
|       - |  3320 | `		/* Insert in label set */` |
|     110 |  3321 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3322 | `	}` |
|     114 |  3323 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 |  3324 | `	return SXRET_OK;` |
|      58 |  3325 |  |
|       - |  3326 | `/*` |
|       - |  3327 | ` * Compile the so hated 'goto' statement.` |
|       - |  3328 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3329 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3330 | ` * a compiler it has to do this.` |
|       - |  3331 | ` * According to the PHP language reference manual` |
|       - |  3332 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3333 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3334 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3335 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3336 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3337 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3338 | ` *   of a multi-level break` |
|       - |  3339 | ` */` |
|     152 |  3340 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 |  3341 |  |
|       - |  3342 | `	JumpFixup sJump;` |
|       - |  3343 | `	sxi32 rc;` |
|     154 |  3344 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 |  3345 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3346 | `		/* Missing label */` |
|     ! 0 |  3347 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3348 | `		if( rc == SXERR_ABORT ){` |
|       - |  3349 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3350 | `			return SXERR_ABORT;` |
|       - |  3351 | `		}` |
|     ! 0 |  3352 | `		return SXRET_OK;` |
|       - |  3353 | `	}` |
|     154 |  3354 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3355 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3356 | `		if( rc == SXERR_ABORT ){` |
|       - |  3357 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3358 | `			return SXERR_ABORT;` |
|       - |  3359 | `		}` |
|       3 |  3360 | `	}else{` |
|     150 |  3361 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3362 | `		GenBlock *pBlock;` |
|       - |  3363 | `		char *zDup;` |
|       - |  3364 | `		/* Prepare the jump destination */` |
|     150 |  3365 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 |  3366 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3367 | `		/* Duplicate label name */` |
|     150 |  3368 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 |  3369 | `		if( zDup == 0 ){` |
|     ! 0 |  3370 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3371 | `			return SXERR_ABORT;` |
|       - |  3372 | `		}` |
|     150 |  3373 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 |  3374 | `		pBlock = pGen->pCurrent;` |
|     312 |  3375 | `		while( pBlock ){` |
|     196 |  3376 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 |  3377 | `				break;` |
|       - |  3378 | `			}` |
|       - |  3379 | `			/* Point to the upper block */` |
|     164 |  3380 | `			pBlock = pBlock->pParent;` |
|       2 |  3381 | `		}` |
|     150 |  3382 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 |  3383 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 |  3384 | `			if( rc == SXERR_ABORT ){` |
|       - |  3385 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3386 | `				return SXERR_ABORT;` |
|       - |  3387 | `			}` |
|       3 |  3388 | `		}` |
|     150 |  3389 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 |  3390 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3391 | `		}else{` |
|     124 |  3392 | `			sJump.pFunc = 0;` |
|       - |  3393 | `		}` |
|       - |  3394 | `		/* Emit the unconditional jump */` |
|     150 |  3395 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 |  3396 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3397 | `		}` |
|       - |  3398 | `	}` |
|     154 |  3399 | `	pGen->pIn++; /* Jump the label name */` |
|     154 |  3400 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3402 | `	}` |
|       - |  3403 | `	/* Statement successfully compiled */` |
|     154 |  3404 | `	return SXRET_OK;` |
|      78 |  3405 |  |
|       - |  3406 | `/*` |
|       - |  3407 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3408 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3409 | ` * failure.` |
|       - |  3410 | ` */` |
|      20 |  3411 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3412 |  |
|       - |  3413 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3414 | `	sxu32 nRawObj;` |
|      10 |  3415 | `	sxu32 nObjIdx;` |
|       - |  3416 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3417 | `	 * a PHP block.` |
|       - |  3418 | `	 */` |
|      10 |  3419 | `Consume:` |
|      21 |  3420 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3421 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3422 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3423 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3424 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3425 | `			return SXERR_ABORT;` |
|       - |  3426 | `		}` |
|       - |  3427 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3428 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3429 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3430 | `		++nRawObj;` |
|     ! 0 |  3431 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3432 | `	}` |
|      21 |  3433 | `	if( nRawObj > 0 ){` |
|       - |  3434 | `		/* Emit the consume instruction */` |
|     ! 0 |  3435 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3436 | `	}` |
|      21 |  3437 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3438 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3439 | `		/* Reset the token set */` |
|     ! 0 |  3440 | `		SySetReset(pTokenSet);` |
|       - |  3441 | `		/* Tokenize input */` |
|     ! 0 |  3442 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3443 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3444 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3445 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3446 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3447 | `		/* Advance the stream cursor */` |
|     ! 0 |  3448 | `		pGen->pRawIn++;` |
|       - |  3449 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3450 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3451 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3452 | `			sxi32 rc;` |
|       - |  3453 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3454 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3455 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3456 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3457 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3458 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3459 | `				return SXERR_ABORT;` |
|     ! 0 |  3460 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3461 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3462 | `			}` |
|     ! 0 |  3463 | `			goto Consume;` |
|       - |  3464 | `		}` |
|     ! 0 |  3465 | `	}else{` |
|       - |  3466 | `		/* No more chunks to process */` |
|      21 |  3467 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3468 | `		return SXERR_EOF;` |
|       - |  3469 | `	}` |
|     ! 0 |  3470 | `	return SXRET_OK;` |
|      11 |  3471 |  |
|       - |  3472 | `/*` |
|       - |  3473 | ` * Compile a PHP block.` |
|       - |  3474 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3475 | ` * optionally delimited by braces {}.` |
|       - |  3476 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3477 | ` * and this function takes care of generating the appropriate error` |
|       - |  3478 | ` * message.` |
|       - |  3479 | ` */` |
|  319720 |  3480 | `static sxi32 PH7_CompileBlock(` |
|       - |  3481 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3482 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3483 | `	)` |
|       2 |  3484 |  |
|       - |  3485 | `	sxi32 rc;` |
|       - |  3486 | `	sxu32 nLine;` |
|  319722 |  3487 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  318314 |  3488 | `		nLine = pGen->pIn->nLine;` |
|  318314 |  3489 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  318314 |  3490 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3491 | `			return SXERR_ABORT;` |
|       - |  3492 | `		}` |
|  318314 |  3493 | `		pGen->pIn++;` |
|       - |  3494 | `		/* Compile until we hit the closing braces '}' */` |
|  439360 |  3495 | `		for(;;){` |
|  878722 |  3496 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3497 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3498 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3499 | `			 	   return SXERR_ABORT;` |
|       - |  3500 | `				}` |
|      21 |  3501 | `				if( rc == SXERR_EOF ){` |
|       - |  3502 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3503 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3504 | `					break;` |
|       - |  3505 | `				}` |
|     ! 0 |  3506 | `			}` |
|  878702 |  3507 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3508 | `				/* Closing braces found,break immediately*/` |
|  318294 |  3509 | `				pGen->pIn++;` |
|  318294 |  3510 | `				break;` |
|       - |  3511 | `			}` |
|       - |  3512 | `			/* Compile a single statement */` |
|  560410 |  3513 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  560410 |  3514 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3515 | `				return SXERR_ABORT;` |
|       - |  3516 | `			}` |
|       2 |  3517 | `		}` |
|  318314 |  3518 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  160566 |  3519 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3520 | `		pGen->pIn++;` |
|     ! 0 |  3521 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3522 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3523 | `			return SXERR_ABORT;` |
|       - |  3524 | `		}` |
|       - |  3525 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3526 | `		for(;;){` |
|     ! 0 |  3527 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3528 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3529 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3530 | `			 	   return SXERR_ABORT;` |
|       - |  3531 | `				}` |
|     ! 0 |  3532 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3533 | `					/* No more token to process */` |
|     ! 0 |  3534 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3535 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3536 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3537 | `					}` |
|     ! 0 |  3538 | `					break;` |
|       - |  3539 | `				}` |
|     ! 0 |  3540 | `			}` |
|     ! 0 |  3541 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3542 | `				sxi32 nKwrd;` |
|       - |  3543 | `				/* Keyword found */` |
|     ! 0 |  3544 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3545 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3546 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3547 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3548 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3549 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3550 | `						}` |
|     ! 0 |  3551 | `						break;` |
|       - |  3552 | `				}` |
|     ! 0 |  3553 | `			}` |
|       - |  3554 | `			/* Compile a single statement */` |
|     ! 0 |  3555 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3556 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3557 | `				return SXERR_ABORT;` |
|       - |  3558 | `			}` |
|     ! 0 |  3559 | `		}` |
|     ! 0 |  3560 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3561 | `	}else{` |
|       - |  3562 | `		/* Compile a single statement */` |
|    1410 |  3563 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1410 |  3564 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3565 | `			return SXERR_ABORT;` |
|       - |  3566 | `		}` |
|       - |  3567 | `	}` |
|       - |  3568 | `	/* Jump trailing semi-colons ';' */` |
|  319722 |  3569 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3570 | `		pGen->pIn++;` |
|     ! 0 |  3571 | `	}` |
|  319722 |  3572 | `	return SXRET_OK;` |
|  159862 |  3573 |  |
|       - |  3574 | `/*` |
|       - |  3575 | ` * Compile the gentle 'while' statement.` |
|       - |  3576 | ` * According to the PHP language reference` |
|       - |  3577 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3578 | ` *  The basic form of a while statement is:` |
|       - |  3579 | ` *  while (expr)` |
|       - |  3580 | ` *   statement` |
|       - |  3581 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3582 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3583 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3584 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3585 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3586 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3587 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3588 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3589 | ` *  while (expr):` |
|       - |  3590 | ` *    statement` |
|       - |  3591 | ` *   endwhile;` |
|       - |  3592 | ` */` |
|   11710 |  3593 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3594 |  |
|   11712 |  3595 | `	GenBlock *pWhileBlock = 0;` |
|   11712 |  3596 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3597 | `	sxu32 nFalseJump;` |
|       - |  3598 | `	sxu32 nLine;` |
|       - |  3599 | `	sxi32 rc;` |
|   11712 |  3600 | `	nLine = pGen->pIn->nLine;` |
|       - |  3601 | `	/* Jump the 'while' keyword */` |
|   11712 |  3602 | `	pGen->pIn++;` |
|   11712 |  3603 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3604 | `		/* Syntax error */` |
|     ! 0 |  3605 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3606 | `		if( rc == SXERR_ABORT ){` |
|       - |  3607 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3608 | `			return SXERR_ABORT;` |
|       - |  3609 | `		}` |
|     ! 0 |  3610 | `		goto Synchronize;` |
|       - |  3611 | `	}` |
|       - |  3612 | `	/* Jump the left parenthesis '(' */` |
|   11712 |  3613 | `	pGen->pIn++;` |
|       - |  3614 | `	/* Create the loop block */` |
|   11712 |  3615 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11712 |  3616 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3617 | `		return SXERR_ABORT;` |
|       - |  3618 | `	}` |
|       - |  3619 | `	/* Delimit the condition */` |
|   11712 |  3620 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11712 |  3621 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3622 | `		/* Empty expression */` |
|       3 |  3623 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3624 | `		if( rc == SXERR_ABORT ){` |
|       - |  3625 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3626 | `			return SXERR_ABORT;` |
|       - |  3627 | `		}` |
|       1 |  3628 | `	}` |
|       - |  3629 | `	/* Swap token streams */` |
|   11712 |  3630 | `	pTmp = pGen->pEnd;` |
|   11712 |  3631 | `	pGen->pEnd = pEnd;` |
|       - |  3632 | `	/* Compile the expression */` |
|   11712 |  3633 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11712 |  3634 | `	if( rc == SXERR_ABORT ){` |
|       - |  3635 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3636 | `		return SXERR_ABORT;` |
|       - |  3637 | `	}` |
|       - |  3638 | `	/* Update token stream */` |
|   11712 |  3639 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3640 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3641 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3642 | `			return SXERR_ABORT;` |
|       - |  3643 | `		}` |
|     ! 0 |  3644 | `		pGen->pIn++;` |
|     ! 0 |  3645 | `	}` |
|       - |  3646 | `	/* Synchronize pointers */` |
|   11712 |  3647 | `	pGen->pIn  = &pEnd[1];` |
|   11712 |  3648 | `	pGen->pEnd = pTmp;` |
|       - |  3649 | `	/* Emit the false jump */` |
|   11712 |  3650 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3651 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11712 |  3652 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3653 | `	/* Compile the loop body */` |
|   11712 |  3654 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11712 |  3655 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3656 | `		return SXERR_ABORT;` |
|       - |  3657 | `	}` |
|       - |  3658 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11712 |  3659 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3660 | `	/* Fix all jumps now the destination is resolved */` |
|   11712 |  3661 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3662 | `	/* Release the loop block */` |
|   11712 |  3663 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3664 | `	/* Statement successfully compiled */` |
|   11712 |  3665 | `	return SXRET_OK;` |
|     ! 0 |  3666 | `Synchronize:` |
|       - |  3667 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3668 | `	 * compiling this erroneous block.` |
|       - |  3669 | `	 */` |
|     ! 0 |  3670 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3671 | `		pGen->pIn++;` |
|     ! 0 |  3672 | `	}` |
|     ! 0 |  3673 | `	return SXRET_OK;` |
|    5857 |  3674 |  |
|       - |  3675 | `/*` |
|       - |  3676 | ` * Compile the ugly do..while() statement.` |
|       - |  3677 | ` * According to the PHP language reference` |
|       - |  3678 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3679 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3680 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3681 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3682 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3683 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3684 | ` *  would end immediately).` |
|       - |  3685 | ` *  There is just one syntax for do-while loops:` |
|       - |  3686 | ` *  <?php` |
|       - |  3687 | ` *  $i = 0;` |
|       - |  3688 | ` *  do {` |
|       - |  3689 | ` *   echo $i;` |
|       - |  3690 | ` *  } while ($i > 0);` |
|       - |  3691 | ` * ?>` |
|       - |  3692 | ` */` |
|       2 |  3693 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3694 |  |
|       3 |  3695 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3696 | `	GenBlock *pDoBlock = 0;` |
|       - |  3697 | `	sxu32 nLine;` |
|       - |  3698 | `	sxi32 rc;` |
|       3 |  3699 | `	nLine = pGen->pIn->nLine;` |
|       - |  3700 | `	/* Jump the 'do' keyword */` |
|       3 |  3701 | `	pGen->pIn++;` |
|       - |  3702 | `	/* Create the loop block */` |
|       3 |  3703 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3704 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3705 | `		return SXERR_ABORT;` |
|       - |  3706 | `	}` |
|       - |  3707 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3708 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3709 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3710 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3711 | `		return SXERR_ABORT;` |
|       - |  3712 | `	}` |
|       3 |  3713 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3714 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3715 | `	}` |
|       3 |  3716 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3717 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3718 | `			/* Missing 'while' statement */` |
|       3 |  3719 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3720 | `			if( rc == SXERR_ABORT ){` |
|       - |  3721 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3722 | `				return SXERR_ABORT;` |
|       - |  3723 | `			}` |
|       3 |  3724 | `			goto Synchronize;` |
|       - |  3725 | `	}` |
|       - |  3726 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3727 | `	pGen->pIn++;` |
|     ! 0 |  3728 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3729 | `		/* Syntax error */` |
|     ! 0 |  3730 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3731 | `		if( rc == SXERR_ABORT ){` |
|       - |  3732 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3733 | `			return SXERR_ABORT;` |
|       - |  3734 | `		}` |
|     ! 0 |  3735 | `		goto Synchronize;` |
|       - |  3736 | `	}` |
|       - |  3737 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3738 | `	pGen->pIn++;` |
|       - |  3739 | `	/* Delimit the condition */` |
|     ! 0 |  3740 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3741 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3742 | `		/* Empty expression */` |
|     ! 0 |  3743 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3744 | `		if( rc == SXERR_ABORT ){` |
|       - |  3745 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3746 | `			return SXERR_ABORT;` |
|       - |  3747 | `		}` |
|     ! 0 |  3748 | `		goto Synchronize;` |
|       - |  3749 | `	}` |
|       - |  3750 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3751 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3752 | `		JumpFixup *aPost;` |
|       - |  3753 | `		VmInstr *pInstr;` |
|       - |  3754 | `		sxu32 nJumpDest;` |
|       - |  3755 | `		sxu32 n;` |
|     ! 0 |  3756 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3757 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3758 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3759 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3760 | `			if( pInstr ){` |
|       - |  3761 | `				/* Fix */` |
|     ! 0 |  3762 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3763 | `			}` |
|     ! 0 |  3764 | `		}` |
|     ! 0 |  3765 | `	}` |
|       - |  3766 | `	/* Swap token streams */` |
|     ! 0 |  3767 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3768 | `	pGen->pEnd = pEnd;` |
|       - |  3769 | `	/* Compile the expression */` |
|     ! 0 |  3770 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3771 | `	if( rc == SXERR_ABORT ){` |
|       - |  3772 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3773 | `		return SXERR_ABORT;` |
|       - |  3774 | `	}` |
|       - |  3775 | `	/* Update token stream */` |
|     ! 0 |  3776 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3777 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3778 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3779 | `			return SXERR_ABORT;` |
|       - |  3780 | `		}` |
|     ! 0 |  3781 | `		pGen->pIn++;` |
|     ! 0 |  3782 | `	}` |
|     ! 0 |  3783 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3784 | `	pGen->pEnd = pTmp;` |
|       - |  3785 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3786 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3787 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3788 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3789 | `	/* Release the loop block */` |
|     ! 0 |  3790 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3791 | `	/* Statement successfully compiled */` |
|     ! 0 |  3792 | `	return SXRET_OK;` |
|       1 |  3793 | `Synchronize:` |
|       - |  3794 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3795 | `	 * compiling this erroneous block.` |
|       - |  3796 | `	 */` |
|       3 |  3797 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3798 | `		pGen->pIn++;` |
|     ! 0 |  3799 | `	}` |
|       3 |  3800 | `	return SXRET_OK;` |
|       2 |  3801 |  |
|       - |  3802 | `/*` |
|       - |  3803 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3804 | ` * According to the PHP language reference` |
|       - |  3805 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3806 | ` *  The syntax of a for loop is:` |
|       - |  3807 | ` *  for (expr1; expr2; expr3)` |
|       - |  3808 | ` *   statement` |
|       - |  3809 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3810 | ` *  the beginning of the loop.` |
|       - |  3811 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3812 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3813 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3814 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3815 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3816 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3817 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3818 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3819 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3820 | ` *  of using the for truth expression.` |
|       - |  3821 | ` */` |
|   11714 |  3822 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3823 |  |
|   11716 |  3824 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11716 |  3825 | `	GenBlock *pForBlock = 0;` |
|       - |  3826 | `	sxu32 nFalseJump;` |
|       - |  3827 | `	sxu32 nLine;` |
|       - |  3828 | `	sxi32 rc;` |
|   11716 |  3829 | `	nLine = pGen->pIn->nLine;` |
|       - |  3830 | `	/* Jump the 'for' keyword */` |
|   11716 |  3831 | `	pGen->pIn++;` |
|   11716 |  3832 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3833 | `		/* Syntax error */` |
|     ! 0 |  3834 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3835 | `		if( rc == SXERR_ABORT ){` |
|       - |  3836 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3837 | `			return SXERR_ABORT;` |
|       - |  3838 | `		}` |
|     ! 0 |  3839 | `		return SXRET_OK;` |
|       - |  3840 | `	}` |
|       - |  3841 | `	/* Jump the left parenthesis '(' */` |
|   11716 |  3842 | `	pGen->pIn++;` |
|       - |  3843 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11716 |  3844 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11716 |  3845 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3846 | `		/* Empty expression */` |
|     ! 0 |  3847 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3848 | `		if( rc == SXERR_ABORT ){` |
|       - |  3849 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3850 | `			return SXERR_ABORT;` |
|       - |  3851 | `		}` |
|       - |  3852 | `		/* Synchronize */` |
|     ! 0 |  3853 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3854 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3855 | `			pGen->pIn++;` |
|     ! 0 |  3856 | `		}` |
|     ! 0 |  3857 | `		return SXRET_OK;` |
|       - |  3858 | `	}` |
|       - |  3859 | `	/* Swap token streams */` |
|   11716 |  3860 | `	pTmp = pGen->pEnd;` |
|   11716 |  3861 | `	pGen->pEnd = pEnd;` |
|       - |  3862 | `	/* Compile initialization expressions if available */` |
|   11716 |  3863 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3864 | `	/* Pop operand lvalues */` |
|   11716 |  3865 | `	if( rc == SXERR_ABORT ){` |
|       - |  3866 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3867 | `		return SXERR_ABORT;` |
|   11716 |  3868 | `	}else if( rc != SXERR_EMPTY ){` |
|   11714 |  3869 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5856 |  3870 | `	}` |
|   11716 |  3871 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3872 | `		/* Syntax error */` |
|     ! 0 |  3873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3874 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3875 | `		if( rc == SXERR_ABORT ){` |
|       - |  3876 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3877 | `			return SXERR_ABORT;` |
|       - |  3878 | `		}` |
|     ! 0 |  3879 | `		return SXRET_OK;` |
|       - |  3880 | `	}` |
|       - |  3881 | `	/* Jump the trailing ';' */` |
|   11716 |  3882 | `	pGen->pIn++;` |
|       - |  3883 | `	/* Create the loop block */` |
|   11716 |  3884 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11716 |  3885 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3886 | `		return SXERR_ABORT;` |
|       - |  3887 | `	}` |
|       - |  3888 | `	/* Deffer continue jumps */` |
|   11716 |  3889 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3890 | `	/* Compile the condition */` |
|   11716 |  3891 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11716 |  3892 | `	if( rc == SXERR_ABORT ){` |
|       - |  3893 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3894 | `		return SXERR_ABORT;` |
|   11716 |  3895 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3896 | `		/* Emit the false jump */` |
|   11714 |  3897 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3898 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11714 |  3899 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5856 |  3900 | `	}` |
|   11716 |  3901 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3902 | `		/* Syntax error */` |
|       5 |  3903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3904 | `			"for: Expected ';' after conditionals expressions");` |
|       5 |  3905 | `		if( rc == SXERR_ABORT ){` |
|       - |  3906 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3907 | `			return SXERR_ABORT;` |
|       - |  3908 | `		}` |
|       5 |  3909 | `		return SXRET_OK;` |
|       - |  3910 | `	}` |
|       - |  3911 | `	/* Jump the trailing ';' */` |
|   11712 |  3912 | `	pGen->pIn++;` |
|       - |  3913 | `	/* Save the post condition stream */` |
|   11712 |  3914 | `	pPostStart = pGen->pIn;` |
|       - |  3915 | `	/* Compile the loop body */` |
|   11712 |  3916 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11712 |  3917 | `	pGen->pEnd = pTmp;` |
|   11712 |  3918 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11712 |  3919 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3920 | `		return SXERR_ABORT;` |
|       - |  3921 | `	}` |
|       - |  3922 | `	/* Fix post-continue jumps */` |
|   11712 |  3923 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3924 | `		JumpFixup *aPost;` |
|       - |  3925 | `		VmInstr *pInstr;` |
|       - |  3926 | `		sxu32 nJumpDest;` |
|       - |  3927 | `		sxu32 n;` |
|      14 |  3928 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3929 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3930 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3931 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3932 | `			if( pInstr ){` |
|       - |  3933 | `				/* Fix jump */` |
|      14 |  3934 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3935 | `			}` |
|       8 |  3936 | `		}` |
|       6 |  3937 | `	}` |
|       - |  3938 | `	/* compile the post-expressions if available */` |
|   11712 |  3939 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3940 | `		pPostStart++;` |
|     ! 0 |  3941 | `	}` |
|   11712 |  3942 | `	if( pPostStart < pEnd ){` |
|       - |  3943 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11712 |  3944 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11712 |  3945 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11712 |  3946 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3947 | `			/* Syntax error */` |
|     ! 0 |  3948 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  3949 | `			if( rc == SXERR_ABORT ){` |
|       - |  3950 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3951 | `				return SXERR_ABORT;` |
|       - |  3952 | `			}` |
|     ! 0 |  3953 | `			return SXRET_OK;` |
|       - |  3954 | `		}` |
|   11712 |  3955 | `		RE_SWAP_DELIMITER(pGen);` |
|   11712 |  3956 | `		if( rc == SXERR_ABORT ){` |
|       - |  3957 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3958 | `			return SXERR_ABORT;` |
|   11712 |  3959 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  3960 | `			/* Pop operand lvalue */` |
|   11712 |  3961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5855 |  3962 | `		}` |
|    5855 |  3963 | `	}` |
|       - |  3964 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11712 |  3965 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  3966 | `	/* Fix all jumps now the destination is resolved */` |
|   11712 |  3967 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3968 | `	/* Release the loop block */` |
|   11712 |  3969 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3970 | `	/* Statement successfully compiled */` |
|   11712 |  3971 | `	return SXRET_OK;` |
|    5859 |  3972 |  |
|       - |  3973 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  3974 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  3975 | ` * are allowed.` |
|       - |  3976 | ` */` |
|    6222 |  3977 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  3978 |  |
|    6224 |  3979 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6224 |  3980 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  3981 | `		/* Unexpected expression */` |
|     ! 0 |  3982 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  3983 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  3984 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  3985 | `			rc = SXERR_INVALID;` |
|     ! 0 |  3986 | `		}` |
|     ! 0 |  3987 | `	}` |
|    6224 |  3988 | `	return rc;` |
|       2 |  3989 |  |
|       - |  3990 | `/*` |
|       - |  3991 | ` * Compile the 'foreach' statement.` |
|       - |  3992 | ` * According to the PHP language reference` |
|       - |  3993 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  3994 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  3995 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  3996 | ` *  is a minor but useful extension of the first:` |
|       - |  3997 | ` *  foreach (array_expression as $value)` |
|       - |  3998 | ` *    statement` |
|       - |  3999 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4000 | ` *   statement` |
|       - |  4001 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4002 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4003 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4004 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4005 | ` *  to the variable $key on each loop.` |
|       - |  4006 | ` *  Note:` |
|       - |  4007 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4008 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4009 | ` *  Note:` |
|       - |  4010 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4011 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4012 | ` *  or after the foreach without resetting it.` |
|       - |  4013 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4014 | ` *  of copying the value.` |
|       - |  4015 | ` */` |
|    3168 |  4016 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  4017 |  |
|    3170 |  4018 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3170 |  4019 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3170 |  4020 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4021 | `	ph7_foreach_info *pInfo;` |
|       - |  4022 | `	sxu32 nFalseJump;` |
|       - |  4023 | `	VmInstr *pInstr;` |
|       - |  4024 | `	sxu32 nLine;` |
|       - |  4025 | `	sxi32 rc;` |
|    3170 |  4026 | `	nLine = pGen->pIn->nLine;` |
|       - |  4027 | `	/* Jump the 'foreach' keyword */` |
|    3170 |  4028 | `	pGen->pIn++;` |
|    3170 |  4029 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4030 | `		/* Syntax error */` |
|     ! 0 |  4031 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4032 | `		if( rc == SXERR_ABORT ){` |
|       - |  4033 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4034 | `			return SXERR_ABORT;` |
|       - |  4035 | `		}` |
|     ! 0 |  4036 | `		goto Synchronize;` |
|       - |  4037 | `	}` |
|       - |  4038 | `	/* Jump the left parenthesis '(' */` |
|    3170 |  4039 | `	pGen->pIn++;` |
|       - |  4040 | `	/* Create the loop block */` |
|    3170 |  4041 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3170 |  4042 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4043 | `		return SXERR_ABORT;` |
|       - |  4044 | `	}` |
|       - |  4045 | `	/* Delimit the expression */` |
|    3170 |  4046 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3170 |  4047 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4048 | `		/* Empty expression */` |
|     ! 0 |  4049 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4050 | `		if( rc == SXERR_ABORT ){` |
|       - |  4051 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4052 | `			return SXERR_ABORT;` |
|       - |  4053 | `		}` |
|       - |  4054 | `		/* Synchronize */` |
|     ! 0 |  4055 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4056 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4057 | `			pGen->pIn++;` |
|     ! 0 |  4058 | `		}` |
|     ! 0 |  4059 | `		return SXRET_OK;` |
|       - |  4060 | `	}` |
|       - |  4061 | `	/* Compile the array expression */` |
|    3170 |  4062 | `	pCur = pGen->pIn;` |
|   21282 |  4063 | `	while( pCur < pEnd ){` |
|   21282 |  4064 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3180 |  4065 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3180 |  4066 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4067 | `				/* Break with the first 'as' found */` |
|    3170 |  4068 | `				break;` |
|       - |  4069 | `			}` |
|       5 |  4070 | `		}` |
|       - |  4071 | `		/* Advance the stream cursor */` |
|   18114 |  4072 | `		pCur++;` |
|       2 |  4073 | `	}` |
|    3170 |  4074 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4075 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4076 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4077 | `		if( rc == SXERR_ABORT ){` |
|       - |  4078 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4079 | `			return SXERR_ABORT;` |
|       - |  4080 | `		}` |
|     ! 0 |  4081 | `		goto Synchronize;` |
|       - |  4082 | `	}` |
|       - |  4083 | `	/* Swap token streams */` |
|    3170 |  4084 | `	pTmp = pGen->pEnd;` |
|    3170 |  4085 | `	pGen->pEnd = pCur;` |
|    3170 |  4086 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3170 |  4087 | `	if( rc == SXERR_ABORT ){` |
|       - |  4088 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4089 | `		return SXERR_ABORT;` |
|       - |  4090 | `	}` |
|       - |  4091 | `	/* Update token stream */` |
|    3170 |  4092 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4093 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4094 | `		if( rc == SXERR_ABORT ){` |
|       - |  4095 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4096 | `			return SXERR_ABORT;` |
|       - |  4097 | `		}` |
|     ! 0 |  4098 | `		pGen->pIn++;` |
|     ! 0 |  4099 | `	}` |
|    3170 |  4100 | `	pCur++; /* Jump the 'as' keyword */` |
|    3170 |  4101 | `	pGen->pIn = pCur;` |
|    3170 |  4102 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4103 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4104 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4105 | `			return SXERR_ABORT;` |
|       - |  4106 | `		}` |
|     ! 0 |  4107 | `	}` |
|       - |  4108 | `	/* Create the foreach context */` |
|    3170 |  4109 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3170 |  4110 | `	if( pInfo == 0 ){` |
|     ! 0 |  4111 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4112 | `		return SXERR_ABORT;` |
|       - |  4113 | `	}` |
|       - |  4114 | `	/* Zero the structure */` |
|    3170 |  4115 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4116 | `	/* Initialize structure fields */` |
|    3170 |  4117 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4118 | `	/* Check if we have a key field */` |
|    9556 |  4119 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6388 |  4120 | `		pCur++;` |
|       2 |  4121 | `	}` |
|    3170 |  4122 | `	if( pCur < pEnd ){` |
|       - |  4123 | `		/* Compile the expression holding the key name */` |
|    3066 |  4124 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4125 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4126 | `			if( rc == SXERR_ABORT ){` |
|       - |  4127 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4128 | `				return SXERR_ABORT;` |
|       - |  4129 | `			}` |
|     ! 0 |  4130 | `		}else{` |
|    3066 |  4131 | `			pGen->pEnd = pCur;` |
|    3066 |  4132 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3066 |  4133 | `			if( rc == SXERR_ABORT ){` |
|       - |  4134 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4135 | `				return SXERR_ABORT;` |
|       - |  4136 | `			}` |
|    3066 |  4137 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3066 |  4138 | `			if( pInstr->p3 ){` |
|       - |  4139 | `				/* Record key name */` |
|    3066 |  4140 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1532 |  4141 | `			}` |
|    3066 |  4142 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4143 | `		}` |
|    3066 |  4144 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1532 |  4145 | `	}` |
|    3170 |  4146 | `	pGen->pEnd = pEnd;` |
|    3170 |  4147 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4148 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4149 | `		if( rc == SXERR_ABORT ){` |
|       - |  4150 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4151 | `			return SXERR_ABORT;` |
|       - |  4152 | `		}` |
|     ! 0 |  4153 | `		goto Synchronize;` |
|       - |  4154 | `	}` |
|    3170 |  4155 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4156 | `		pGen->pIn++;` |
|       - |  4157 | `		/* Pass by reference  */` |
|      11 |  4158 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4159 | `	}` |
|       - |  4160 | `	/* Check if the value target is list() */` |
|    3170 |  4161 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4162 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4163 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4164 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4165 | `		 */` |
|       - |  4166 | `		static int iForeachListCnt = 0;` |
|       - |  4167 | `		char zTmp[128];` |
|       - |  4168 | `		sxu32 nLen;` |
|       - |  4169 | `		char *zDup;` |
|      10 |  4170 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4171 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4172 | `		if( zDup == 0 ){` |
|     ! 0 |  4173 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4174 | `			return SXERR_ABORT;` |
|       - |  4175 | `		}` |
|      10 |  4176 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4177 | `		/* Save list() token boundaries */` |
|      10 |  4178 | `		pListStart = pGen->pIn;` |
|       - |  4179 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4180 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4181 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4182 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4183 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4184 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4185 | `				return SXERR_ABORT;` |
|       - |  4186 | `			}` |
|       3 |  4187 | `			goto Synchronize;` |
|       - |  4188 | `		}` |
|       7 |  4189 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4190 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4191 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4192 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4193 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4194 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4195 | `				return SXERR_ABORT;` |
|       - |  4196 | `			}` |
|     ! 0 |  4197 | `			goto Synchronize;` |
|       - |  4198 | `		}` |
|       7 |  4199 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4200 | `		pListEnd = pGen->pIn;` |
|       7 |  4201 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3165 |  4202 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4203 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4204 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4205 | `		 */` |
|       - |  4206 | `		static int iForeachShortListCnt = 0;` |
|       - |  4207 | `		char zTmp[128];` |
|       - |  4208 | `		sxu32 nLen;` |
|       - |  4209 | `		char *zDup;` |
|       3 |  4210 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4211 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4212 | `		if( zDup == 0 ){` |
|     ! 0 |  4213 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4214 | `			return SXERR_ABORT;` |
|       - |  4215 | `		}` |
|       3 |  4216 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4217 | `		/* Save [...] token boundaries */` |
|       3 |  4218 | `		pListStart = pGen->pIn;` |
|       - |  4219 | `		/* Advance past [...] */` |
|       3 |  4220 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4221 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4222 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4223 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4224 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4225 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4226 | `				return SXERR_ABORT;` |
|       - |  4227 | `			}` |
|     ! 0 |  4228 | `			goto Synchronize;` |
|       - |  4229 | `		}` |
|       3 |  4230 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4231 | `		pListEnd = pGen->pIn;` |
|       3 |  4232 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4233 | `	}else{` |
|       - |  4234 | `		/* Compile the expression holding the value name */` |
|    3160 |  4235 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3160 |  4236 | `		if( rc == SXERR_ABORT ){` |
|       - |  4237 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4238 | `			return SXERR_ABORT;` |
|       - |  4239 | `		}` |
|    3160 |  4240 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3160 |  4241 | `		if( pInstr->p3 ){` |
|       - |  4242 | `			/* Record value name */` |
|    3160 |  4243 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1579 |  4244 | `		}` |
|       - |  4245 | `	}` |
|       - |  4246 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3168 |  4247 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4248 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3168 |  4249 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4250 | `	/* Record the first instruction to execute */` |
|    3168 |  4251 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4252 | `	/* Emit the FOREACH_STEP instruction */` |
|    3168 |  4253 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4254 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3168 |  4255 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4256 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3168 |  4257 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4258 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4259 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4260 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4261 | `		 */` |
|       9 |  4262 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4263 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4264 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4265 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4266 | `		 */` |
|       9 |  4267 | `		pSavedIn = pGen->pIn;` |
|       9 |  4268 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4269 | `		pGen->pIn = pListStart;` |
|       9 |  4270 | `		pGen->pEnd = pListEnd;` |
|       9 |  4271 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4272 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4273 | `		}else{` |
|       7 |  4274 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4275 | `		}` |
|       9 |  4276 | `		pGen->pIn = pSavedIn;` |
|       9 |  4277 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4278 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4279 | `			return SXERR_ABORT;` |
|       - |  4280 | `		}` |
|       - |  4281 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4282 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4283 | `	}` |
|       - |  4284 | `	/* Compile the loop body */` |
|    3168 |  4285 | `	pGen->pIn = &pEnd[1];` |
|    3168 |  4286 | `	pGen->pEnd = pTmp;` |
|    3168 |  4287 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3168 |  4288 | `	if( rc == SXERR_ABORT ){` |
|       - |  4289 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4290 | `		return SXERR_ABORT;` |
|       - |  4291 | `	}` |
|       - |  4292 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3168 |  4293 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4294 | `	/* Fix all jumps now the destination is resolved */` |
|    3168 |  4295 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4296 | `	/* Release the loop block */` |
|    3168 |  4297 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4298 | `	/* Statement successfully compiled */` |
|    3168 |  4299 | `	return SXRET_OK;` |
|       1 |  4300 | `Synchronize:` |
|       - |  4301 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4302 | `	 * compiling this erroneous block.` |
|       - |  4303 | `	 */` |
|       3 |  4304 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4305 | `		pGen->pIn++;` |
|     ! 0 |  4306 | `	}` |
|       3 |  4307 | `	return SXRET_OK;` |
|    1586 |  4308 |  |
|       - |  4309 | `/*` |
|       - |  4310 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4311 | ` * According to the PHP language reference` |
|       - |  4312 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4313 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4314 | ` *  that is similar to that of C:` |
|       - |  4315 | ` *  if (expr)` |
|       - |  4316 | ` *   statement` |
|       - |  4317 | ` *  else construct:` |
|       - |  4318 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4319 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4320 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4321 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4322 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4323 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4324 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4325 | ` *  elseif` |
|       - |  4326 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4327 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4328 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4329 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4330 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4331 | ` *   <?php` |
|       - |  4332 | ` *    if ($a > $b) {` |
|       - |  4333 | ` *     echo "a is bigger than b";` |
|       - |  4334 | ` *    } elseif ($a == $b) {` |
|       - |  4335 | ` *     echo "a is equal to b";` |
|       - |  4336 | ` *    } else {` |
|       - |  4337 | ` *     echo "a is smaller than b";` |
|       - |  4338 | ` *    }` |
|       - |  4339 | ` *    ?>` |
|       - |  4340 | ` */` |
|  116228 |  4341 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4342 |  |
|  116230 |  4343 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  116230 |  4344 | `	GenBlock *pCondBlock = 0;` |
|       - |  4345 | `	sxu32 nJumpIdx;` |
|       - |  4346 | `	sxu32 nKeyID;` |
|       - |  4347 | `	sxi32 rc;` |
|       - |  4348 | `	/* Jump the 'if' keyword */` |
|  116230 |  4349 | `	pGen->pIn++;` |
|  116230 |  4350 | `	pToken = pGen->pIn;` |
|       - |  4351 | `	/* Create the conditional block */` |
|  116230 |  4352 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  116230 |  4353 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4354 | `		return SXERR_ABORT;` |
|       - |  4355 | `	}` |
|       - |  4356 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   63931 |  4357 | `	for(;;){` |
|  127864 |  4358 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4359 | `			/* Syntax error */` |
|     ! 0 |  4360 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4361 | `				pToken--;` |
|     ! 0 |  4362 | `			}` |
|     ! 0 |  4363 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4364 | `			if( rc == SXERR_ABORT ){` |
|       - |  4365 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4366 | `				return SXERR_ABORT;` |
|       - |  4367 | `			}` |
|     ! 0 |  4368 | `			goto Synchronize;` |
|       - |  4369 | `		}` |
|       - |  4370 | `		/* Jump the left parenthesis '(' */` |
|  127864 |  4371 | `		pToken++;` |
|       - |  4372 | `		/* Delimit the condition */` |
|  127864 |  4373 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  127864 |  4374 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4375 | `			/* Syntax error */` |
|     ! 0 |  4376 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4377 | `				pToken--;` |
|     ! 0 |  4378 | `			}` |
|     ! 0 |  4379 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4380 | `			if( rc == SXERR_ABORT ){` |
|       - |  4381 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4382 | `				return SXERR_ABORT;` |
|       - |  4383 | `			}` |
|     ! 0 |  4384 | `			goto Synchronize;` |
|       - |  4385 | `		}` |
|       - |  4386 | `		/* Swap token streams */` |
|  127864 |  4387 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4388 | `		/* Compile the condition */` |
|  127864 |  4389 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4390 | `		/* Update token stream */` |
|  127864 |  4391 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4392 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4393 | `			pGen->pIn++;` |
|     ! 0 |  4394 | `		}` |
|  127864 |  4395 | `		pGen->pIn  = &pEnd[1];` |
|  127864 |  4396 | `		pGen->pEnd = pTmp;` |
|  127864 |  4397 | `		if( rc == SXERR_ABORT ){` |
|       - |  4398 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4399 | `			return SXERR_ABORT;` |
|       - |  4400 | `		}` |
|       - |  4401 | `		/* Emit the false jump */` |
|  127864 |  4402 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4403 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  127864 |  4404 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4405 | `		/* Compile the body */` |
|  127864 |  4406 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  127864 |  4407 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4408 | `			return SXERR_ABORT;` |
|       - |  4409 | `		}` |
|  127864 |  4410 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   34394 |  4411 | `			break;` |
|       - |  4412 | `		}` |
|       - |  4413 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   59080 |  4414 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   59080 |  4415 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   38012 |  4416 | `			break;` |
|       - |  4417 | `		}` |
|       - |  4418 | `		/* Emit the unconditional jump */` |
|   21070 |  4419 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4420 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21070 |  4421 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   21070 |  4422 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   15240 |  4423 | `			pToken = &pGen->pIn[1];` |
|   15240 |  4424 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5834 |  4425 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4719 |  4426 | `					break;` |
|       - |  4427 | `			}` |
|    5806 |  4428 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2902 |  4429 | `		}` |
|   11636 |  4430 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4431 | `		/* Synchronize cursors */` |
|   11636 |  4432 | `		pToken = pGen->pIn;` |
|       - |  4433 | `		/* Fix the false jump */` |
|   11636 |  4434 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4435 | `	} /* For(;;) */` |
|       - |  4436 | `	/* Fix the false jump */` |
|  116230 |  4437 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  116230 |  4438 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   47444 |  4439 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4440 | `			/* Compile the else block */` |
|    9436 |  4441 | `			pGen->pIn++;` |
|    9436 |  4442 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9436 |  4443 | `			if( rc == SXERR_ABORT ){` |
|       - |  4444 |  |
|     ! 0 |  4445 | `				return SXERR_ABORT;` |
|       - |  4446 | `			}` |
|    4717 |  4447 | `	}` |
|  116230 |  4448 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4449 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  116230 |  4450 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4451 | `	/* Release the conditional block */` |
|  116230 |  4452 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4453 | `	/* Statement successfully compiled */` |
|  116230 |  4454 | `	return SXRET_OK;` |
|     ! 0 |  4455 | `Synchronize:` |
|       - |  4456 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4457 | `	 */` |
|     ! 0 |  4458 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4459 | `		pGen->pIn++;` |
|     ! 0 |  4460 | `	}` |
|     ! 0 |  4461 | `	return SXRET_OK;` |
|   58116 |  4462 |  |
|       - |  4463 | `/*` |
|       - |  4464 | ` * Compile the global construct.` |
|       - |  4465 | ` * According to the PHP language reference` |
|       - |  4466 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4467 | ` *  to be used in that function.` |
|       - |  4468 | ` *  Example #1 Using global` |
|       - |  4469 | ` *  <?php` |
|       - |  4470 | ` *   $a = 1;` |
|       - |  4471 | ` *   $b = 2;` |
|       - |  4472 | ` *   function Sum()` |
|       - |  4473 | ` *   {` |
|       - |  4474 | ` *    global $a, $b;` |
|       - |  4475 | ` *    $b = $a + $b;` |
|       - |  4476 | ` *   }` |
|       - |  4477 | ` *   Sum();` |
|       - |  4478 | ` *   echo $b;` |
|       - |  4479 | ` *  ?>` |
|       - |  4480 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4481 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4482 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4483 | ` */` |
|      32 |  4484 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4485 |  |
|      34 |  4486 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4487 | `	sxi32 nExpr;` |
|       - |  4488 | `	sxi32 rc;` |
|       - |  4489 | `	/* Jump the 'global' keyword */` |
|      34 |  4490 | `	pGen->pIn++;` |
|      34 |  4491 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4492 | `		/* Nothing to process */` |
|     ! 0 |  4493 | `		return SXRET_OK;` |
|       - |  4494 | `	}` |
|      34 |  4495 | `	pTmp = pGen->pEnd;` |
|      34 |  4496 | `	nExpr = 0;` |
|      68 |  4497 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      36 |  4498 | `		if( pGen->pIn < pNext ){` |
|      36 |  4499 | `			pGen->pEnd = pNext;` |
|      36 |  4500 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4501 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4502 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4503 | `					return SXERR_ABORT;` |
|       - |  4504 | `				}` |
|     ! 0 |  4505 | `			}else{` |
|      36 |  4506 | `				pGen->pIn++;` |
|      36 |  4507 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4508 | `					/* Emit a warning */` |
|     ! 0 |  4509 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4510 | `				}else{` |
|      36 |  4511 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      36 |  4512 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4513 | `						return SXERR_ABORT;` |
|      36 |  4514 | `					}else if(rc != SXERR_EMPTY ){` |
|      36 |  4515 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      36 |  4516 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4517 | `							/* Variable name, not a constant */` |
|      36 |  4518 | `							pLast->iP1 = 0;` |
|      17 |  4519 | `						}` |
|      36 |  4520 | `						nExpr++;` |
|      17 |  4521 | `					}` |
|       - |  4522 | `				}` |
|       - |  4523 | `			}` |
|      17 |  4524 | `		}` |
|       - |  4525 | `		/* Next expression in the stream */` |
|      36 |  4526 | `		pGen->pIn = pNext;` |
|       - |  4527 | `		/* Jump trailing commas */` |
|      38 |  4528 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4529 | `			pGen->pIn++;` |
|       1 |  4530 | `		}` |
|       2 |  4531 | `	}` |
|       - |  4532 | `	/* Restore token stream */` |
|      34 |  4533 | `	pGen->pEnd = pTmp;` |
|      34 |  4534 | `	if( nExpr > 0 ){` |
|       - |  4535 | `		/* Emit the uplink instruction */` |
|      34 |  4536 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      16 |  4537 | `	}` |
|      34 |  4538 | `	return SXRET_OK;` |
|      18 |  4539 |  |
|       - |  4540 | `/*` |
|       - |  4541 | ` * Compile the return statement.` |
|       - |  4542 | ` * According to the PHP language reference` |
|       - |  4543 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4544 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4545 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4546 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4547 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4548 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4549 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4550 | ` *  from within the main script file, then script execution end.` |
|       - |  4551 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4552 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4553 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4554 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4555 | ` */` |
|  169070 |  4556 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4557 |  |
|  169072 |  4558 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4559 | `	sxi32 rc;` |
|       - |  4560 | `	/* Jump the 'return' keyword */` |
|  169072 |  4561 | `	pGen->pIn++;` |
|  169072 |  4562 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4563 | `		/* Compile the expression */` |
|  169050 |  4564 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  169050 |  4565 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4566 | `			return SXERR_ABORT;` |
|  169050 |  4567 | `		}else if(rc != SXERR_EMPTY ){` |
|  169050 |  4568 | `			nRet = 1;` |
|   84524 |  4569 | `		}` |
|   84524 |  4570 | `	}` |
|       - |  4571 | `	/* Emit the done instruction */` |
|  169072 |  4572 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  169072 |  4573 | `	return SXRET_OK;` |
|   84537 |  4574 |  |
|       - |  4575 | `/*` |
|       - |  4576 | ` * Compile a yield expression.` |
|       - |  4577 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4578 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4579 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4580 | ` */` |
|      34 |  4581 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  4582 |  |
|       - |  4583 | `	SyToken *pTmp, *pSplit;` |
|      36 |  4584 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      36 |  4585 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4586 | `	sxi32 rc;` |
|      17 |  4587 | `	(void)iCompileFlag;` |
|       - |  4588 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      36 |  4589 | `	pGen->pIn++;` |
|       - |  4590 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4591 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      36 |  4592 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4593 | `		/* Bare yield — no value */` |
|     ! 0 |  4594 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4595 | `		return SXRET_OK;` |
|       - |  4596 | `	}` |
|       - |  4597 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      36 |  4598 | `	pSplit = 0;` |
|       - |  4599 | `	{` |
|      36 |  4600 | `		SyToken *pCur = pGen->pIn;` |
|      36 |  4601 | `		sxi32 nNest = 0;` |
|      84 |  4602 | `		while( pCur < pGen->pEnd ){` |
|      56 |  4603 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4604 | `				nNest++;` |
|      56 |  4605 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4606 | `				nNest--;` |
|      56 |  4607 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 |  4608 | `				pSplit = pCur;` |
|       7 |  4609 | `				break;` |
|       - |  4610 | `			}` |
|      50 |  4611 | `			pCur++;` |
|       2 |  4612 | `		}` |
|       - |  4613 | `	}` |
|      36 |  4614 | `	pTmp = pGen->pEnd;` |
|      36 |  4615 | `	if( pSplit ){` |
|       - |  4616 | `		/* yield $key => $value */` |
|       7 |  4617 | `		pGen->pEnd = pSplit;` |
|       7 |  4618 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4619 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4620 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 |  4621 | `		pGen->pEnd = pTmp;` |
|       7 |  4622 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4623 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4624 | `		iP1 = 1;` |
|       7 |  4625 | `		iP2 = 1;` |
|       4 |  4626 | `	}else{` |
|       - |  4627 | `		/* yield $value */` |
|      30 |  4628 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      30 |  4629 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      30 |  4630 | `		if( rc != SXERR_EMPTY ){` |
|      30 |  4631 | `			iP1 = 1;` |
|      14 |  4632 | `		}` |
|       - |  4633 | `	}` |
|      36 |  4634 | `	pGen->pEnd = pTmp;` |
|      36 |  4635 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      36 |  4636 | `	return SXRET_OK;` |
|      19 |  4637 |  |
|       - |  4638 | `/*` |
|       - |  4639 | ` * Compile the die/exit language construct.` |
|       - |  4640 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4641 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4642 | ` */` |
|      88 |  4643 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 |  4644 |  |
|      90 |  4645 | `	sxi32 nExpr = 0;` |
|       - |  4646 | `	sxi32 rc;` |
|       - |  4647 | `	/* Jump the die/exit keyword */` |
|      90 |  4648 | `	pGen->pIn++;` |
|      90 |  4649 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4650 | `		/* Compile the expression */` |
|      90 |  4651 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 |  4652 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4653 | `			return SXERR_ABORT;` |
|      90 |  4654 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 |  4655 | `			nExpr = 1;` |
|      44 |  4656 | `		}` |
|      44 |  4657 | `	}` |
|       - |  4658 | `	/* Emit the HALT instruction */` |
|      90 |  4659 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 |  4660 | `	return SXRET_OK;` |
|      46 |  4661 |  |
|       - |  4662 | `/*` |
|       - |  4663 | ` * Compile the 'echo' language construct.` |
|       - |  4664 | ` */` |
|   11780 |  4665 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4666 |  |
|   11782 |  4667 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4668 | `	sxi32 rc;` |
|       - |  4669 | `	/* Jump the 'echo' keyword */` |
|   11782 |  4670 | `	pGen->pIn++;` |
|       - |  4671 | `	/* Compile arguments one after one */` |
|   11782 |  4672 | `	pTmp = pGen->pEnd;` |
|   24620 |  4673 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   12840 |  4674 | `		if( pGen->pIn < pNext ){` |
|   12840 |  4675 | `			pGen->pEnd = pNext;` |
|   12840 |  4676 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   12840 |  4677 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4678 | `				return SXERR_ABORT;` |
|   12840 |  4679 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4680 | `				/* Emit the consume instruction */` |
|   12816 |  4681 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6407 |  4682 | `			}` |
|    6419 |  4683 | `		}` |
|       - |  4684 | `		/* Jump trailing commas */` |
|   13898 |  4685 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    1060 |  4686 | `			pNext++;` |
|       2 |  4687 | `		}` |
|   12840 |  4688 | `		pGen->pIn = pNext;` |
|       2 |  4689 | `	}` |
|       - |  4690 | `	/* Restore token stream */` |
|   11782 |  4691 | `	pGen->pEnd = pTmp;` |
|   11782 |  4692 | `	return SXRET_OK;` |
|    5892 |  4693 |  |
|       - |  4694 | `/*` |
|       - |  4695 | ` * Compile the static statement.` |
|       - |  4696 | ` * According to the PHP language reference` |
|       - |  4697 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4698 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4699 | ` *  when program execution leaves this scope.` |
|       - |  4700 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4701 | ` * Symisc eXtension.` |
|       - |  4702 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4703 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4704 | ` *  Example` |
|       - |  4705 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4706 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4707 | ` */` |
|       2 |  4708 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 |  4709 |  |
|       - |  4710 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4711 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4712 | `	GenBlock *pBlock;` |
|       - |  4713 | `	SyString *pName;` |
|       - |  4714 | `	char *zDup;` |
|       - |  4715 | `	sxu32 nLine;` |
|       - |  4716 | `	sxi32 rc;` |
|       - |  4717 | `	/* Jump the static keyword */` |
|       3 |  4718 | `	nLine = pGen->pIn->nLine;` |
|       3 |  4719 | `	pGen->pIn++;` |
|       - |  4720 | `	/* Extract the enclosing function if any */` |
|       3 |  4721 | `	pBlock = pGen->pCurrent;` |
|       5 |  4722 | `	while( pBlock ){` |
|       5 |  4723 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 |  4724 | `			break;` |
|       - |  4725 | `		}` |
|       - |  4726 | `		/* Point to the upper block */` |
|       3 |  4727 | `		pBlock = pBlock->pParent;` |
|       1 |  4728 | `	}` |
|       3 |  4729 | `	if( pBlock == 0 ){` |
|       - |  4730 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4731 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4732 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4733 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4734 | `				return SXERR_ABORT;` |
|       - |  4735 | `			}` |
|     ! 0 |  4736 | `			goto Synchronize;` |
|       - |  4737 | `		}` |
|       - |  4738 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4739 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4740 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4741 | `			return SXERR_ABORT;` |
|     ! 0 |  4742 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4743 | `			/* Emit the POP instruction */` |
|     ! 0 |  4744 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4745 | `		}` |
|     ! 0 |  4746 | `		return SXRET_OK;` |
|       - |  4747 | `	}` |
|       3 |  4748 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4749 | `	/* Make sure we are dealing with a valid statement */` |
|       3 |  4750 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 |  4751 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4752 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4753 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4754 | `				return SXERR_ABORT;` |
|       - |  4755 | `			}` |
|       3 |  4756 | `			goto Synchronize;` |
|       - |  4757 | `	}` |
|     ! 0 |  4758 | `	pGen->pIn++;` |
|       - |  4759 | `	/* Extract variable name */` |
|     ! 0 |  4760 | `	pName = &pGen->pIn->sData;` |
|     ! 0 |  4761 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 |  4762 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4763 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4764 | `		goto Synchronize;` |
|       - |  4765 | `	}` |
|       - |  4766 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 |  4767 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 |  4768 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4769 | `	/* Duplicate variable name */` |
|     ! 0 |  4770 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 |  4771 | `	if( zDup == 0 ){` |
|     ! 0 |  4772 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4773 | `		return SXERR_ABORT;` |
|       - |  4774 | `	}` |
|     ! 0 |  4775 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4776 | `	/* Check if we have an expression to compile */` |
|     ! 0 |  4777 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4778 | `		SySet *pInstrContainer;` |
|       - |  4779 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4780 | `		 * Static variable can take any complex expression including function` |
|       - |  4781 | `		 * call as their initialization value.` |
|       - |  4782 | `		 * Example:` |
|       - |  4783 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4784 | `		 */` |
|     ! 0 |  4785 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4786 | `		/* Swap bytecode container */` |
|     ! 0 |  4787 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 |  4788 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4789 | `		/* Compile the expression */` |
|     ! 0 |  4790 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4791 | `		/* Emit the done instruction */` |
|     ! 0 |  4792 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4793 | `		/* Restore default bytecode container */` |
|     ! 0 |  4794 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  4795 | `	}` |
|       - |  4796 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 |  4797 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 |  4798 | `	return SXRET_OK;` |
|       1 |  4799 | `Synchronize:` |
|       - |  4800 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4801 | `	 * statement.` |
|       - |  4802 | `	 */` |
|       5 |  4803 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4804 | `		pGen->pIn++;` |
|       1 |  4805 | `	}` |
|       3 |  4806 | `	return SXRET_OK;` |
|       2 |  4807 |  |
|       - |  4808 | `/*` |
|       - |  4809 | ` * Compile the var statement.` |
|       - |  4810 | ` * Symisc Extension:` |
|       - |  4811 | ` *      var statement can be used outside of a class definition.` |
|       - |  4812 | ` */` |
|       4 |  4813 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4814 |  |
|       - |  4815 | `	sxu32 nLine;` |
|       - |  4816 | `	sxi32 rc;` |
|       5 |  4817 | `	nLine = pGen->pIn->nLine;` |
|       - |  4818 | `	/* Jump the 'var' keyword */` |
|       5 |  4819 | `	pGen->pIn++;` |
|       5 |  4820 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4821 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4822 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4823 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4824 | `			pGen->pIn++;` |
|     ! 0 |  4825 | `		}` |
|     ! 0 |  4826 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4827 | `			return SXERR_ABORT;` |
|       - |  4828 | `		}` |
|     ! 0 |  4829 | `	}else{` |
|       - |  4830 | `		/* Compile the expression */` |
|       5 |  4831 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4832 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4833 | `			return SXERR_ABORT;` |
|       5 |  4834 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4835 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4836 | `		}` |
|       - |  4837 | `	}` |
|       5 |  4838 | `	return SXRET_OK;` |
|       3 |  4839 |  |
|       - |  4840 | `/*` |
|       - |  4841 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4842 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4843 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4844 | ` */` |
|       - |  4845 | `/*` |
|       - |  4846 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4847 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4848 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4849 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4850 | ` *` |
|       - |  4851 | ` * Resolution order:` |
|       - |  4852 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4853 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4854 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4855 | ` *` |
|       - |  4856 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4857 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4858 | ` * Returns the (possibly new) literal index.` |
|       - |  4859 | ` */` |
|  346940 |  4860 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 |  4861 |  |
|       - |  4862 | `	ph7_value *pLit;` |
|       - |  4863 | `	const char *zLit;` |
|       - |  4864 | `	SyString sQualified;` |
|       - |  4865 | `	sxu32 nLit;` |
|       - |  4866 | `	sxu32 k;` |
|       - |  4867 | `	sxu32 nNewIdx;` |
|       - |  4868 | `	int hasNsSep;` |
|       - |  4869 | `	SyHashEntry *pImport;` |
|       - |  4870 | `	ph7_value *pNew;` |
|  346942 |  4871 | `	if( pFromImport ){` |
|  331536 |  4872 | `		*pFromImport = 0;` |
|  165767 |  4873 | `	}` |
|  346942 |  4874 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  346942 |  4875 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4876 | `		return nOrigIdx;` |
|       - |  4877 | `	}` |
|  346942 |  4878 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  346942 |  4879 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4880 | `	/* Skip if already qualified (contains backslash) */` |
|  346942 |  4881 | `	hasNsSep = 0;` |
| 3730232 |  4882 | `	for( k = 0; k < nLit; k++ ){` |
| 3383328 |  4883 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1691647 |  4884 | `	}` |
|  346942 |  4885 | `	if( hasNsSep ){` |
|      38 |  4886 | `		return nOrigIdx;` |
|       - |  4887 | `	}` |
|       - |  4888 | `	/* Check use imports first (works even outside namespaces) */` |
|  346906 |  4889 | `	SyBlobReset(&pGen->sWorker);` |
|  346906 |  4890 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  346906 |  4891 | `	if( pImport ){` |
|      38 |  4892 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4893 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4894 | `		if( pFromImport ){` |
|      18 |  4895 | `			*pFromImport = 1;` |
|       8 |  4896 | `		}` |
|      20 |  4897 | `	}else{` |
|  346870 |  4898 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  346782 |  4899 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4900 | `		}` |
|       - |  4901 | `		/* Prepend current namespace */` |
|      90 |  4902 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      90 |  4903 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      90 |  4904 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4905 | `	}` |
|       - |  4906 | `	/* Look up or create a new literal for the qualified name */` |
|     126 |  4907 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     126 |  4908 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 |  4909 | `		return nNewIdx; /* Already interned */` |
|       - |  4910 | `	}` |
|      74 |  4911 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      74 |  4912 | `	if( pNew == 0 ){` |
|     ! 0 |  4913 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4914 | `	}` |
|      74 |  4915 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      74 |  4916 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      74 |  4917 | `	return nNewIdx;` |
|  173472 |  4918 |  |
|       - |  4919 | `/*` |
|       - |  4920 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4921 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4922 | ` */` |
|   29438 |  4923 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4924 |  |
|       - |  4925 | `	SyHashEntry *pImport;` |
|       - |  4926 | `	/* Check use imports first */` |
|   29440 |  4927 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   29440 |  4928 | `	if( pImport ){` |
|      12 |  4929 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 |  4930 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 |  4931 | `		return;` |
|       - |  4932 | `	}` |
|       - |  4933 | `	/* Prepend current namespace if active */` |
|   29430 |  4934 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4935 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4936 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4937 | `	}` |
|   29430 |  4938 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   14721 |  4939 |  |
|       - |  4940 | `/*` |
|       - |  4941 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4942 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4943 | ` * The caller must release pOut when done.` |
|       - |  4944 | ` */` |
|   50172 |  4945 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4946 |  |
|   50174 |  4947 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      54 |  4948 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      54 |  4949 | `		SyBlobAppend(pOut,"\\",1);` |
|      26 |  4950 | `	}` |
|   50174 |  4951 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   50174 |  4952 |  |
|       - |  4953 | `/*` |
|       - |  4954 | ` * Compile a namespace statement` |
|       - |  4955 | ` * According to the PHP language reference manual` |
|       - |  4956 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  4957 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  4958 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  4959 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  4960 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  4961 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  4962 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  4963 | ` *  programming world.` |
|       - |  4964 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  4965 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  4966 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  4967 | ` *  classes/functions/constants.` |
|       - |  4968 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  4969 | ` *  readability of source code.` |
|       - |  4970 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  4971 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  4972 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  4973 | ` *       class MyClass {}` |
|       - |  4974 | ` *       function myfunction() {}` |
|       - |  4975 | ` *       const MYCONST = 1;` |
|       - |  4976 | ` *       $a = new MyClass;` |
|       - |  4977 | ` *       $c = new \my\name\MyClass;` |
|       - |  4978 | ` *       $a = strlen('hi');` |
|       - |  4979 | ` *       $d = namespace\MYCONST;` |
|       - |  4980 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  4981 | ` *       echo constant($d);` |
|       - |  4982 | ` * NOTE` |
|       - |  4983 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  4984 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  4985 | ` */` |
|       - |  4986 | `/*` |
|       - |  4987 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  4988 | ` */` |
|      14 |  4989 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 |  4990 |  |
|      15 |  4991 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 |  4992 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 |  4993 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 |  4994 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 |  4995 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 |  4996 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  4997 | `	return "token";` |
|       8 |  4998 |  |
|     100 |  4999 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 |  5000 |  |
|       - |  5001 | `	sxu32 nLine;` |
|       - |  5002 | `	sxi32 rc;` |
|     102 |  5003 | `	nLine = pGen->pIn->nLine;` |
|     102 |  5004 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5005 | `	/* Reset namespace and clear previous use imports */` |
|     102 |  5006 | `	SyBlobReset(&pGen->sNamespace);` |
|     102 |  5007 | `	SyHashRelease(&pGen->hUseImports);` |
|     102 |  5008 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  5009 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     102 |  5010 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  5011 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     102 |  5012 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  5013 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5014 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5015 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5016 | `		return SXRET_OK;` |
|       - |  5017 | `	}` |
|     102 |  5018 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5019 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5020 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5021 | `		return SXRET_OK;` |
|       - |  5022 | `	}` |
|     102 |  5023 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5024 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5025 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5026 | `		return SXRET_OK;` |
|       - |  5027 | `	}` |
|       - |  5028 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     240 |  5029 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     140 |  5030 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5031 | `			/* Append backslash separator */` |
|      21 |  5032 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 |  5033 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 |  5034 | `			}` |
|      11 |  5035 | `		}else{` |
|       - |  5036 | `			/* Append identifier */` |
|     120 |  5037 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5038 | `		}` |
|     140 |  5039 | `		pGen->pIn++;` |
|       2 |  5040 | `	}` |
|       - |  5041 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5042 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5043 | `	{` |
|     102 |  5044 | `		char *zNsDup = 0;` |
|     102 |  5045 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     149 |  5046 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      98 |  5047 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      49 |  5048 | `		}` |
|     102 |  5049 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5050 | `	}` |
|     102 |  5051 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 |  5052 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5053 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5054 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 |  5055 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5056 | `			return SXERR_ABORT;` |
|       - |  5057 | `		}` |
|       2 |  5058 | `	}` |
|     102 |  5059 | `	return SXRET_OK;` |
|      52 |  5060 |  |
|       - |  5061 | `/*` |
|       - |  5062 | ` * Compile the 'use' statement` |
|       - |  5063 | ` * According to the PHP language reference manual` |
|       - |  5064 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5065 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5066 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5067 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5068 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5069 | ` *  a function or constant is not supported.` |
|       - |  5070 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5071 | ` * NOTE` |
|       - |  5072 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5073 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5074 | ` */` |
|      66 |  5075 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 |  5076 |  |
|       - |  5077 | `	sxu32 nLine;` |
|       - |  5078 | `	sxi32 rc;` |
|       - |  5079 | `	SyBlob sPath;` |
|       - |  5080 | `	SyString sAlias;` |
|       - |  5081 | `	SyToken *pLast;` |
|       - |  5082 | `	char *zDup;` |
|       - |  5083 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5084 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5085 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 |  5086 | `	nLine = pGen->pIn->nLine;` |
|      68 |  5087 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5088 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 |  5089 | `	iUseType = 0;` |
|      68 |  5090 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5091 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5092 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5093 | `			iUseType = 1;` |
|      16 |  5094 | `			pGen->pIn++;` |
|      23 |  5095 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5096 | `			iUseType = 2;` |
|      16 |  5097 | `			pGen->pIn++;` |
|       7 |  5098 | `		}` |
|      14 |  5099 | `	}` |
|       - |  5100 | `	/* Select target hash tables based on import type */` |
|      68 |  5101 | `	switch( iUseType ){` |
|       7 |  5102 | `		case 1:` |
|      16 |  5103 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5104 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5105 | `			break;` |
|       7 |  5106 | `		case 2:` |
|      16 |  5107 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5108 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5109 | `			break;` |
|      19 |  5110 | `		default:` |
|      40 |  5111 | `			pGenHash = &pGen->hUseImports;` |
|      40 |  5112 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 |  5113 | `			break;` |
|       - |  5114 | `	}` |
|      68 |  5115 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5116 | `	/* Process one or more use declarations separated by commas */` |
|      34 |  5117 | `	for(;;){` |
|      70 |  5118 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5119 | `			break;` |
|       - |  5120 | `		}` |
|      70 |  5121 | `		SyBlobReset(&sPath);` |
|      70 |  5122 | `		pLast = 0;` |
|       - |  5123 | `		/* Collect the full namespace path */` |
|     254 |  5124 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 |  5125 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 |  5126 | `				pLast = pGen->pIn;` |
|     126 |  5127 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 |  5128 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5129 | `				}` |
|     126 |  5130 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 |  5131 | `			}` |
|     186 |  5132 | `			pGen->pIn++;` |
|       2 |  5133 | `		}` |
|      70 |  5134 | `		if( pLast == 0 ){` |
|       - |  5135 | `			/* Empty path */` |
|       5 |  5136 | `			break;` |
|       - |  5137 | `		}` |
|       - |  5138 | `		/* Default alias is the last component of the path */` |
|      66 |  5139 | `		sAlias = pLast->sData;` |
|       - |  5140 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 |  5141 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 |  5142 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5143 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5144 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5145 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5146 | `				pGen->pIn++;` |
|       8 |  5147 | `			}` |
|       8 |  5148 | `		}` |
|       - |  5149 | `		/* Check for duplicate import alias (per-type) */` |
|      66 |  5150 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 |  5151 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5152 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5153 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 |  5154 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5155 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5156 | `				return SXERR_ABORT;` |
|       - |  5157 | `			}` |
|       2 |  5158 | `		}` |
|       - |  5159 | `		/* Register the import: alias -> FQN.` |
|       - |  5160 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5161 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5162 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 |  5163 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 |  5164 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 |  5165 | `		if( zDup ){` |
|      66 |  5166 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 |  5167 | `			if( pVmHash ){` |
|       - |  5168 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5169 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 |  5170 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 |  5171 | `				if( zAliasDup ){` |
|      38 |  5172 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 |  5173 | `				}` |
|      18 |  5174 | `			}` |
|      66 |  5175 | `			if( iUseType == 2 ){` |
|       - |  5176 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5177 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5178 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5179 | `				if( zAliasDup ){` |
|       - |  5180 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5181 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5182 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5183 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5184 | `					if( azPair ){` |
|      16 |  5185 | `						azPair[0] = zAliasDup;` |
|      16 |  5186 | `						azPair[1] = zDup;` |
|      16 |  5187 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5188 | `					}` |
|       7 |  5189 | `				}` |
|       7 |  5190 | `			}` |
|      32 |  5191 | `		}` |
|       - |  5192 | `		/* Check for comma (multiple use declarations) */` |
|      66 |  5193 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5194 | `			pGen->pIn++;` |
|       2 |  5195 | `		}else{` |
|      33 |  5196 | `			break;` |
|       - |  5197 | `		}` |
|       1 |  5198 | `	}` |
|      68 |  5199 | `	SyBlobRelease(&sPath);` |
|      68 |  5200 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5201 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5202 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5203 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5204 | `			return SXERR_ABORT;` |
|       - |  5205 | `		}` |
|       1 |  5206 | `	}` |
|      68 |  5207 | `	return SXRET_OK;` |
|      35 |  5208 |  |
|       - |  5209 | `/*` |
|       - |  5210 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5211 | ` *` |
|       - |  5212 | ` * According to the PHP language reference manual.` |
|       - |  5213 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5214 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5215 | ` *  declare (directive)` |
|       - |  5216 | ` *   statement` |
|       - |  5217 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5218 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5219 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5220 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5221 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5222 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5223 | ` * <?php` |
|       - |  5224 | ` * // these are the same:` |
|       - |  5225 | ` * // you can use this:` |
|       - |  5226 | ` * declare(ticks=1) {` |
|       - |  5227 | ` *   // entire script here` |
|       - |  5228 | ` * }` |
|       - |  5229 | ` * // or you can use this:` |
|       - |  5230 | ` * declare(ticks=1);` |
|       - |  5231 | ` * // entire script here` |
|       - |  5232 | ` * ?>` |
|       - |  5233 | ` *` |
|       - |  5234 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5235 | ` */` |
|       8 |  5236 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 |  5237 |  |
|       9 |  5238 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 |  5239 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - |  5240 | `	sxi32 rc;` |
|       9 |  5241 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 |  5242 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5243 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5244 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5245 | `			return SXERR_ABORT;` |
|       - |  5246 | `		}` |
|       5 |  5247 | `		goto Synchro;` |
|       - |  5248 | `	}` |
|       5 |  5249 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - |  5250 | `	/* Delimit the directive */` |
|       5 |  5251 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 |  5252 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  5253 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5254 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5255 | `			return SXERR_ABORT;` |
|       - |  5256 | `		}` |
|     ! 0 |  5257 | `		return SXRET_OK;` |
|       - |  5258 | `	}` |
|       - |  5259 | `	/* Update the cursor */` |
|       5 |  5260 | `	pGen->pIn = &pEnd[1];` |
|       5 |  5261 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 |  5262 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5263 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5264 | `			return SXERR_ABORT;` |
|       - |  5265 | `		}` |
|     ! 0 |  5266 | `	}` |
|       - |  5267 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 |  5268 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - |  5269 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5270 | `		ph7_lib_version()` |
|       - |  5271 | `		);` |
|       - |  5272 | `	/*All done */` |
|       5 |  5273 | `	return SXRET_OK;` |
|       2 |  5274 | `Synchro:` |
|       - |  5275 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5276 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5277 | `		pGen->pIn++;` |
|       1 |  5278 | `	}` |
|       5 |  5279 | `	return SXRET_OK;` |
|       5 |  5280 |  |
|       - |  5281 | `/*` |
|       - |  5282 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5283 | ` * as follows:` |
|       - |  5284 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5285 | ` * {` |
|       - |  5286 | ` *   return "Making a cup of $type.\n";` |
|       - |  5287 | ` * }` |
|       - |  5288 | ` * Symisc eXtension.` |
|       - |  5289 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5290 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5291 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5292 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5293 | ` *      {` |
|       - |  5294 | ` *       var_dump($a);` |
|       - |  5295 | ` *      }` |
|       - |  5296 | ` *     //call test without args` |
|       - |  5297 | ` *      test();` |
|       - |  5298 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5299 | ` *      Example:` |
|       - |  5300 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5301 | ` * 3 -) Function overloading!!` |
|       - |  5302 | ` *      Example:` |
|       - |  5303 | ` *      function foo($a) {` |
|       - |  5304 | ` *   	  return $a.PHP_EOL;` |
|       - |  5305 | ` *	    }` |
|       - |  5306 | ` *	    function foo($a, $b) {` |
|       - |  5307 | ` *   	  return $a + $b;` |
|       - |  5308 | ` *	    }` |
|       - |  5309 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5310 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5311 | ` *      // Same arg` |
|       - |  5312 | ` *	   function foo(string $a)` |
|       - |  5313 | ` *	   {` |
|       - |  5314 | ` *	     echo "a is a string\n";` |
|       - |  5315 | ` *	     var_dump($a);` |
|       - |  5316 | ` *	   }` |
|       - |  5317 | ` *	  function foo(int $a)` |
|       - |  5318 | ` *	  {` |
|       - |  5319 | ` *	    echo "a is integer\n";` |
|       - |  5320 | ` *	    var_dump($a);` |
|       - |  5321 | ` *	  }` |
|       - |  5322 | ` *	  function foo(array $a)` |
|       - |  5323 | ` *	  {` |
|       - |  5324 | ` * 	    echo "a is an array\n";` |
|       - |  5325 | ` * 	    var_dump($a);` |
|       - |  5326 | ` *	  }` |
|       - |  5327 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5328 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5329 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5330 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5331 | ` * introduced by the PH7 engine.` |
|       - |  5332 | ` */` |
|   46462 |  5333 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5334 |  |
|       - |  5335 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5336 | `	SySet *pInstrContainer;` |
|       - |  5337 | `	sxi32 rc;` |
|       - |  5338 | `	/* Swap token stream */` |
|   46464 |  5339 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   46464 |  5340 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   46464 |  5341 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5342 | `	/* Compile the expression holding the argument value */` |
|   46464 |  5343 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5344 | `	/* Emit the done instruction */` |
|   46464 |  5345 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   46464 |  5346 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   46464 |  5347 | `	RE_SWAP_DELIMITER(pGen);` |
|   46464 |  5348 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5349 | `		return SXERR_ABORT;` |
|       - |  5350 | `	}` |
|   46464 |  5351 | `	return SXRET_OK;` |
|   23233 |  5352 |  |
|       - |  5353 | `/*` |
|       - |  5354 | ` * Collect function arguments one after one.` |
|       - |  5355 | ` * According to the PHP language reference manual.` |
|       - |  5356 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5357 | ` * list of expressions.` |
|       - |  5358 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5359 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5360 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5361 | ` * for more information.` |
|       - |  5362 | ` * Example #1 Passing arrays to functions` |
|       - |  5363 | ` * <?php` |
|       - |  5364 | ` * function takes_array($input)` |
|       - |  5365 | ` * {` |
|       - |  5366 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5367 | ` * }` |
|       - |  5368 | ` * ?>` |
|       - |  5369 | ` * Making arguments be passed by reference` |
|       - |  5370 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5371 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5372 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5373 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5374 | ` * to the argument name in the function definition:` |
|       - |  5375 | ` * Example #2 Passing function parameters by reference` |
|       - |  5376 | ` * <?php` |
|       - |  5377 | ` * function add_some_extra(&$string)` |
|       - |  5378 | ` * {` |
|       - |  5379 | ` *   $string .= 'and something extra.';` |
|       - |  5380 | ` * }` |
|       - |  5381 | ` * $str = 'This is a string, ';` |
|       - |  5382 | ` * add_some_extra($str);` |
|       - |  5383 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5384 | ` * ?>` |
|       - |  5385 | ` *` |
|       - |  5386 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5387 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5388 | ` * on these extension.` |
|       - |  5389 | ` */` |
|   55974 |  5390 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       2 |  5391 |  |
|       - |  5392 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5393 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5394 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5395 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5396 | `	sxi32 rc;` |
|       - |  5397 |  |
|   55976 |  5398 | `	pIn = pGen->pIn;` |
|   55976 |  5399 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5400 | `	/* Process arguments one after one */` |
|   70779 |  5401 | `	for(;;){` |
|  141560 |  5402 | `		if( pIn >= pEnd ){` |
|       - |  5403 | `			/* No more arguments to process */` |
|   55964 |  5404 | `			break;` |
|       - |  5405 | `		}` |
|   85598 |  5406 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   85598 |  5407 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   85598 |  5408 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   85598 |  5409 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5410 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|   85598 |  5411 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   52366 |  5412 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   52366 |  5413 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      42 |  5414 | `				if( !bCtorCtx ){` |
|       5 |  5415 | `					if( bAbstractCtx ){` |
|       3 |  5416 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5417 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5418 | `					}else{` |
|       3 |  5419 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5420 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5421 | `					}` |
|       5 |  5422 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5423 | `						return SXERR_ABORT;` |
|       - |  5424 | `					}` |
|       5 |  5425 | `					return SXERR_SYNTAX;` |
|       - |  5426 | `				}` |
|      38 |  5427 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      38 |  5428 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5429 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      37 |  5430 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5431 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5432 | `				}else{` |
|      34 |  5433 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5434 | `				}` |
|      38 |  5435 | `				pIn++;` |
|      18 |  5436 | `			}` |
|   26180 |  5437 | `		}` |
|       - |  5438 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  114702 |  5439 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   73369 |  5440 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   59685 |  5441 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   58204 |  5442 | `			sxu32 nLineLocal = pIn->nLine;` |
|   58204 |  5443 | `			sxi32 iTFlags = 0;` |
|   58204 |  5444 | `			pGen->pIn = pIn;` |
|   58204 |  5445 | `			rc = GenStateParseUnionTypeDecl(` |
|   29101 |  5446 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   29101 |  5447 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5448 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5449 | `				/* bAllowVoid */ 0,` |
|   29101 |  5450 | `						nLineLocal);` |
|   58204 |  5451 | `			pIn = pGen->pIn;` |
|   58204 |  5452 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5453 | `				return SXERR_ABORT;` |
|   58204 |  5454 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5455 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5456 | `				return SXERR_SYNTAX;` |
|   58202 |  5457 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5458 | `				if( pIn < pEnd ){` |
|       7 |  5459 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5460 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5461 | `						&pIn->sData);` |
|       3 |  5462 | `				}else{` |
|     ! 0 |  5463 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5464 | `						"syntax error, unexpected end of file");` |
|       - |  5465 | `				}` |
|       5 |  5466 | `				return SXERR_SYNTAX;` |
|       - |  5467 | `			}` |
|   58198 |  5468 | `			sArg.iFlags \|= iTFlags;` |
|   29098 |  5469 | `		}` |
|   85588 |  5470 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5471 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5472 | `			return rc;` |
|       - |  5473 | `		}` |
|   85588 |  5474 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5475 | `			/* Pass by reference,record that */` |
|    2930 |  5476 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2930 |  5477 | `			pIn++;` |
|    1464 |  5478 | `		}` |
|   85588 |  5479 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5480 | `			/* Variadic parameter: ...$args */` |
|      40 |  5481 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      40 |  5482 | `			pIn++;` |
|      19 |  5483 | `		}` |
|   85588 |  5484 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5485 | `			/* Invalid argument */` |
|     ! 0 |  5486 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5487 | `			return rc;` |
|       - |  5488 | `		}` |
|   85588 |  5489 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5490 | `		/* Copy argument name */` |
|   85588 |  5491 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   85588 |  5492 | `		if( zDup == 0 ){` |
|     ! 0 |  5493 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5494 | `			return SXERR_ABORT;` |
|       - |  5495 | `		}` |
|   85588 |  5496 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   85588 |  5497 | `		pIn++;` |
|   85588 |  5498 | `		if( pIn < pEnd ){` |
|   52868 |  5499 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5500 | `				SyToken *pDefend;` |
|   46466 |  5501 | `				sxi32 iNest = 0;` |
|   46466 |  5502 | `				pIn++; /* Jump the equal sign */` |
|   46466 |  5503 | `				pDefend = pIn;` |
|       - |  5504 | `				/* Process the default value associated with this argument */` |
|   98732 |  5505 | `				while( pDefend < pEnd ){` |
|   75492 |  5506 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   23226 |  5507 | `						break;` |
|       - |  5508 | `					}` |
|   52268 |  5509 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5510 | `						/* Increment nesting level */` |
|    2904 |  5511 | `						iNest++;` |
|   50817 |  5512 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5513 | `						/* Decrement nesting level */` |
|    2904 |  5514 | `						iNest--;` |
|    1451 |  5515 | `					}` |
|   52268 |  5516 | `					pDefend++;` |
|       2 |  5517 | `				}` |
|   46466 |  5518 | `				if( pIn >= pDefend ){` |
|       3 |  5519 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5520 | `					return rc;` |
|       - |  5521 | `				}` |
|       - |  5522 | `				/* Process default value */` |
|   46464 |  5523 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   46464 |  5524 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5525 | `					return rc;` |
|       - |  5526 | `				}` |
|       - |  5527 | `				/* Point beyond the default value */` |
|   46464 |  5528 | `				pIn = pDefend;` |
|   23231 |  5529 | `			}` |
|   52866 |  5530 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5531 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5532 | `				return rc;` |
|       - |  5533 | `			}` |
|   52866 |  5534 | `			pIn++; /* Jump the trailing comma */` |
|   26432 |  5535 | `		}` |
|       - |  5536 | `		/* Append argument signature */` |
|   85586 |  5537 | `		if( sArg.nType > 0 ){` |
|   58158 |  5538 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5539 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5822 |  5540 | `				int marker = 'o';` |
|    5822 |  5541 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5822 |  5542 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2912 |  5543 | `			}else{` |
|       - |  5544 | `				int c;` |
|   52338 |  5545 | `				c = 'n'; /* cc warning */` |
|       - |  5546 | `				/* Type leading character */` |
|   52338 |  5547 | `				switch(sArg.nType){` |
|     ! 0 |  5548 | `				case MEMOBJ_HASHMAP:` |
|       - |  5549 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5550 | `					c = 'h';` |
|     ! 0 |  5551 | `					break;` |
|    7283 |  5552 | `				case MEMOBJ_INT:` |
|       - |  5553 | `					/* Integer */` |
|   14568 |  5554 | `					c = 'i';` |
|   14568 |  5555 | `					break;` |
|     ! 0 |  5556 | `				case MEMOBJ_BOOL:` |
|       - |  5557 | `					/* Bool */` |
|     ! 0 |  5558 | `					c = 'b';` |
|     ! 0 |  5559 | `					break;` |
|     ! 0 |  5560 | `				case MEMOBJ_REAL:` |
|       - |  5561 | `					/* Float */` |
|     ! 0 |  5562 | `					c = 'f';` |
|     ! 0 |  5563 | `					break;` |
|   18878 |  5564 | `				case MEMOBJ_STRING:` |
|       - |  5565 | `					/* String */` |
|   37758 |  5566 | `					c = 's';` |
|   37758 |  5567 | `					break;` |
|       7 |  5568 | `				case MEMOBJ_OBJ:` |
|       - |  5569 | `					/* Object */` |
|      16 |  5570 | `					c = 'o';` |
|      14 |  5571 | `					break;` |
|     ! 0 |  5572 | `				default:` |
|     ! 0 |  5573 | `					break;` |
|       - |  5574 | `				}` |
|   52338 |  5575 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5576 | `			}` |
|   29080 |  5577 | `		}else{` |
|       - |  5578 | `			/* No type is associated with this parameter which mean` |
|       - |  5579 | `			 * that this function is not condidate for overloading.` |
|       - |  5580 | `			 */` |
|   27430 |  5581 | `			SyBlobRelease(&sSig);` |
|       - |  5582 | `		}` |
|       - |  5583 | `		/* Save in the argument set */` |
|   85586 |  5584 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5585 | `	}` |
|   55964 |  5586 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5587 | `		/* Save function signature */` |
|   34922 |  5588 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   17460 |  5589 | `	}` |
|   55964 |  5590 | `	return SXRET_OK;` |
|   27989 |  5591 |  |
|       - |  5592 | `/*` |
|       - |  5593 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5594 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5595 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5596 | ` */` |
|  155148 |  5597 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5598 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5599 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5600 | `	)` |
|       2 |  5601 |  |
|       - |  5602 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5603 | `	GenBlock *pBlock;` |
|       - |  5604 | `	sxu32 nGotoOfft;` |
|       - |  5605 | `	sxi32 rc;` |
|       - |  5606 | `	/* Attach the new function */` |
|  155150 |  5607 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  155150 |  5608 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5609 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5610 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5611 | `		return SXERR_ABORT;` |
|       - |  5612 | `	}` |
|  155150 |  5613 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5614 | `	/* Swap bytecode containers */` |
|  155150 |  5615 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  155150 |  5616 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5617 | `	/* Emit constructor property promotion prologue:` |
|       - |  5618 | `	 *   $this->NAME = $NAME;` |
|       - |  5619 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5620 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5621 | `	{` |
|  155150 |  5622 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5623 | `		sxu32 i;` |
|  237754 |  5624 | `		for( i = 0; i < nArg; i++ ){` |
|   82606 |  5625 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5626 | `			char *zSrc;` |
|       - |  5627 | `			sxu32 nSrc,nName;` |
|       - |  5628 | `			SySet sToken;` |
|       - |  5629 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5630 | `			sxi32 rcPromote;` |
|   82606 |  5631 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   82578 |  5632 | `				continue;` |
|       - |  5633 | `			}` |
|       - |  5634 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5635 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5636 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5637 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5638 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      30 |  5639 | `			nName = SyStringLength(&pArg->sName);` |
|      30 |  5640 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      30 |  5641 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      30 |  5642 | `			if( zSrc == 0 ){` |
|     ! 0 |  5643 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5644 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5645 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5646 | `				return SXERR_ABORT;` |
|       - |  5647 | `			}` |
|       - |  5648 | `			{` |
|      30 |  5649 | `				char *z = zSrc;` |
|      30 |  5650 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      30 |  5651 | `				z += sizeof("$this->")-1;` |
|      30 |  5652 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5653 | `				z += nName;` |
|      30 |  5654 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      30 |  5655 | `				z += sizeof(" = $")-1;` |
|      30 |  5656 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5657 | `				z += nName;` |
|      30 |  5658 | `				*z = 0;` |
|       - |  5659 | `			}` |
|      30 |  5660 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      30 |  5661 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      30 |  5662 | `			pTmpIn = pGen->pIn;` |
|      30 |  5663 | `			pTmpEnd = pGen->pEnd;` |
|      30 |  5664 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      30 |  5665 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      30 |  5666 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  5667 | `			pGen->pIn = pTmpIn;` |
|      30 |  5668 | `			pGen->pEnd = pTmpEnd;` |
|      30 |  5669 | `			SySetRelease(&sToken);` |
|      30 |  5670 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5671 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5672 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5673 | `				return SXERR_ABORT;` |
|       - |  5674 | `			}` |
|       - |  5675 | `			/* Discard the assignment result — this is a statement expression. */` |
|      30 |  5676 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      16 |  5677 | `		}` |
|       - |  5678 | `	}` |
|       - |  5679 | `	/* Compile the body */` |
|  155150 |  5680 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5681 | `	/* Fix exception jumps now the destination is resolved */` |
|  155150 |  5682 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5683 | `	/* Emit the final return if not yet done */` |
|  155150 |  5684 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5685 | `	/* Fix gotos jumps now the destination is resolved */` |
|  155150 |  5686 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5687 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5688 | `	}` |
|  155150 |  5689 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5690 | `	/* Restore the default container */` |
|  155150 |  5691 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5692 | `	/* Leave function block */` |
|  155150 |  5693 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  155150 |  5694 | `	if( rc == SXERR_ABORT ){` |
|       - |  5695 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5696 | `		return SXERR_ABORT;` |
|       - |  5697 | `	}` |
|       - |  5698 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5699 | `	{` |
|  155150 |  5700 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5701 | `		sxu32 i;` |
| 3219796 |  5702 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3064666 |  5703 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5704 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5705 | `				break;` |
|       - |  5706 | `			}` |
| 1532325 |  5707 | `		}` |
|       - |  5708 | `	}` |
|       - |  5709 | `	/* All done, function body compiled */` |
|  155150 |  5710 | `	return SXRET_OK;` |
|   77576 |  5711 |  |
|       - |  5712 | `/*` |
|       - |  5713 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5714 | ` * According to the PHP language reference manual.` |
|       - |  5715 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5716 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5717 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5718 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5719 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5720 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5721 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5722 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5723 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5724 | ` *` |
|       - |  5725 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5726 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5727 | ` * on these extension.` |
|       - |  5728 | ` */` |
|       - |  5729 | `/*` |
|       - |  5730 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5731 | ` */` |
|      76 |  5732 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5733 |  |
|       - |  5734 | `	sxu32 i;` |
|     238 |  5735 | `	for( i = 0; i < n; i++ ){` |
|     212 |  5736 | `		int a = zA[i], b = zB[i];` |
|     212 |  5737 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     212 |  5738 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     212 |  5739 | `		if( a != b ) return a - b;` |
|      82 |  5740 | `	}` |
|      28 |  5741 | `	return 0;` |
|      40 |  5742 |  |
|       - |  5743 | `/*` |
|       - |  5744 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5745 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5746 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5747 | ` */` |
|       - |  5748 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5749 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5750 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5751 |  |
|       - |  5752 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5753 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5754 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5755 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5756 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5757 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5758 |  |
|       - |  5759 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5760 | `struct PhlTypeAtom {` |
|       - |  5761 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5762 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5763 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5764 | `	sxu32 nCanon;` |
|       - |  5765 | `};` |
|       - |  5766 |  |
|       - |  5767 | `/*` |
|       - |  5768 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5769 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5770 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5771 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5772 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5773 | ` * already be consumed by the caller.` |
|       - |  5774 | ` */` |
|   58496 |  5775 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5776 |  |
|   58498 |  5777 | `	SyToken *pIn = pGen->pIn;` |
|   58498 |  5778 | `	SyZero(pOut, sizeof(*pOut));` |
|   58498 |  5779 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   58498 |  5780 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5781 | `		return SXERR_SYNTAX;` |
|       - |  5782 | `	}` |
|       - |  5783 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   58498 |  5784 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5785 | `		pIn++;` |
|       8 |  5786 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5787 | `			return SXERR_SYNTAX;` |
|       - |  5788 | `		}` |
|       3 |  5789 | `	}` |
|   58498 |  5790 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5791 | `		return SXERR_SYNTAX;` |
|       - |  5792 | `	}` |
|   58498 |  5793 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   52614 |  5794 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   52614 |  5795 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5796 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   52607 |  5797 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       8 |  5798 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   52597 |  5799 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   14702 |  5800 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   45244 |  5801 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   37844 |  5802 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   18973 |  5803 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      22 |  5804 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      42 |  5805 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5806 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5807 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5808 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5809 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5810 | `			pOut->sClass = pIn->sData;` |
|       4 |  5811 | `		}else{` |
|       3 |  5812 | `			return SXERR_SYNTAX;` |
|       - |  5813 | `		}` |
|   52612 |  5814 | `		pIn++;` |
|   26307 |  5815 | `	}else{` |
|       - |  5816 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5817 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    5886 |  5818 | `		SyString *pT = &pIn->sData;` |
|    5886 |  5819 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5820 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5821 | `			pIn++;` |
|    5881 |  5822 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      12 |  5823 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      12 |  5824 | `			pIn++;` |
|    5871 |  5825 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5826 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5827 | `			pIn++;` |
|       2 |  5828 | `		}else{` |
|       - |  5829 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    5864 |  5830 | `			SyToken *pFirst = pIn;` |
|    5864 |  5831 | `			SyToken *pLast = pIn;` |
|    5864 |  5832 | `			pOut->nType = SXU32_HIGH;` |
|    5864 |  5833 | `			pOut->sClass = pIn->sData;` |
|    5864 |  5834 | `			pIn++;` |
|    8796 |  5835 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    5867 |  5836 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  5837 | `				pLast = &pIn[1];` |
|       3 |  5838 | `				pIn += 2;` |
|       1 |  5839 | `			}` |
|    5864 |  5840 | `			if( pLast != pFirst ){` |
|       3 |  5841 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  5842 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  5843 | `				pOut->sClass.zString = zFirst;` |
|       3 |  5844 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  5845 | `			}` |
|       - |  5846 | `		}` |
|       - |  5847 | `	}` |
|   58496 |  5848 | `	pGen->pIn = pIn;` |
|   58496 |  5849 | `	return SXRET_OK;` |
|   29250 |  5850 |  |
|       - |  5851 |  |
|       - |  5852 | `/*` |
|       - |  5853 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  5854 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  5855 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  5856 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  5857 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  5858 | ` */` |
|   58400 |  5859 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  5860 |  |
|       - |  5861 | `	int i;` |
|   58402 |  5862 | `	int nNonNull = 0;` |
|  116882 |  5863 | `	for( i = 0; i < nAtoms; i++ ){` |
|   58482 |  5864 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   58472 |  5865 | `			nNonNull++;` |
|   29235 |  5866 | `		}` |
|   29242 |  5867 | `	}` |
|   58402 |  5868 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  5869 | `		/* Shorthand: ?T */` |
|      54 |  5870 | `		for( i = 0; i < nAtoms; i++ ){` |
|      54 |  5871 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      54 |  5872 | `			SyBlobAppend(pBlob, "?", 1);` |
|      54 |  5873 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5874 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  5875 | `			}else{` |
|      44 |  5876 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  5877 | `			}` |
|      54 |  5878 | `			return;` |
|     ! 0 |  5879 | `		}` |
|     ! 0 |  5880 | `	}` |
|       - |  5881 | `	{` |
|   58350 |  5882 | `		int bFirst = 1;` |
|       - |  5883 | `		/* 1) Classes in declaration order */` |
|  116772 |  5884 | `		for( i = 0; i < nAtoms; i++ ){` |
|   58424 |  5885 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    5858 |  5886 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    5858 |  5887 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    5858 |  5888 | `				bFirst = 0;` |
|    2928 |  5889 | `			}` |
|   29213 |  5890 | `		}` |
|       - |  5891 | `		/* 2) Built-ins in canonical order */` |
|       - |  5892 | `		{` |
|       - |  5893 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  5894 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  5895 | `			int k;` |
|  408438 |  5896 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  647978 |  5897 | `				for( i = 0; i < nAtoms; i++ ){` |
|  350444 |  5898 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   52556 |  5899 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   52556 |  5900 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   52556 |  5901 | `						bFirst = 0;` |
|   52556 |  5902 | `						break;` |
|       - |  5903 | `					}` |
|  148946 |  5904 | `				}` |
|  175046 |  5905 | `			}` |
|       - |  5906 | `		}` |
|       - |  5907 | `		/* 3) null suffix */` |
|   58350 |  5908 | `		if( bNullable ){` |
|       6 |  5909 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  5910 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  5911 | `		}` |
|       - |  5912 | `	}` |
|   29202 |  5913 |  |
|       - |  5914 |  |
|       - |  5915 | `/*` |
|       - |  5916 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  5917 | ` *` |
|       - |  5918 | ` * Outputs:` |
|       - |  5919 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  5920 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  5921 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  5922 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  5923 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  5924 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  5925 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  5926 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  5927 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  5928 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  5929 | ` *` |
|       - |  5930 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  5931 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  5932 | ` */` |
|   58410 |  5933 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  5934 | `	ph7_gen_state *pGen,` |
|       - |  5935 | `	sxu32 *pnType,` |
|       - |  5936 | `	SyString *pClass,` |
|       - |  5937 | `	SySet *pAlts,` |
|       - |  5938 | `	sxi32 *piTypeFlags,` |
|       - |  5939 | `	SyString *pTypeText,` |
|       - |  5940 | `	int iNullableFlag,` |
|       - |  5941 | `	int iUnionFlag,` |
|       - |  5942 | `	int bAllowVoid,` |
|       - |  5943 | `	sxu32 nLine` |
|       2 |  5944 | `){` |
|       - |  5945 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   58412 |  5946 | `	int nAtoms = 0;` |
|   58412 |  5947 | `	int bShortNullable = 0;` |
|   58412 |  5948 | `	int bExplicitNull = 0;` |
|       - |  5949 | `	sxi32 rc;` |
|   58412 |  5950 | `	*pnType = 0;` |
|   58412 |  5951 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   58412 |  5952 | `	*piTypeFlags = 0;` |
|   58412 |  5953 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  5954 |  |
|   58412 |  5955 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5956 | `		return SXRET_OK;` |
|       - |  5957 | `	}` |
|       - |  5958 | ``	/* Optional `?` shorthand prefix */`` |
|   58410 |  5959 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      50 |  5960 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      50 |  5961 | `		bShortNullable = 1;` |
|      50 |  5962 | `		pGen->pIn++;` |
|      50 |  5963 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5964 | `			return SXERR_SYNTAX;` |
|       - |  5965 | `		}` |
|      24 |  5966 | `	}` |
|       - |  5967 | `	/* First atom is mandatory */` |
|   58412 |  5968 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   58412 |  5969 | `	if( rc != SXRET_OK ){` |
|       3 |  5970 | `		return rc;` |
|       - |  5971 | `	}` |
|   58410 |  5972 | `	nAtoms = 1;` |
|       - |  5973 | ``	/* Subsequent atoms separated by `\|` */`` |
|   87743 |  5974 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   58541 |  5975 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      90 |  5976 | `		if( bShortNullable ){` |
|       - |  5977 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  5978 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  5979 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  5980 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  5981 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  5982 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  5983 | `		}` |
|      88 |  5984 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  5985 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5986 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  5987 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  5988 | `		}` |
|      88 |  5989 | ``		pGen->pIn++; /* skip `\|` */`` |
|      88 |  5990 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      88 |  5991 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  5992 | `			return rc;` |
|       - |  5993 | `		}` |
|      88 |  5994 | `		nAtoms++;` |
|       2 |  5995 | `	}` |
|       - |  5996 | `	/* Validation pass.` |
|       - |  5997 | `	 *` |
|       - |  5998 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  5999 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6000 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6001 | `	 */` |
|       - |  6002 | `	{` |
|       - |  6003 | `		int i, j;` |
|   58408 |  6004 | `		int bHasNonNull = 0;` |
|  116894 |  6005 | `		for( i = 0; i < nAtoms; i++ ){` |
|   58494 |  6006 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      12 |  6007 | `				if( nAtoms > 1 ){` |
|       3 |  6008 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6009 | `						"Void can only be used as a standalone type");` |
|       3 |  6010 | `					return SXERR_SYNTAX;` |
|       - |  6011 | `				}` |
|      10 |  6012 | `				if( !bAllowVoid ){` |
|     ! 0 |  6013 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6014 | `						"void cannot be used here");` |
|     ! 0 |  6015 | `					return SXERR_SYNTAX;` |
|       - |  6016 | `				}` |
|      10 |  6017 | `				if( bShortNullable ){` |
|     ! 0 |  6018 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6019 | `						"Void type cannot be nullable");` |
|     ! 0 |  6020 | `					return SXERR_SYNTAX;` |
|       - |  6021 | `				}` |
|       4 |  6022 | `			}` |
|   58492 |  6023 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6024 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6025 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6026 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6027 | `				 * does not return), and folding them would mislead any` |
|       - |  6028 | `				 * future return-enforcement work. */` |
|       3 |  6029 | `				if( nAtoms > 1 ){` |
|       3 |  6030 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6031 | `						"never can only be used as a standalone type");` |
|       3 |  6032 | `					return SXERR_SYNTAX;` |
|       - |  6033 | `				}` |
|     ! 0 |  6034 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6035 | `					"never type is not yet implemented");` |
|     ! 0 |  6036 | `				return SXERR_SYNTAX;` |
|       - |  6037 | `			}` |
|   58490 |  6038 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  6039 | `				bExplicitNull = 1;` |
|       7 |  6040 | `			}else{` |
|   58480 |  6041 | `				bHasNonNull = 1;` |
|       - |  6042 | `			}` |
|       - |  6043 | `			/* Duplicate detection */` |
|   58606 |  6044 | `			for( j = 0; j < i; j++ ){` |
|     120 |  6045 | `				int bDup = 0;` |
|     120 |  6046 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  6047 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6048 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6049 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6050 | `								aAtoms[j].sClass.zString,` |
|      12 |  6051 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6052 | `							bDup = 1;` |
|     ! 0 |  6053 | `						}` |
|       8 |  6054 | `					}else{` |
|       3 |  6055 | `						bDup = 1;` |
|       - |  6056 | `					}` |
|       7 |  6057 | `				}` |
|     120 |  6058 | `				if( bDup ){` |
|       - |  6059 | `					const char *zName;` |
|       - |  6060 | `					sxu32 nName;` |
|       3 |  6061 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6062 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6063 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6064 | `					}else{` |
|       3 |  6065 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6066 | `						nName = aAtoms[i].nCanon;` |
|       - |  6067 | `					}` |
|       4 |  6068 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6069 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6070 | `					return SXERR_SYNTAX;` |
|       - |  6071 | `				}` |
|      60 |  6072 | `			}` |
|   29245 |  6073 | `		}` |
|   58402 |  6074 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  6075 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6076 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  6077 | `			return SXERR_SYNTAX;` |
|       - |  6078 | `		}` |
|       - |  6079 | `	}` |
|       - |  6080 | `	/* Compute nullability flag */` |
|   58402 |  6081 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      58 |  6082 | `		*piTypeFlags \|= iNullableFlag;` |
|      28 |  6083 | `	}` |
|       - |  6084 | `	/* Build canonical type text */` |
|   58402 |  6085 | `	if( pTypeText ){` |
|       - |  6086 | `		SyBlob sBlob;` |
|   58402 |  6087 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   87579 |  6088 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   29200 |  6089 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   58402 |  6090 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   87590 |  6091 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   58392 |  6092 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   58394 |  6093 | `			if( zDup ){` |
|   58394 |  6094 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   29196 |  6095 | `			}` |
|   29196 |  6096 | `		}` |
|   58402 |  6097 | `		SyBlobRelease(&sBlob);` |
|   29200 |  6098 | `	}` |
|       - |  6099 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6100 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6101 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6102 | `	{` |
|   58402 |  6103 | `		int nNonNull = 0;` |
|   58402 |  6104 | `		int iNonNullIdx = -1;` |
|       - |  6105 | `		int i;` |
|  116882 |  6106 | `		for( i = 0; i < nAtoms; i++ ){` |
|   58482 |  6107 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   58472 |  6108 | `				nNonNull++;` |
|   58472 |  6109 | `				iNonNullIdx = i;` |
|   29235 |  6110 | `			}` |
|   29242 |  6111 | `		}` |
|   58402 |  6112 | `		if( nNonNull <= 1 ){` |
|       - |  6113 | `			/* Fast path: store as single type. */` |
|   58348 |  6114 | `			if( iNonNullIdx >= 0 ){` |
|   58348 |  6115 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   58348 |  6116 | `				if( pA->nType == SXU32_HIGH ){` |
|    8762 |  6117 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    2920 |  6118 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    5842 |  6119 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    5842 |  6120 | `					*pnType = SXU32_HIGH;` |
|    5842 |  6121 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   55428 |  6122 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      10 |  6123 | `					*pnType = MEMOBJ_VOID;` |
|       6 |  6124 | `				}else{` |
|       - |  6125 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6126 | `					 * pass above rejects it as not-yet-implemented. */` |
|   52500 |  6127 | `					*pnType = pA->nType;` |
|       - |  6128 | `				}` |
|   29173 |  6129 | `			}` |
|   29175 |  6130 | `		}else{` |
|       - |  6131 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      56 |  6132 | `			*piTypeFlags \|= iUnionFlag;` |
|     184 |  6133 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6134 | `				ph7_type_alt sAlt;` |
|     130 |  6135 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     126 |  6136 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     126 |  6137 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      41 |  6138 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      13 |  6139 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      28 |  6140 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      28 |  6141 | `					sAlt.nType = SXU32_HIGH;` |
|      28 |  6142 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      15 |  6143 | `				}else{` |
|     100 |  6144 | `					sAlt.nType = aAtoms[i].nType;` |
|     100 |  6145 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6146 | `				}` |
|     126 |  6147 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      64 |  6148 | `			}` |
|       - |  6149 | `		}` |
|       - |  6150 | `	}` |
|   58402 |  6151 | `	return SXRET_OK;` |
|   29207 |  6152 |  |
|       - |  6153 |  |
|       - |  6154 | `/*` |
|       - |  6155 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6156 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6157 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6158 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6159 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6160 | `` *          and union types `: T\|U`.`` |
|       - |  6161 | ` */` |
|  178508 |  6162 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6163 |  |
|  178510 |  6164 | `	sxi32 iFlags = 0;` |
|       - |  6165 | `	sxi32 rc;` |
|       - |  6166 | `	sxu32 nLine;` |
|  178510 |  6167 | `	pFunc->nReturnType = 0;` |
|  178510 |  6168 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  178510 |  6169 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  178510 |  6170 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  178418 |  6171 | `		return SXRET_OK;` |
|       - |  6172 | `	}` |
|      94 |  6173 | `	pGen->pIn++; /* Skip ':' */` |
|      94 |  6174 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6175 | `		return SXRET_OK;` |
|       - |  6176 | `	}` |
|      94 |  6177 | `	nLine = pGen->pIn->nLine;` |
|      94 |  6178 | `	rc = GenStateParseUnionTypeDecl(` |
|      46 |  6179 | `		pGen,` |
|      46 |  6180 | `		&pFunc->nReturnType,` |
|      46 |  6181 | `		&pFunc->sReturnClass,` |
|      46 |  6182 | `		&pFunc->aReturnUnion,` |
|       - |  6183 | `		&iFlags,` |
|      46 |  6184 | `		&pFunc->sReturnTypeName,` |
|       - |  6185 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6186 | `		/* iUnionFlag */ 0,` |
|       - |  6187 | `		/* bAllowVoid */ 1,` |
|      46 |  6188 | `		nLine);` |
|      46 |  6189 | `	(void)iFlags;` |
|      94 |  6190 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6191 | `		return SXERR_ABORT;` |
|       - |  6192 | `	}` |
|      94 |  6193 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6194 | `		/* Error already reported */` |
|     ! 0 |  6195 | `		return SXERR_SYNTAX;` |
|       - |  6196 | `	}` |
|      94 |  6197 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6198 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6199 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6200 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6201 | `				&pGen->pIn->sData);` |
|       3 |  6202 | `		}else{` |
|     ! 0 |  6203 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6204 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6205 | `		}` |
|       5 |  6206 | `		return SXERR_SYNTAX;` |
|       - |  6207 | `	}` |
|      90 |  6208 | `	return SXRET_OK;` |
|   89256 |  6209 |  |
|       - |  6210 |  |
|   38568 |  6211 | `static sxi32 GenStateCompileFunc(` |
|       - |  6212 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6213 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6214 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6215 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6216 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6217 | `	)` |
|       2 |  6218 |  |
|       - |  6219 | `	ph7_vm_func *pFunc;` |
|       - |  6220 | `	SyToken *pEnd;` |
|       - |  6221 | `	sxu32 nLine;` |
|       - |  6222 | `	char *zName;` |
|       - |  6223 | `	sxi32 rc;` |
|       - |  6224 | `	/* Extract line number */` |
|   38570 |  6225 | `	nLine = pGen->pIn->nLine;` |
|       - |  6226 | `	/* Jump the left parenthesis '(' */` |
|   38570 |  6227 | `	pGen->pIn++;` |
|       - |  6228 | `	/* Delimit the function signature */` |
|   38570 |  6229 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   38570 |  6230 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6231 | `		/* Syntax error */` |
|       7 |  6232 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6233 | `		if( rc == SXERR_ABORT ){` |
|       - |  6234 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6235 | `			return SXERR_ABORT;` |
|       - |  6236 | `		}` |
|       7 |  6237 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6238 | `		return SXRET_OK;` |
|       - |  6239 | `	}` |
|       - |  6240 | `	/* Create the function state */` |
|   38564 |  6241 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   38564 |  6242 | `	if( pFunc == 0 ){` |
|     ! 0 |  6243 | `		goto OutOfMem;` |
|       - |  6244 | `	}` |
|       - |  6245 | `	/* Build the function name, prepending namespace if active */` |
|   38571 |  6246 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6247 | `		SyBlob sFQN;` |
|       - |  6248 | `		sxu32 nLen;` |
|      16 |  6249 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6250 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6251 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6252 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6253 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6254 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6255 | `		SyBlobRelease(&sFQN);` |
|      16 |  6256 | `		if( zName == 0 ){` |
|     ! 0 |  6257 | `			goto OutOfMem;` |
|       - |  6258 | `		}` |
|      16 |  6259 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6260 | `	}else{` |
|   38550 |  6261 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   38550 |  6262 | `		if( zName == 0 ){` |
|     ! 0 |  6263 | `			goto OutOfMem;` |
|       - |  6264 | `		}` |
|   38550 |  6265 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6266 | `	}` |
|   38564 |  6267 | `	if( pGen->pIn < pEnd ){` |
|       - |  6268 | `		/* Collect function arguments */` |
|   26760 |  6269 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   26760 |  6270 | `		if( rc == SXERR_ABORT ){` |
|       - |  6271 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6272 | `			return SXERR_ABORT;` |
|       - |  6273 | `		}` |
|   13379 |  6274 | `	}` |
|       - |  6275 | `	/* Point past ')' and parse optional return type ': type' */` |
|   38564 |  6276 | `	pGen->pIn = &pEnd[1];` |
|       - |  6277 | `	{` |
|   38564 |  6278 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   38564 |  6279 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6280 | `			return SXERR_ABORT;` |
|   38564 |  6281 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6282 | `			return SXERR_SYNTAX;` |
|       - |  6283 | `		}` |
|       - |  6284 | `	}` |
|   38560 |  6285 | `	if( bHandleClosure ){` |
|       - |  6286 | `		ph7_vm_func_closure_env sEnv;` |
|     178 |  6287 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     176 |  6288 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      97 |  6289 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 |  6290 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6291 | `				/* Closure,record environment variable */` |
|      16 |  6292 | `				pGen->pIn++;` |
|      16 |  6293 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6294 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6295 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6296 | `						return SXERR_ABORT;` |
|       - |  6297 | `					}` |
|     ! 0 |  6298 | `				}` |
|      16 |  6299 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6300 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 |  6301 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 |  6302 | `					int iFlagsLocal = 0;` |
|      34 |  6303 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 |  6304 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 |  6305 | `						break;` |
|       - |  6306 | `					}` |
|      20 |  6307 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 |  6308 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6309 | `						/* Pass by reference,record that */` |
|     ! 0 |  6310 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6311 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6312 | `							);` |
|     ! 0 |  6313 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6314 | `						pGen->pIn++;` |
|     ! 0 |  6315 | `					}` |
|      18 |  6316 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 |  6317 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6318 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6319 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6320 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6321 | `								return SXERR_ABORT;` |
|       - |  6322 | `							}` |
|       - |  6323 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6324 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6325 | `								pGen->pIn++;` |
|     ! 0 |  6326 | `							}` |
|     ! 0 |  6327 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6328 | `								pGen->pIn++;` |
|     ! 0 |  6329 | `							}` |
|     ! 0 |  6330 | `							break;` |
|       - |  6331 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6332 | `					}else{` |
|       - |  6333 | `						SyString *pNameLocal;` |
|       - |  6334 | `						char *zDup;` |
|       - |  6335 | `						/* Duplicate variable name */` |
|      20 |  6336 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 |  6337 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 |  6338 | `						if( zDup ){` |
|       - |  6339 | `							/* Zero the structure */` |
|      20 |  6340 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 |  6341 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 |  6342 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 |  6343 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 |  6344 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6345 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6346 | `									got_this = 1;` |
|     ! 0 |  6347 | `							}` |
|       - |  6348 | `							/* Save imported variable */` |
|      20 |  6349 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 |  6350 | `						}else{` |
|     ! 0 |  6351 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6352 | `							 return SXERR_ABORT;` |
|       - |  6353 | `						}` |
|       - |  6354 | `					}` |
|      20 |  6355 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 |  6356 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6357 | `						/* Ignore trailing commas */` |
|       7 |  6358 | `						pGen->pIn++;` |
|       1 |  6359 | `					}` |
|       2 |  6360 | `				}` |
|      16 |  6361 | `				if( !got_this ){` |
|       - |  6362 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6363 | `					 * available to the closure environment.` |
|       - |  6364 | `					 */` |
|      16 |  6365 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 |  6366 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 |  6367 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 |  6368 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 |  6369 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 |  6370 | `				}` |
|      16 |  6371 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6372 | `					/* Mark as closure */` |
|      16 |  6373 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 |  6374 | `				}` |
|       7 |  6375 | `		}` |
|      88 |  6376 | `	}` |
|       - |  6377 | `	/* Compile the body */` |
|   38560 |  6378 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   38560 |  6379 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6380 | `		return SXERR_ABORT;` |
|       - |  6381 | `	}` |
|   38560 |  6382 | `	if( ppFunc ){` |
|     178 |  6383 | `		*ppFunc = pFunc;` |
|      88 |  6384 | `	}` |
|   38560 |  6385 | `	rc = SXRET_OK;` |
|   38560 |  6386 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6387 | `		/* Finally register the function */` |
|   38546 |  6388 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   19272 |  6389 | `	}` |
|   38560 |  6390 | `	if( rc == SXRET_OK ){` |
|   38560 |  6391 | `		return SXRET_OK;` |
|       - |  6392 | `	}` |
|       - |  6393 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6394 | `OutOfMem:` |
|       - |  6395 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6396 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6397 | `	 */` |
|     ! 0 |  6398 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6399 | `	return SXERR_ABORT;` |
|   19286 |  6400 |  |
|       - |  6401 | `/*` |
|       - |  6402 | ` * Compile a standard PHP function.` |
|       - |  6403 | ` *  Refer to the block-comment above for more information.` |
|       - |  6404 | ` */` |
|   38398 |  6405 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6406 |  |
|       - |  6407 | `	SyString *pName;` |
|       - |  6408 | `	sxi32 iFlags;` |
|       - |  6409 | `	sxu32 nLine;` |
|       - |  6410 | `	sxi32 rc;` |
|       - |  6411 |  |
|   38400 |  6412 | `	nLine = pGen->pIn->nLine;` |
|   38400 |  6413 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   38400 |  6414 | `	iFlags = 0;` |
|   38400 |  6415 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6416 | `		/* Return by reference,remember that */` |
|       7 |  6417 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6418 | `		/* Jump the '&' token */` |
|       7 |  6419 | `		pGen->pIn++;` |
|       3 |  6420 | `	}` |
|   38400 |  6421 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6422 | `		/* Invalid function name */` |
|       5 |  6423 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6424 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6425 | `			return SXERR_ABORT;` |
|       - |  6426 | `		}` |
|       - |  6427 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6428 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6429 | `			pGen->pIn++;` |
|       1 |  6430 | `		}` |
|       5 |  6431 | `		return SXRET_OK;` |
|       - |  6432 | `	}` |
|   38396 |  6433 | `	pName = &pGen->pIn->sData;` |
|   38396 |  6434 | `	nLine = pGen->pIn->nLine;` |
|       - |  6435 | `	/* Jump the function name */` |
|   38396 |  6436 | `	pGen->pIn++;` |
|   38396 |  6437 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6438 | `		/* Syntax error */` |
|       3 |  6439 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6440 | `		if( rc == SXERR_ABORT ){` |
|       - |  6441 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6442 | `			return SXERR_ABORT;` |
|       - |  6443 | `		}` |
|       - |  6444 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6445 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6446 | `			pGen->pIn++;` |
|     ! 0 |  6447 | `		}` |
|       3 |  6448 | `		return SXRET_OK;` |
|       - |  6449 | `	}` |
|       - |  6450 | `	/* Compile function body */` |
|   38394 |  6451 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   38394 |  6452 | `	return rc;` |
|   19201 |  6453 |  |
|       - |  6454 | `/*` |
|       - |  6455 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6456 | ` * According to the PHP language reference manual` |
|       - |  6457 | ` *  Visibility:` |
|       - |  6458 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6459 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6460 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6461 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6462 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6463 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6464 | ` */` |
|  178050 |  6465 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6466 |  |
|  178052 |  6467 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8776 |  6468 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  169278 |  6469 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   20358 |  6470 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6471 | `	}` |
|       - |  6472 | `	/* Assume public by default */` |
|  148922 |  6473 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   89027 |  6474 |  |
|       - |  6475 | `/*` |
|       - |  6476 | ` * Compile a class constant.` |
|       - |  6477 | ` * According to the PHP language reference manual` |
|       - |  6478 | ` *  Class Constants` |
|       - |  6479 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6480 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6481 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6482 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6483 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6484 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6485 | ` * Symisc eXtension.` |
|       - |  6486 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6487 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6488 | ` *  Example:` |
|       - |  6489 | ` *   class Test{` |
|       - |  6490 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6491 | ` *   };` |
|       - |  6492 | ` *   var_dump(TEST::MyConst);` |
|       - |  6493 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6494 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6495 | ` */` |
|      30 |  6496 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6497 |  |
|      32 |  6498 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6499 | `	SySet *pInstrContainer;` |
|       - |  6500 | `	ph7_class_attr *pCons;` |
|       - |  6501 | `	SyString *pName;` |
|       - |  6502 | `	sxi32 rc;` |
|       - |  6503 | `	/* Extract visibility level */` |
|      32 |  6504 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6505 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6506 | `loop:` |
|       - |  6507 | `	/* Mark as constant */` |
|      32 |  6508 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6509 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6510 | `		/* Invalid constant name */` |
|     ! 0 |  6511 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6512 | `		if( rc == SXERR_ABORT ){` |
|       - |  6513 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6514 | `			return SXERR_ABORT;` |
|       - |  6515 | `		}` |
|     ! 0 |  6516 | `		goto Synchronize;` |
|       - |  6517 | `	}` |
|       - |  6518 | `	/* Peek constant name */` |
|      32 |  6519 | `	pName = &pGen->pIn->sData;` |
|       - |  6520 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6521 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6522 | `		/* Reserved constant name */` |
|     ! 0 |  6523 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6524 | `		if( rc == SXERR_ABORT ){` |
|       - |  6525 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6526 | `			return SXERR_ABORT;` |
|       - |  6527 | `		}` |
|     ! 0 |  6528 | `		goto Synchronize;` |
|       - |  6529 | `	}` |
|       - |  6530 | `	/* Advance the stream cursor */` |
|      32 |  6531 | `	pGen->pIn++;` |
|      32 |  6532 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6533 | `		/* Invalid declaration */` |
|     ! 0 |  6534 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6535 | `		if( rc == SXERR_ABORT ){` |
|       - |  6536 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6537 | `			return SXERR_ABORT;` |
|       - |  6538 | `		}` |
|     ! 0 |  6539 | `		goto Synchronize;` |
|       - |  6540 | `	}` |
|      32 |  6541 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6542 | `	/* Allocate a new class attribute */` |
|      32 |  6543 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6544 | `	if( pCons == 0 ){` |
|     ! 0 |  6545 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6546 | `		return SXERR_ABORT;` |
|       - |  6547 | `	}` |
|       - |  6548 | `	/* Swap bytecode container */` |
|      32 |  6549 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6550 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6551 | `	/* Compile constant value.` |
|       - |  6552 | `	 */` |
|      32 |  6553 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6554 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6555 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6556 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6557 | `			return SXERR_ABORT;` |
|       - |  6558 | `		}` |
|       1 |  6559 | `	}` |
|       - |  6560 | `	/* Emit the done instruction */` |
|      32 |  6561 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6562 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6563 | `	if( rc == SXERR_ABORT ){` |
|       - |  6564 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6565 | `		return SXERR_ABORT;` |
|       - |  6566 | `	}` |
|       - |  6567 | `	/* All done,install the constant */` |
|      32 |  6568 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6569 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6570 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6571 | `		return SXERR_ABORT;` |
|       - |  6572 | `	}` |
|      32 |  6573 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6574 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6575 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6576 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6577 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6578 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6579 | `				pTok--;` |
|     ! 0 |  6580 | `			}` |
|     ! 0 |  6581 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6582 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6583 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6584 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6585 | `				return SXERR_ABORT;` |
|       - |  6586 | `			}` |
|     ! 0 |  6587 | `		}else{` |
|     ! 0 |  6588 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6589 | `				goto loop;` |
|       - |  6590 | `			}` |
|       - |  6591 | `		}` |
|     ! 0 |  6592 | `	}` |
|      32 |  6593 | `	return SXRET_OK;` |
|     ! 0 |  6594 | `Synchronize:` |
|       - |  6595 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6596 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6597 | `		pGen->pIn++;` |
|     ! 0 |  6598 | `	}` |
|     ! 0 |  6599 | `	return SXERR_CORRUPT;` |
|      17 |  6600 |  |
|       - |  6601 | `/*` |
|       - |  6602 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6603 | ` * According to the PHP language reference manual` |
|       - |  6604 | ` *  Properties` |
|       - |  6605 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6606 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6607 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6608 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6609 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6610 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6611 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6612 | ` * Symisc eXtension.` |
|       - |  6613 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6614 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6615 | ` *  Example:` |
|       - |  6616 | ` *   class Test{` |
|       - |  6617 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6618 | ` *   };` |
|       - |  6619 | ` *   var_dump(TEST::myVar);` |
|       - |  6620 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6621 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6622 | ` */` |
|       - |  6623 | `/*` |
|       - |  6624 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6625 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6626 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6627 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6628 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6629 | ` */` |
|  116682 |  6630 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6631 |  |
|  116684 |  6632 | `	SyToken *p = pStart;` |
|  116684 |  6633 | `	if( p >= pEnd ) return 0;` |
|  116684 |  6634 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6635 | `		p++;` |
|      16 |  6636 | `		if( p >= pEnd ) return 0;` |
|       7 |  6637 | `	}` |
|  116684 |  6638 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6639 | `		p++;` |
|       3 |  6640 | `		if( p >= pEnd ) return 0;` |
|       1 |  6641 | `	}` |
|  116684 |  6642 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6643 | `		return 0;` |
|       - |  6644 | `	}` |
|       - |  6645 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6646 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6647 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6648 | `	 * dispatch site. */` |
|  116684 |  6649 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  116666 |  6650 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  116713 |  6651 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3053 |  6652 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  116568 |  6653 | `			return 0;` |
|       - |  6654 | `		}` |
|      49 |  6655 | `	}` |
|     118 |  6656 | `	p++;` |
|       - |  6657 | `	/* Consume optional namespace path */` |
|     120 |  6658 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6659 | `		p += 2;` |
|       1 |  6660 | `	}` |
|       - |  6661 | ``	/* Consume any `\| Type` union alternatives */`` |
|     192 |  6662 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      78 |  6663 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6664 | `		p++;` |
|      14 |  6665 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6666 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6667 | `		p++;` |
|      14 |  6668 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6669 | `			p += 2;` |
|     ! 0 |  6670 | `		}` |
|       2 |  6671 | `	}` |
|     118 |  6672 | `	if( p >= pEnd ) return 0;` |
|     118 |  6673 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   58343 |  6674 |  |
|       - |  6675 |  |
|       - |  6676 | `/*` |
|       - |  6677 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6678 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6679 | ` * if not). Recognized forms:` |
|       - |  6680 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6681 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6682 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6683 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6684 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6685 | ` * on unrecoverable error.` |
|       - |  6686 | ` *` |
|       - |  6687 | ` * When a type is parsed:` |
|       - |  6688 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6689 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6690 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6691 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6692 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6693 | ` */` |
|     116 |  6694 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6695 | `	ph7_gen_state *pGen,` |
|       - |  6696 | `	sxu32 *pnType,` |
|       - |  6697 | `	SyString *pClass,` |
|       - |  6698 | `	sxi32 *piTypeFlags,` |
|       - |  6699 | `	SyString *pTypeText,` |
|       - |  6700 | `	SySet *pAlts` |
|       2 |  6701 | `){` |
|     118 |  6702 | `	sxi32 iFlags = 0;` |
|       - |  6703 | `	sxi32 rc;` |
|     118 |  6704 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6705 | `		return SXRET_OK;` |
|       - |  6706 | `	}` |
|       - |  6707 | `	/* If the first token is '$', there's no type */` |
|     118 |  6708 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6709 | `		return SXRET_OK;` |
|       - |  6710 | `	}` |
|     118 |  6711 | `	rc = GenStateParseUnionTypeDecl(` |
|      58 |  6712 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6713 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6714 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6715 | `		/* bAllowVoid */ 0,` |
|     116 |  6716 | `		pGen->pIn->nLine);` |
|     118 |  6717 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6718 | `		return rc;` |
|       - |  6719 | `	}` |
|       - |  6720 | `	/* Verify next token is '$' (start of property name) */` |
|     118 |  6721 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6722 | `		return SXERR_SYNTAX;` |
|       - |  6723 | `	}` |
|     118 |  6724 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     118 |  6725 | `	return SXRET_OK;` |
|      60 |  6726 |  |
|       - |  6727 |  |
|       - |  6728 | `/*` |
|       - |  6729 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6730 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6731 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6732 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6733 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6734 | ` * by the type parser itself before reaching here.` |
|       - |  6735 | ` *` |
|       - |  6736 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6737 | ` * use in the error message.` |
|       - |  6738 | ` */` |
|     182 |  6739 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6740 | `	sxu32 nType,` |
|       - |  6741 | `	const SyString *pClass,` |
|       - |  6742 | `	const char **pzName,` |
|       - |  6743 | `	sxu32 *pnName)` |
|       2 |  6744 |  |
|       - |  6745 | `	const char *z;` |
|       - |  6746 | `	sxu32 n;` |
|     184 |  6747 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     154 |  6748 | `		return 0;` |
|       - |  6749 | `	}` |
|      32 |  6750 | `	z = pClass->zString;` |
|      32 |  6751 | `	n = pClass->nByte;` |
|      32 |  6752 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       5 |  6753 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6754 | `	}` |
|      28 |  6755 | `	if( n == 5 && SyMemcmpNoCase(z,"mixed",5) == 0 ){` |
|     ! 0 |  6756 | `		*pzName = "mixed"; *pnName = 5; return 1;` |
|       - |  6757 | `	}` |
|      28 |  6758 | `	if( n == 8 && SyMemcmpNoCase(z,"iterable",8) == 0 ){` |
|     ! 0 |  6759 | `		*pzName = "iterable"; *pnName = 8; return 1;` |
|       - |  6760 | `	}` |
|      28 |  6761 | `	return 0;` |
|      93 |  6762 |  |
|       - |  6763 |  |
|       - |  6764 | `/*` |
|       - |  6765 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6766 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6767 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6768 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6769 | `` * unions like `callable\|int`).`` |
|       - |  6770 | ` *` |
|       - |  6771 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6772 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6773 | ` */` |
|     154 |  6774 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6775 | `	ph7_gen_state *pGen,` |
|       - |  6776 | `	ph7_class *pClass,` |
|       - |  6777 | `	const SyString *pPropName,` |
|       - |  6778 | `	sxu32 nType,` |
|       - |  6779 | `	const SyString *pTypeClass,` |
|       - |  6780 | `	const SyString *pTypeText,` |
|       - |  6781 | `	SySet *pUnionAlts,` |
|       - |  6782 | `	sxu32 nLine)` |
|       2 |  6783 |  |
|     156 |  6784 | `	const char *zBad = 0;` |
|     156 |  6785 | `	sxu32 nBad = 0;` |
|       - |  6786 | `	SyString sFallback;` |
|       - |  6787 | `	const SyString *pBad;` |
|       - |  6788 | `	sxi32 rc;` |
|     156 |  6789 | `	int bDisallowed = 0;` |
|     156 |  6790 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6791 | `		bDisallowed = 1;` |
|     155 |  6792 | `	}else if( pUnionAlts ){` |
|       - |  6793 | `		sxu32 i;` |
|      42 |  6794 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      30 |  6795 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      30 |  6796 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6797 | `				bDisallowed = 1;` |
|       3 |  6798 | `				break;` |
|       - |  6799 | `			}` |
|      15 |  6800 | `		}` |
|       7 |  6801 | `	}` |
|     156 |  6802 | `	if( !bDisallowed ){` |
|     152 |  6803 | `		return SXRET_OK;` |
|       - |  6804 | `	}` |
|       - |  6805 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6806 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6807 | `	 * canonical spelling if the type text is unavailable. */` |
|       5 |  6808 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       5 |  6809 | `		pBad = pTypeText;` |
|       3 |  6810 | `	}else{` |
|     ! 0 |  6811 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6812 | `		pBad = &sFallback;` |
|       - |  6813 | `	}` |
|       7 |  6814 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6815 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6816 | `		&pClass->sName,pPropName,pBad);` |
|       5 |  6817 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6818 | `		return SXERR_ABORT;` |
|       - |  6819 | `	}` |
|       5 |  6820 | `	return SXERR_SYNTAX;` |
|      79 |  6821 |  |
|       - |  6822 |  |
|   38154 |  6823 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6824 |  |
|   38156 |  6825 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6826 | `	ph7_class_attr *pAttr;` |
|       - |  6827 | `	SyString *pName;` |
|       - |  6828 | `	sxi32 rc;` |
|   38156 |  6829 | `	sxu32 nType = 0;` |
|       - |  6830 | `	SyString sTypeClass;` |
|       - |  6831 | `	SyString sTypeText;` |
|       - |  6832 | `	SySet aUnionAlts;` |
|   38156 |  6833 | `	sxi32 iTypeFlags = 0;` |
|   38156 |  6834 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   38156 |  6835 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   38156 |  6836 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6837 | `	/* Extract visibility level */` |
|   38156 |  6838 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6839 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   38214 |  6840 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     118 |  6841 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     118 |  6842 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6843 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6844 | `			goto Synchronize;` |
|     118 |  6845 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  6846 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6847 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  6848 | `				&pGen->pIn->sData);` |
|     ! 0 |  6849 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6850 | `				return SXERR_ABORT;` |
|       - |  6851 | `			}` |
|     ! 0 |  6852 | `			goto Synchronize;` |
|     118 |  6853 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6854 | `			return SXERR_ABORT;` |
|       - |  6855 | `		}` |
|      58 |  6856 | `	}` |
|     ! 0 |  6857 | `loop:` |
|   38160 |  6858 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6859 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  6860 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6861 | `			return SXERR_ABORT;` |
|       - |  6862 | `		}` |
|     ! 0 |  6863 | `		goto Synchronize;` |
|       - |  6864 | `	}` |
|   38160 |  6865 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   38160 |  6866 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  6867 | `		/* Invalid attribute name */` |
|     ! 0 |  6868 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  6869 | `		if( rc == SXERR_ABORT ){` |
|       - |  6870 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6871 | `			return SXERR_ABORT;` |
|       - |  6872 | `		}` |
|     ! 0 |  6873 | `		goto Synchronize;` |
|       - |  6874 | `	}` |
|       - |  6875 | `	/* Peek attribute name */` |
|   38160 |  6876 | `	pName = &pGen->pIn->sData;` |
|       - |  6877 | `	/* Advance the stream cursor */` |
|   38160 |  6878 | `	pGen->pIn++;` |
|   38160 |  6879 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  6880 | `		/* Invalid declaration */` |
|       3 |  6881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  6882 | `		if( rc == SXERR_ABORT ){` |
|       - |  6883 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6884 | `			return SXERR_ABORT;` |
|       - |  6885 | `		}` |
|       3 |  6886 | `		goto Synchronize;` |
|       - |  6887 | `	}` |
|       - |  6888 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  6889 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  6890 | `	 * by the type parser. */` |
|   38158 |  6891 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     182 |  6892 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  6893 | `			&sTypeText,` |
|     120 |  6894 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     122 |  6895 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6896 | `			return SXERR_ABORT;` |
|     122 |  6897 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  6898 | `			goto Synchronize;` |
|       - |  6899 | `		}` |
|      60 |  6900 | `	}` |
|       - |  6901 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   38158 |  6902 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  6903 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  6904 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  6905 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6906 | `			return SXERR_ABORT;` |
|       - |  6907 | `		}` |
|       3 |  6908 | `		goto Synchronize;` |
|       - |  6909 | `	}` |
|       - |  6910 | `	/* Allocate a new class attribute */` |
|   38156 |  6911 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   38156 |  6912 | `	if( pAttr == 0 ){` |
|     ! 0 |  6913 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  6914 | `		return SXERR_ABORT;` |
|       - |  6915 | `	}` |
|   38156 |  6916 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     120 |  6917 | `		pAttr->nType = nType;` |
|     120 |  6918 | `		pAttr->sClass = sTypeClass;` |
|     120 |  6919 | `		pAttr->sTypeName = sTypeText;` |
|     120 |  6920 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6921 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  6922 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  6923 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  6924 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  6925 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  6926 | `			sxu32 i;` |
|      32 |  6927 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  6928 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  6929 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  6930 | `			}` |
|       5 |  6931 | `		}` |
|      59 |  6932 | `	}` |
|   38156 |  6933 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  6934 | `		SySet *pInstrContainer;` |
|   11936 |  6935 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  6936 | `		/* Swap bytecode container */` |
|   11936 |  6937 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   11936 |  6938 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  6939 | `		/* Compile attribute value.` |
|       - |  6940 | `		 */` |
|   11936 |  6941 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   11936 |  6942 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  6943 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  6944 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6945 | `				return SXERR_ABORT;` |
|       - |  6946 | `			}` |
|     ! 0 |  6947 | `		}` |
|       - |  6948 | `		/* Emit the done instruction */` |
|   11936 |  6949 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   11936 |  6950 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5967 |  6951 | `	}` |
|       - |  6952 | `	/* All done,install the attribute */` |
|   38156 |  6953 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   38156 |  6954 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6955 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6956 | `		return SXERR_ABORT;` |
|       - |  6957 | `	}` |
|   38156 |  6958 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6959 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  6960 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  6961 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  6962 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6963 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6964 | `				pTok--;` |
|     ! 0 |  6965 | `			}` |
|     ! 0 |  6966 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6967 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  6968 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6969 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6970 | `				return SXERR_ABORT;` |
|       - |  6971 | `			}` |
|     ! 0 |  6972 | `		}else{` |
|       5 |  6973 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  6974 | `				goto loop;` |
|       - |  6975 | `			}` |
|       - |  6976 | `		}` |
|     ! 0 |  6977 | `	}` |
|   38152 |  6978 | `	SySetRelease(&aUnionAlts);` |
|   38152 |  6979 | `	return SXRET_OK;` |
|       2 |  6980 | `Synchronize:` |
|       - |  6981 | `	/* Synchronize with the first semi-colon */` |
|      11 |  6982 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  6983 | `		pGen->pIn++;` |
|       1 |  6984 | `	}` |
|       5 |  6985 | `	SySetRelease(&aUnionAlts);` |
|       5 |  6986 | `	return SXERR_CORRUPT;` |
|   19079 |  6987 |  |
|       - |  6988 | `/*` |
|       - |  6989 | ` * Compile a class method.` |
|       - |  6990 | ` *` |
|       - |  6991 | ` * Refer to the official documentation for more information` |
|       - |  6992 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  6993 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  6994 | ` * overloading and many more.` |
|       - |  6995 | ` */` |
|  139866 |  6996 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  6997 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6998 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  6999 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7000 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7001 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7002 | `	)` |
|       2 |  7003 |  |
|  139868 |  7004 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7005 | `	ph7_class_method *pMeth;` |
|       - |  7006 | `	sxi32 iFuncFlags;` |
|       - |  7007 | `	SyString *pName;` |
|       - |  7008 | `	SyToken *pEnd;` |
|       - |  7009 | `	sxi32 rc;` |
|       - |  7010 | `	/* Extract visibility level */` |
|  139868 |  7011 | `	iProtection = GetProtectionLevel(iProtection);` |
|  139868 |  7012 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  139868 |  7013 | `	iFuncFlags = 0;` |
|  139868 |  7014 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7015 | `		/* Invalid method name */` |
|     ! 0 |  7016 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7017 | `		if( rc == SXERR_ABORT ){` |
|       - |  7018 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7019 | `			return SXERR_ABORT;` |
|       - |  7020 | `		}` |
|     ! 0 |  7021 | `		goto Synchronize;` |
|       - |  7022 | `	}` |
|  139868 |  7023 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7024 | `		/* Return by reference,remember that */` |
|     ! 0 |  7025 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7026 | `		/* Jump the '&' token */` |
|     ! 0 |  7027 | `		pGen->pIn++;` |
|     ! 0 |  7028 | `	}` |
|  139868 |  7029 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7030 | `		/* Invalid method name */` |
|     ! 0 |  7031 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7032 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7033 | `			return SXERR_ABORT;` |
|       - |  7034 | `		}` |
|     ! 0 |  7035 | `		goto Synchronize;` |
|       - |  7036 | `	}` |
|       - |  7037 | `	/* Peek method name */` |
|  139868 |  7038 | `	pName = &pGen->pIn->sData;` |
|  139868 |  7039 | `	nLine = pGen->pIn->nLine;` |
|       - |  7040 | `	/* Jump the method name */` |
|  139868 |  7041 | `	pGen->pIn++;` |
|  139868 |  7042 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7043 | `		/* Abstract method */` |
|   23268 |  7044 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7045 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7046 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7047 | `				&pClass->sName,pName);` |
|     ! 0 |  7048 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7049 | `				return SXERR_ABORT;` |
|       - |  7050 | `			}` |
|     ! 0 |  7051 | `		}` |
|       - |  7052 | `		/* Assemble method signature only */` |
|   23268 |  7053 | `		doBody = FALSE;` |
|   11633 |  7054 | `	}` |
|  139868 |  7055 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7056 | `		/* Syntax error */` |
|     ! 0 |  7057 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7058 | `		if( rc == SXERR_ABORT ){` |
|       - |  7059 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7060 | `			return SXERR_ABORT;` |
|       - |  7061 | `		}` |
|     ! 0 |  7062 | `		goto Synchronize;` |
|       - |  7063 | `	}` |
|       - |  7064 | `	/* Allocate a new class_method instance */` |
|  139868 |  7065 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  139868 |  7066 | `	if( pMeth == 0 ){` |
|     ! 0 |  7067 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7068 | `		return SXERR_ABORT;` |
|       - |  7069 | `	}` |
|       - |  7070 | `	/* Jump the left parenthesis '(' */` |
|  139868 |  7071 | `	pGen->pIn++;` |
|  139868 |  7072 | `	pEnd = 0; /* cc warning */` |
|       - |  7073 | `	/* Delimit the method signature */` |
|  139868 |  7074 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  139868 |  7075 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7076 | `		/* Syntax error */` |
|       3 |  7077 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7078 | `		if( rc == SXERR_ABORT ){` |
|       - |  7079 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7080 | `			return SXERR_ABORT;` |
|       - |  7081 | `		}` |
|       3 |  7082 | `		goto Synchronize;` |
|       - |  7083 | `	}` |
|       - |  7084 | `	{` |
|  139866 |  7085 | `		int bIsCtor = 0;` |
|  139866 |  7086 | `		int bAbstractCtor = 0;` |
|  202496 |  7087 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   84500 |  7088 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  132566 |  7089 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   14602 |  7090 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7091 | `				bAbstractCtor = 1;` |
|       2 |  7092 | `			}else{` |
|   14600 |  7093 | `				bIsCtor = 1;` |
|       - |  7094 | `			}` |
|    7300 |  7095 | `		}` |
|  139866 |  7096 | `		if( pGen->pIn < pEnd ){` |
|       - |  7097 | `			/* Collect method arguments */` |
|   29166 |  7098 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   29166 |  7099 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7100 | `				return SXERR_ABORT;` |
|       - |  7101 | `			}` |
|   14582 |  7102 | `		}` |
|       - |  7103 | `	}` |
|       - |  7104 | `	/* Point past ')' and parse optional return type ': type' */` |
|  139866 |  7105 | `	pGen->pIn = &pEnd[1];` |
|       - |  7106 | `	{` |
|  139866 |  7107 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  139866 |  7108 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7109 | `			return SXERR_ABORT;` |
|  139866 |  7110 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7111 | `			goto Synchronize;` |
|       - |  7112 | `		}` |
|       - |  7113 | `	}` |
|       - |  7114 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7115 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7116 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7117 | `	{` |
|  139866 |  7118 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7119 | `		sxu32 i;` |
|  189378 |  7120 | `		for( i = 0; i < nArg; i++ ){` |
|   49522 |  7121 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7122 | `			ph7_class_attr *pAttr;` |
|   49522 |  7123 | `			sxi32 iAttrFlags = 0;` |
|   49522 |  7124 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   49486 |  7125 | `				continue;` |
|       - |  7126 | `			}` |
|      38 |  7127 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7128 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7129 | `					"Cannot declare variadic promoted property");` |
|       3 |  7130 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7131 | `					return SXERR_ABORT;` |
|       - |  7132 | `				}` |
|       3 |  7133 | `				goto Synchronize;` |
|       - |  7134 | `			}` |
|       - |  7135 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7136 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7137 | `			 * appear as an alternative of a union type. */` |
|      34 |  7138 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       6 |  7139 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      53 |  7140 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7141 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7142 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7143 | `					nLine);` |
|      36 |  7144 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7145 | `					return SXERR_ABORT;` |
|      36 |  7146 | `				}else if( rc != SXRET_OK ){` |
|       5 |  7147 | `					goto Synchronize;` |
|       - |  7148 | `				}` |
|      15 |  7149 | `			}` |
|       - |  7150 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      32 |  7151 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7152 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7153 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7154 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7155 | `					return SXERR_ABORT;` |
|       - |  7156 | `				}` |
|       3 |  7157 | `				goto Synchronize;` |
|       - |  7158 | `			}` |
|      30 |  7159 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      28 |  7160 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7161 | `			}` |
|      30 |  7162 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7163 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7164 | `			}` |
|      30 |  7165 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7166 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7167 | `			}` |
|      30 |  7168 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      30 |  7169 | `			if( pAttr == 0 ){` |
|     ! 0 |  7170 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7171 | `				return SXERR_ABORT;` |
|       - |  7172 | `			}` |
|      30 |  7173 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      28 |  7174 | `				pAttr->nType = pArg->nType;` |
|      28 |  7175 | `				pAttr->sClass = pArg->sClass;` |
|      28 |  7176 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      28 |  7177 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7178 | `					sxu32 k;` |
|     ! 0 |  7179 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7180 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7181 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7182 | `					}` |
|     ! 0 |  7183 | `				}` |
|      13 |  7184 | `			}` |
|      30 |  7185 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      30 |  7186 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7187 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7188 | `				return SXERR_ABORT;` |
|       - |  7189 | `			}` |
|      16 |  7190 | `		}` |
|       - |  7191 | `	}` |
|  139858 |  7192 | `	if( doBody ){` |
|       - |  7193 | `		/* Compile method body */` |
|  116592 |  7194 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  116592 |  7195 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7196 | `			return SXERR_ABORT;` |
|       - |  7197 | `		}` |
|   58297 |  7198 | `	}else{` |
|       - |  7199 | `		/* Only method signature is allowed */` |
|   23268 |  7200 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7201 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7202 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7203 | `				if( rc == SXERR_ABORT ){` |
|       - |  7204 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7205 | `					return SXERR_ABORT;` |
|       - |  7206 | `				}` |
|     ! 0 |  7207 | `				return SXERR_CORRUPT;` |
|       - |  7208 | `			}` |
|       - |  7209 | `	}` |
|       - |  7210 | `	/* All done,install the method */` |
|  139858 |  7211 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  139858 |  7212 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7213 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7214 | `		return SXERR_ABORT;` |
|       - |  7215 | `	}` |
|  139858 |  7216 | `	return SXRET_OK;` |
|       5 |  7217 | `Synchronize:` |
|       - |  7218 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7219 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      21 |  7220 | `		pGen->pIn++;` |
|       1 |  7221 | `	}` |
|      11 |  7222 | `	return SXERR_CORRUPT;` |
|   69935 |  7223 |  |
|       - |  7224 | `/*` |
|       - |  7225 | ` * Compile an object interface.` |
|       - |  7226 | ` *  According to the PHP language reference manual` |
|       - |  7227 | ` *   Object Interfaces:` |
|       - |  7228 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7229 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7230 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7231 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7232 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7233 | ` */` |
|    8744 |  7234 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  7235 |  |
|    8746 |  7236 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7237 | `	ph7_class *pClass,*pBase;` |
|       - |  7238 | `	SyToken *pEnd,*pTmp;` |
|       - |  7239 | `	SyString *pName;` |
|       - |  7240 | `	sxi32 nKwrd;` |
|       - |  7241 | `	sxi32 rc;` |
|       - |  7242 | `	/* Jump the 'interface' keyword */` |
|    8746 |  7243 | `	pGen->pIn++;` |
|       - |  7244 | `	/* Extract interface name */` |
|    8746 |  7245 | `	pName = &pGen->pIn->sData;` |
|       - |  7246 | `	/* Advance the stream cursor */` |
|    8746 |  7247 | `	pGen->pIn++;` |
|       - |  7248 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7249 | `		SyBlob sFQN;` |
|       - |  7250 | `		SyString sFQNStr;` |
|    8746 |  7251 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8746 |  7252 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8746 |  7253 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8746 |  7254 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8746 |  7255 | `		SyBlobRelease(&sFQN);` |
|       - |  7256 | `	}` |
|    8746 |  7257 | `	if( pClass == 0 ){` |
|     ! 0 |  7258 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7259 | `		return SXERR_ABORT;` |
|       - |  7260 | `	}` |
|       - |  7261 | `	/* Mark as an interface */` |
|    8746 |  7262 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7263 | `	/* Assume no base class is given */` |
|    8746 |  7264 | `	pBase = 0;` |
|    8746 |  7265 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  7266 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  7267 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7268 | `			SyString *pBaseName;` |
|       - |  7269 | `			/* Extract base interface */` |
|       3 |  7270 | `			pGen->pIn++;` |
|       3 |  7271 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7272 | `				/* Syntax error */` |
|     ! 0 |  7273 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7274 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7275 | `					pName);` |
|     ! 0 |  7276 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7277 | `				if( rc == SXERR_ABORT ){` |
|       - |  7278 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7279 | `					return SXERR_ABORT;` |
|       - |  7280 | `				}` |
|     ! 0 |  7281 | `				return SXRET_OK;` |
|       - |  7282 | `			}` |
|       3 |  7283 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  7284 | `			{` |
|       - |  7285 | `				SyBlob sResolved;` |
|       3 |  7286 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 |  7287 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 |  7288 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 |  7289 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 |  7290 | `				SyBlobRelease(&sResolved);` |
|       - |  7291 | `			}` |
|       - |  7292 | `			/* Only interfaces is allowed */` |
|       3 |  7293 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7294 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7295 | `			}` |
|       3 |  7296 | `			if( pBase == 0 ){` |
|       - |  7297 | `				/* Inexistant interface */` |
|     ! 0 |  7298 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 |  7299 | `				if( rc == SXERR_ABORT ){` |
|       - |  7300 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7301 | `					return SXERR_ABORT;` |
|       - |  7302 | `				}` |
|     ! 0 |  7303 | `			}` |
|       - |  7304 | `			/* Advance the stream cursor */` |
|       3 |  7305 | `			pGen->pIn++;` |
|       1 |  7306 | `		}` |
|       1 |  7307 | `	}` |
|    8746 |  7308 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7309 | `		/* Syntax error */` |
|     ! 0 |  7310 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7311 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7312 | `		if( rc == SXERR_ABORT ){` |
|       - |  7313 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7314 | `			return SXERR_ABORT;` |
|       - |  7315 | `		}` |
|     ! 0 |  7316 | `		return SXRET_OK;` |
|       - |  7317 | `	}` |
|    8746 |  7318 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8746 |  7319 | `	pEnd = 0; /* cc warning */` |
|       - |  7320 | `	/* Delimit the interface body */` |
|    8746 |  7321 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8746 |  7322 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7323 | `		/* Syntax error */` |
|     ! 0 |  7324 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7325 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7326 | `		if( rc == SXERR_ABORT ){` |
|       - |  7327 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7328 | `			return SXERR_ABORT;` |
|       - |  7329 | `		}` |
|     ! 0 |  7330 | `		return SXRET_OK;` |
|       - |  7331 | `	}` |
|       - |  7332 | `	/* Swap token stream */` |
|    8746 |  7333 | `	pTmp = pGen->pEnd;` |
|    8746 |  7334 | `	pGen->pEnd = pEnd;` |
|       - |  7335 | `	/* Start the parse process` |
|       - |  7336 | `	 * Note (According to the PHP reference manual):` |
|       - |  7337 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7338 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7339 | `	 */` |
|   15999 |  7340 | `	for(;;){` |
|       - |  7341 | `		/* Jump leading/trailing semi-colons */` |
|   55254 |  7342 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   23256 |  7343 | `			pGen->pIn++;` |
|       2 |  7344 | `		}` |
|   32000 |  7345 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7346 | `			/* End of interface body */` |
|    8744 |  7347 | `			break;` |
|       - |  7348 | `		}` |
|   23258 |  7349 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7350 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7351 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7352 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7353 | `			if( rc == SXERR_ABORT ){` |
|       - |  7354 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7355 | `				return SXERR_ABORT;` |
|       - |  7356 | `			}` |
|     ! 0 |  7357 | `			goto done;` |
|       - |  7358 | `		}` |
|       - |  7359 | `		/* Extract the current keyword */` |
|   23258 |  7360 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   23258 |  7361 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7362 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7363 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7364 | `			const char *zKind = "member";` |
|       3 |  7365 | `			SyString *pMemberName = 0;` |
|       3 |  7366 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7367 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7368 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7369 | `					zKind = "constant";` |
|       3 |  7370 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7371 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7372 | `					}` |
|       1 |  7373 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7374 | `					zKind = "method";` |
|     ! 0 |  7375 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7376 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7377 | `					}` |
|     ! 0 |  7378 | `				}` |
|       1 |  7379 | `			}` |
|       3 |  7380 | `			if( pMemberName ){` |
|       4 |  7381 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7382 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7383 | `			}else{` |
|     ! 0 |  7384 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7385 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7386 | `			}` |
|       3 |  7387 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7388 | `				return SXERR_ABORT;` |
|       - |  7389 | `			}` |
|       3 |  7390 | `			goto done;` |
|       - |  7391 | `		}` |
|   23256 |  7392 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7393 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7394 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7395 | `			if( rc == SXERR_ABORT ){` |
|       - |  7396 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7397 | `				return SXERR_ABORT;` |
|       - |  7398 | `			}` |
|     ! 0 |  7399 | `			goto done;` |
|       - |  7400 | `		}` |
|   23256 |  7401 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7402 | `			/* Advance the stream cursor */` |
|   23252 |  7403 | `			pGen->pIn++;` |
|   23252 |  7404 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7405 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7406 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7407 | `				if( rc == SXERR_ABORT ){` |
|       - |  7408 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7409 | `					return SXERR_ABORT;` |
|       - |  7410 | `				}` |
|     ! 0 |  7411 | `				goto done;` |
|       - |  7412 | `			}` |
|   23252 |  7413 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   23252 |  7414 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7415 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7416 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7417 | `				if( rc == SXERR_ABORT ){` |
|       - |  7418 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7419 | `					return SXERR_ABORT;` |
|       - |  7420 | `				}` |
|     ! 0 |  7421 | `				goto done;` |
|       - |  7422 | `			}` |
|   11625 |  7423 | `		}` |
|   23256 |  7424 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7425 | `			/* Parse constant */` |
|       3 |  7426 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7427 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7428 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7429 | `					return SXERR_ABORT;` |
|       - |  7430 | `				}` |
|     ! 0 |  7431 | `				goto done;` |
|       - |  7432 | `			}` |
|       2 |  7433 | `		}else{` |
|   23254 |  7434 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   23254 |  7435 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7436 | `				/* Static method,record that */` |
|     ! 0 |  7437 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7438 | `				/* Advance the stream cursor */` |
|     ! 0 |  7439 | `				pGen->pIn++;` |
|     ! 0 |  7440 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  7441 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7442 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7443 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7444 | `						if( rc == SXERR_ABORT ){` |
|       - |  7445 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7446 | `							return SXERR_ABORT;` |
|       - |  7447 | `						}` |
|     ! 0 |  7448 | `						goto done;` |
|       - |  7449 | `				}` |
|     ! 0 |  7450 | `			}` |
|       - |  7451 | `			/* Process method signature (no body for interface methods) */` |
|   23254 |  7452 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   23254 |  7453 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7454 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7455 | `					return SXERR_ABORT;` |
|       - |  7456 | `				}` |
|     ! 0 |  7457 | `				goto done;` |
|       - |  7458 | `			}` |
|       - |  7459 | `		}` |
|       2 |  7460 | `	}` |
|       - |  7461 | `	/* Install the interface */` |
|    8744 |  7462 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8744 |  7463 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7464 | `		/* Inherit from the base interface */` |
|       3 |  7465 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 |  7466 | `	}` |
|    8744 |  7467 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7468 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7469 | `		return SXERR_ABORT;` |
|       - |  7470 | `	}` |
|    4371 |  7471 | `done:` |
|       - |  7472 | `	/* Point beyond the interface body */` |
|    8746 |  7473 | `	pGen->pIn  = &pEnd[1];` |
|    8746 |  7474 | `	pGen->pEnd = pTmp;` |
|    8746 |  7475 | `	return PH7_OK;` |
|    4374 |  7476 |  |
|       - |  7477 | `/*` |
|       - |  7478 | ` * Compile a user-defined class.` |
|       - |  7479 | ` * According to the PHP language reference manual` |
|       - |  7480 | ` *  class` |
|       - |  7481 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7482 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7483 | ` *  of the properties and methods belonging to the class.` |
|       - |  7484 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7485 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7486 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7487 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7488 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7489 | ` *  (called "methods").` |
|       - |  7490 | ` */` |
|       - |  7491 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7492 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7493 | `struct TraitUseEntry {` |
|       - |  7494 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7495 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7496 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7497 | `};` |
|       - |  7498 | `/*` |
|       - |  7499 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7500 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7501 | ` */` |
|   41346 |  7502 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7503 |  |
|       - |  7504 | `	ph7_class **apIface;` |
|       - |  7505 | `	sxu32 nIface,i;` |
|       - |  7506 | `	sxi32 rc;` |
|   41348 |  7507 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7508 | `		return SXRET_OK;` |
|       - |  7509 | `	}` |
|   41348 |  7510 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   41348 |  7511 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   44288 |  7512 | `	for(i = 0; i < nIface; i++){` |
|    2942 |  7513 | `		ph7_class *pIface = apIface[i];` |
|       - |  7514 | `		SyHashEntry *pEntry;` |
|    2942 |  7515 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   17530 |  7516 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   14590 |  7517 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7518 | `			ph7_class_method *pImplMeth;` |
|   14590 |  7519 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7520 | `			/* Find the implementing method in the class */` |
|   14590 |  7521 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   14590 |  7522 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7523 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7524 | `			}` |
|       - |  7525 | `			/* Check visibility: interface methods must be implemented as public */` |
|   14576 |  7526 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7527 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7528 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7529 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7530 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7531 | `					return SXERR_ABORT;` |
|       - |  7532 | `				}` |
|       1 |  7533 | `			}` |
|       - |  7534 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7535 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7536 | `			 */` |
|       - |  7537 | `			{` |
|   14576 |  7538 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   14576 |  7539 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   14576 |  7540 | `				int sigError = 0;` |
|   14576 |  7541 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7542 | `					sigError = 1;` |
|   14575 |  7543 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7544 | `					/* Extra parameters must all have default values */` |
|       5 |  7545 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7546 | `					sxu32 k;` |
|       7 |  7547 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7548 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7549 | `							sigError = 1;` |
|       3 |  7550 | `							break;` |
|       - |  7551 | `						}` |
|       2 |  7552 | `					}` |
|       2 |  7553 | `				}` |
|   14576 |  7554 | `				if( sigError ){` |
|       - |  7555 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7556 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7557 | `					sxu32 j;` |
|       5 |  7558 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7559 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7560 | `					/* Build implementing method signature */` |
|       5 |  7561 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7562 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7563 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7564 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7565 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7566 | `					}` |
|       - |  7567 | `					/* Build interface method signature */` |
|       5 |  7568 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7569 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7570 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7571 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7572 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7573 | `					}` |
|       7 |  7574 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7575 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7576 | `						&pClass->sName,pMName,` |
|       4 |  7577 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7578 | `						&pIface->sName,pMName,` |
|       4 |  7579 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7580 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7581 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7582 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7583 | `						return SXERR_ABORT;` |
|       - |  7584 | `					}` |
|       2 |  7585 | `				}` |
|       - |  7586 | `			}` |
|       2 |  7587 | `		}` |
|    1472 |  7588 | `	}` |
|   41348 |  7589 | `	return SXRET_OK;` |
|   20675 |  7590 |  |
|       - |  7591 | `/*` |
|       - |  7592 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7593 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7594 | ` */` |
|   41346 |  7595 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7596 |  |
|       - |  7597 | `	ph7_class_method *pMeth;` |
|       - |  7598 | `	SyHashEntry *pEntry;` |
|       - |  7599 | `	sxu32 nAbstract;` |
|       - |  7600 | `	SyBlob sMsg;` |
|       - |  7601 | `	sxi32 rc;` |
|       - |  7602 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   41348 |  7603 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      22 |  7604 | `		return SXRET_OK;` |
|       - |  7605 | `	}` |
|       - |  7606 | `	/* Count abstract methods */` |
|   41328 |  7607 | `	nAbstract = 0;` |
|   41328 |  7608 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  390506 |  7609 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  349180 |  7610 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  349180 |  7611 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7612 | `			nAbstract++;` |
|       8 |  7613 | `		}` |
|       2 |  7614 | `	}` |
|   41328 |  7615 | `	if( nAbstract == 0 ){` |
|   41314 |  7616 | `		return SXRET_OK;` |
|       - |  7617 | `	}` |
|       - |  7618 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7619 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7620 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7621 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7622 | `		&pClass->sName,nAbstract,` |
|       7 |  7623 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7624 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7625 | `	/* Second pass: list methods with origins */` |
|       - |  7626 | `	{` |
|      15 |  7627 | `		sxu32 nListed = 0;` |
|      15 |  7628 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7629 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7630 | `			ph7_class *pOrigin = 0;` |
|       - |  7631 | `			SyString *pMName;` |
|      19 |  7632 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7633 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7634 | `				continue;` |
|       - |  7635 | `			}` |
|      17 |  7636 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7637 | `			if( nListed > 0 ){` |
|       3 |  7638 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7639 | `			}` |
|       - |  7640 | `			/* Find the origin of this abstract method.` |
|       - |  7641 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7642 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7643 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7644 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7645 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7646 | `			 * class's namespace.` |
|       - |  7647 | `			 */` |
|       - |  7648 | `			{` |
|       - |  7649 | `				ph7_class **apIface;` |
|       - |  7650 | `				ph7_class **apTrait;` |
|       - |  7651 | `				ph7_class *pWalk;` |
|       - |  7652 | `				sxu32 i;` |
|       - |  7653 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7654 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7655 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7656 | `				 */` |
|      17 |  7657 | `				if( pClass->pBase ){` |
|       9 |  7658 | `					pWalk = pClass->pBase;` |
|      17 |  7659 | `					while( pWalk ){` |
|       - |  7660 | `						ph7_class_method *pParentMeth;` |
|      11 |  7661 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7662 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7663 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7664 | `							 * in this class's ancestor chain.` |
|       - |  7665 | `							 */` |
|      11 |  7666 | `							int fromIface = 0;` |
|      11 |  7667 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7668 | `							while( pAnc ){` |
|       - |  7669 | `								ph7_class **apPI;` |
|       - |  7670 | `								sxu32 j;` |
|      13 |  7671 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7672 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7673 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7674 | `										fromIface = 1;` |
|       9 |  7675 | `										break;` |
|       - |  7676 | `									}` |
|     ! 0 |  7677 | `								}` |
|      13 |  7678 | `								if( fromIface ) break;` |
|       5 |  7679 | `								pAnc = pAnc->pBase;` |
|       1 |  7680 | `							}` |
|      11 |  7681 | `							if( !fromIface ){` |
|       3 |  7682 | `								pOrigin = pWalk;` |
|       3 |  7683 | `								break;` |
|       - |  7684 | `							}` |
|       4 |  7685 | `						}` |
|       9 |  7686 | `						pWalk = pWalk->pBase;` |
|       1 |  7687 | `					}` |
|       4 |  7688 | `				}` |
|       - |  7689 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7690 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7691 | `				 */` |
|      17 |  7692 | `				if( !pOrigin ){` |
|      15 |  7693 | `					pWalk = pClass;` |
|      37 |  7694 | `					while( pWalk && !pOrigin ){` |
|      23 |  7695 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7696 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7697 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7698 | `							ph7_class *pDeepest = 0;` |
|      25 |  7699 | `							while( pIface ){` |
|      13 |  7700 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7701 | `									pDeepest = pIface;` |
|       6 |  7702 | `								}` |
|      13 |  7703 | `								pIface = pIface->pBase;` |
|       1 |  7704 | `							}` |
|      13 |  7705 | `							if( pDeepest ){` |
|      13 |  7706 | `								pOrigin = pDeepest;` |
|      13 |  7707 | `								break;` |
|       - |  7708 | `							}` |
|     ! 0 |  7709 | `						}` |
|      23 |  7710 | `						pWalk = pWalk->pBase;` |
|       1 |  7711 | `					}` |
|       7 |  7712 | `				}` |
|       - |  7713 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7714 | `				if( !pOrigin ){` |
|       3 |  7715 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7716 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7717 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7718 | `							pOrigin = pClass;` |
|       3 |  7719 | `							break;` |
|       - |  7720 | `						}` |
|     ! 0 |  7721 | `					}` |
|       1 |  7722 | `				}` |
|       - |  7723 | `			}` |
|      17 |  7724 | `			if( pOrigin ){` |
|      17 |  7725 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7726 | `			}else{` |
|       - |  7727 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7728 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7729 | `			}` |
|      17 |  7730 | `			nListed++;` |
|       1 |  7731 | `		}` |
|       - |  7732 | `	}` |
|      15 |  7733 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7734 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7735 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7736 | `	SyBlobRelease(&sMsg);` |
|      15 |  7737 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7738 | `		return SXERR_ABORT;` |
|       - |  7739 | `	}` |
|      15 |  7740 | `	return SXRET_OK;` |
|   20675 |  7741 |  |
|   41360 |  7742 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  7743 |  |
|   41362 |  7744 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7745 | `	ph7_class *pClass,*pBase;` |
|       - |  7746 | `	SyToken *pEnd,*pTmp;` |
|       - |  7747 | `	sxi32 iProtection;` |
|       - |  7748 | `	SySet aInterfaces;` |
|       - |  7749 | `	SySet aUseEntries;` |
|       - |  7750 | `	sxi32 iAttrflags;` |
|       - |  7751 | `	SyString *pName;` |
|       - |  7752 | `	sxi32 nKwrd;` |
|       - |  7753 | `	sxi32 rc;` |
|       - |  7754 | `	/* Jump the 'class' keyword */` |
|   41362 |  7755 | `	pGen->pIn++;` |
|   41362 |  7756 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7757 | `		/* Syntax error */` |
|     ! 0 |  7758 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  7759 | `		if( rc == SXERR_ABORT ){` |
|       - |  7760 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7761 | `			return SXERR_ABORT;` |
|       - |  7762 | `		}` |
|       - |  7763 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  7764 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  7765 | `			pGen->pIn++;` |
|     ! 0 |  7766 | `		}` |
|     ! 0 |  7767 | `		return SXRET_OK;` |
|       - |  7768 | `	}` |
|       - |  7769 | `	/* Extract class name */` |
|   41362 |  7770 | `	pName = &pGen->pIn->sData;` |
|       - |  7771 | `	/* Advance the stream cursor */` |
|   41362 |  7772 | `	pGen->pIn++;` |
|       - |  7773 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7774 | `		SyBlob sFQN;` |
|       - |  7775 | `		SyString sFQNStr;` |
|   41362 |  7776 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   41362 |  7777 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   41362 |  7778 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   41362 |  7779 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   41362 |  7780 | `		SyBlobRelease(&sFQN);` |
|       - |  7781 | `	}` |
|   41362 |  7782 | `	if( pClass == 0 ){` |
|     ! 0 |  7783 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7784 | `		return SXERR_ABORT;` |
|       - |  7785 | `	}` |
|       - |  7786 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   41362 |  7787 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   41362 |  7788 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  7789 | `	/* Assume a standalone class */` |
|   41362 |  7790 | `	pBase = 0;` |
|   41362 |  7791 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7792 | `		SyString *pBaseName;` |
|   29178 |  7793 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   29178 |  7794 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   26240 |  7795 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   26240 |  7796 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7797 | `				/* Syntax error */` |
|     ! 0 |  7798 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7799 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 |  7800 | `					pName);` |
|     ! 0 |  7801 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7802 | `				if( rc == SXERR_ABORT ){` |
|       - |  7803 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7804 | `					return SXERR_ABORT;` |
|       - |  7805 | `				}` |
|     ! 0 |  7806 | `				return SXRET_OK;` |
|       - |  7807 | `			}` |
|       - |  7808 | `			/* Extract base class name and resolve through namespace/imports */` |
|   26240 |  7809 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  7810 | `			{` |
|       - |  7811 | `				SyBlob sResolved;` |
|   26240 |  7812 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   26240 |  7813 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   39359 |  7814 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   26238 |  7815 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   26240 |  7816 | `				SyBlobRelease(&sResolved);` |
|       - |  7817 | `			}` |
|       - |  7818 | `			/* Interfaces are not allowed */` |
|   26240 |  7819 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  7820 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7821 | `			}` |
|   26240 |  7822 | `			if( pBase == 0 ){` |
|       - |  7823 | `				/* Inexistant base class */` |
|     ! 0 |  7824 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 |  7825 | `				if( rc == SXERR_ABORT ){` |
|       - |  7826 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7827 | `					return SXERR_ABORT;` |
|       - |  7828 | `				}` |
|     ! 0 |  7829 | `			}else{` |
|   26240 |  7830 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  7831 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7832 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  7833 | `					if( rc == SXERR_ABORT ){` |
|       - |  7834 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7835 | `						return SXERR_ABORT;` |
|       - |  7836 | `					}` |
|     ! 0 |  7837 | `				}` |
|       - |  7838 | `			}` |
|       - |  7839 | `			/* Advance the stream cursor */` |
|   26240 |  7840 | `			pGen->pIn++;` |
|   13119 |  7841 | `		}` |
|   29178 |  7842 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  7843 | `			ph7_class *pInterface;` |
|       - |  7844 | `			SyString *pIntName;` |
|       - |  7845 | `			/* Interface implementation */` |
|    2942 |  7846 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1470 |  7847 | `			for(;;){` |
|    2942 |  7848 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7849 | `					/* Syntax error */` |
|     ! 0 |  7850 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7851 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  7852 | `						pName);` |
|     ! 0 |  7853 | `					if( rc == SXERR_ABORT ){` |
|       - |  7854 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7855 | `						return SXERR_ABORT;` |
|       - |  7856 | `					}` |
|     ! 0 |  7857 | `					break;` |
|       - |  7858 | `				}` |
|       - |  7859 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2942 |  7860 | `				pIntName = &pGen->pIn->sData;` |
|       - |  7861 | `				{` |
|       - |  7862 | `					SyBlob sResolved;` |
|    2942 |  7863 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2942 |  7864 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5882 |  7865 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2940 |  7866 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2942 |  7867 | `					SyBlobRelease(&sResolved);` |
|       - |  7868 | `				}` |
|       - |  7869 | `				/* Only interfaces are allowed */` |
|    2942 |  7870 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7871 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  7872 | `				}` |
|    2942 |  7873 | `				if( pInterface == 0 ){` |
|       - |  7874 | `					/* Inexistant interface */` |
|     ! 0 |  7875 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 |  7876 | `					if( rc == SXERR_ABORT ){` |
|       - |  7877 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7878 | `						return SXERR_ABORT;` |
|       - |  7879 | `					}` |
|     ! 0 |  7880 | `				}else{` |
|       - |  7881 | `					/* Register interface */` |
|    2942 |  7882 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  7883 | `				}` |
|       - |  7884 | `				/* Advance the stream cursor */` |
|    2942 |  7885 | `				pGen->pIn++;` |
|    2942 |  7886 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1472 |  7887 | `					break;` |
|       - |  7888 | `				}` |
|     ! 0 |  7889 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  7890 | `			}` |
|    1470 |  7891 | `		}` |
|   14588 |  7892 | `	}` |
|   41362 |  7893 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7894 | `		/* Syntax error */` |
|     ! 0 |  7895 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  7896 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7897 | `		if( rc == SXERR_ABORT ){` |
|       - |  7898 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7899 | `			return SXERR_ABORT;` |
|       - |  7900 | `		}` |
|     ! 0 |  7901 | `		return SXRET_OK;` |
|       - |  7902 | `	}` |
|   41362 |  7903 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   41362 |  7904 | `	pEnd = 0; /* cc warning */` |
|       - |  7905 | `	/* Delimit the class body */` |
|   41362 |  7906 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   41362 |  7907 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7908 | `		/* Syntax error */` |
|     ! 0 |  7909 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  7910 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7911 | `		if( rc == SXERR_ABORT ){` |
|       - |  7912 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7913 | `			return SXERR_ABORT;` |
|       - |  7914 | `		}` |
|     ! 0 |  7915 | `		return SXRET_OK;` |
|       - |  7916 | `	}` |
|       - |  7917 | `	/* Swap token stream */` |
|   41362 |  7918 | `	pTmp = pGen->pEnd;` |
|   41362 |  7919 | `	pGen->pEnd = pEnd;` |
|       - |  7920 | `	/* Set the inherited flags */` |
|   41362 |  7921 | `	pClass->iFlags = iFlags;` |
|       - |  7922 | `	/* Start the parse process */` |
|   78972 |  7923 | `	for(;;){` |
|       - |  7924 | `		/* Jump leading/trailing semi-colons */` |
|  234322 |  7925 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   38208 |  7926 | `			pGen->pIn++;` |
|       2 |  7927 | `		}` |
|  196116 |  7928 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7929 | `			/* End of class body */` |
|   41348 |  7930 | `			break;` |
|       - |  7931 | `		}` |
|  154770 |  7932 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  7933 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7934 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7935 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7936 | `			if( rc == SXERR_ABORT ){` |
|       - |  7937 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7938 | `				return SXERR_ABORT;` |
|       - |  7939 | `			}` |
|     ! 0 |  7940 | `			goto done;` |
|       - |  7941 | `		}` |
|       - |  7942 | `		/* Assume public visibility */` |
|  154770 |  7943 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  154770 |  7944 | `		iAttrflags = 0;` |
|  154770 |  7945 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  7946 | `			/* Extract the current keyword */` |
|  154770 |  7947 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  154770 |  7948 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  7949 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  7950 | `				TraitUseEntry sUse;` |
|      44 |  7951 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  7952 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  7953 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  7954 | `				for(;;){` |
|       - |  7955 | `					ph7_class *pTrait;` |
|       - |  7956 | `					SyString *pTraitName;` |
|      52 |  7957 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7958 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7959 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  7960 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7961 | `							return SXERR_ABORT;` |
|       - |  7962 | `						}` |
|     ! 0 |  7963 | `						break;` |
|       - |  7964 | `					}` |
|      52 |  7965 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  7966 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  7967 | `						SyBlob sResolved;` |
|      52 |  7968 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  7969 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  7970 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  7971 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  7972 | `						SyBlobRelease(&sResolved);` |
|       - |  7973 | `					}` |
|       - |  7974 | `					/* Only traits are allowed */` |
|      52 |  7975 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  7976 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  7977 | `					}` |
|      52 |  7978 | `					if( pTrait == 0 ){` |
|     ! 0 |  7979 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7980 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  7981 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7982 | `							return SXERR_ABORT;` |
|       - |  7983 | `						}` |
|     ! 0 |  7984 | `					}else{` |
|      52 |  7985 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  7986 | `					}` |
|      52 |  7987 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  7988 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  7989 | `						break;` |
|       - |  7990 | `					}` |
|       9 |  7991 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  7992 | `				}` |
|       - |  7993 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  7994 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  7995 | `					SyToken *pBlock;` |
|       9 |  7996 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  7997 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  7998 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  7999 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  8000 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  8001 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  8002 | `					}else{` |
|     ! 0 |  8003 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8004 | `					}` |
|       4 |  8005 | `				}` |
|      44 |  8006 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8007 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  8008 | `				continue;` |
|       - |  8009 | `			}` |
|  154728 |  8010 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  151706 |  8011 | `				iProtection = nKwrd;` |
|  151706 |  8012 | `				pGen->pIn++; /* Jump the visibility token */` |
|  151704 |  8013 | `				if( pGen->pIn >= pGen->pEnd` |
|  151706 |  8014 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8015 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8016 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8017 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8018 | `					if( rc == SXERR_ABORT ){` |
|       - |  8019 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8020 | `						return SXERR_ABORT;` |
|       - |  8021 | `					}` |
|     ! 0 |  8022 | `					goto done;` |
|       - |  8023 | `				}` |
|  151706 |  8024 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8025 | `					/* Attribute declaration (untyped) */` |
|   38018 |  8026 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   38018 |  8027 | `					if( rc != SXRET_OK ){` |
|       3 |  8028 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8029 | `							return SXERR_ABORT;` |
|       - |  8030 | `						}` |
|       3 |  8031 | `						goto done;` |
|       - |  8032 | `					}` |
|   38016 |  8033 | `					continue;` |
|       - |  8034 | `				}` |
|  113690 |  8035 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8036 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     106 |  8037 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     106 |  8038 | `					if( rc != SXRET_OK ){` |
|       3 |  8039 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8040 | `							return SXERR_ABORT;` |
|       - |  8041 | `						}` |
|       3 |  8042 | `						goto done;` |
|       - |  8043 | `					}` |
|     104 |  8044 | `					continue;` |
|       - |  8045 | `				}` |
|       - |  8046 | `				/* Extract the keyword */` |
|  113586 |  8047 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   56792 |  8048 | `			}` |
|  116608 |  8049 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8050 | `				/* Process constant declaration */` |
|      30 |  8051 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  8052 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8053 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8054 | `						return SXERR_ABORT;` |
|       - |  8055 | `					}` |
|     ! 0 |  8056 | `					goto done;` |
|       - |  8057 | `				}` |
|      16 |  8058 | `			}else{` |
|  116580 |  8059 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8060 | `					/* Static method or attribute,record that */` |
|    2942 |  8061 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2942 |  8062 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2942 |  8063 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8064 | `						/* Extract the keyword */` |
|    2936 |  8065 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2936 |  8066 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8067 | `							iProtection = nKwrd;` |
|     ! 0 |  8068 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8069 | `						}` |
|    1467 |  8070 | `					}` |
|    2940 |  8071 | `					if( pGen->pIn >= pGen->pEnd` |
|    2942 |  8072 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8073 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8074 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8075 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8076 | `						if( rc == SXERR_ABORT ){` |
|       - |  8077 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8078 | `							return SXERR_ABORT;` |
|       - |  8079 | `						}` |
|     ! 0 |  8080 | `						goto done;` |
|       - |  8081 | `					}` |
|    2942 |  8082 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8083 | `						/* Attribute declaration */` |
|       5 |  8084 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8085 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8086 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8087 | `								return SXERR_ABORT;` |
|       - |  8088 | `							}` |
|     ! 0 |  8089 | `							goto done;` |
|       - |  8090 | `						}` |
|       5 |  8091 | `						continue;` |
|       - |  8092 | `					}` |
|    2938 |  8093 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8094 | `						/* Typed static attribute declaration */` |
|      10 |  8095 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 |  8096 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8097 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8098 | `								return SXERR_ABORT;` |
|       - |  8099 | `							}` |
|     ! 0 |  8100 | `							goto done;` |
|       - |  8101 | `						}` |
|      10 |  8102 | `						continue;` |
|       - |  8103 | `					}` |
|       - |  8104 | `					/* Extract the keyword */` |
|    2930 |  8105 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  115104 |  8106 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8107 | `					/* Abstract method,record that */` |
|      12 |  8108 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8109 | `					/* Mark the whole class as abstract */` |
|      12 |  8110 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8111 | `					/* Advance the stream cursor */` |
|      12 |  8112 | `					pGen->pIn++;` |
|      12 |  8113 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8114 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8115 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8116 | `							iProtection = nKwrd;` |
|      10 |  8117 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8118 | `						}` |
|       5 |  8119 | `					}` |
|      12 |  8120 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8121 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8122 | `							/* Static method */` |
|     ! 0 |  8123 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8124 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8125 | `					}` |
|      12 |  8126 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8127 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8128 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8129 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8130 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8131 | `							if( rc == SXERR_ABORT ){` |
|       - |  8132 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8133 | `								return SXERR_ABORT;` |
|       - |  8134 | `							}` |
|     ! 0 |  8135 | `							goto done;` |
|       - |  8136 | `					}` |
|      12 |  8137 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  113635 |  8138 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8139 | `					/* final method ,record that */` |
|       5 |  8140 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  8141 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  8142 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8143 | `						/* Extract the keyword */` |
|       5 |  8144 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8145 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8146 | `							iProtection = nKwrd;` |
|       5 |  8147 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8148 | `						}` |
|       2 |  8149 | `					}` |
|       5 |  8150 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8151 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8152 | `							/* Static method */` |
|     ! 0 |  8153 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8154 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8155 | `					}` |
|       5 |  8156 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8157 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8158 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8159 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8160 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8161 | `							if( rc == SXERR_ABORT ){` |
|       - |  8162 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8163 | `								return SXERR_ABORT;` |
|       - |  8164 | `							}` |
|     ! 0 |  8165 | `							goto done;` |
|       - |  8166 | `					}` |
|       5 |  8167 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8168 | `				}` |
|  116568 |  8169 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8170 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8171 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8172 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8173 | `						if( rc == SXERR_ABORT ){` |
|       - |  8174 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8175 | `							return SXERR_ABORT;` |
|       - |  8176 | `						}` |
|     ! 0 |  8177 | `						goto done;` |
|       - |  8178 | `				}` |
|  116568 |  8179 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8180 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8181 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8182 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8183 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8184 | `						if( rc == SXERR_ABORT ){` |
|       - |  8185 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8186 | `							return SXERR_ABORT;` |
|       - |  8187 | `						}` |
|     ! 0 |  8188 | `						goto done;` |
|       - |  8189 | `					}` |
|       - |  8190 | `					/* Attribute declaration */` |
|       7 |  8191 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8192 | `				}else{` |
|       - |  8193 | `					/* Process method declaration */` |
|  116562 |  8194 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8195 | `				}` |
|  116568 |  8196 | `				if( rc != SXRET_OK ){` |
|      11 |  8197 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8198 | `						return SXERR_ABORT;` |
|       - |  8199 | `					}` |
|      11 |  8200 | `					goto done;` |
|       - |  8201 | `				}` |
|       - |  8202 | `			}` |
|   58294 |  8203 | `		}else{` |
|       - |  8204 | `			/* Attribute declaration */` |
|     ! 0 |  8205 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8206 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8207 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8208 | `					return SXERR_ABORT;` |
|       - |  8209 | `				}` |
|     ! 0 |  8210 | `				goto done;` |
|       - |  8211 | `			}` |
|       - |  8212 | `		}` |
|       2 |  8213 | `	}` |
|       - |  8214 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8215 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8216 | `	 */` |
|       - |  8217 | `	{` |
|       - |  8218 | `		TraitUseEntry *apUse;` |
|       - |  8219 | `		sxu32 nU;` |
|   41348 |  8220 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   41390 |  8221 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  8222 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  8223 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  8224 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  8225 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8226 | `			sxu32 nT;` |
|      44 |  8227 | `			if( !hasResolution ){` |
|       - |  8228 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  8229 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  8230 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  8231 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8232 | `						break;` |
|       - |  8233 | `					}` |
|      22 |  8234 | `				}` |
|      19 |  8235 | `			}else{` |
|       - |  8236 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8237 | `				 * then use the block to resolve method conflicts.` |
|       - |  8238 | `				 */` |
|       - |  8239 | `				SyToken *pR;` |
|      19 |  8240 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  8241 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8242 | `					ph7_class_attr *pAR;` |
|       - |  8243 | `					SyHashEntry *pER;` |
|       - |  8244 | `					SyString *pNR;` |
|      11 |  8245 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  8246 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8247 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8248 | `						pNR = &pAR->sName;` |
|     ! 0 |  8249 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8250 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8251 | `						}` |
|     ! 0 |  8252 | `					}` |
|      11 |  8253 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  8254 | `				}` |
|       - |  8255 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  8256 | `				pR = pUse->pResolvStart;` |
|      21 |  8257 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8258 | `					SyString sTrait,sMethod;` |
|       - |  8259 | `					ph7_class *pSrcTrait;` |
|       - |  8260 | `					ph7_class_method *pMeth;` |
|       - |  8261 | `					sxi32 nRKwrd;` |
|      33 |  8262 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8263 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8264 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8265 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8266 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8267 | `					sMethod = pR->sData;` |
|      13 |  8268 | `					pR++;` |
|      13 |  8269 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8270 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8271 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8272 | `							sTrait = sMethod;` |
|       7 |  8273 | `							pR++;` |
|       7 |  8274 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8275 | `							sMethod = pR->sData;` |
|       7 |  8276 | `							pR++;` |
|       3 |  8277 | `						}` |
|       3 |  8278 | `					}` |
|      13 |  8279 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8280 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8281 | `						continue;` |
|       - |  8282 | `					}` |
|      13 |  8283 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8284 | `					pR++;` |
|      13 |  8285 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8286 | `						pSrcTrait = 0;` |
|       7 |  8287 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8288 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8289 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8290 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8291 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8292 | `								break;` |
|       - |  8293 | `							}` |
|       2 |  8294 | `						}` |
|       5 |  8295 | `						if( pSrcTrait ){` |
|       5 |  8296 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8297 | `							if( pMeth ){` |
|       5 |  8298 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8299 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8300 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8301 | `								}` |
|       2 |  8302 | `							}` |
|       2 |  8303 | `						}` |
|       2 |  8304 | `					}` |
|      29 |  8305 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8306 | `				}` |
|       - |  8307 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8308 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8309 | `					ph7_class_method *pMR;` |
|       - |  8310 | `					SyHashEntry *pER;` |
|       - |  8311 | `					SyString *pNR;` |
|      11 |  8312 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8313 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8314 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8315 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8316 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8317 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8318 | `						}` |
|       1 |  8319 | `					}` |
|       6 |  8320 | `				}` |
|       - |  8321 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8322 | `				pR = pUse->pResolvStart;` |
|      21 |  8323 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8324 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8325 | `					ph7_class *pSrcTrait;` |
|       - |  8326 | `					ph7_class_method *pMeth;` |
|      21 |  8327 | `					int hasQual = 0;` |
|       - |  8328 | `					sxi32 nRKwrd;` |
|      33 |  8329 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8330 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8331 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8332 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8333 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8334 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8335 | `					sMethod = pR->sData;` |
|      13 |  8336 | `					pR++;` |
|      13 |  8337 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8338 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8339 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8340 | `							sTrait = sMethod;` |
|       7 |  8341 | `							hasQual = 1;` |
|       7 |  8342 | `							pR++;` |
|       7 |  8343 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8344 | `							sMethod = pR->sData;` |
|       7 |  8345 | `							pR++;` |
|       3 |  8346 | `						}` |
|       3 |  8347 | `					}` |
|      13 |  8348 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8349 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8350 | `						continue;` |
|       - |  8351 | `					}` |
|      13 |  8352 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8353 | `					pR++;` |
|      13 |  8354 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8355 | `						sxi32 iNewVis = -1;` |
|       9 |  8356 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8357 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8358 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8359 | `								iNewVis = nAK;` |
|       7 |  8360 | `								pR++;` |
|       3 |  8361 | `							}` |
|       3 |  8362 | `						}` |
|       9 |  8363 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8364 | `							sAlias = pR->sData;` |
|       7 |  8365 | `							pR++;` |
|       3 |  8366 | `						}` |
|       9 |  8367 | `						pMeth = 0;` |
|       9 |  8368 | `						if( hasQual ){` |
|       3 |  8369 | `							pSrcTrait = 0;` |
|       5 |  8370 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8371 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8372 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8373 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8374 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8375 | `									break;` |
|       - |  8376 | `								}` |
|       2 |  8377 | `							}` |
|       3 |  8378 | `							if( pSrcTrait ){` |
|       3 |  8379 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8380 | `							}` |
|       2 |  8381 | `						}else{` |
|       7 |  8382 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8383 | `						}` |
|       9 |  8384 | `						if( pMeth ){` |
|       9 |  8385 | `							if( sAlias.nByte > 0 ){` |
|       - |  8386 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8387 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8388 | `								 */` |
|       - |  8389 | `								ph7_class_method *pAlias;` |
|       - |  8390 | `								char *zAliasDup;` |
|       7 |  8391 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8392 | `								if( pAlias ){` |
|       7 |  8393 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8394 | `									if( iNewVis >= 0 ){` |
|       5 |  8395 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8396 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8397 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8398 | `									}` |
|       7 |  8399 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8400 | `									if( zAliasDup ){` |
|       7 |  8401 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8402 | `									}` |
|       4 |  8403 | `								}` |
|       6 |  8404 | `							}else if( iNewVis >= 0 ){` |
|       - |  8405 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8406 | `								ph7_class_method *pCopy;` |
|       3 |  8407 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8408 | `								if( pCopy ){` |
|       3 |  8409 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8410 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8411 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8412 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8413 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8414 | `									/* Replace the method in the class hash */` |
|       3 |  8415 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8416 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8417 | `								}` |
|       1 |  8418 | `							}` |
|       4 |  8419 | `						}` |
|       4 |  8420 | `						SXUNUSED(hasQual);` |
|       4 |  8421 | `					}` |
|      17 |  8422 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8423 | `				}` |
|       - |  8424 | `			}` |
|      44 |  8425 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  8426 | `		}` |
|       - |  8427 | `	}` |
|       - |  8428 | `	/* Install the class */` |
|   41348 |  8429 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   41348 |  8430 | `	if( rc == SXRET_OK ){` |
|       - |  8431 | `		ph7_class **apInterface;` |
|       - |  8432 | `		sxu32 n;` |
|   41348 |  8433 | `		if( pBase ){` |
|       - |  8434 | `			/* Inherit from base class and mark as a subclass */` |
|   26240 |  8435 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   13119 |  8436 | `		}` |
|   41348 |  8437 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   44288 |  8438 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8439 | `			/* Implements one or more interface */` |
|    2942 |  8440 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2942 |  8441 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8442 | `				break;` |
|       - |  8443 | `			}` |
|    1472 |  8444 | `		}` |
|       - |  8445 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   41348 |  8446 | `		if( rc == SXRET_OK ){` |
|   41348 |  8447 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   41348 |  8448 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8449 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8450 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8451 | `				return SXERR_ABORT;` |
|       - |  8452 | `			}` |
|   20673 |  8453 | `		}` |
|       - |  8454 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   41348 |  8455 | `		if( rc == SXRET_OK ){` |
|   41348 |  8456 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   41348 |  8457 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8458 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8459 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8460 | `				return SXERR_ABORT;` |
|       - |  8461 | `			}` |
|   20673 |  8462 | `		}` |
|   20673 |  8463 | `	}` |
|   41348 |  8464 | `	SySetRelease(&aUseEntries);` |
|   41348 |  8465 | `	SySetRelease(&aInterfaces);` |
|   41348 |  8466 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8467 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8468 | `		return SXERR_ABORT;` |
|       - |  8469 | `	}` |
|   20673 |  8470 | `done:` |
|       - |  8471 | `	/* Point beyond the class body */` |
|   41362 |  8472 | `	pGen->pIn = &pEnd[1];` |
|   41362 |  8473 | `	pGen->pEnd = pTmp;` |
|   41362 |  8474 | `	return PH7_OK;` |
|   20682 |  8475 |  |
|       - |  8476 | `/*` |
|       - |  8477 | ` * Compile a user-defined abstract class.` |
|       - |  8478 | ` *  According to the PHP language reference manual` |
|       - |  8479 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8480 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8481 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8482 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8483 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8484 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8485 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8486 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8487 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8488 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8489 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8490 | ` *   could differ.` |
|       - |  8491 | ` */` |
|      18 |  8492 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8493 |  |
|       - |  8494 | `	sxi32 rc;` |
|      20 |  8495 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      20 |  8496 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      20 |  8497 | `	return rc;` |
|       2 |  8498 |  |
|       - |  8499 | `/*` |
|       - |  8500 | ` * Compile a user-defined final class.` |
|       - |  8501 | ` *  According to the PHP language reference manual` |
|       - |  8502 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8503 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8504 | ` *    final then it cannot be extended.` |
|       - |  8505 | ` */` |
|       2 |  8506 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8507 |  |
|       - |  8508 | `	sxi32 rc;` |
|       3 |  8509 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8510 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8511 | `	return rc;` |
|       1 |  8512 |  |
|       - |  8513 | `/*` |
|       - |  8514 | ` * Compile a user-defined trait.` |
|       - |  8515 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8516 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8517 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8518 | ` */` |
|      54 |  8519 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8520 |  |
|      56 |  8521 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8522 | `	ph7_class *pClass;` |
|       - |  8523 | `	SyToken *pEnd,*pTmp;` |
|       - |  8524 | `	sxi32 iProtection;` |
|       - |  8525 | `	sxi32 iAttrflags;` |
|       - |  8526 | `	SyString *pName;` |
|       - |  8527 | `	sxi32 nKwrd;` |
|       - |  8528 | `	sxi32 rc;` |
|       - |  8529 | `	/* Jump the 'trait' keyword */` |
|      56 |  8530 | `	pGen->pIn++;` |
|      56 |  8531 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8532 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8533 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8534 | `			return SXERR_ABORT;` |
|       - |  8535 | `		}` |
|     ! 0 |  8536 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8537 | `			pGen->pIn++;` |
|     ! 0 |  8538 | `		}` |
|     ! 0 |  8539 | `		return SXRET_OK;` |
|       - |  8540 | `	}` |
|       - |  8541 | `	/* Extract trait name */` |
|      56 |  8542 | `	pName = &pGen->pIn->sData;` |
|      56 |  8543 | `	pGen->pIn++;` |
|       - |  8544 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8545 | `		SyBlob sFQN;` |
|       - |  8546 | `		SyString sFQNStr;` |
|      56 |  8547 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8548 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8549 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8550 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8551 | `		SyBlobRelease(&sFQN);` |
|       - |  8552 | `	}` |
|      56 |  8553 | `	if( pClass == 0 ){` |
|     ! 0 |  8554 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8555 | `		return SXERR_ABORT;` |
|       - |  8556 | `	}` |
|       - |  8557 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8558 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8559 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8560 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8561 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8562 | `			return SXERR_ABORT;` |
|       - |  8563 | `		}` |
|     ! 0 |  8564 | `		return SXRET_OK;` |
|       - |  8565 | `	}` |
|      56 |  8566 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8567 | `	pEnd = 0;` |
|      56 |  8568 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8569 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8570 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8571 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8572 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8573 | `			return SXERR_ABORT;` |
|       - |  8574 | `		}` |
|     ! 0 |  8575 | `		return SXRET_OK;` |
|       - |  8576 | `	}` |
|       - |  8577 | `	/* Swap token stream */` |
|      56 |  8578 | `	pTmp = pGen->pEnd;` |
|      56 |  8579 | `	pGen->pEnd = pEnd;` |
|       - |  8580 | `	/* Mark as trait */` |
|      56 |  8581 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8582 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8583 | `	for(;;){` |
|     154 |  8584 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8585 | `			pGen->pIn++;` |
|       2 |  8586 | `		}` |
|     130 |  8587 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8588 | `			break;` |
|       - |  8589 | `		}` |
|      76 |  8590 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8591 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8592 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8593 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8594 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8595 | `				return SXERR_ABORT;` |
|       - |  8596 | `			}` |
|     ! 0 |  8597 | `			goto done;` |
|       - |  8598 | `		}` |
|      76 |  8599 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8600 | `		iAttrflags = 0;` |
|      76 |  8601 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8602 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8603 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8604 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8605 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8606 | `				for(;;){` |
|       - |  8607 | `					ph7_class *pUsedTrait;` |
|       - |  8608 | `					SyString *pUsedName;` |
|       5 |  8609 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8610 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8611 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8612 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8613 | `							return SXERR_ABORT;` |
|       - |  8614 | `						}` |
|     ! 0 |  8615 | `						break;` |
|       - |  8616 | `					}` |
|       5 |  8617 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8618 | `					{` |
|       - |  8619 | `						SyBlob sResolved;` |
|       5 |  8620 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8621 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8622 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8623 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8624 | `						SyBlobRelease(&sResolved);` |
|       - |  8625 | `					}` |
|       5 |  8626 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8627 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8628 | `					}` |
|       5 |  8629 | `					if( pUsedTrait == 0 ){` |
|       4 |  8630 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8631 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8632 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8633 | `							return SXERR_ABORT;` |
|       - |  8634 | `						}` |
|       2 |  8635 | `					}else{` |
|       3 |  8636 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8637 | `					}` |
|       5 |  8638 | `					pGen->pIn++;` |
|       5 |  8639 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8640 | `						break;` |
|       - |  8641 | `					}` |
|     ! 0 |  8642 | `					pGen->pIn++;` |
|     ! 0 |  8643 | `				}` |
|       5 |  8644 | `				continue;` |
|       - |  8645 | `			}` |
|      72 |  8646 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8647 | `				iProtection = nKwrd;` |
|      68 |  8648 | `				pGen->pIn++;` |
|      66 |  8649 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8650 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8651 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8652 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8653 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8654 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8655 | `						return SXERR_ABORT;` |
|       - |  8656 | `					}` |
|     ! 0 |  8657 | `					goto done;` |
|       - |  8658 | `				}` |
|      68 |  8659 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8660 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8661 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8662 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8663 | `							return SXERR_ABORT;` |
|       - |  8664 | `						}` |
|     ! 0 |  8665 | `						goto done;` |
|       - |  8666 | `					}` |
|      11 |  8667 | `					continue;` |
|       - |  8668 | `				}` |
|      58 |  8669 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8670 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8671 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8672 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8673 | `							return SXERR_ABORT;` |
|       - |  8674 | `						}` |
|     ! 0 |  8675 | `						goto done;` |
|       - |  8676 | `					}` |
|       5 |  8677 | `					continue;` |
|       - |  8678 | `				}` |
|      53 |  8679 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8680 | `			}` |
|      57 |  8681 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8682 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8683 | `					"Traits cannot have constants");` |
|     ! 0 |  8684 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8685 | `					return SXERR_ABORT;` |
|       - |  8686 | `				}` |
|     ! 0 |  8687 | `				goto done;` |
|     ! 0 |  8688 | `			}else{` |
|      57 |  8689 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8690 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8691 | `					pGen->pIn++;` |
|       5 |  8692 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8693 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8694 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8695 | `							iProtection = nKwrd;` |
|     ! 0 |  8696 | `							pGen->pIn++;` |
|     ! 0 |  8697 | `						}` |
|       1 |  8698 | `					}` |
|       4 |  8699 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8700 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8701 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8702 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8703 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8704 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8705 | `							return SXERR_ABORT;` |
|       - |  8706 | `						}` |
|     ! 0 |  8707 | `						goto done;` |
|       - |  8708 | `					}` |
|       5 |  8709 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8710 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8711 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8712 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8713 | `								return SXERR_ABORT;` |
|       - |  8714 | `							}` |
|     ! 0 |  8715 | `							goto done;` |
|       - |  8716 | `						}` |
|       3 |  8717 | `						continue;` |
|       - |  8718 | `					}` |
|       3 |  8719 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8720 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8721 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8722 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8723 | `								return SXERR_ABORT;` |
|       - |  8724 | `							}` |
|     ! 0 |  8725 | `							goto done;` |
|       - |  8726 | `						}` |
|     ! 0 |  8727 | `						continue;` |
|       - |  8728 | `					}` |
|       3 |  8729 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  8730 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  8731 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  8732 | `					pGen->pIn++;` |
|       5 |  8733 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  8734 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8735 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8736 | `							iProtection = nKwrd;` |
|       5 |  8737 | `							pGen->pIn++;` |
|       2 |  8738 | `						}` |
|       2 |  8739 | `					}` |
|       5 |  8740 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8741 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8742 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8743 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  8744 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8745 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8746 | `							return SXERR_ABORT;` |
|       - |  8747 | `						}` |
|     ! 0 |  8748 | `						goto done;` |
|       - |  8749 | `					}` |
|       5 |  8750 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8751 | `				}` |
|      55 |  8752 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8753 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8754 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  8755 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8756 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8757 | `						return SXERR_ABORT;` |
|       - |  8758 | `					}` |
|     ! 0 |  8759 | `					goto done;` |
|       - |  8760 | `				}` |
|      55 |  8761 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  8762 | `					pGen->pIn++;` |
|     ! 0 |  8763 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8764 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8765 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8766 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8767 | `							return SXERR_ABORT;` |
|       - |  8768 | `						}` |
|     ! 0 |  8769 | `						goto done;` |
|       - |  8770 | `					}` |
|     ! 0 |  8771 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8772 | `				}else{` |
|      55 |  8773 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8774 | `				}` |
|      55 |  8775 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8776 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8777 | `						return SXERR_ABORT;` |
|       - |  8778 | `					}` |
|     ! 0 |  8779 | `					goto done;` |
|       - |  8780 | `				}` |
|       - |  8781 | `			}` |
|      28 |  8782 | `		}else{` |
|     ! 0 |  8783 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8784 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8785 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8786 | `					return SXERR_ABORT;` |
|       - |  8787 | `				}` |
|     ! 0 |  8788 | `				goto done;` |
|       - |  8789 | `			}` |
|       - |  8790 | `		}` |
|       1 |  8791 | `	}` |
|       - |  8792 | `	/* Install the trait */` |
|      56 |  8793 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  8794 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8795 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8796 | `		return SXERR_ABORT;` |
|       - |  8797 | `	}` |
|      27 |  8798 | `done:` |
|       - |  8799 | `	/* Point beyond the trait body */` |
|      56 |  8800 | `	pGen->pIn = &pEnd[1];` |
|      56 |  8801 | `	pGen->pEnd = pTmp;` |
|      56 |  8802 | `	return PH7_OK;` |
|      29 |  8803 |  |
|       - |  8804 | `/*` |
|       - |  8805 | ` * Compile a user-defined class.` |
|       - |  8806 | ` *  According to the PHP language reference manual` |
|       - |  8807 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  8808 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  8809 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  8810 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  8811 | ` *   and functions (called "methods").` |
|       - |  8812 | ` */` |
|   41340 |  8813 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  8814 |  |
|       - |  8815 | `	sxi32 rc;` |
|   41342 |  8816 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   41342 |  8817 | `	return rc;` |
|       2 |  8818 |  |
|       - |  8819 | `/*` |
|       - |  8820 | ` * Exception handling.` |
|       - |  8821 | ` *  According to the PHP language reference manual` |
|       - |  8822 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  8823 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  8824 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  8825 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  8826 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  8827 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  8828 | ` *    (or re-thrown) within a catch block.` |
|       - |  8829 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  8830 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  8831 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  8832 | ` *    been defined with set_exception_handler().` |
|       - |  8833 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  8834 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  8835 | ` */` |
|       - |  8836 | `/*` |
|       - |  8837 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  8838 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  8839 | ` * indicates failure.` |
|       - |  8840 | ` */` |
|    8808 |  8841 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  8842 |  |
|    8810 |  8843 | `	sxi32 rc = SXRET_OK;` |
|    8810 |  8844 | `	if( pRoot->pOp ){` |
|    8804 |  8845 | `		switch( pRoot->pOp->iOp ){` |
|    4401 |  8846 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  8847 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  8848 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  8849 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  8850 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  8851 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    8804 |  8852 | `			break;` |
|     ! 0 |  8853 | `		default:` |
|       - |  8854 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  8855 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  8856 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  8857 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8858 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  8859 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  8860 | `				rc = SXERR_INVALID;` |
|     ! 0 |  8861 | `			}` |
|     ! 0 |  8862 | `			break;` |
|       - |  8863 | `		}` |
|    4409 |  8864 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  8865 | `		/* Unexpected expression */` |
|     ! 0 |  8866 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8867 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  8868 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  8869 | `			rc = SXERR_INVALID;` |
|     ! 0 |  8870 | `		}` |
|     ! 0 |  8871 | `	}` |
|    8810 |  8872 | `	return rc;` |
|       2 |  8873 |  |
|       - |  8874 | `/*` |
|       - |  8875 | ` * Compile a 'throw' statement.` |
|       - |  8876 | ` * throw: This is how you trigger an exception.` |
|       - |  8877 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  8878 | ` */` |
|    8772 |  8879 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  8880 |  |
|    8774 |  8881 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8882 | `	GenBlock *pBlock;` |
|       - |  8883 | `	sxu32 nIdx;` |
|       - |  8884 | `	sxi32 rc;` |
|    8774 |  8885 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  8886 | `	/* Compile the expression */` |
|    8774 |  8887 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8774 |  8888 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8889 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  8890 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8891 | `			return SXERR_ABORT;` |
|       - |  8892 | `		}` |
|     ! 0 |  8893 | `		return SXRET_OK;` |
|       - |  8894 | `	}` |
|    8774 |  8895 | `	pBlock = pGen->pCurrent;` |
|       - |  8896 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   40762 |  8897 | `	while(pBlock->pParent){` |
|   40758 |  8898 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8770 |  8899 | `			break;` |
|       - |  8900 | `		}` |
|       - |  8901 | `		/* Point to the parent block */` |
|   31990 |  8902 | `		pBlock = pBlock->pParent;` |
|       2 |  8903 | `	}` |
|       - |  8904 | `	/* Emit the throw instruction */` |
|    8774 |  8905 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  8906 | `	/* Emit the jump */` |
|    8774 |  8907 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8774 |  8908 | `	return SXRET_OK;` |
|    4388 |  8909 |  |
|       - |  8910 | `/*` |
|       - |  8911 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  8912 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  8913 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  8914 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  8915 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  8916 | ` */` |
|      36 |  8917 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  8918 |  |
|      38 |  8919 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8920 | `	GenBlock *pBlock;` |
|       - |  8921 | `	sxu32 nIdx;` |
|       - |  8922 | `	sxi32 rc;` |
|      18 |  8923 | `	(void)iCompileFlag;` |
|      38 |  8924 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  8925 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  8926 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  8927 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  8928 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8929 | `			return SXERR_ABORT;` |
|       - |  8930 | `		}` |
|     ! 0 |  8931 | `		return SXRET_OK;` |
|       - |  8932 | `	}` |
|      38 |  8933 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  8934 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8935 | `		return SXERR_ABORT;` |
|       - |  8936 | `	}` |
|      38 |  8937 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8938 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  8939 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  8940 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8941 | `			return SXERR_ABORT;` |
|       - |  8942 | `		}` |
|     ! 0 |  8943 | `		return SXRET_OK;` |
|       - |  8944 | `	}` |
|       - |  8945 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  8946 | `	pBlock = pGen->pCurrent;` |
|      60 |  8947 | `	while( pBlock->pParent ){` |
|      49 |  8948 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  8949 | `			break;` |
|       - |  8950 | `		}` |
|      23 |  8951 | `		pBlock = pBlock->pParent;` |
|       1 |  8952 | `	}` |
|      38 |  8953 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  8954 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  8955 | `	return SXRET_OK;` |
|      20 |  8956 |  |
|       - |  8957 | `/*` |
|       - |  8958 | ` * Compile a 'catch' block.` |
|       - |  8959 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  8960 | ` * an object containing the exception information.` |
|       - |  8961 | ` */` |
|     192 |  8962 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  8963 |  |
|     194 |  8964 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8965 | `	ph7_exception_block sCatch;` |
|       - |  8966 | `	SySet *pInstrContainer;` |
|       - |  8967 | `	SyString sClassName;` |
|       - |  8968 | `	GenBlock *pCatch;` |
|       - |  8969 | `	SyToken *pToken;` |
|       - |  8970 | `	SyString *pName;` |
|       - |  8971 | `	char *zDup;` |
|       - |  8972 | `	sxi32 rc;` |
|     194 |  8973 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  8974 | `	/* Zero the structure */` |
|     194 |  8975 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  8976 | `	/* Initialize fields */` |
|     194 |  8977 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     194 |  8978 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     194 |  8979 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  8980 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  8981 | `			pToken = pGen->pIn;` |
|     ! 0 |  8982 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8983 | `				pToken--;` |
|     ! 0 |  8984 | `			}` |
|     ! 0 |  8985 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8986 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8987 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8988 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8989 | `				return SXERR_ABORT;` |
|       - |  8990 | `			}` |
|     ! 0 |  8991 | `			return SXERR_INVALID;` |
|       - |  8992 | `	}` |
|       - |  8993 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     194 |  8994 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     108 |  8995 | `	for(;;){` |
|     218 |  8996 | `		int isAbsolute = 0;` |
|       - |  8997 | `		SyBlob sName;` |
|     218 |  8998 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - |  8999 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     218 |  9000 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       9 |  9001 | `			isAbsolute = 1;` |
|       9 |  9002 | `			pGen->pIn++;` |
|       4 |  9003 | `		}` |
|     218 |  9004 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     216 |  9005 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 |  9006 | `			SyBlobRelease(&sName);` |
|       5 |  9007 | `			pToken = pGen->pIn;` |
|       5 |  9008 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9009 | `				pToken--;` |
|     ! 0 |  9010 | `			}` |
|       7 |  9011 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9012 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9013 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  9014 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9015 | `				return SXERR_ABORT;` |
|       - |  9016 | `			}` |
|       5 |  9017 | `			return SXERR_INVALID;` |
|       - |  9018 | `		}` |
|       - |  9019 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     214 |  9020 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     214 |  9021 | `		pGen->pIn++;` |
|     324 |  9022 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|     114 |  9023 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 |  9024 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 |  9025 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 |  9026 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 |  9027 | `			pGen->pIn++;` |
|       1 |  9028 | `		}` |
|       - |  9029 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     214 |  9030 | `		if( !isAbsolute ){` |
|       - |  9031 | `			SyString sRaw;` |
|       - |  9032 | `			SyBlob sResolved;` |
|     206 |  9033 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     206 |  9034 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     206 |  9035 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     308 |  9036 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     204 |  9037 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     206 |  9038 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     206 |  9039 | `			SyBlobRelease(&sResolved);` |
|     104 |  9040 | `		}else{` |
|       - |  9041 | `			/* Absolute name: use as-is without namespace prefix */` |
|      13 |  9042 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       8 |  9043 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       9 |  9044 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - |  9045 | `		}` |
|     214 |  9046 | `		SyBlobRelease(&sName);` |
|     214 |  9047 | `		if( zDup == 0 ){` |
|     ! 0 |  9048 | `			goto Mem;` |
|       - |  9049 | `		}` |
|     214 |  9050 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     214 |  9051 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9052 | `			goto Mem;` |
|       - |  9053 | `		}` |
|       - |  9054 | `		/* Check for '\|' (multi-catch separator) */` |
|     224 |  9055 | `		if( pGen->pIn < pGen->pEnd &&` |
|     212 |  9056 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 |  9057 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 |  9058 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 |  9059 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 |  9060 | `			continue;` |
|       - |  9061 | `		}` |
|     190 |  9062 | `		break;` |
|     ! 0 |  9063 | `	}` |
|     282 |  9064 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     190 |  9065 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9066 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9067 | `			pToken = pGen->pIn;` |
|     ! 0 |  9068 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9069 | `				pToken--;` |
|     ! 0 |  9070 | `			}` |
|     ! 0 |  9071 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9072 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9073 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9074 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9075 | `				return SXERR_ABORT;` |
|       - |  9076 | `			}` |
|     ! 0 |  9077 | `			return SXERR_INVALID;` |
|       - |  9078 | `	}` |
|     190 |  9079 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9080 | `	/* Duplicate instance name */` |
|     190 |  9081 | `	pName = &pGen->pIn->sData;` |
|     190 |  9082 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     190 |  9083 | `	if( zDup == 0 ){` |
|     ! 0 |  9084 | `		goto Mem;` |
|       - |  9085 | `	}` |
|     190 |  9086 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     190 |  9087 | `	pGen->pIn++;` |
|     190 |  9088 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9089 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9090 | `		pToken = pGen->pIn;` |
|     ! 0 |  9091 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9092 | `			pToken--;` |
|     ! 0 |  9093 | `		}` |
|     ! 0 |  9094 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9095 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9096 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9097 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9098 | `			return SXERR_ABORT;` |
|       - |  9099 | `		}` |
|     ! 0 |  9100 | `		return SXERR_INVALID;` |
|       - |  9101 | `	}` |
|       - |  9102 | `	/* Compile the block */` |
|     190 |  9103 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9104 | `	/* Create the catch block */` |
|     190 |  9105 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     190 |  9106 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9107 | `		return SXERR_ABORT;` |
|       - |  9108 | `	}` |
|       - |  9109 | `	/* Swap bytecode container */` |
|     190 |  9110 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     190 |  9111 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9112 | `	/* Compile the block */` |
|     190 |  9113 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9114 | `	/* Fix forward jumps now the destination is resolved  */` |
|     190 |  9115 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9116 | `	/* Emit the DONE instruction */` |
|     190 |  9117 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9118 | `	/* Leave the block */` |
|     190 |  9119 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9120 | `	/* Restore the default container */` |
|     190 |  9121 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9122 | `	/* Install the catch block */` |
|     190 |  9123 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     190 |  9124 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9125 | `		goto Mem;` |
|       - |  9126 | `	}` |
|     190 |  9127 | `	return SXRET_OK;` |
|     ! 0 |  9128 | `Mem:` |
|     ! 0 |  9129 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9130 | `	return SXERR_ABORT;` |
|      98 |  9131 |  |
|       - |  9132 | `/*` |
|       - |  9133 | ` * Compile a 'try' block.` |
|       - |  9134 | ` * A function using an exception should be in a "try" block.` |
|       - |  9135 | ` * If the exception does not trigger, the code will continue` |
|       - |  9136 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9137 | ` * is "thrown".` |
|       - |  9138 | ` */` |
|     200 |  9139 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  9140 |  |
|       - |  9141 | `	ph7_exception *pException;` |
|     202 |  9142 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9143 | `	GenBlock *pTry;` |
|       - |  9144 | `	sxu32 nJmpIdx;` |
|       - |  9145 | `	sxi32 rc;` |
|       - |  9146 | `	/* Create the exception container */` |
|     202 |  9147 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     202 |  9148 | `	if( pException == 0 ){` |
|     ! 0 |  9149 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9150 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9151 | `		return SXERR_ABORT;` |
|       - |  9152 | `	}` |
|       - |  9153 | `	/* Zero the structure */` |
|     202 |  9154 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9155 | `	/* Initialize fields */` |
|     202 |  9156 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     202 |  9157 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     202 |  9158 | `	pException->iHasFinally = 0;` |
|     202 |  9159 | `	pException->iFinallyDone = 0;` |
|     202 |  9160 | `	pException->pVm = pGen->pVm;` |
|       - |  9161 | `	/* Create the try block */` |
|     202 |  9162 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     202 |  9163 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9164 | `		return SXERR_ABORT;` |
|       - |  9165 | `	}` |
|       - |  9166 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     202 |  9167 | `	pTry->pUserData = pException;` |
|       - |  9168 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     202 |  9169 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9170 | `	/* Fix the jump later when the destination is resolved */` |
|     202 |  9171 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     202 |  9172 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9173 | `	/* Compile the block */` |
|     202 |  9174 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     202 |  9175 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9176 | `		return SXERR_ABORT;` |
|       - |  9177 | `	}` |
|       - |  9178 | `	/* Fix forward jumps now the destination is resolved */` |
|     202 |  9179 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9180 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     202 |  9181 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9182 | `	/* Leave the block */` |
|     202 |  9183 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9184 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     202 |  9185 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     198 |  9186 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9187 | `		/* Compile one or more catch blocks */` |
|     190 |  9188 | `		for(;;){` |
|     380 |  9189 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     298 |  9190 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      96 |  9191 | `					break;` |
|       - |  9192 | `			}` |
|     194 |  9193 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     194 |  9194 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9195 | `				return SXERR_ABORT;` |
|       - |  9196 | `			}` |
|       2 |  9197 | `		}` |
|      94 |  9198 | `	}` |
|       - |  9199 | `	/* Compile optional finally block */` |
|     202 |  9200 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      92 |  9201 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9202 | `		SySet *pInstrContainer;` |
|       - |  9203 | `		GenBlock *pFinBlock;` |
|      32 |  9204 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9205 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  9206 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  9207 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9208 | `			return SXERR_ABORT;` |
|       - |  9209 | `		}` |
|       - |  9210 | `		/* Swap bytecode container */` |
|      32 |  9211 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  9212 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9213 | `		/* Compile the finally body */` |
|      32 |  9214 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  9215 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9216 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9217 | `			return SXERR_ABORT;` |
|       - |  9218 | `		}` |
|       - |  9219 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  9220 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9221 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  9222 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9223 | `		/* Leave the block */` |
|      32 |  9224 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9225 | `		/* Restore the default container */` |
|      32 |  9226 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  9227 | `		pException->iHasFinally = 1;` |
|      15 |  9228 | `	}` |
|       - |  9229 | `	/* Must have at least one catch or finally */` |
|     202 |  9230 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  9231 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9232 | `			"Cannot use try without catch or finally");` |
|       7 |  9233 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9234 | `			return SXERR_ABORT;` |
|       - |  9235 | `		}` |
|       3 |  9236 | `	}` |
|     202 |  9237 | `	return SXRET_OK;` |
|     102 |  9238 |  |
|       - |  9239 | `/*` |
|       - |  9240 | ` * Compile a switch block.` |
|       - |  9241 | ` *  (See block-comment below for more information)` |
|       - |  9242 | ` */` |
|     108 |  9243 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  9244 |  |
|     110 |  9245 | `	sxi32 rc = SXRET_OK;` |
|     110 |  9246 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9247 | `		/* Unexpected token */` |
|     ! 0 |  9248 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9249 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9250 | `			return SXERR_ABORT;` |
|       - |  9251 | `		}` |
|     ! 0 |  9252 | `		pGen->pIn++;` |
|     ! 0 |  9253 | `	}` |
|     110 |  9254 | `	pGen->pIn++;` |
|       - |  9255 | `	/* First instruction to execute in this block. */` |
|     110 |  9256 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9257 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9258 | `	 * or the '}' token */` |
|     182 |  9259 | `	for(;;){` |
|     366 |  9260 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9261 | `			/* No more input to process */` |
|     ! 0 |  9262 | `			break;` |
|       - |  9263 | `		}` |
|     366 |  9264 | `		rc = SXRET_OK;` |
|     366 |  9265 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  9266 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  9267 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9268 | `					/* Unexpected token */` |
|     ! 0 |  9269 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9270 | `						&pGen->pIn->sData);` |
|     ! 0 |  9271 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9272 | `						return SXERR_ABORT;` |
|       - |  9273 | `					}` |
|       - |  9274 | `					/* FALL THROUGH */` |
|     ! 0 |  9275 | `				}` |
|      28 |  9276 | `				rc = SXERR_EOF;` |
|      28 |  9277 | `				break;` |
|       - |  9278 | `			}` |
|      23 |  9279 | `		}else{` |
|       - |  9280 | `			sxi32 nKwrd;` |
|       - |  9281 | `			/* Extract the keyword */` |
|     298 |  9282 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  9283 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  9284 | `				break;` |
|       - |  9285 | `			}` |
|     218 |  9286 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9287 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9288 | `					/* Unexpected token */` |
|     ! 0 |  9289 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9290 | `						&pGen->pIn->sData);` |
|     ! 0 |  9291 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9292 | `						return SXERR_ABORT;` |
|       - |  9293 | `					}` |
|       - |  9294 | `					/* FALL THROUGH */` |
|     ! 0 |  9295 | `				}` |
|       - |  9296 | `				/* Block compiled */` |
|       3 |  9297 | `				break;` |
|       - |  9298 | `			}` |
|       - |  9299 | `		}` |
|       - |  9300 | `		/* Compile block */` |
|     258 |  9301 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  9302 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9303 | `			return SXERR_ABORT;` |
|       - |  9304 | `		}` |
|       2 |  9305 | `	}` |
|     110 |  9306 | `	return rc;` |
|      56 |  9307 |  |
|       - |  9308 | `/*` |
|       - |  9309 | ` * Compile a case eXpression.` |
|       - |  9310 | ` *  (See block-comment below for more information)` |
|       - |  9311 | ` */` |
|      88 |  9312 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  9313 |  |
|       - |  9314 | `	SySet *pInstrContainer;` |
|       - |  9315 | `	SyToken *pEnd,*pTmp;` |
|      90 |  9316 | `	sxi32 iNest = 0;` |
|       - |  9317 | `	sxi32 rc;` |
|       - |  9318 | `	/* Delimit the expression */` |
|      90 |  9319 | `	pEnd = pGen->pIn;` |
|     186 |  9320 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  9321 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9322 | `			/* Increment nesting level */` |
|       3 |  9323 | `			iNest++;` |
|     185 |  9324 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9325 | `			/* Decrement nesting level */` |
|       3 |  9326 | `			iNest--;` |
|     183 |  9327 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  9328 | `			break;` |
|       - |  9329 | `		}` |
|      98 |  9330 | `		pEnd++;` |
|       2 |  9331 | `	}` |
|      90 |  9332 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9333 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9334 | `		if( rc == SXERR_ABORT ){` |
|       - |  9335 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9336 | `			return SXERR_ABORT;` |
|       - |  9337 | `		}` |
|     ! 0 |  9338 | `	}` |
|       - |  9339 | `	/* Swap token stream */` |
|      90 |  9340 | `	pTmp = pGen->pEnd;` |
|      90 |  9341 | `	pGen->pEnd = pEnd;` |
|      90 |  9342 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  9343 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9344 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9345 | `	/* Emit the done instruction */` |
|      90 |  9346 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9347 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9348 | `	/* Update token stream */` |
|      90 |  9349 | `	pGen->pIn  = pEnd;` |
|      90 |  9350 | `	pGen->pEnd = pTmp;` |
|      90 |  9351 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9352 | `		return SXERR_ABORT;` |
|       - |  9353 | `	}` |
|      90 |  9354 | `	return SXRET_OK;` |
|      46 |  9355 |  |
|       - |  9356 | `/*` |
|       - |  9357 | ` * Compile the smart switch statement.` |
|       - |  9358 | ` * According to the PHP language reference manual` |
|       - |  9359 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9360 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9361 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9362 | ` *  This is exactly what the switch statement is for.` |
|       - |  9363 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9364 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9365 | ` *  of the outer loop, use continue 2.` |
|       - |  9366 | ` *  Note that switch/case does loose comparision.` |
|       - |  9367 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9368 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9369 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9370 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9371 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9372 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9373 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9374 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9375 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9376 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9377 | ` *  list for the next case.` |
|       - |  9378 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9379 | ` *  or floating-point numbers and strings.` |
|       - |  9380 | ` */` |
|      28 |  9381 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9382 |  |
|       - |  9383 | `	GenBlock *pSwitchBlock;` |
|       - |  9384 | `	SyToken *pTmp,*pEnd;` |
|       - |  9385 | `	ph7_switch *pSwitch;` |
|       - |  9386 | `	sxu32 nToken;` |
|       - |  9387 | `	sxu32 nLine;` |
|       - |  9388 | `	sxi32 rc;` |
|      30 |  9389 | `	nLine = pGen->pIn->nLine;` |
|       - |  9390 | `	/* Jump the 'switch' keyword */` |
|      30 |  9391 | `	pGen->pIn++;` |
|      30 |  9392 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9393 | `		/* Syntax error */` |
|     ! 0 |  9394 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9395 | `		if( rc == SXERR_ABORT ){` |
|       - |  9396 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9397 | `			return SXERR_ABORT;` |
|       - |  9398 | `		}` |
|     ! 0 |  9399 | `		goto Synchronize;` |
|       - |  9400 | `	}` |
|       - |  9401 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9402 | `	pGen->pIn++;` |
|      30 |  9403 | `	pEnd = 0; /* cc warning */` |
|       - |  9404 | `	/* Create the loop block */` |
|      44 |  9405 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9406 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9407 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9408 | `		return SXERR_ABORT;` |
|       - |  9409 | `	}` |
|       - |  9410 | `	/* Delimit the condition */` |
|      30 |  9411 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9412 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9413 | `		/* Empty expression */` |
|     ! 0 |  9414 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9415 | `		if( rc == SXERR_ABORT ){` |
|       - |  9416 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9417 | `			return SXERR_ABORT;` |
|       - |  9418 | `		}` |
|     ! 0 |  9419 | `	}` |
|       - |  9420 | `	/* Swap token streams */` |
|      30 |  9421 | `	pTmp = pGen->pEnd;` |
|      30 |  9422 | `	pGen->pEnd = pEnd;` |
|       - |  9423 | `	/* Compile the expression */` |
|      30 |  9424 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9425 | `	if( rc == SXERR_ABORT ){` |
|       - |  9426 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9427 | `		return SXERR_ABORT;` |
|       - |  9428 | `	}` |
|       - |  9429 | `	/* Update token stream */` |
|      30 |  9430 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9431 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9432 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9433 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9434 | `			return SXERR_ABORT;` |
|       - |  9435 | `		}` |
|     ! 0 |  9436 | `		pGen->pIn++;` |
|     ! 0 |  9437 | `	}` |
|      30 |  9438 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9439 | `	pGen->pEnd = pTmp;` |
|      30 |  9440 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9441 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9442 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9443 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9444 | `				pTmp--;` |
|     ! 0 |  9445 | `			}` |
|       - |  9446 | `			/* Unexpected token */` |
|     ! 0 |  9447 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9448 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9449 | `				return SXERR_ABORT;` |
|       - |  9450 | `			}` |
|     ! 0 |  9451 | `			goto Synchronize;` |
|       - |  9452 | `	}` |
|       - |  9453 | `	/* Set the delimiter token */` |
|      30 |  9454 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9455 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9456 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9457 | `	}else{` |
|      28 |  9458 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9459 | `	}` |
|      30 |  9460 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9461 | `	/* Create the switch blocks container */` |
|      30 |  9462 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9463 | `	if( pSwitch == 0 ){` |
|       - |  9464 | `		/* Abort compilation */` |
|     ! 0 |  9465 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9466 | `		return SXERR_ABORT;` |
|       - |  9467 | `	}` |
|       - |  9468 | `	/* Zero the structure */` |
|      30 |  9469 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9470 | `	/* Initialize fields */` |
|      30 |  9471 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9472 | `	/* Emit the switch instruction */` |
|      30 |  9473 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9474 | `	/* Compile case blocks */` |
|      96 |  9475 | `	for(;;){` |
|       - |  9476 | `		sxu32 nKwrd;` |
|     112 |  9477 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9478 | `			/* No more input to process */` |
|     ! 0 |  9479 | `			break;` |
|       - |  9480 | `		}` |
|     112 |  9481 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9482 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9483 | `				/* Unexpected token */` |
|     ! 0 |  9484 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9485 | `					&pGen->pIn->sData);` |
|     ! 0 |  9486 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9487 | `					return SXERR_ABORT;` |
|       - |  9488 | `				}` |
|       - |  9489 | `				/* FALL THROUGH */` |
|     ! 0 |  9490 | `			}` |
|       - |  9491 | `			/* Block compiled */` |
|     ! 0 |  9492 | `			break;` |
|       - |  9493 | `		}` |
|       - |  9494 | `		/* Extract the keyword */` |
|     112 |  9495 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9496 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9497 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9498 | `				/* Unexpected token */` |
|     ! 0 |  9499 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9500 | `					&pGen->pIn->sData);` |
|     ! 0 |  9501 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9502 | `					return SXERR_ABORT;` |
|       - |  9503 | `				}` |
|       - |  9504 | `				/* FALL THROUGH */` |
|     ! 0 |  9505 | `			}` |
|       - |  9506 | `			/* Block compiled */` |
|       3 |  9507 | `			break;` |
|       - |  9508 | `		}` |
|     110 |  9509 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9510 | `			/*` |
|       - |  9511 | `			 * Accroding to the PHP language reference manual` |
|       - |  9512 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9513 | `			 *  that wasn't matched by the other cases.` |
|       - |  9514 | `			 */` |
|      22 |  9515 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9516 | `				/* Default case already compiled */` |
|     ! 0 |  9517 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9518 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9519 | `					return SXERR_ABORT;` |
|       - |  9520 | `				}` |
|     ! 0 |  9521 | `			}` |
|      22 |  9522 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9523 | `			/* Compile the default block */` |
|      22 |  9524 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9525 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9526 | `				return SXERR_ABORT;` |
|      22 |  9527 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9528 | `				break;` |
|       1 |  9529 | `			}` |
|      91 |  9530 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9531 | `			ph7_case_expr sCase;` |
|       - |  9532 | `			/* Standard case block */` |
|      90 |  9533 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9534 | `			/* initialize the structure */` |
|      90 |  9535 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9536 | `			/* Compile the case expression */` |
|      90 |  9537 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9538 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9539 | `				return SXERR_ABORT;` |
|       - |  9540 | `			}` |
|       - |  9541 | `			/* Compile the case block */` |
|      90 |  9542 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9543 | `			/* Insert in the switch container */` |
|      90 |  9544 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9545 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9546 | `				return SXERR_ABORT;` |
|      90 |  9547 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9548 | `				break;` |
|       - |  9549 | `			}` |
|      42 |  9550 | `		}else{` |
|       - |  9551 | `			/* Unexpected token */` |
|     ! 0 |  9552 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9553 | `				&pGen->pIn->sData);` |
|     ! 0 |  9554 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9555 | `				return SXERR_ABORT;` |
|       - |  9556 | `			}` |
|     ! 0 |  9557 | `			break;` |
|       - |  9558 | `		}` |
|       2 |  9559 | `	}` |
|       - |  9560 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9561 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9562 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9563 | `	/* Release the loop block */` |
|      30 |  9564 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9565 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9566 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9567 | `		pGen->pIn++;` |
|      14 |  9568 | `	}` |
|       - |  9569 | `	/* Statement successfully compiled */` |
|      30 |  9570 | `	return SXRET_OK;` |
|     ! 0 |  9571 | `Synchronize:` |
|       - |  9572 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9573 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9574 | `		pGen->pIn++;` |
|     ! 0 |  9575 | `	}` |
|     ! 0 |  9576 | `	return SXRET_OK;` |
|      16 |  9577 |  |
|       - |  9578 | `/*` |
|       - |  9579 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9580 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9581 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9582 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9583 | ` */` |
|       - |  9584 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9585 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9586 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9587 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9588 |  |
|       - |  9589 | `/*` |
|       - |  9590 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9591 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9592 | ` * patched entries from the pending set.` |
|       - |  9593 | ` */` |
| 1980294 |  9594 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       2 |  9595 |  |
| 1980296 |  9596 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9597 | `	sxu32 nTarget;` |
|       - |  9598 | `	sxu32 *aIdx;` |
|       - |  9599 | `	sxu32 i;` |
| 1980296 |  9600 | `	if( nCur <= nBaseline ){` |
| 1980206 |  9601 | `		return;` |
|       - |  9602 | `	}` |
|      92 |  9603 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      92 |  9604 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     190 |  9605 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     100 |  9606 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     100 |  9607 | `		if( pInstr ){` |
|     100 |  9608 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9609 | `		}` |
|      51 |  9610 | `	}` |
|      92 |  9611 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  990149 |  9612 |  |
|       - |  9613 |  |
|       - |  9614 | `/*` |
|       - |  9615 | ` * Generate bytecode for a given expression tree.` |
|       - |  9616 | ` * If something goes wrong while generating bytecode` |
|       - |  9617 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9618 | ` * this function takes care of generating the appropriate` |
|       - |  9619 | ` * error message.` |
|       - |  9620 | ` */` |
| 2611246 |  9621 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9622 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9623 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9624 | `	sxi32 iFlags /* Control flags */` |
|       - |  9625 | `	)` |
|       2 |  9626 |  |
|       - |  9627 | `	VmInstr *pInstr;` |
|       - |  9628 | `	sxu32 nJmpIdx;` |
| 2611248 |  9629 | `	sxi32 iP1 = 0;` |
| 2611248 |  9630 | `	sxu32 iP2 = 0;` |
| 2611248 |  9631 | `	void *p3  = 0;` |
|       - |  9632 | `	sxi32 iVmOp;` |
|       - |  9633 | `	sxi32 rc;` |
| 2611248 |  9634 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 2611248 |  9635 | `	sxu32 nRhsNsBase = 0;` |
| 2611248 |  9636 | `	if( pNode->xCode ){` |
|       - |  9637 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9638 | `		/* Compile node */` |
| 1618472 |  9639 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1618472 |  9640 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1618472 |  9641 | `		RE_SWAP_DELIMITER(pGen);` |
| 1618472 |  9642 | `		return rc;` |
|       - |  9643 | `	}` |
|  992778 |  9644 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9645 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9646 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9647 | `		return SXERR_ABORT;` |
|       - |  9648 | `	}` |
|  992778 |  9649 | `	iVmOp = pNode->pOp->iVmOp;` |
|  992778 |  9650 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9651 | `		sxu32 nJmp = 0;` |
|       - |  9652 | `		sxu32 nNcNsBase;` |
|       - |  9653 | `		VmInstr *pInstrFix;` |
|       - |  9654 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9655 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9656 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9657 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9658 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9659 | `		if( pNode->pRight ){` |
|      47 |  9660 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9661 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9662 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9663 | `				return rc;` |
|       - |  9664 | `			}` |
|      47 |  9665 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - |  9666 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9667 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9668 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9669 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9670 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9671 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9672 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9673 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9674 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9675 | `				pInstrFix->iP2 = 3;` |
|       9 |  9676 | `			}` |
|      23 |  9677 | `		}` |
|       - |  9678 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9679 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9680 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9681 | `		if( pNode->pLeft ){` |
|      47 |  9682 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9683 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9684 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9685 | `				return rc;` |
|       - |  9686 | `			}` |
|      47 |  9687 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      23 |  9688 | `		}` |
|       - |  9689 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9690 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9691 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9692 | `		if( nJmp > 0 ){` |
|      47 |  9693 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9694 | `			if( pInstrFix ){` |
|      47 |  9695 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9696 | `			}` |
|      23 |  9697 | `		}` |
|      47 |  9698 | `		return SXRET_OK;` |
|       - |  9699 | `	}` |
|  992732 |  9700 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9701 | `		sxu32 nJz,nJmp;` |
|       - |  9702 | `		sxu32 nTernaryNsBase;` |
|       - |  9703 | `		/* Ternary operator require special handling */` |
|       - |  9704 | `		/* Phase#1: Compile the condition */` |
|    2026 |  9705 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2026 |  9706 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2026 |  9707 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9708 | `			return rc;` |
|       - |  9709 | `		}` |
|       - |  9710 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - |  9711 | `		 * compiling the condition must short-circuit to the end of the` |
|       - |  9712 | `		 * condition expression, not leak past the ternary. */` |
|    2026 |  9713 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2026 |  9714 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2026 |  9715 | `		if( pNode->pLeft ){` |
|       - |  9716 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9717 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1958 |  9718 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9719 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1958 |  9720 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    1958 |  9721 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1958 |  9722 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9723 | `				return rc;` |
|       - |  9724 | `			}` |
|    1958 |  9725 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     980 |  9726 | `		}else{` |
|       - |  9727 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9728 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9729 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9730 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9731 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9732 | `		}` |
|       - |  9733 | `		/* Phase#4: Emit the unconditional jump */` |
|    2026 |  9734 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9735 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2026 |  9736 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2026 |  9737 | `		if( pInstr ){` |
|    2026 |  9738 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1012 |  9739 | `		}` |
|    2026 |  9740 | `		if( !pNode->pLeft ){` |
|       - |  9741 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9742 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9743 | `		}` |
|       - |  9744 | `		/* Phase#6: Compile the 'else' expression */` |
|    2026 |  9745 | `		if( pNode->pRight ){` |
|    2026 |  9746 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2026 |  9747 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2026 |  9748 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9749 | `				return rc;` |
|       - |  9750 | `			}` |
|    2026 |  9751 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1012 |  9752 | `		}` |
|    2026 |  9753 | `		if( nJmp > 0 ){` |
|       - |  9754 | `			/* Phase#7: Fix the unconditional jump */` |
|    2026 |  9755 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2026 |  9756 | `			if( pInstr ){` |
|    2026 |  9757 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1012 |  9758 | `			}` |
|    1012 |  9759 | `		}` |
|       - |  9760 | `		/* All done */` |
|    2026 |  9761 | `		return SXRET_OK;` |
|       - |  9762 | `	}` |
|  990708 |  9763 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - |  9764 | `	/* Generate code for the left tree */` |
|  990708 |  9765 | `	if( pNode->pLeft ){` |
|  990670 |  9766 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  990670 |  9767 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - |  9768 | `			ph7_expr_node **apNode;` |
|  332386 |  9769 | `			int hasSpread = 0;` |
|  332386 |  9770 | `			int hasNamed = 0;` |
|       - |  9771 | `			sxi32 nArgs;` |
|       - |  9772 | `			sxi32 n;` |
|       - |  9773 | `			/* Recurse and generate bytecodes for function arguments */` |
|  332386 |  9774 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  332386 |  9775 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - |  9776 | `			/* Validate: no positional arguments after named arguments */` |
|       - |  9777 | `			{` |
|  332386 |  9778 | `				int seenNamed = 0;` |
|  663764 |  9779 | `				for( n = 0; n < nArgs; ++n ){` |
|  331382 |  9780 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     176 |  9781 | `						seenNamed = 1;` |
|     176 |  9782 | `						hasNamed = 1;` |
|  331295 |  9783 | `					}else if( seenNamed && !(apNode[n]->iFlags & EXPR_NODE_SPREAD) ){` |
|       3 |  9784 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - |  9785 | `							"Cannot use positional argument after named argument");` |
|       3 |  9786 | `						return SXERR_SYNTAX;` |
|       - |  9787 | `					}` |
|  165691 |  9788 | `				}` |
|       - |  9789 | `			}` |
|       - |  9790 | `			/* Read-only load */` |
|  332384 |  9791 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  663760 |  9792 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  331378 |  9793 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  331378 |  9794 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  331378 |  9795 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9796 | `					return rc;` |
|       - |  9797 | `				}` |
|       - |  9798 | `				/* Each argument is an independent nullsafe scope. */` |
|  331378 |  9799 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  331378 |  9800 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - |  9801 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 |  9802 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 |  9803 | `					hasSpread = 1;` |
|       9 |  9804 | `				}` |
|  165690 |  9805 | `			}` |
|       - |  9806 | `			/* Total number of given arguments */` |
|  332384 |  9807 | `			iP1 = nArgs;` |
|  332384 |  9808 | `			iP2 = hasSpread;` |
|       - |  9809 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - |  9810 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  332384 |  9811 | `			if( hasNamed ){` |
|      94 |  9812 | `				sxu32 nStrBytes = 0;` |
|       - |  9813 | `				char *zBuf;` |
|     278 |  9814 | `				for( n = 0; n < nArgs; ++n ){` |
|     186 |  9815 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9816 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      86 |  9817 | `					}` |
|      94 |  9818 | `				}` |
|       - |  9819 | `				{` |
|      94 |  9820 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      94 |  9821 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      92 |  9822 | `					&pGen->pVm->sAllocator, mapSize);` |
|      94 |  9823 | `				if( pMap ){` |
|      94 |  9824 | `					SyZero(pMap, mapSize);` |
|      94 |  9825 | `					pMap->bHasNamed = 1;` |
|      94 |  9826 | `					pMap->nTotal = (sxu32)nArgs;` |
|      94 |  9827 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      94 |  9828 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     278 |  9829 | `					for( n = 0; n < nArgs; ++n ){` |
|     186 |  9830 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9831 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     174 |  9832 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     174 |  9833 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     174 |  9834 | `							zBuf += nb;` |
|      86 |  9835 | `						}` |
|       - |  9836 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      94 |  9837 | `					}` |
|      94 |  9838 | `					p3 = (void *)pMap;` |
|      46 |  9839 | `				}` |
|       - |  9840 | `				}` |
|      46 |  9841 | `			}` |
|       - |  9842 | `			/* Remove stale flags now */` |
|  332384 |  9843 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  166191 |  9844 | `		}` |
|  990668 |  9845 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  990668 |  9846 | `		if( rc != SXRET_OK ){` |
|      31 |  9847 | `			return rc;` |
|       - |  9848 | `		}` |
|  990638 |  9849 | `		if( !bIsChainOp ){` |
|       - |  9850 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - |  9851 | `			 * target the end of that LHS chain, which is right here. */` |
|  471762 |  9852 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  235880 |  9853 | `		}` |
|  990638 |  9854 | `		if( iVmOp == PH7_OP_CALL ){` |
|  332384 |  9855 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  332384 |  9856 | `			if( pInstr ){` |
|  332384 |  9857 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  331536 |  9858 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - |  9859 | `					sxu32 nQual;` |
|       - |  9860 | `					/* Prevent constant expansion */` |
|  331536 |  9861 | `					pInstr->iP1 = 0;` |
|       - |  9862 | `					/* Namespace-qualify the function name for CALL.` |
|       - |  9863 | `					 * Only check function imports — class imports must NOT` |
|       - |  9864 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - |  9865 | `					 * handler fires before NEW; we store the original literal` |
|       - |  9866 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - |  9867 | `					 * can recover the unqualified name and re-qualify with` |
|       - |  9868 | `					 * class imports. */ {` |
|  331536 |  9869 | `						int fromImport = 0;` |
|  331536 |  9870 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  331536 |  9871 | `						pInstr->iP2 = (sxi32)nQual;` |
|  331536 |  9872 | `						if( nQual != nOrig ){` |
|       - |  9873 | `							/* Store original literal index in CALL's iP2 so the` |
|       - |  9874 | `							 * NEW handler can recover the unqualified name. */` |
|      72 |  9875 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      72 |  9876 | `							if( !fromImport ){` |
|       - |  9877 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      62 |  9878 | `								if( p3 == 0 ){` |
|      62 |  9879 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      60 |  9880 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      62 |  9881 | `									if( pMap ){` |
|      62 |  9882 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      62 |  9883 | `										p3 = (void *)pMap;` |
|      30 |  9884 | `									}` |
|      30 |  9885 | `								}` |
|      62 |  9886 | `								if( p3 ){` |
|      62 |  9887 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      30 |  9888 | `								}` |
|      30 |  9889 | `							}` |
|      37 |  9890 | `						}` |
|       - |  9891 | `					}` |
|  166617 |  9892 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - |  9893 | `					/* Method call,flag that */` |
|     694 |  9894 | `					pInstr->iP2 = 1;` |
|     346 |  9895 | `				}` |
|  166193 |  9896 | `			}` |
|  824447 |  9897 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - |  9898 | `			ph7_expr_node **apNode;` |
|       - |  9899 | `			sxi32 n;` |
|       - |  9900 | `			/* Recurse and generate bytecodes for array index */` |
|   74446 |  9901 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  134310 |  9902 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   59866 |  9903 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   59866 |  9904 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   59866 |  9905 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9906 | `					return rc;` |
|       - |  9907 | `				}` |
|       - |  9908 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   59866 |  9909 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   29934 |  9910 | `			}` |
|   74446 |  9911 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   59866 |  9912 | `				iP1 = 1; /* Node have an index associated with it */` |
|   29932 |  9913 | `			}` |
|   74446 |  9914 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - |  9915 | `				/* Create an empty entry when the desired index is not found */` |
|   29420 |  9916 | `				iP2 = 1;` |
|   14711 |  9917 | `			}` |
|  621034 |  9918 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - |  9919 | `			/* POP the left node */` |
|      32 |  9920 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 |  9921 | `		}` |
|  495318 |  9922 | `	}` |
|  990676 |  9923 | `	rc = SXRET_OK;` |
|  990676 |  9924 | `	nJmpIdx = 0;` |
|       - |  9925 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - |  9926 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - |  9927 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  990676 |  9928 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     270 |  9929 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     270 |  9930 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     270 |  9931 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     270 |  9932 | `			int isSpecial = 0;` |
|     270 |  9933 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     182 |  9934 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     182 |  9935 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     195 |  9936 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     160 |  9937 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      83 |  9938 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      90 |  9939 | `					isSpecial = 1;` |
|      44 |  9940 | `				}` |
|     112 |  9941 | `			}` |
|     314 |  9942 | `			pInstr->iP1 = 0;` |
|     314 |  9943 | `			if( !isSpecial ){` |
|     138 |  9944 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 |  9945 | `			}` |
|       - |  9946 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - |  9947 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     226 |  9948 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     138 |  9949 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     138 |  9950 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 |  9951 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 |  9952 | `					return SXRET_OK;` |
|       - |  9953 | `				}` |
|      47 |  9954 | `			}` |
|      91 |  9955 | `		}` |
|     167 |  9956 | `	}` |
|       - |  9957 | `	/* Generate code for the right tree */` |
|  990598 |  9958 | `	if( pNode->pRight ){` |
|  517640 |  9959 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - |  9960 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9128 |  9961 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  513077 |  9962 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - |  9963 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3052 |  9964 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  506989 |  9965 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - |  9966 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      84 |  9967 | `			iVmOp = 0; /* No binary operator to emit */` |
|      84 |  9968 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  505472 |  9969 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - |  9970 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - |  9971 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - |  9972 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - |  9973 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - |  9974 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - |  9975 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     100 |  9976 | `			sxu32 nNsJmp = 0;` |
|     100 |  9977 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     100 |  9978 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  505333 |  9979 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  225868 |  9980 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  112933 |  9981 | `		}` |
|  517640 |  9982 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  517640 |  9983 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  517640 |  9984 | `		if( !bIsChainOp ){` |
|       - |  9985 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - |  9986 | `			 * operator instruction is emitted. */` |
|  405632 |  9987 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  202815 |  9988 | `		}` |
|  517640 |  9989 | `		if( iVmOp == PH7_OP_STORE ){` |
|  222782 |  9990 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  222756 |  9991 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - |  9992 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - |  9993 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - |  9994 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - |  9995 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - |  9996 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - |  9997 | `				 */` |
|      54 |  9998 | `				iVmOp = 0;` |
|  222756 |  9999 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  222730 | 10000 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10001 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   49632 | 10002 | `					iP2 = 1;` |
|   24817 | 10003 | `				}else{` |
|  173100 | 10004 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10005 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   29358 | 10006 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   29358 | 10007 | `						iP1 = pInstr->iP1;` |
|   14680 | 10008 | `					}else{` |
|  143744 | 10009 | `						p3 = pInstr->p3;` |
|       - | 10010 | `					}` |
|       - | 10011 | `					/* POP the last dynamic load instruction */` |
|  173100 | 10012 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10013 | `				}` |
|  111366 | 10014 | `			}` |
|  406250 | 10015 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 10016 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 10017 | `			if( pInstr ){` |
|      48 | 10018 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10019 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10020 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10021 | `					 */` |
|      15 | 10022 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10023 | `					iP1 = pInstr->iP1;` |
|      15 | 10024 | `					iP2 = pInstr->iP2;` |
|      15 | 10025 | `					p3  = pInstr->p3;` |
|       8 | 10026 | `				}else{` |
|      34 | 10027 | `					p3 = pInstr->p3;` |
|       - | 10028 | `				}` |
|      23 | 10029 | `			}` |
|      23 | 10030 | `		}` |
|  258819 | 10031 | `	}` |
|  990598 | 10032 | `	if( iVmOp > 0 ){` |
|  990434 | 10033 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11870 | 10034 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10035 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8716 | 10036 | `				iP1 = 1;` |
|    4359 | 10037 | `			}` |
|  984500 | 10038 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10039 | `			/* Namespace-qualify the class name for NEW */ {` |
|   15238 | 10040 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   15238 | 10041 | `				VmInstr *pCallInstr = 0;` |
|   15238 | 10042 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   15222 | 10043 | `					pCallInstr = pPeek;` |
|   15222 | 10044 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7610 | 10045 | `				}` |
|   15238 | 10046 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - | 10047 | `					sxu32 nLitForClass;` |
|       - | 10048 | `					/* If the CALL handler already qualified the name using` |
|       - | 10049 | `					 * function imports, recover the original unqualified` |
|       - | 10050 | `					 * literal so we can re-qualify with class imports. */` |
|   15236 | 10051 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 | 10052 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 | 10053 | `					}else{` |
|   15204 | 10054 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10055 | `					}` |
|   15236 | 10056 | `					pPeek->iP1 = 0;` |
|   15236 | 10057 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7617 | 10058 | `				}` |
|       - | 10059 | `			}` |
|   15238 | 10060 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   15238 | 10061 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10062 | `				VmInstr *pPrev;` |
|   15222 | 10063 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   15222 | 10064 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10065 | `					/* Pop the call instruction, preserve named-arg map */` |
|   15222 | 10066 | `					iP1 = pInstr->iP1;` |
|   15222 | 10067 | `					if( pInstr->p3 ){` |
|      38 | 10068 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      18 | 10069 | `					}` |
|   15222 | 10070 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7610 | 10071 | `				}` |
|    7612 | 10072 | `			}` |
|  970948 | 10073 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10074 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10075 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 | 10076 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 | 10077 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 | 10078 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 10079 | `				int isSpecialIs = 0;` |
|      50 | 10080 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 10081 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 10082 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 10083 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 10084 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 10085 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 10086 | `						isSpecialIs = 1;` |
|       5 | 10087 | `					}` |
|      23 | 10088 | `				}` |
|      52 | 10089 | `				pInstr->iP1 = 0;` |
|      52 | 10090 | `				if( !isSpecialIs ){` |
|      38 | 10091 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 10092 | `				}` |
|      25 | 10093 | `			}` |
|  963309 | 10094 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10095 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10096 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10097 | `			 * should not trigger constant lookup. */` |
|  112010 | 10098 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  112010 | 10099 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  111972 | 10100 | `				pInstr->iP1 = 0;` |
|   55985 | 10101 | `			}` |
|  112010 | 10102 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10103 | `				/* Static member access,remember that */` |
|     192 | 10104 | `				iP1 = 1;` |
|     192 | 10105 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     192 | 10106 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      32 | 10107 | `					p3 = pInstr->p3;` |
|      32 | 10108 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      15 | 10109 | `				}` |
|      95 | 10110 | `			}` |
|   56004 | 10111 | `		}` |
|       - | 10112 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  990432 | 10113 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  495215 | 10114 | `	}` |
|  990596 | 10115 | `	if( nJmpIdx > 0 ){` |
|       - | 10116 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   12260 | 10117 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   12260 | 10118 | `		if( pInstr ){` |
|   12260 | 10119 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6129 | 10120 | `		}` |
|    6129 | 10121 | `	}` |
|  990596 | 10122 | `	return rc;` |
| 1305606 | 10123 |  |
|       - | 10124 | `/*` |
|       - | 10125 | ` * Compile a PHP expression.` |
|       - | 10126 | ` * According to the PHP language reference manual:` |
|       - | 10127 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10128 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10129 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10130 | ` *  is "anything that has a value".` |
|       - | 10131 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10132 | ` * function takes care of generating the appropriate error` |
|       - | 10133 | ` * message.` |
|       - | 10134 | ` */` |
|  705760 | 10135 | `static sxi32 PH7_CompileExpr(` |
|       - | 10136 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10137 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10138 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10139 | `	)` |
|       2 | 10140 |  |
|       - | 10141 | `	ph7_expr_node *pRoot;` |
|       - | 10142 | `	SySet sExprNode;` |
|       - | 10143 | `	SyToken *pEnd;` |
|       - | 10144 | `	sxi32 nExpr;` |
|       - | 10145 | `	sxi32 iNest;` |
|       - | 10146 | `	sxi32 rc;` |
|       - | 10147 | `	sxu32 nNullsafeBase;` |
|       - | 10148 | `	/* Initialize worker variables */` |
|  705762 | 10149 | `	nExpr = 0;` |
|  705762 | 10150 | `	pRoot = 0;` |
|       - | 10151 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10152 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  705762 | 10153 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  705762 | 10154 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  705762 | 10155 | `	SySetAlloc(&sExprNode,0x10);` |
|  705762 | 10156 | `	rc = SXRET_OK;` |
|       - | 10157 | `	/* Delimit the expression */` |
|  705762 | 10158 | `	pEnd = pGen->pIn;` |
|  705762 | 10159 | `	iNest = 0;` |
| 4756706 | 10160 | `	while( pEnd < pGen->pEnd ){` |
| 4510468 | 10161 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10162 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     328 | 10163 | `			iNest++;` |
| 4510305 | 10164 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     336 | 10165 | `			iNest--;` |
| 4509975 | 10166 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  459738 | 10167 | `			if( iNest <= 0 ){` |
|  459524 | 10168 | `				break;` |
|       - | 10169 | `			}` |
|     107 | 10170 | `		}` |
| 4050946 | 10171 | `		pEnd++;` |
|       2 | 10172 | `	}` |
|  705762 | 10173 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   11966 | 10174 | `		SyToken *pEnd2 = pGen->pIn;` |
|   11966 | 10175 | `		iNest = 0;` |
|       - | 10176 | `		/* Stop at the first comma */` |
|   23980 | 10177 | `		while( pEnd2 < pEnd ){` |
|   12020 | 10178 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      16 | 10179 | `				iNest++;` |
|   12013 | 10180 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      16 | 10181 | `				iNest--;` |
|   11999 | 10182 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      13 | 10183 | `				if( iNest <= 0 ){` |
|       5 | 10184 | `					break;` |
|       - | 10185 | `				}` |
|       4 | 10186 | `			}` |
|   12016 | 10187 | `			pEnd2++;` |
|       2 | 10188 | `		}` |
|   11966 | 10189 | `		if( pEnd2 <pEnd ){` |
|       5 | 10190 | `			pEnd = pEnd2;` |
|       2 | 10191 | `		}` |
|    5982 | 10192 | `	}` |
|  705762 | 10193 | `	if( pEnd > pGen->pIn ){` |
|  705752 | 10194 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10195 | `		/* Swap delimiter */` |
|  705752 | 10196 | `		pGen->pEnd = pEnd;` |
|       - | 10197 | `		/* Try to get an expression tree */` |
|  705752 | 10198 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  705752 | 10199 | `		if( rc == SXRET_OK && pRoot ){` |
|  705570 | 10200 | `			rc = SXRET_OK;` |
|  705570 | 10201 | `			if( xTreeValidator ){` |
|       - | 10202 | `				/* Call the upper layer validator callback */` |
|   21644 | 10203 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   10821 | 10204 | `			}` |
|  705570 | 10205 | `			if( rc != SXERR_ABORT ){` |
|       - | 10206 | `				/* Generate code for the given tree */` |
|  705570 | 10207 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10208 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10209 | `				 * expression so they short-circuit to its end. */` |
|  705570 | 10210 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  352784 | 10211 | `			}` |
|  705570 | 10212 | `			nExpr = 1;` |
|  352784 | 10213 | `		}` |
|       - | 10214 | `		/* Release the whole tree */` |
|  705752 | 10215 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10216 | `		/* Synchronize token stream */` |
|  705752 | 10217 | `		pGen->pEnd = pTmp;` |
|  705752 | 10218 | `		pGen->pIn  = pEnd;` |
|  705752 | 10219 | `		if( rc == SXERR_ABORT ){` |
|      11 | 10220 | `			SySetRelease(&sExprNode);` |
|      11 | 10221 | `			return SXERR_ABORT;` |
|       - | 10222 | `		}` |
|  352870 | 10223 | `	}` |
|  705752 | 10224 | `	SySetRelease(&sExprNode);` |
|  705752 | 10225 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  352882 | 10226 |  |
|       - | 10227 | `/*` |
|       - | 10228 | ` * Return a pointer to the node construct handler associated` |
|       - | 10229 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10230 | ` */` |
|  175512 | 10231 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 10232 |  |
|  175514 | 10233 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10234 | `		/* Numeric literal: Either real or integer */` |
|   96346 | 10235 | `		return PH7_CompileNumLiteral;` |
|   79170 | 10236 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10237 | `		/* Double quoted string */` |
|   16952 | 10238 | `		return PH7_CompileString;` |
|   62220 | 10239 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10240 | `		/* Single quoted string */` |
|   62108 | 10241 | `		return PH7_CompileSimpleString;` |
|     114 | 10242 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10243 | `		/* Heredoc */` |
|      66 | 10244 | `		return PH7_CompileHereDoc;` |
|      50 | 10245 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10246 | `		/* Nowdoc */` |
|      44 | 10247 | `		return PH7_CompileNowDoc;` |
|       7 | 10248 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10249 | `		/* Backtick quoted string */` |
|       5 | 10250 | `		return PH7_CompileBacktic;` |
|       - | 10251 | `	}` |
|       3 | 10252 | `	return 0;` |
|   87758 | 10253 |  |
|       - | 10254 | `/*` |
|       - | 10255 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10256 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10257 | ` * in write context" parse error.` |
|       - | 10258 | ` */` |
|    6454 | 10259 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       2 | 10260 |  |
|       - | 10261 | `	sxi32 rc;` |
|    6456 | 10262 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6454 | 10263 | `		return SXRET_OK;` |
|       - | 10264 | `	}` |
|       5 | 10265 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10266 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10267 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10268 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3229 | 10269 |  |
|       - | 10270 | `/*` |
|       - | 10271 | ` * Compile an unset() statement.` |
|       - | 10272 | ` * unset($var, $arr[$key], ...);` |
|       - | 10273 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10274 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10275 | ` * parent array before extracting the element to unset.` |
|       - | 10276 | ` */` |
|    2798 | 10277 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 10278 |  |
|    2800 | 10279 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2800 | 10280 | `	sxu32 nIdx = 0;` |
|       - | 10281 | `	SyString sName;` |
|       - | 10282 | `	sxi32 rc;` |
|       - | 10283 | `	/* Jump the 'unset' keyword */` |
|    2800 | 10284 | `	pGen->pIn++;` |
|       - | 10285 | `	/* Save delimiter */` |
|    2800 | 10286 | `	pTmp = pGen->pEnd;` |
|       - | 10287 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2800 | 10288 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2800 | 10289 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10290 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10291 | `		SyToken *pClose;` |
|    2800 | 10292 | `		pGen->pIn++;   /* Skip '(' */` |
|    2800 | 10293 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2800 | 10294 | `		pEnd = pClose; /* Stop at ')' */` |
|    1399 | 10295 | `	}` |
|    2800 | 10296 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10297 | `	/* Resolve the 'unset' builtin name once */` |
|    2800 | 10298 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     336 | 10299 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     336 | 10300 | `		if( pObj == 0 ){` |
|     ! 0 | 10301 | `			return SXERR_ABORT;` |
|       - | 10302 | `		}` |
|     336 | 10303 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     336 | 10304 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     167 | 10305 | `	}` |
|       - | 10306 | `	/* Compile each comma-separated argument */` |
|    9256 | 10307 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6458 | 10308 | `		if( pGen->pIn < pNext ){` |
|    6458 | 10309 | `			pGen->pEnd = pNext;` |
|    6458 | 10310 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10311 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,` |
|       - | 10312 | `				GenStateUnsetValidator);` |
|    6458 | 10313 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10314 | `				return SXERR_ABORT;` |
|       - | 10315 | `			}` |
|    6458 | 10316 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10317 | `				/* Emit call for this single argument */` |
|    6456 | 10318 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6456 | 10319 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6456 | 10320 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3227 | 10321 | `			}` |
|    3228 | 10322 | `		}` |
|       - | 10323 | `		/* Jump trailing commas */` |
|   10118 | 10324 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3662 | 10325 | `			pNext++;` |
|       2 | 10326 | `		}` |
|    6458 | 10327 | `		pGen->pIn = pNext;` |
|       2 | 10328 | `	}` |
|       - | 10329 | `	/* Skip past the closing ')' if present */` |
|    2800 | 10330 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2800 | 10331 | `		pGen->pIn++;` |
|    1399 | 10332 | `	}` |
|       - | 10333 | `	/* Restore token stream */` |
|    2800 | 10334 | `	pGen->pEnd = pTmp;` |
|    2800 | 10335 | `	return SXRET_OK;` |
|    1401 | 10336 |  |
|       - | 10337 | `/*` |
|       - | 10338 | ` * PHP Language construct table.` |
|       - | 10339 | ` */` |
|       - | 10340 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10341 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10342 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10343 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10344 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10345 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10346 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10347 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10348 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10349 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10350 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10351 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10352 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10353 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10354 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10355 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10356 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10357 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10358 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10359 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10360 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10361 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10362 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10363 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10364 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10365 | `};` |
|       - | 10366 | `/*` |
|       - | 10367 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10368 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10369 | ` */` |
|  427704 | 10370 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10371 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10372 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10373 | `	)` |
|       2 | 10374 |  |
|  427706 | 10375 | `	sxu32 n = 0;` |
| 1800812 | 10376 | `	for(;;){` |
| 3601626 | 10377 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   50298 | 10378 | `			break;` |
|       - | 10379 | `		}` |
| 3551330 | 10380 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  377410 | 10381 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10382 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10383 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10384 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10385 | `					return 0;` |
|       - | 10386 | `				}` |
|     ! 0 | 10387 | `			}` |
|  377408 | 10388 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 10389 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 10390 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10391 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10392 | `				return 0;` |
|       - | 10393 | `			}` |
|       - | 10394 | `			/* Return a pointer to the handler.` |
|       - | 10395 | `			*/` |
|  377410 | 10396 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10397 | `		}` |
| 3173922 | 10398 | `		n++;` |
|       2 | 10399 | `	}` |
|   50298 | 10400 | `	if( pLookahed ){` |
|   50298 | 10401 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8746 | 10402 | `			return PH7_CompileClassInterface;` |
|   41554 | 10403 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   41342 | 10404 | `			return PH7_CompileClass;` |
|     214 | 10405 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 | 10406 | `			return PH7_CompileTrait;` |
|     158 | 10407 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      21 | 10408 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      20 | 10409 | `				return PH7_CompileAbstractClass;` |
|     140 | 10410 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 10411 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10412 | `				return PH7_CompileFinalClass;` |
|       - | 10413 | `		}` |
|      69 | 10414 | `	}` |
|       - | 10415 | `	/* Not a language construct */` |
|     140 | 10416 | `	return 0;` |
|  213854 | 10417 |  |
|       - | 10418 | `/*` |
|       - | 10419 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10420 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10421 | ` */` |
|     138 | 10422 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 10423 |  |
|       - | 10424 | `	int rc;` |
|     140 | 10425 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     140 | 10426 | `	if( rc == FALSE ){` |
|      44 | 10427 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 | 10428 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10429 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10430 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10431 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10432 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10433 | `			*/` |
|       - | 10434 | `			){` |
|      38 | 10435 | `				rc = TRUE;` |
|      18 | 10436 | `		}` |
|      22 | 10437 | `	}` |
|     140 | 10438 | `	return rc;` |
|       2 | 10439 |  |
|       - | 10440 | `/*` |
|       - | 10441 | ` * Compile a PHP chunk.` |
|       - | 10442 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10443 | ` * takes care of generating the appropriate error message.` |
|       - | 10444 | ` */` |
|  574186 | 10445 | `static sxi32 GenStateCompileChunk(` |
|       - | 10446 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10447 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10448 | `	)` |
|       2 | 10449 |  |
|       - | 10450 | `	ProcLangConstruct xCons;` |
|       - | 10451 | `	sxi32 rc;` |
|  574188 | 10452 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  343139 | 10453 | `	for(;;){` |
|  686280 | 10454 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10455 | `			/* No more input to process */` |
|   12362 | 10456 | `			break;` |
|       - | 10457 | `		}` |
|  673920 | 10458 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10459 | `			/* Compile block */` |
|      16 | 10460 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      16 | 10461 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10462 | `				break;` |
|       - | 10463 | `			}` |
|       9 | 10464 | `		}else{` |
|  673906 | 10465 | `			xCons = 0;` |
|  673906 | 10466 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  427706 | 10467 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10468 | `				/* Try to extract a language construct handler */` |
|  427706 | 10469 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  427706 | 10470 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10471 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10472 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10473 | `						&pGen->pIn->sData);` |
|       9 | 10474 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10475 | `						break;` |
|       - | 10476 | `					}` |
|       - | 10477 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10478 | `					 * this erroneous statement.` |
|       - | 10479 | `					 */` |
|       9 | 10480 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10481 | `				}` |
|  460054 | 10482 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   43094 | 10483 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10484 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10485 | `				xCons = PH7_CompileLabel;` |
|      56 | 10486 | `			}` |
|  673906 | 10487 | `			if( xCons == 0 ){` |
|       - | 10488 | `				/* Assume an expression an try to compile it */` |
|  246220 | 10489 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  246220 | 10490 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10491 | `					/* Pop l-value */` |
|  246070 | 10492 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  123034 | 10493 | `				}` |
|  123111 | 10494 | `			}else{` |
|       - | 10495 | `				/* Go compile the sucker */` |
|  427688 | 10496 | `				rc = xCons(&(*pGen));` |
|       - | 10497 | `			}` |
|  673906 | 10498 | `			if( rc == SXERR_ABORT ){` |
|       - | 10499 | `				/* Request to abort compilation */` |
|      11 | 10500 | `				break;` |
|       - | 10501 | `			}` |
|       - | 10502 | `		}` |
|       - | 10503 | `		/* Ignore trailing semi-colons ';' */` |
| 1116050 | 10504 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  442142 | 10505 | `			pGen->pIn++;` |
|       2 | 10506 | `		}` |
|  673910 | 10507 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10508 | `			/* Compile a single statement and return */` |
|  561818 | 10509 | `			break;` |
|       - | 10510 | `		}` |
|       - | 10511 | `		/* LOOP ONE */` |
|       - | 10512 | `		/* LOOP TWO */` |
|       - | 10513 | `		/* LOOP THREE */` |
|       - | 10514 | `		/* LOOP FOUR */` |
|       2 | 10515 | `	}` |
|       - | 10516 | `	/* Return compilation status */` |
|  574188 | 10517 | `	return rc;` |
|       2 | 10518 |  |
|       - | 10519 | `/*` |
|       - | 10520 | ` * Compile a Raw PHP chunk.` |
|       - | 10521 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10522 | ` * takes care of generating the appropriate error message.` |
|       - | 10523 | ` */` |
|   12372 | 10524 | `static sxi32 PH7_CompilePHP(` |
|       - | 10525 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10526 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10527 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10528 | `	)` |
|       2 | 10529 |  |
|   12374 | 10530 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10531 | `	sxi32 rc;` |
|       - | 10532 | `	/* Reset the token set */` |
|   12374 | 10533 | `	SySetReset(&(*pTokenSet));` |
|       - | 10534 | `	/* Mark as the default token set */` |
|   12374 | 10535 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10536 | `	/* Advance the stream cursor */` |
|   12374 | 10537 | `	pGen->pRawIn++;` |
|       - | 10538 | `	/* Tokenize the PHP chunk first */` |
|   12374 | 10539 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10540 | `	/* Point to the head and tail of the token stream. */` |
|   12374 | 10541 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   12374 | 10542 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   12374 | 10543 | `	if( is_expr ){` |
|     ! 0 | 10544 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10545 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10546 | `			/* A simple expression,compile it */` |
|     ! 0 | 10547 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10548 | `		}` |
|       - | 10549 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10550 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10551 | `		return SXRET_OK;` |
|       - | 10552 | `	}` |
|   12374 | 10553 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10554 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10555 | `		/*` |
|       - | 10556 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10557 | `		 * According to the PHP reference manual:` |
|       - | 10558 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10559 | `		 *  immediately follow` |
|       - | 10560 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10561 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10562 | `		 * Symisc extension:` |
|       - | 10563 | `		 *   This short syntax works with all PHP opening` |
|       - | 10564 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10565 | `		 *   only short tag.` |
|       - | 10566 | `		 */` |
|       - | 10567 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10568 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10569 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 10570 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 10571 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 10572 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 10573 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 10574 | `		}` |
|       3 | 10575 | `		return SXRET_OK;` |
|       - | 10576 | `	}` |
|       - | 10577 | `	/* Compile the PHP chunk */` |
|   12372 | 10578 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 10579 | `	/* Fix exceptions jumps */` |
|   12372 | 10580 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10581 | `	/* Fix gotos now, the jump destination is resolved */` |
|   12372 | 10582 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 10583 | `		rc = SXERR_ABORT;` |
|       1 | 10584 | `	}` |
|       - | 10585 | `	/* Reset container */` |
|   12372 | 10586 | `	SySetReset(&pGen->aGoto);` |
|   12372 | 10587 | `	SySetReset(&pGen->aLabel);` |
|   12372 | 10588 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 10589 | `	/* Compilation result */` |
|   12372 | 10590 | `	return rc;` |
|    6188 | 10591 |  |
|       - | 10592 | `/*` |
|       - | 10593 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 10594 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 10595 | ` * This is the only compile interface exported from this file.` |
|       - | 10596 | ` */` |
|   14676 | 10597 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 10598 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 10599 | `	SyString *pScript,  /* Script to compile */` |
|       - | 10600 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 10601 | `	)` |
|       2 | 10602 |  |
|       - | 10603 | `	SySet aPhpToken,aRawToken;` |
|       - | 10604 | `	ph7_gen_state *pCodeGen;` |
|       - | 10605 | `	ph7_value *pRawObj;` |
|       - | 10606 | `	sxu32 nObjIdx;` |
|       - | 10607 | `	sxi32 nRawObj;` |
|       - | 10608 | `	int is_expr;` |
|       - | 10609 | `	sxi32 rc;` |
|   14678 | 10610 | `	if( pScript->nByte < 1 ){` |
|       - | 10611 | `		/* Nothing to compile */` |
|     ! 0 | 10612 | `		return PH7_OK;` |
|       - | 10613 | `	}` |
|       - | 10614 | `	/* Initialize the tokens containers */` |
|   14678 | 10615 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14678 | 10616 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14678 | 10617 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14678 | 10618 | `	is_expr = 0;` |
|   14678 | 10619 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 10620 | `		SyToken sTmp;` |
|       - | 10621 | `		/* PHP only: -*/` |
|    2932 | 10622 | `		sTmp.nLine = 1;` |
|    2932 | 10623 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2932 | 10624 | `		sTmp.pUserData = 0;` |
|    2932 | 10625 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2932 | 10626 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2932 | 10627 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 10628 | `			/* A simple PHP expression */` |
|     ! 0 | 10629 | `			is_expr = 1;` |
|     ! 0 | 10630 | `		}` |
|    1467 | 10631 | `	}else{` |
|       - | 10632 | `		/* Tokenize raw text */` |
|   11748 | 10633 | `		SySetAlloc(&aRawToken,32);` |
|   11748 | 10634 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 10635 | `	}` |
|   14678 | 10636 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 10637 | `	/* Process high-level tokens */` |
|   14678 | 10638 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14678 | 10639 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14678 | 10640 | `	rc = PH7_OK;` |
|   14678 | 10641 | `	if( is_expr ){` |
|       - | 10642 | `		/* Compile the expression */` |
|     ! 0 | 10643 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 10644 | `		goto cleanup;` |
|       - | 10645 | `	}` |
|   14678 | 10646 | `	nObjIdx = 0;` |
|       - | 10647 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 10648 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 10649 | `	 * preventing namespace bleeding across include()d files. */` |
|   14678 | 10650 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 10651 | `	/* Start the compilation process */` |
|   13215 | 10652 | `	for(;;){` |
|   38792 | 10653 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14666 | 10654 | `			break; /* No more tokens to process */` |
|       - | 10655 | `		}` |
|   24128 | 10656 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 10657 | `			/* Compile the PHP chunk */` |
|   12374 | 10658 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   12374 | 10659 | `			if( rc == SXERR_ABORT ){` |
|      13 | 10660 | `				break;` |
|       - | 10661 | `			}` |
|   12362 | 10662 | `			continue;` |
|       - | 10663 | `		}` |
|       - | 10664 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11756 | 10665 | `		nRawObj = 0;` |
|   23510 | 10666 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 10667 | `			/* Consume the raw chunk without any processing */` |
|   11756 | 10668 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11756 | 10669 | `			if( pRawObj == 0 ){` |
|     ! 0 | 10670 | `				rc = SXERR_MEM;` |
|     ! 0 | 10671 | `				break;` |
|       - | 10672 | `			}` |
|       - | 10673 | `			/* Mark as constant and emit the load constant instruction */` |
|   11756 | 10674 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11756 | 10675 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11756 | 10676 | `			++nRawObj;` |
|   11756 | 10677 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 10678 | `		}` |
|   11756 | 10679 | `		if( nRawObj > 0 ){` |
|       - | 10680 | `			/* Emit the consume instruction */` |
|   11756 | 10681 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5877 | 10682 | `		}` |
|    7340 | 10683 | `	}` |
|    7338 | 10684 | `cleanup:` |
|   14678 | 10685 | `	SySetRelease(&aRawToken);` |
|   14678 | 10686 | `	SySetRelease(&aPhpToken);` |
|   14678 | 10687 | `	return rc;` |
|    7340 | 10688 |  |
|       - | 10689 | `/*` |
|       - | 10690 | ` * Utility routines.Initialize the code generator.` |
|       - | 10691 | ` */` |
|    2902 | 10692 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 10693 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10694 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10695 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10696 | `	)` |
|       2 | 10697 |  |
|    2904 | 10698 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10699 | `	/* Zero the structure */` |
|    2904 | 10700 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 10701 | `	/* Initial state */` |
|    2904 | 10702 | `	pGen->pVm  = &(*pVm);` |
|    2904 | 10703 | `	pGen->xErr = xErr;` |
|    2904 | 10704 | `	pGen->pErrData = pErrData;` |
|    2904 | 10705 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2904 | 10706 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2904 | 10707 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    2904 | 10708 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2904 | 10709 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 10710 | `	/* Error log buffer */` |
|    2904 | 10711 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 10712 | `	/* General purpose working buffer */` |
|    2904 | 10713 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 10714 | `	/* Namespace state */` |
|    2904 | 10715 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2904 | 10716 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2904 | 10717 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2904 | 10718 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10719 | `	/* Create the global scope */` |
|    2904 | 10720 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 10721 | `	/* Point to the global scope */` |
|    2904 | 10722 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2904 | 10723 | `	return SXRET_OK;` |
|       2 | 10724 |  |
|       - | 10725 | `/*` |
|       - | 10726 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 10727 | ` */` |
|   17280 | 10728 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 10729 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10730 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10731 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10732 | `	)` |
|       2 | 10733 |  |
|   17282 | 10734 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10735 | `	GenBlock *pBlock,*pParent;` |
|       - | 10736 | `	/* Reset state */` |
|   17282 | 10737 | `	SySetReset(&pGen->aLabel);` |
|   17282 | 10738 | `	SySetReset(&pGen->aGoto);` |
|   17282 | 10739 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   17282 | 10740 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   17282 | 10741 | `	SyBlobRelease(&pGen->sWorker);` |
|   17282 | 10742 | `	SyBlobRelease(&pGen->sNamespace);` |
|   17282 | 10743 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   17282 | 10744 | `	SyHashRelease(&pGen->hUseImports);` |
|   17282 | 10745 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   17282 | 10746 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   17282 | 10747 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   17282 | 10748 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   17282 | 10749 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10750 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 10751 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 10752 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 10753 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 10754 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 10755 | `	 * number of unique names, which is acceptable. */` |
|       - | 10756 | `	/* Point to the global scope */` |
|   17282 | 10757 | `	pBlock = pGen->pCurrent;` |
|   17282 | 10758 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 10759 | `		pParent = pBlock->pParent;` |
|     ! 0 | 10760 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 10761 | `		pBlock = pParent;` |
|     ! 0 | 10762 | `	}` |
|   17282 | 10763 | `	pGen->xErr = xErr;` |
|   17282 | 10764 | `	pGen->pErrData = pErrData;` |
|   17282 | 10765 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   17282 | 10766 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   17282 | 10767 | `	pGen->pIn = pGen->pEnd = 0;` |
|   17282 | 10768 | `	pGen->nErr = 0;` |
|   17282 | 10769 | `	return SXRET_OK;` |
|       2 | 10770 |  |
|       - | 10771 | `/*` |
|       - | 10772 | ` * Generate a compile-time error message.` |
|       - | 10773 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 10774 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 10775 | ` * abort compilation immediately.` |
|       - | 10776 | ` */` |
|     554 | 10777 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 10778 |  |
|     556 | 10779 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     556 | 10780 | `	const char *zErr = "Error";` |
|       - | 10781 | `	SyString *pFile;` |
|       - | 10782 | `	va_list ap;` |
|       - | 10783 | `	sxi32 rc;` |
|       - | 10784 | `	/* Reset the working buffer */` |
|     556 | 10785 | `	SyBlobReset(pWorker);` |
|       - | 10786 | `	/* Peek the processed file path if available */` |
|     556 | 10787 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     556 | 10788 | `	if( nErrType == E_ERROR ){` |
|       - | 10789 | `		/* Increment the error counter */` |
|     454 | 10790 | `		pGen->nErr++;` |
|     454 | 10791 | `		if( pGen->nErr > 15 ){` |
|       - | 10792 | `			/* Error count limit reached */` |
|       5 | 10793 | `			if( pGen->xErr ){` |
|       5 | 10794 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 10795 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 10796 | `				if( pFile ){` |
|       5 | 10797 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 10798 | `				}` |
|       5 | 10799 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 10800 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 10801 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 10802 | `				}` |
|       2 | 10803 | `			}` |
|       - | 10804 | `			/* Abort immediately */` |
|       5 | 10805 | `			return SXERR_ABORT;` |
|       - | 10806 | `		}` |
|     224 | 10807 | `	}` |
|     552 | 10808 | `	if( pGen->xErr == 0 ){` |
|       - | 10809 | `		/* No available error consumer,return immediately */` |
|       3 | 10810 | `		return SXRET_OK;` |
|       - | 10811 | `	}` |
|     549 | 10812 | `	switch(nErrType){` |
|     447 | 10813 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 10814 | `	case E_WARNING: zErr = "Warning";     break;` |
|      69 | 10815 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 10816 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 10817 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 10818 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 10819 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 10820 | `	default:` |
|     ! 0 | 10821 | `		break;` |
|       - | 10822 | `	}` |
|     549 | 10823 | `	rc = SXRET_OK;` |
|       - | 10824 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     549 | 10825 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     549 | 10826 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     549 | 10827 | `	va_start(ap,zFormat);` |
|     549 | 10828 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     549 | 10829 | `	va_end(ap);` |
|     549 | 10830 | `	if( pFile ){` |
|     549 | 10831 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     274 | 10832 | `	}` |
|       - | 10833 | `	/* Append a new line */` |
|     549 | 10834 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     549 | 10835 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 10836 | `		/* Consume the generated error message */` |
|     549 | 10837 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     274 | 10838 | `	}` |
|     549 | 10839 | `	return rc;` |
|     279 | 10840 |  |
|       - | 10841 |  |
