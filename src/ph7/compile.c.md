# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4570/5845 lines (78.19%)

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
|    3068 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3070 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    8639 |   131 | `	for(;;){` |
|   17280 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    2962 |   133 | `			iCount--; /* Decrement nesting level */` |
|    2962 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    2936 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   14346 |   140 | `		pBlock = pBlock->pParent;` |
|   14346 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1536 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  598268 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  598270 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  598270 |   162 | `	pBlock->pUserData   = pUserData;` |
|  598270 |   163 | `	pBlock->pGen        = pGen;` |
|  598270 |   164 | `	pBlock->iFlags      = iType;` |
|  598270 |   165 | `	pBlock->pParent     = 0;` |
|  598270 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  598270 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  598270 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  595466 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  595468 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  595468 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  595468 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  595468 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  595468 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  595468 |   200 | `	pGen->pCurrent = pBlock;` |
|  595468 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  288178 |   203 | `		*ppBlock = pBlock;` |
|  144088 |   204 | `	}` |
|  595468 |   205 | `	return SXRET_OK;` |
|  297735 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  595458 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  595460 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  595460 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  595460 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  595458 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  595460 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  595460 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  595460 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  595460 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  595458 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  595460 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  595460 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  595460 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  595460 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  595460 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  595460 |   244 | `	return SXRET_OK;` |
|  297731 |   245 |  |
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
|  181380 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  181382 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  181382 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  181382 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  181382 |   265 | `	return rc;` |
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
|  423634 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  423636 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  777034 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  353400 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  137608 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  215794 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   34416 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  181380 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  181380 |   298 | `		if( pInstr ){` |
|  181380 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  181380 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  181380 |   302 | `			aFix[n].nJumpType = -1;` |
|   90689 |   303 | `		}` |
|   90691 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  423636 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  161702 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  161704 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  161850 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  161702 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  161834 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  161702 |   358 | `	return SXRET_OK;` |
|   80853 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  526496 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  526498 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  526498 |   367 | `	if( pEntry == 0 ){` |
|  259762 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  266738 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  266738 |   371 | `	return SXRET_OK;` |
|  263250 |   372 |  |
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
|  259760 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  259762 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  259762 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  129880 |   387 | `	}` |
|  259762 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|   92310 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|   92312 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|   92312 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   92312 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|   92312 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|   92312 |   408 | `	return pObj;` |
|   46157 |   409 |  |
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
|   92832 |   470 | `static int GenStateFindBadNumericSeparator(` |
|       - |   471 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   472 |  |
|   92834 |   473 | `	const char *z = pRaw->zString;` |
|   92834 |   474 | `	sxu32 n = pRaw->nByte;` |
|   92834 |   475 | `	int base = 10;` |
|       - |   476 | `	sxu32 i, start;` |
|   92834 |   477 | `	if( n < 2 ) return 0;` |
|    8326 |   478 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   479 | `		base = 16;` |
|    8291 |   480 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   481 | `		base = 2;` |
|     139 |   482 | `	}` |
|   30916 |   483 | `	for( i = 0; i < n; ++i ){` |
|   22606 |   484 | `		if( z[i] != '_' ) continue;` |
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
|    8312 |   501 | `	return 0;` |
|   46418 |   502 |  |
|       - |   503 | `/*` |
|       - |   504 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   505 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   506 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   507 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   508 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   509 | ` * so callers can bail from the current construct).` |
|       - |   510 | ` */` |
|   92832 |   511 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   512 |  |
|   92834 |   513 | `	const char *zBad = 0;` |
|   92834 |   514 | `	sxu32 nBad = 0;` |
|       - |   515 | `	SyString sBad;` |
|       - |   516 | `	sxi32 rc;` |
|   92834 |   517 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   92820 |   518 | `		return SXRET_OK;` |
|       - |   519 | `	}` |
|      15 |   520 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   521 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   522 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   523 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   524 | `		return SXERR_ABORT;` |
|       - |   525 | `	}` |
|      15 |   526 | `	return SXERR_SYNTAX;` |
|   46418 |   527 |  |
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
|   92818 |   544 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   545 | `	SyMemBackend *pAlloc,` |
|       - |   546 | `	const SyString *pToken,` |
|       - |   547 | `	char *zScratch, sxu32 nScratch,` |
|       - |   548 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   549 |  |
|       - |   550 | `	sxu32 i, j;` |
|   92820 |   551 | `	int hasUnderscore = 0;` |
|       - |   552 | `	char *zBuf;` |
|   92820 |   553 | `	*pzAlloc = 0;` |
|  197852 |   554 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  105286 |   555 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   52518 |   556 | `	}` |
|   92820 |   557 | `	if( !hasUnderscore ){` |
|   92568 |   558 | `		SyStringDupPtr(pOut, pToken);` |
|   92568 |   559 | `		return SXRET_OK;` |
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
|   46411 |   576 |  |
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
|   92804 |   593 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   594 |  |
|   92806 |   595 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   92806 |   596 | `	sxu32 nIdx = 0;` |
|       - |   597 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   92806 |   598 | `	char *zAlloc = 0;` |
|       - |   599 | `	SyString sNum;` |
|       - |   600 | `	sxi32 rc;` |
|   46402 |   601 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   92806 |   602 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   92806 |   603 | `	if( rc != SXRET_OK ){` |
|      11 |   604 | `		return rc;` |
|       - |   605 | `	}` |
|  139193 |   606 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   46397 |   607 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   92796 |   608 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   609 | `		return SXERR_ABORT;` |
|       - |   610 | `	}` |
|   92796 |   611 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   612 | `		ph7_value *pObj;` |
|       - |   613 | `		sxi64 iValue;` |
|   92312 |   614 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   92312 |   615 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   92312 |   616 | `		if( pObj == 0 ){` |
|     ! 0 |   617 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   618 | `			return SXERR_ABORT;` |
|       - |   619 | `		}` |
|   92312 |   620 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   46157 |   621 | `	}else{` |
|       - |   622 | `		/* Real number */` |
|       - |   623 | `		ph7_value *pObj;` |
|       - |   624 | `		/* Reserve a new constant */` |
|     486 |   625 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     486 |   626 | `		if( pObj == 0 ){` |
|     ! 0 |   627 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   628 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   629 | `			return SXERR_ABORT;` |
|       - |   630 | `		}` |
|     486 |   631 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     486 |   632 | `		PH7_MemObjToReal(pObj);` |
|       - |   633 | `	}` |
|   92796 |   634 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   635 | `	/* Emit the load constant instruction */` |
|   92796 |   636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   637 | `	/* Node successfully compiled */` |
|   92796 |   638 | `	return SXRET_OK;` |
|   46404 |   639 |  |
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
|   60044 |   651 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   652 |  |
|   60046 |   653 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   654 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   655 | `	ph7_value *pObj;` |
|       - |   656 | `	sxu32 nIdx;` |
|   60046 |   657 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   658 | `	/* Delimit the string */` |
|   60046 |   659 | `	zIn  = pStr->zString;` |
|   60046 |   660 | `	zEnd = &zIn[pStr->nByte];` |
|   60046 |   661 | `	if( zIn >= zEnd ){` |
|       - |   662 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   663 | `		 * rather than reserving a new object each time. */` |
|     140 |   664 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     140 |   665 | `		return SXRET_OK;` |
|       - |   666 | `	}` |
|   59908 |   667 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   668 | `		/* Already processed,emit the load constant instruction` |
|       - |   669 | `		 * and return.` |
|       - |   670 | `		 */` |
|   17606 |   671 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17606 |   672 | `		return SXRET_OK;` |
|       - |   673 | `	}` |
|       - |   674 | `	/* Reserve a new constant */` |
|   42304 |   675 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   42304 |   676 | `	if( pObj == 0 ){` |
|     ! 0 |   677 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   678 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   679 | `		return SXERR_ABORT;` |
|       - |   680 | `	}` |
|   42304 |   681 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   682 | `	/* Compile the node */` |
|   42344 |   683 | `	for(;;){` |
|   84690 |   684 | `		if( zIn >= zEnd ){` |
|       - |   685 | `			/* End of input */` |
|   42304 |   686 | `			break;` |
|       - |   687 | `		}` |
|   42388 |   688 | `		zCur = zIn;` |
|  673594 |   689 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  631208 |   690 | `			zIn++;` |
|       2 |   691 | `		}` |
|   42388 |   692 | `		if( zIn > zCur ){` |
|       - |   693 | `			/* Append raw contents*/` |
|   42368 |   694 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   21183 |   695 | `		}` |
|   42388 |   696 | `		zIn++;` |
|   42388 |   697 | `		if( zIn < zEnd ){` |
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
|   42388 |   712 | `		zIn++;` |
|       2 |   713 | `	}` |
|       - |   714 | `	/* Emit the load constant instruction */` |
|   42304 |   715 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   42304 |   716 | `	if( pStr->nByte < 1024 ){` |
|       - |   717 | `		/* Install in the literal table */` |
|   42304 |   718 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   21151 |   719 | `	}` |
|       - |   720 | `	/* Node successfully compiled */` |
|   42304 |   721 | `	return SXRET_OK;` |
|   30024 |   722 |  |
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
|    1822 |   888 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1824 |   899 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   900 | `	/* Preallocate some slots */` |
|    1824 |   901 | `	SySetAlloc(&sToken,0x08);` |
|       - |   902 | `	/* Tokenize the text */` |
|    1824 |   903 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   904 | `	/* Swap delimiter */` |
|    1824 |   905 | `	pTmpIn  = pGen->pIn;` |
|    1824 |   906 | `	pTmpEnd = pGen->pEnd;` |
|    1824 |   907 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1824 |   908 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   909 | `	/* Compile the expression */` |
|    1824 |   910 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   911 | `	/* Restore token stream */` |
|    1824 |   912 | `	pGen->pIn  = pTmpIn;` |
|    1824 |   913 | `	pGen->pEnd = pTmpEnd;` |
|       - |   914 | `	/* Release the token set */` |
|    1824 |   915 | `	SySetRelease(&sToken);` |
|       - |   916 | `	/* Compilation result */` |
|    1824 |   917 | `	return rc;` |
|       2 |   918 |  |
|       - |   919 | `/*` |
|       - |   920 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   921 | ` */` |
|   17652 |   922 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   923 |  |
|       - |   924 | `	ph7_value *pConstObj;` |
|   17654 |   925 | `	sxu32 nIdx = 0;` |
|       - |   926 | `	/* Reserve a new constant */` |
|   17654 |   927 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   17654 |   928 | `	if( pConstObj == 0 ){` |
|     ! 0 |   929 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   930 | `		return 0;` |
|       - |   931 | `	}` |
|   17654 |   932 | `	(*pCount)++;` |
|   17654 |   933 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   934 | `	/* Emit the load constant instruction */` |
|   17654 |   935 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   17654 |   936 | `	return pConstObj;` |
|    8828 |   937 |  |
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
|   16362 |   976 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |   977 |  |
|   16364 |   978 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |   979 | `	const char *zIn,*zCur,*zEnd;` |
|   16364 |   980 | `	ph7_value *pObj = 0;` |
|       - |   981 | `	sxi32 iCons;` |
|       - |   982 | `	sxi32 rc;` |
|       - |   983 | `	/* Delimit the string */` |
|   16364 |   984 | `	zIn  = pStr->zString;` |
|   16364 |   985 | `	zEnd = &zIn[pStr->nByte];` |
|   16364 |   986 | `	if( zIn >= zEnd ){` |
|       - |   987 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |   988 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |   989 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |   990 | `		 */` |
|     234 |   991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     234 |   992 | `		return SXRET_OK;` |
|       - |   993 | `	}` |
|   16132 |   994 | `	zCur = 0;` |
|       - |   995 | `	/* Compile the node */` |
|   16132 |   996 | `	iCons = 0;` |
|    8976 |   997 | `	for(;;){` |
|   27180 |   998 | `		zCur = zIn;` |
|  141874 |   999 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  116518 |  1000 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      51 |  1001 | `				break;` |
|  116420 |  1002 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1726 |  1003 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     863 |  1004 | `					break;` |
|       - |  1005 | `			}` |
|  114696 |  1006 | `			zIn++;` |
|       2 |  1007 | `		}` |
|   27180 |  1008 | `		if( zIn > zCur ){` |
|   12376 |  1009 | `			if( pObj == 0 ){` |
|   12100 |  1010 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12100 |  1011 | `				if( pObj == 0 ){` |
|     ! 0 |  1012 | `					return SXERR_ABORT;` |
|       - |  1013 | `				}` |
|    6049 |  1014 | `			}` |
|   12376 |  1015 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6187 |  1016 | `		}` |
|   27180 |  1017 | `		if( zIn >= zEnd ){` |
|   16132 |  1018 | `			break;` |
|       - |  1019 | `		}` |
|   11050 |  1020 | `		if( zIn[0] == '\\' ){` |
|    9228 |  1021 | `			const char *zPtr = 0;` |
|       - |  1022 | `			sxu32 n;` |
|    9228 |  1023 | `			zIn++;` |
|    9228 |  1024 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1025 | `				break;` |
|       - |  1026 | `			}` |
|    9228 |  1027 | `			if( pObj == 0 ){` |
|    5556 |  1028 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5556 |  1029 | `				if( pObj == 0 ){` |
|     ! 0 |  1030 | `					return SXERR_ABORT;` |
|       - |  1031 | `				}` |
|    2777 |  1032 | `			}` |
|    9228 |  1033 | `			n = sizeof(char); /* size of conversion */` |
|    9228 |  1034 | `			switch( zIn[0] ){` |
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
|    4248 |  1055 | `			case 'n':` |
|       - |  1056 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8498 |  1057 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8498 |  1058 | `				break;` |
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
|    9228 |  1126 | `			zIn += n;` |
|    9228 |  1127 | `			continue;` |
|       - |  1128 | `		}` |
|    1824 |  1129 | `		if( zIn[0] == '{' ){` |
|       - |  1130 | `			/* Curly syntax */` |
|       - |  1131 | `			const char *zExpr;` |
|     101 |  1132 | `			sxi32 iNest = 1;` |
|     101 |  1133 | `			zIn++;` |
|     101 |  1134 | `			zExpr = zIn;` |
|       - |  1135 | `			/* Synchronize with the next closing curly braces */` |
|    1135 |  1136 | `			while( zIn < zEnd ){` |
|    1135 |  1137 | `				if( zIn[0] == '{' ){` |
|       - |  1138 | `					/* Increment nesting level */` |
|       9 |  1139 | `					iNest++;` |
|    1131 |  1140 | `				}else if(zIn[0] == '}' ){` |
|       - |  1141 | `					/* Decrement nesting level */` |
|     109 |  1142 | `					iNest--;` |
|     109 |  1143 | `					if( iNest <= 0 ){` |
|     101 |  1144 | `						break;` |
|       - |  1145 | `					}` |
|       4 |  1146 | `				}` |
|    1035 |  1147 | `				zIn++;` |
|       1 |  1148 | `			}` |
|       - |  1149 | `			/* Process the expression */` |
|     101 |  1150 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     101 |  1151 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1152 | `				return SXERR_ABORT;` |
|       - |  1153 | `			}` |
|     101 |  1154 | `			if( rc != SXERR_EMPTY ){` |
|     101 |  1155 | `				++iCons;` |
|      50 |  1156 | `			}` |
|     101 |  1157 | `			if( zIn < zEnd ){` |
|       - |  1158 | `				/* Jump the trailing curly */` |
|     101 |  1159 | `				zIn++;` |
|      50 |  1160 | `			}` |
|      51 |  1161 | `		}else{` |
|       - |  1162 | `			/* Simple syntax */` |
|    1724 |  1163 | `			const char *zExpr = zIn;` |
|       - |  1164 | `			/* Assemble variable name */` |
|     867 |  1165 | `			for(;;){` |
|       - |  1166 | `				/* Jump leading dollars */` |
|    3458 |  1167 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1724 |  1168 | `					zIn++;` |
|       2 |  1169 | `				}` |
|     867 |  1170 | `				for(;;){` |
|   10417 |  1171 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7816 |  1172 | `						zIn++;` |
|       2 |  1173 | `					}` |
|    1736 |  1174 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1175 | `						/* UTF-8 stream */` |
|     ! 0 |  1176 | `						zIn++;` |
|     ! 0 |  1177 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1178 | `							zIn++;` |
|     ! 0 |  1179 | `						}` |
|     ! 0 |  1180 | `						continue;` |
|       - |  1181 | `					}` |
|    1736 |  1182 | `					break;` |
|     ! 0 |  1183 | `				}` |
|    1736 |  1184 | `				if( zIn >= zEnd ){` |
|     106 |  1185 | `					break;` |
|       - |  1186 | `				}` |
|    1632 |  1187 | `				if( zIn[0] == '[' ){` |
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
|    1624 |  1205 | `				}else if(zIn[0] == '{' ){` |
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
|    1620 |  1223 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1224 | `					/* Member access operator '->' */` |
|      13 |  1225 | `					zIn += 2;` |
|    1614 |  1226 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1227 | `					/* Static member access operator '::' */` |
|     ! 0 |  1228 | `					zIn += 2;` |
|     ! 0 |  1229 | `				}else{` |
|     805 |  1230 | `					break;` |
|       - |  1231 | `				}` |
|       1 |  1232 | `			}` |
|       - |  1233 | `			/* Process the expression */` |
|    1724 |  1234 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1724 |  1235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1236 | `				return SXERR_ABORT;` |
|       - |  1237 | `			}` |
|    1724 |  1238 | `			if( rc != SXERR_EMPTY ){` |
|    1722 |  1239 | `				++iCons;` |
|     860 |  1240 | `			}` |
|       - |  1241 | `		}` |
|       - |  1242 | `		/* Invalidate the previously used constant */` |
|    1824 |  1243 | `		pObj = 0;` |
|       2 |  1244 | `	}/*for(;;)*/` |
|   16132 |  1245 | `	if( iCons > 1 ){` |
|       - |  1246 | `		/* Concatenate all compiled constants */` |
|    1376 |  1247 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     687 |  1248 | `	}` |
|       - |  1249 | `	/* Node successfully compiled */` |
|   16132 |  1250 | `	return SXRET_OK;` |
|    8183 |  1251 |  |
|       - |  1252 | `/*` |
|       - |  1253 | ` * Compile a double quoted string.` |
|       - |  1254 | ` *  See the block-comment above for more information.` |
|       - |  1255 | ` */` |
|   16302 |  1256 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1257 |  |
|       - |  1258 | `	sxi32 rc;` |
|   16304 |  1259 | `	rc = GenStateCompileString(&(*pGen));` |
|    8151 |  1260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1261 | `	/* Compilation result */` |
|   16304 |  1262 | `	return rc;` |
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
|   16564 |  1306 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   16566 |  1317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1318 | `	/* Compile the expression*/` |
|   16566 |  1319 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1320 | `	/* Restore token stream */` |
|   16566 |  1321 | `	RE_SWAP_DELIMITER(pGen);` |
|   16566 |  1322 | `	return rc;` |
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
|   24456 |  1361 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1362 |  |
|       - |  1363 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1364 | `	SyToken *pKey,*pCur;` |
|   24458 |  1365 | `	sxi32 iEmitRef = 0;` |
|   24458 |  1366 | `	sxi32 nPair = 0;` |
|       - |  1367 | `	sxi32 iNest;` |
|       - |  1368 | `	sxi32 rc;` |
|   24458 |  1369 | `	xValidator = 0;` |
|   19843 |  1370 | `	for(;;){` |
|       - |  1371 | `		/* Jump leading commas */` |
|   44822 |  1372 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5136 |  1373 | `			pGen->pIn++;` |
|       2 |  1374 | `		}` |
|   39688 |  1375 | `		pCur = pGen->pIn;` |
|   39688 |  1376 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1377 | `			/* No more entry to process */` |
|   24446 |  1378 | `			break;` |
|       - |  1379 | `		}` |
|   15244 |  1380 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1381 | `			continue;` |
|       - |  1382 | `		}` |
|       - |  1383 | `		/* Compile the key if available */` |
|   15244 |  1384 | `		pKey = pCur;` |
|   15244 |  1385 | `		iNest = 0;` |
|   42370 |  1386 | `		while( pCur < pGen->pIn ){` |
|   28348 |  1387 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 |  1388 | `				break;` |
|       - |  1389 | `			}` |
|       - |  1390 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1391 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1392 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1393 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1394 | `			 */` |
|   27132 |  1395 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      74 |  1396 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      74 |  1397 | `				SyToken *pFn = pCur;` |
|      72 |  1398 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 |  1399 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 |  1400 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1401 | `					pFn = &pCur[1];` |
|     ! 0 |  1402 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1403 | `				}` |
|      74 |  1404 | `				if( nKw == PH7_TKWRD_FN ){` |
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
|      34 |  1434 | `			}` |
|   27128 |  1435 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      78 |  1436 | `				iNest++;` |
|   27090 |  1437 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - |  1438 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - |  1439 | `				 * parser will shortly detect any syntax error.` |
|       - |  1440 | `				 */` |
|      78 |  1441 | `				iNest--;` |
|      38 |  1442 | `			}` |
|   27128 |  1443 | `			pCur++;` |
|       2 |  1444 | `		}` |
|   15244 |  1445 | `		rc = SXERR_EMPTY;` |
|   15244 |  1446 | `		if( pCur < pGen->pIn ){` |
|    1218 |  1447 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1448 | `				/* Missing value */` |
|      11 |  1449 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 |  1450 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1451 | `					return SXERR_ABORT;` |
|       - |  1452 | `				}` |
|      11 |  1453 | `				return SXRET_OK;` |
|       - |  1454 | `			}` |
|       - |  1455 | `			/* Compile the expression holding the key */` |
|    1208 |  1456 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1457 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1208 |  1458 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1459 | `				return SXERR_ABORT;` |
|       - |  1460 | `			}` |
|    1208 |  1461 | `			pCur++; /* Jump the '=>' operator */` |
|   14631 |  1462 | `		}else if( pKey == pCur ){` |
|       - |  1463 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1464 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1465 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1466 | `		}else{` |
|       - |  1467 | `			/* Reset back the cursor and point to the entry value */` |
|   14028 |  1468 | `			pCur = pKey;` |
|       - |  1469 | `		}` |
|   15234 |  1470 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1471 | `			/* No available key,load NULL */` |
|   14030 |  1472 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7014 |  1473 | `		}` |
|   15234 |  1474 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1475 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 |  1476 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 |  1477 | `			iEmitRef = 1;` |
|      34 |  1478 | `			pCur++; /* Jump the '&' token */` |
|      34 |  1479 | `			if( pCur >= pGen->pIn ){` |
|       - |  1480 | `				/* Missing value */` |
|       3 |  1481 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1482 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1483 | `					return SXERR_ABORT;` |
|       - |  1484 | `				}` |
|       3 |  1485 | `				return SXRET_OK;` |
|       - |  1486 | `			}` |
|      15 |  1487 | `		}` |
|       - |  1488 | `		/* Compile indice value */` |
|   15232 |  1489 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15232 |  1490 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1491 | `			return SXERR_ABORT;` |
|       - |  1492 | `		}` |
|   15232 |  1493 | `		if( iEmitRef ){` |
|       - |  1494 | `			/* Emit the load reference instruction */` |
|      32 |  1495 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1496 | `		}` |
|   15232 |  1497 | `		xValidator = 0;` |
|   15232 |  1498 | `		iEmitRef = 0;` |
|   15232 |  1499 | `		nPair++;` |
|       2 |  1500 | `	}` |
|       - |  1501 | `	/* Emit the load map instruction */` |
|   24446 |  1502 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1503 | `	/* Node successfully compiled */` |
|   24446 |  1504 | `	return SXRET_OK;` |
|   12230 |  1505 |  |
|       - |  1506 | `/*` |
|       - |  1507 | ` * Compile the 'array' language construct.` |
|       - |  1508 | ` *	 According to the PHP language reference manual` |
|       - |  1509 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1510 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1511 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1512 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1513 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1514 | ` */` |
|   24172 |  1515 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1516 |  |
|       - |  1517 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   24174 |  1518 | `	pGen->pIn += 2;` |
|   24174 |  1519 | `	pGen->pEnd--;` |
|   12086 |  1520 | `	SXUNUSED(iCompileFlag);` |
|   24174 |  1521 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1522 |  |
|       - |  1523 | `/*` |
|       - |  1524 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1525 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1526 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1527 | ` */` |
|     284 |  1528 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1529 |  |
|       - |  1530 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     286 |  1531 | `	pGen->pIn++;` |
|     286 |  1532 | `	pGen->pEnd--;` |
|     142 |  1533 | `	SXUNUSED(iCompileFlag);` |
|     286 |  1534 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1535 |  |
|       - |  1536 | `/*` |
|       - |  1537 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1538 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1539 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1540 | ` * error message.` |
|       - |  1541 | ` * See the routine responible of compiling the list language construct` |
|       - |  1542 | ` * for more inforation.` |
|       - |  1543 | ` */` |
|     128 |  1544 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1545 |  |
|     130 |  1546 | `	sxi32 rc = SXRET_OK;` |
|     130 |  1547 | `	if( pRoot->pOp ){` |
|     ! 0 |  1548 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 |  1549 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1550 | `				/* Unexpected expression */` |
|     ! 0 |  1551 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1552 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1553 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1554 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1555 | `				}` |
|     ! 0 |  1556 | `		}` |
|     130 |  1557 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1558 | `		/* Unexpected expression */` |
|       5 |  1559 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1560 | `			"list(): Expecting a variable not an expression");` |
|       5 |  1561 | `		if( rc != SXERR_ABORT ){` |
|       5 |  1562 | `			rc = SXERR_INVALID;` |
|       2 |  1563 | `		}` |
|       2 |  1564 | `	}` |
|     130 |  1565 | `	return rc;` |
|       2 |  1566 |  |
|       - |  1567 | `/*` |
|       - |  1568 | ` * Compile the 'list' language construct.` |
|       - |  1569 | ` *  According to the PHP language reference` |
|       - |  1570 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1571 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1572 | ` *  Description` |
|       - |  1573 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1574 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1575 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1576 | ` *  Parameters` |
|       - |  1577 | ` *   $varname: A variable.` |
|       - |  1578 | ` *  Return Values` |
|       - |  1579 | ` *   The assigned array.` |
|       - |  1580 | ` */` |
|       - |  1581 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1582 | `struct NestedListEntry {` |
|       - |  1583 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1584 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1585 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1586 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1587 | `};` |
|       - |  1588 | `/*` |
|       - |  1589 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1590 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1591 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1592 | ` */` |
|      74 |  1593 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 |  1594 |  |
|       - |  1595 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1596 | `	SyToken *pNext;` |
|       - |  1597 | `	sxi32 nExpr;` |
|       - |  1598 | `	sxi32 rc;` |
|      76 |  1599 | `	nExpr = 0;` |
|      76 |  1600 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 |  1601 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 |  1602 | `		if( pGen->pIn < pNext ){` |
|       - |  1603 | `			/* Check for nested list() */` |
|     144 |  1604 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1605 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1606 | `				/* Record this nested list for post-processing */` |
|       3 |  1607 | `				SyToken *pListEnd = 0;` |
|       3 |  1608 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1609 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1610 | `				}` |
|       3 |  1611 | `				if( pListEnd ){` |
|       - |  1612 | `					struct NestedListEntry sEntry;` |
|       3 |  1613 | `					sEntry.nIndex = nExpr;` |
|       3 |  1614 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1615 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1616 | `					sEntry.isShort = 0;` |
|       3 |  1617 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1618 | `				}` |
|       - |  1619 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1620 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 |  1621 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1622 | `				/* Nested short destructuring [...] */` |
|      13 |  1623 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1624 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1625 | `				if( pBracketEnd ){` |
|       - |  1626 | `					struct NestedListEntry sEntry;` |
|      13 |  1627 | `					sEntry.nIndex = nExpr;` |
|      13 |  1628 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1629 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1630 | `					sEntry.isShort = 1;` |
|      13 |  1631 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1632 | `				}` |
|       - |  1633 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1634 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1635 | `			}else{` |
|       - |  1636 | `				/* Compile the expression holding the variable */` |
|     130 |  1637 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 |  1638 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1639 | `					SySetRelease(&sNested);` |
|     ! 0 |  1640 | `					return SXRET_OK;` |
|       - |  1641 | `				}` |
|       - |  1642 | `			}` |
|      73 |  1643 | `		}else{` |
|       - |  1644 | `			/* Empty entry,load NULL */` |
|      13 |  1645 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1646 | `		}` |
|     156 |  1647 | `		nExpr++;` |
|       - |  1648 | `		/* Advance the stream cursor */` |
|     156 |  1649 | `		pGen->pIn = &pNext[1];` |
|       2 |  1650 | `	}` |
|       - |  1651 | `	/* Emit the LOAD_LIST instruction */` |
|      76 |  1652 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1653 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1654 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1655 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1656 | `	 */` |
|      76 |  1657 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1658 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1659 | `		sxu32 i;` |
|      27 |  1660 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1661 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1662 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1663 | `			ph7_value *pIdx;` |
|       - |  1664 | `			sxu32 nConstIdx;` |
|       - |  1665 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1666 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1667 | `			/* Push the integer index for this nested entry */` |
|      15 |  1668 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1669 | `			if( pIdx == 0 ){` |
|     ! 0 |  1670 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1671 | `				SySetRelease(&sNested);` |
|     ! 0 |  1672 | `				return SXERR_ABORT;` |
|       - |  1673 | `			}` |
|      15 |  1674 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1675 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1676 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1677 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1678 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1679 | `			 */` |
|      15 |  1680 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1681 | `			/* Recursively compile the inner list */` |
|      15 |  1682 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1683 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1684 | `			if( apNested[i].isShort ){` |
|      13 |  1685 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1686 | `			}else{` |
|       3 |  1687 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1688 | `			}` |
|      15 |  1689 | `			pGen->pIn = pSavedIn;` |
|      15 |  1690 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1691 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1692 | `				SySetRelease(&sNested);` |
|     ! 0 |  1693 | `				return SXERR_ABORT;` |
|       - |  1694 | `			}` |
|       - |  1695 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1696 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1697 | `		}` |
|       6 |  1698 | `	}` |
|      76 |  1699 | `	SySetRelease(&sNested);` |
|       - |  1700 | `	/* Node successfully compiled */` |
|      76 |  1701 | `	return SXRET_OK;` |
|      39 |  1702 |  |
|      32 |  1703 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1704 |  |
|       - |  1705 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 |  1706 | `	pGen->pIn += 2;` |
|      34 |  1707 | `	pGen->pEnd--;` |
|      16 |  1708 | `	SXUNUSED(iCompileFlag);` |
|      34 |  1709 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1710 |  |
|      42 |  1711 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1712 |  |
|       - |  1713 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 |  1714 | `	pGen->pIn++;` |
|      44 |  1715 | `	pGen->pEnd--;` |
|      21 |  1716 | `	SXUNUSED(iCompileFlag);` |
|      44 |  1717 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1718 |  |
|       - |  1719 | `/* Forward declarations */` |
|       - |  1720 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1721 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1722 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1723 | `/*` |
|       - |  1724 | ` * Compile an annoynmous function or a closure.` |
|       - |  1725 | ` * According to the PHP language reference` |
|       - |  1726 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1727 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1728 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1729 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1730 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1731 | ` *  Example Anonymous function variable assignment example` |
|       - |  1732 | ` * <?php` |
|       - |  1733 | ` * $greet = function($name)` |
|       - |  1734 | ` * {` |
|       - |  1735 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1736 | ` * };` |
|       - |  1737 | ` * $greet('World');` |
|       - |  1738 | ` * $greet('PHP');` |
|       - |  1739 | ` * ?>` |
|       - |  1740 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1741 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1742 | ` */` |
|     168 |  1743 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1744 |  |
|       - |  1745 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1746 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1747 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1748 | `							  * one thread is allowed to compile the script.` |
|       - |  1749 | `						      */` |
|       - |  1750 | `	ph7_value *pObj;` |
|       - |  1751 | `	SyString sName;` |
|       - |  1752 | `	sxu32 nIdx;` |
|       - |  1753 | `	sxu32 nLen;` |
|       - |  1754 | `	sxi32 rc;` |
|       - |  1755 |  |
|     170 |  1756 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     170 |  1757 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1758 | `		pGen->pIn++;` |
|     ! 0 |  1759 | `	}` |
|       - |  1760 | `	/* Reserve a constant for the lambda */` |
|     170 |  1761 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     170 |  1762 | `	if( pObj == 0 ){` |
|     ! 0 |  1763 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1764 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1765 | `		return SXERR_ABORT;` |
|       - |  1766 | `	}` |
|       - |  1767 | `	/* Generate a unique name */` |
|     170 |  1768 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1769 | `	/* Make sure the generated name is unique */` |
|     170 |  1770 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1771 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1772 | `	}` |
|     170 |  1773 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     170 |  1774 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1775 | `	/* Compile the lambda body */` |
|     170 |  1776 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     170 |  1777 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1778 | `		return SXERR_ABORT;` |
|       - |  1779 | `	}` |
|     170 |  1780 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1781 | `		/* Emit the load closure instruction */` |
|      16 |  1782 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 |  1783 | `	}else{` |
|       - |  1784 | `		/* Emit the load constant instruction */` |
|     156 |  1785 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1786 | `	}` |
|       - |  1787 | `	/* Node successfully compiled */` |
|     170 |  1788 | `	return SXRET_OK;` |
|      86 |  1789 |  |
|       - |  1790 | `/*` |
|       - |  1791 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1792 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1793 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1794 | ` */` |
|     120 |  1795 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1796 | `	ph7_gen_state *pGen,` |
|       - |  1797 | `	ph7_vm_func *pFunc,` |
|       - |  1798 | `	const char *zName,` |
|       - |  1799 | `	sxu32 nByte,` |
|       - |  1800 | `	SyString *aShadow,` |
|       - |  1801 | `	sxu32 nShadow)` |
|       1 |  1802 |  |
|       - |  1803 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1804 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1805 | `	sxu32 n, nEnv;` |
|       - |  1806 | `	char *zDup;` |
|     121 |  1807 | `	if( nByte == 0 ){` |
|     ! 0 |  1808 | `		return SXRET_OK;` |
|       - |  1809 | `	}` |
|     120 |  1810 | `	if( nByte == sizeof("this")-1` |
|      65 |  1811 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1812 | `		return SXRET_OK;` |
|       - |  1813 | `	}` |
|     145 |  1814 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      92 |  1815 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      88 |  1816 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      67 |  1817 | `			return SXRET_OK;` |
|       - |  1818 | `		}` |
|      14 |  1819 | `	}` |
|      53 |  1820 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1821 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1822 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1823 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1824 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1825 | `			return SXRET_OK;` |
|       - |  1826 | `		}` |
|      15 |  1827 | `	}` |
|      53 |  1828 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1829 | `	if( zDup == 0 ){` |
|     ! 0 |  1830 | `		return SXERR_ABORT;` |
|       - |  1831 | `	}` |
|      53 |  1832 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1833 | `	sEnv.iFlags = 0;` |
|      53 |  1834 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1835 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1836 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1837 | `	return SXRET_OK;` |
|      61 |  1838 |  |
|       - |  1839 | `/*` |
|       - |  1840 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1841 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1842 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1843 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1844 | ` */` |
|      14 |  1845 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1846 | `	ph7_gen_state *pGen,` |
|       - |  1847 | `	ph7_vm_func *pFunc,` |
|       - |  1848 | `	const char *zIn,` |
|       - |  1849 | `	const char *zEnd,` |
|       - |  1850 | `	SyString *aShadow,` |
|       - |  1851 | `	sxu32 nShadow)` |
|       1 |  1852 |  |
|       - |  1853 | `	sxi32 rc;` |
|     159 |  1854 | `	while( zIn < zEnd ){` |
|     145 |  1855 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1856 | `			zIn++;` |
|     ! 0 |  1857 | `			if( zIn < zEnd ){` |
|     ! 0 |  1858 | `				zIn++;` |
|     ! 0 |  1859 | `			}` |
|     ! 0 |  1860 | `			continue;` |
|       - |  1861 | `		}` |
|     144 |  1862 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1863 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1864 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1865 | `			const char *zName;` |
|      13 |  1866 | `			zIn++; /* skip '$' */` |
|      13 |  1867 | `			zName = zIn;` |
|      39 |  1868 | `			while( zIn < zEnd ){` |
|      35 |  1869 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1870 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1871 | `					zIn++;` |
|     ! 0 |  1872 | `					while( zIn < zEnd` |
|     ! 0 |  1873 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1874 | `						zIn++;` |
|     ! 0 |  1875 | `					}` |
|     ! 0 |  1876 | `					continue;` |
|       - |  1877 | `				}` |
|      35 |  1878 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1879 | `					break;` |
|       - |  1880 | `				}` |
|      27 |  1881 | `				zIn++;` |
|       1 |  1882 | `			}` |
|      13 |  1883 | `			if( zIn > zName ){` |
|      19 |  1884 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1885 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1886 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1887 | `					return SXERR_ABORT;` |
|       - |  1888 | `				}` |
|       6 |  1889 | `			}` |
|      13 |  1890 | `			continue;` |
|       - |  1891 | `		}` |
|     133 |  1892 | `		zIn++;` |
|       1 |  1893 | `	}` |
|      15 |  1894 | `	return SXRET_OK;` |
|       8 |  1895 |  |
|       - |  1896 | `/*` |
|       - |  1897 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1898 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1899 | ` *   - plain $<id> pairs` |
|       - |  1900 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1901 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1902 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1903 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1904 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1905 | ` *     are never mistakenly captured.` |
|       - |  1906 | ` */` |
|     102 |  1907 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1908 | `	ph7_gen_state *pGen,` |
|       - |  1909 | `	ph7_vm_func *pFunc,` |
|       - |  1910 | `	SyToken *pStart,` |
|       - |  1911 | `	SyToken *pEnd,` |
|       - |  1912 | `	SyString *aShadow,` |
|       - |  1913 | `	sxu32 nShadow)` |
|       1 |  1914 |  |
|     103 |  1915 | `	SyToken *pScan = pStart;` |
|       - |  1916 | `	sxi32 rc;` |
|     371 |  1917 | `	while( pScan < pEnd ){` |
|     269 |  1918 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1919 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1920 | `				pScan->sData.zString,` |
|      14 |  1921 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1922 | `				aShadow,nShadow);` |
|      15 |  1923 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1924 | `				return SXERR_ABORT;` |
|       - |  1925 | `			}` |
|      15 |  1926 | `			pScan++;` |
|      15 |  1927 | `			continue;` |
|       - |  1928 | `		}` |
|     255 |  1929 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      19 |  1930 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      19 |  1931 | `			SyToken *pFnKw = pScan;` |
|      18 |  1932 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  1933 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  1934 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1935 | `				pFnKw = &pScan[1];` |
|     ! 0 |  1936 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1937 | `			}` |
|      19 |  1938 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  1939 | `				SyToken *pInnerSigStart;` |
|       - |  1940 | `				SyToken *pInnerSigEnd;` |
|       - |  1941 | `				SyToken *pInnerBodyEnd;` |
|       - |  1942 | `				SyString *aInnerShadow;` |
|       - |  1943 | `				sxu32 nInnerShadow;` |
|       - |  1944 | `				sxu32 nInnerParamMax;` |
|       - |  1945 | `				SyToken *p;` |
|       - |  1946 | `				int iNestInner;` |
|      19 |  1947 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  1948 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1949 | `					pScan++;` |
|     ! 0 |  1950 | `				}` |
|      19 |  1951 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  1952 | `					pScan++;` |
|     ! 0 |  1953 | `					continue;` |
|       - |  1954 | `				}` |
|      19 |  1955 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  1956 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  1957 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  1958 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  1959 | `					pScan = pEnd;` |
|     ! 0 |  1960 | `					continue;` |
|       - |  1961 | `				}` |
|       - |  1962 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  1963 | `				nInnerParamMax = 0;` |
|      57 |  1964 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  1965 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  1966 | `						nInnerParamMax++;` |
|       6 |  1967 | `					}` |
|      20 |  1968 | `				}` |
|      19 |  1969 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  1970 | `					&pGen->pVm->sAllocator,` |
|      18 |  1971 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  1972 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  1973 | `					return SXERR_ABORT;` |
|       - |  1974 | `				}` |
|      19 |  1975 | `				nInnerShadow = 0;` |
|      25 |  1976 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  1977 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  1978 | `				}` |
|      57 |  1979 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  1980 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  1981 | `						continue;` |
|       - |  1982 | `					}` |
|      13 |  1983 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  1984 | `						break;` |
|       - |  1985 | `					}` |
|      13 |  1986 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  1987 | `						continue;` |
|       - |  1988 | `					}` |
|      13 |  1989 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  1990 | `				}` |
|      19 |  1991 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  1992 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1993 | `					pScan++;` |
|     ! 0 |  1994 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  1995 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  1996 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  1997 | `						pScan++;` |
|     ! 0 |  1998 | `					}` |
|     ! 0 |  1999 | `					if( pScan < pEnd` |
|     ! 0 |  2000 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2001 | `						pScan++;` |
|     ! 0 |  2002 | `					}` |
|     ! 0 |  2003 | `				}` |
|      19 |  2004 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2005 | `					pScan++; /* past '=>' */` |
|       9 |  2006 | `				}` |
|      19 |  2007 | `				pInnerBodyEnd = pScan;` |
|      19 |  2008 | `				iNestInner = 0;` |
|     131 |  2009 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2010 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2011 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2012 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2013 | `						break;` |
|       - |  2014 | `					}` |
|     113 |  2015 | `					if( pInnerBodyEnd->nType &` |
|       - |  2016 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2017 | `						iNestInner++;` |
|     112 |  2018 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2019 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2020 | `						iNestInner--;` |
|       1 |  2021 | `					}` |
|     113 |  2022 | `					pInnerBodyEnd++;` |
|       1 |  2023 | `				}` |
|       - |  2024 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2025 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2026 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2027 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2028 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2029 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2030 | `				 *` |
|       - |  2031 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2032 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2033 | `				 * range after the '=' sign. */` |
|       - |  2034 | `				{` |
|      19 |  2035 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2036 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2037 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2038 | `						SyToken *pEq = 0;` |
|      13 |  2039 | `						int iNestArg = 0;` |
|      49 |  2040 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2041 | `							if( iNestArg == 0` |
|      39 |  2042 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2043 | `								break;` |
|       - |  2044 | `							}` |
|      37 |  2045 | `							if( pArgEnd->nType &` |
|       - |  2046 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2047 | `								iNestArg++;` |
|      37 |  2048 | `							}else if( pArgEnd->nType &` |
|       - |  2049 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2050 | `								iNestArg--;` |
|     ! 0 |  2051 | `							}` |
|      36 |  2052 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2053 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2054 | `								pEq = pArgEnd;` |
|       3 |  2055 | `							}` |
|      37 |  2056 | `							pArgEnd++;` |
|       1 |  2057 | `						}` |
|      13 |  2058 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2059 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2060 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2061 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2062 | `								return SXERR_ABORT;` |
|       - |  2063 | `							}` |
|       3 |  2064 | `						}` |
|      13 |  2065 | `						pArgStart = pArgEnd;` |
|      12 |  2066 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2067 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2068 | `							pArgStart++;` |
|       1 |  2069 | `						}` |
|       1 |  2070 | `					}` |
|       - |  2071 | `				}` |
|      28 |  2072 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2073 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2074 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2075 | `					return SXERR_ABORT;` |
|       - |  2076 | `				}` |
|      19 |  2077 | `				pScan = pInnerBodyEnd;` |
|      19 |  2078 | `				continue;` |
|       - |  2079 | `			}` |
|     ! 0 |  2080 | `		}` |
|     237 |  2081 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     129 |  2082 | `			pScan++;` |
|     129 |  2083 | `			continue;` |
|       - |  2084 | `		}` |
|       - |  2085 | `		{` |
|       - |  2086 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     109 |  2087 | `			SyToken *pDollar = pScan;` |
|     162 |  2088 | `			while( &pDollar[1] < pEnd` |
|     109 |  2089 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2090 | `				pDollar++;` |
|     ! 0 |  2091 | `			}` |
|     109 |  2092 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2093 | `				break;` |
|       - |  2094 | `			}` |
|     109 |  2095 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2096 | `				pScan = pDollar + 1;` |
|     ! 0 |  2097 | `				continue;` |
|       - |  2098 | `			}` |
|     163 |  2099 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     108 |  2100 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      54 |  2101 | `				aShadow,nShadow);` |
|     109 |  2102 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2103 | `				return SXERR_ABORT;` |
|       - |  2104 | `			}` |
|     109 |  2105 | `			pScan = pDollar + 2;` |
|       - |  2106 | `		}` |
|       1 |  2107 | `	}` |
|     103 |  2108 | `	return SXRET_OK;` |
|      52 |  2109 |  |
|       - |  2110 | `/*` |
|       - |  2111 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2112 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2113 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2114 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2115 | ` * $this is also made available.` |
|       - |  2116 | ` */` |
|      84 |  2117 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2118 |  |
|       - |  2119 | `	ph7_vm_func *pFunc;` |
|       - |  2120 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2121 | `	GenBlock *pBlock;` |
|       - |  2122 | `	SySet *pInstrContainer;` |
|       - |  2123 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2124 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2125 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2126 | `	SyToken *pSavedEnd;` |
|       - |  2127 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2128 | `	char zName[512];` |
|       - |  2129 | `	static int iCnt = 1;` |
|       - |  2130 | `	char *zDup;` |
|       - |  2131 | `	sxu32 nLen;` |
|       - |  2132 | `	sxu32 nLine;` |
|      86 |  2133 | `	sxi32 iFlags = 0;` |
|      86 |  2134 | `	int bStatic = 0;` |
|       - |  2135 | `	sxi32 rc;` |
|       - |  2136 | `	sxu32 n;` |
|      42 |  2137 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2138 |  |
|      86 |  2139 | `	nLine = pGen->pIn->nLine;` |
|       - |  2140 | `	/* Optional 'static' prefix */` |
|      84 |  2141 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      86 |  2142 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2143 | `		bStatic = 1;` |
|       3 |  2144 | `		pGen->pIn++;` |
|       1 |  2145 | `	}` |
|       - |  2146 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      84 |  2147 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      86 |  2148 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2149 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2150 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2151 | `		return SXERR_SYNTAX;` |
|       - |  2152 | `	}` |
|      86 |  2153 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2154 | `	/* Optional '&' — return by reference */` |
|      86 |  2155 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2156 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2157 | `		pGen->pIn++;` |
|     ! 0 |  2158 | `	}` |
|       - |  2159 | `	/* Expect '(' */` |
|      86 |  2160 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2161 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2162 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2163 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2164 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2165 | `		}else{` |
|     ! 0 |  2166 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2167 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2168 | `		}` |
|       3 |  2169 | `		return SXERR_SYNTAX;` |
|       - |  2170 | `	}` |
|      84 |  2171 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2172 | `	/* Delimit the parameter list */` |
|      84 |  2173 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      84 |  2174 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2175 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2176 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2177 | `		return SXERR_SYNTAX;` |
|       - |  2178 | `	}` |
|       - |  2179 | `	/* Allocate the function state */` |
|      82 |  2180 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      82 |  2181 | `	if( pFunc == 0 ){` |
|     ! 0 |  2182 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2183 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2184 | `		return SXERR_ABORT;` |
|       - |  2185 | `	}` |
|       - |  2186 | `	/* Generate a unique lambda name */` |
|      82 |  2187 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     166 |  2188 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      85 |  2189 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 |  2190 | `	}` |
|      82 |  2191 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      82 |  2192 | `	if( zDup == 0 ){` |
|     ! 0 |  2193 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2194 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2195 | `		return SXERR_ABORT;` |
|       - |  2196 | `	}` |
|      82 |  2197 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2198 | `	/* Collect function arguments */` |
|      82 |  2199 | `	if( pGen->pIn < pSigEnd ){` |
|      52 |  2200 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd);` |
|      52 |  2201 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2202 | `			return SXERR_ABORT;` |
|       - |  2203 | `		}` |
|      25 |  2204 | `	}` |
|       - |  2205 | `	/* Point past ')' and parse optional return type */` |
|      82 |  2206 | `	pGen->pIn = &pSigEnd[1];` |
|      82 |  2207 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      82 |  2208 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2209 | `		return SXERR_ABORT;` |
|      82 |  2210 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2211 | `		return SXERR_SYNTAX;` |
|       - |  2212 | `	}` |
|       - |  2213 | `	/* Expect '=>' */` |
|      82 |  2214 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2215 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2216 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2217 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2218 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2219 | `		}else{` |
|     ! 0 |  2220 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2221 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2222 | `		}` |
|       3 |  2223 | `		return SXERR_SYNTAX;` |
|       - |  2224 | `	}` |
|      79 |  2225 | `	pGen->pIn++; /* Jump '=>' */` |
|      79 |  2226 | `	pBodyStart = pGen->pIn;` |
|      79 |  2227 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2228 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2229 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2230 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2231 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      79 |  2232 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2233 | `	{` |
|      79 |  2234 | `		SyString *aShadow = 0;` |
|      79 |  2235 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      79 |  2236 | `		if( nShadow > 0 ){` |
|      49 |  2237 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      48 |  2238 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      49 |  2239 | `			if( aShadow == 0 ){` |
|     ! 0 |  2240 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2241 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2242 | `				return SXERR_ABORT;` |
|       - |  2243 | `			}` |
|     103 |  2244 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      55 |  2245 | `				aShadow[n] = aArgs[n].sName;` |
|      28 |  2246 | `			}` |
|      24 |  2247 | `		}` |
|     118 |  2248 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      39 |  2249 | `			aShadow,nShadow);` |
|      79 |  2250 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2251 | `			return SXERR_ABORT;` |
|       - |  2252 | `		}` |
|       - |  2253 | `	}` |
|       - |  2254 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2255 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2256 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2257 | `	 * $this. */` |
|      79 |  2258 | `	if( !bStatic ){` |
|       - |  2259 | `		char *zThisDup;` |
|      77 |  2260 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      77 |  2261 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2262 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2263 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2264 | `			return SXERR_ABORT;` |
|       - |  2265 | `		}` |
|      77 |  2266 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      77 |  2267 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      77 |  2268 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      77 |  2269 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      77 |  2270 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      38 |  2271 | `	}` |
|       - |  2272 | `	/* Arrow functions are always closures */` |
|      79 |  2273 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2274 | `	/* Compile the body expression as an implicit return */` |
|     118 |  2275 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      39 |  2276 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      79 |  2277 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2278 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2279 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2280 | `		return SXERR_ABORT;` |
|       - |  2281 | `	}` |
|      79 |  2282 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      79 |  2283 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      79 |  2284 | `	pSavedEnd = pGen->pEnd;` |
|      79 |  2285 | `	pGen->pIn = pBodyStart;` |
|      79 |  2286 | `	pGen->pEnd = pBodyEnd;` |
|      79 |  2287 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      79 |  2288 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2289 | `		return SXERR_ABORT;` |
|       - |  2290 | `	}` |
|       - |  2291 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack' */` |
|      79 |  2292 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      79 |  2293 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      79 |  2294 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2295 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      79 |  2296 | `	pGen->pIn = pBodyEnd;` |
|      79 |  2297 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2298 | `	/* Emit the load-closure instruction */` |
|      79 |  2299 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      79 |  2300 | `	return SXRET_OK;` |
|      44 |  2301 |  |
|       - |  2302 | `/*` |
|       - |  2303 | ` * Compile a backtick quoted string.` |
|       - |  2304 | ` */` |
|       4 |  2305 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  2306 |  |
|       - |  2307 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2308 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2309 | `	 */` |
|       7 |  2310 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2311 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2312 | `		ph7_lib_version()` |
|       - |  2313 | `		);` |
|       - |  2314 | `	/* Load NULL */` |
|       5 |  2315 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2316 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2317 | `	/* Node successfully compiled */` |
|       5 |  2318 | `	return SXRET_OK;` |
|       1 |  2319 |  |
|       - |  2320 | `/*` |
|       - |  2321 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2322 | ` * construct.` |
|       - |  2323 | ` */` |
|      72 |  2324 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2325 |  |
|       - |  2326 | `	SyString *pName;` |
|       - |  2327 | `	sxu32 nKeyID;` |
|       - |  2328 | `	sxi32 rc;` |
|       - |  2329 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      74 |  2330 | `	pName = &pGen->pIn->sData;` |
|      74 |  2331 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      74 |  2332 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      74 |  2333 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2334 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2335 | `		/* Compile arguments one after one */` |
|       9 |  2336 | `		pTmp = pGen->pEnd;` |
|       - |  2337 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2338 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2339 | `		 *  mean that the following expression is valid:` |
|       - |  2340 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2341 | `		 */` |
|       9 |  2342 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2343 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2344 | `			if( pGen->pIn < pNext ){` |
|       9 |  2345 | `				pGen->pEnd = pNext;` |
|       9 |  2346 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2347 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2348 | `					return SXERR_ABORT;` |
|       - |  2349 | `				}` |
|       9 |  2350 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2351 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2352 | `					 * without the overhead of a function call.` |
|       - |  2353 | `					 * This is a very powerful optimization that improve` |
|       - |  2354 | `					 * performance greatly.` |
|       - |  2355 | `					 */` |
|       9 |  2356 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2357 | `				}` |
|       4 |  2358 | `			}` |
|       - |  2359 | `			/* Jump trailing commas */` |
|       9 |  2360 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2361 | `				pNext++;` |
|     ! 0 |  2362 | `			}` |
|       9 |  2363 | `			pGen->pIn = pNext;` |
|       1 |  2364 | `		}` |
|       - |  2365 | `		/* Restore token stream */` |
|       9 |  2366 | `		pGen->pEnd = pTmp;` |
|       5 |  2367 | `	}else{` |
|      66 |  2368 | `		sxi32 nArg = 0;` |
|      66 |  2369 | `		sxu32 nIdx = 0;` |
|      66 |  2370 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      66 |  2371 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2372 | `			return SXERR_ABORT;` |
|      66 |  2373 | `		}else if(rc != SXERR_EMPTY ){` |
|      66 |  2374 | `			nArg = 1;` |
|      32 |  2375 | `		}` |
|      66 |  2376 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2377 | `			ph7_value *pObj;` |
|       - |  2378 | `			/* Emit the call instruction */` |
|      20 |  2379 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2380 | `			if( pObj == 0 ){` |
|     ! 0 |  2381 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2382 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2383 | `				return SXERR_ABORT;` |
|       - |  2384 | `			}` |
|      20 |  2385 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2386 | `			/* Install in the literal table */` |
|      20 |  2387 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 |  2388 | `		}` |
|       - |  2389 | `		/* Emit the call instruction */` |
|      66 |  2390 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      66 |  2391 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,0,0);` |
|       - |  2392 | `	}` |
|       - |  2393 | `	/* Node successfully compiled */` |
|      74 |  2394 | `	return SXRET_OK;` |
|      38 |  2395 |  |
|       - |  2396 | `/*` |
|       - |  2397 | ` * Compile a node holding a variable declaration.` |
|       - |  2398 | ` * According to the PHP language reference` |
|       - |  2399 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2400 | ` *  The variable name is case-sensitive.` |
|       - |  2401 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2402 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2403 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2404 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2405 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2406 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2407 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2408 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2409 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2410 | ` *  the chapter on Expressions.` |
|       - |  2411 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2412 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2413 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2414 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2415 | ` *  is being assigned (the source variable).` |
|       - |  2416 | ` */` |
|  818778 |  2417 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2418 |  |
|  818780 |  2419 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2420 | `	sxi32 iVv;` |
|       - |  2421 | `	sxi32 iP1;` |
|       - |  2422 | `	void *p3;` |
|       - |  2423 | `	sxi32 rc;` |
|  818780 |  2424 | `	iVv = -1; /* Variable variable counter */` |
| 1637570 |  2425 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  818792 |  2426 | `		pGen->pIn++;` |
|  818792 |  2427 | `		iVv++;` |
|       2 |  2428 | `	}` |
|  818780 |  2429 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2430 | `		/* Invalid variable name */` |
|     ! 0 |  2431 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2432 | `		if( rc == SXERR_ABORT ){` |
|       - |  2433 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2434 | `			return SXERR_ABORT;` |
|       - |  2435 | `		}` |
|     ! 0 |  2436 | `		return SXRET_OK;` |
|       - |  2437 | `	}` |
|  818780 |  2438 | `	p3  = 0;` |
|  818780 |  2439 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2440 | `		/* Dynamic variable creation */` |
|      18 |  2441 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 |  2442 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 |  2443 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2444 | `			/* Empty expression */` |
|       3 |  2445 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2446 | `			return SXRET_OK;` |
|       - |  2447 | `		}` |
|       - |  2448 | `		/* Compile the expression holding the variable name */` |
|      16 |  2449 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2450 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2451 | `			return SXERR_ABORT;` |
|      16 |  2452 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2453 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2454 | `			return SXRET_OK;` |
|       - |  2455 | `		}` |
|       7 |  2456 | `	}else{` |
|       - |  2457 | `		SyHashEntry *pEntry;` |
|       - |  2458 | `		SyString *pName;` |
|  818764 |  2459 | `		char *zName = 0;` |
|       - |  2460 | `		/* Extract variable name */` |
|  818764 |  2461 | `		pName = &pGen->pIn->sData;` |
|       - |  2462 | `		/* Advance the stream cursor */` |
|  818764 |  2463 | `		pGen->pIn++;` |
|  818764 |  2464 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  818764 |  2465 | `		if( pEntry == 0 ){` |
|       - |  2466 | `			/* Duplicate name */` |
|  117728 |  2467 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  117728 |  2468 | `			if( zName == 0 ){` |
|     ! 0 |  2469 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2470 | `				return SXERR_ABORT;` |
|       - |  2471 | `			}` |
|       - |  2472 | `			/* Install in the hashtable */` |
|  117728 |  2473 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   58865 |  2474 | `		}else{` |
|       - |  2475 | `			/* Name already available */` |
|  701038 |  2476 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2477 | `		}` |
|  818764 |  2478 | `		p3 = (void *)zName;` |
|       - |  2479 | `	}` |
|  818776 |  2480 | `	iP1 = 0;` |
|  818776 |  2481 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  314594 |  2482 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2483 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  308312 |  2484 | `			iP1 = 1;` |
|  154155 |  2485 | `		}` |
|  157296 |  2486 | `	}` |
|       - |  2487 | `	/* Emit the load instruction */` |
|  818776 |  2488 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  818788 |  2489 | `	while( iVv > 0 ){` |
|      13 |  2490 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2491 | `		iVv--;` |
|       1 |  2492 | `	}` |
|       - |  2493 | `	/* Node successfully compiled */` |
|  818776 |  2494 | `	return SXRET_OK;` |
|  409391 |  2495 |  |
|       - |  2496 | `/*` |
|       - |  2497 | ` * Load a literal.` |
|       - |  2498 | ` */` |
|  549238 |  2499 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2500 |  |
|  549240 |  2501 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2502 | `	ph7_value *pObj;` |
|       - |  2503 | `	SyString *pStr;` |
|       - |  2504 | `	sxu32 nIdx;` |
|       - |  2505 | `	/* Extract token value */` |
|  549240 |  2506 | `	pStr = &pToken->sData;` |
|       - |  2507 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  549240 |  2508 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   99812 |  2509 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2510 | `			/* NULL constant are always indexed at 0 */` |
|   42464 |  2511 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   42464 |  2512 | `			return SXRET_OK;` |
|   57350 |  2513 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2514 | `			/* TRUE constant are always indexed at 1 */` |
|     496 |  2515 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     496 |  2516 | `			return SXRET_OK;` |
|       2 |  2517 | `		}` |
|  521162 |  2518 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   86610 |  2519 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2520 | `			/* FALSE constant are always indexed at 2 */` |
|   36998 |  2521 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   36998 |  2522 | `			return SXRET_OK;` |
|  450719 |  2523 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   76570 |  2524 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2525 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5608 |  2526 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5608 |  2527 | `			if( pObj == 0 ){` |
|     ! 0 |  2528 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2529 | `				return SXERR_ABORT;` |
|       - |  2530 | `			}` |
|    5608 |  2531 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2532 | `			/* Emit the load constant instruction */` |
|    5608 |  2533 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5608 |  2534 | `			return SXRET_OK;` |
|  420956 |  2535 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   28256 |  2536 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2537 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2538 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2539 | `			if( pObj == 0 ){` |
|     ! 0 |  2540 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2541 | `				return SXERR_ABORT;` |
|       - |  2542 | `			}` |
|       7 |  2543 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2544 | `				SyString sNs;` |
|       7 |  2545 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2546 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2547 | `			}else{` |
|     ! 0 |  2548 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2549 | `			}` |
|       7 |  2550 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2551 | `			return SXRET_OK;` |
|  420114 |  2552 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   11806 |  2553 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  414205 |  2554 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   14784 |  2555 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2556 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2557 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2558 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2559 | `				/* Point to the upper block */` |
|      11 |  2560 | `				pBlock = pBlock->pParent;` |
|       1 |  2561 | `			}` |
|      11 |  2562 | `			if( pBlock == 0 ){` |
|       - |  2563 | `				/* Called in the global scope,load NULL */` |
|       5 |  2564 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2565 | `			}else{` |
|       - |  2566 | `				/* Extract the target function/method */` |
|       7 |  2567 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2568 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2569 | `					/* Not a class method,Load null */` |
|       3 |  2570 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2571 | `				}else{` |
|       5 |  2572 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2573 | `					if( pObj == 0 ){` |
|     ! 0 |  2574 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2575 | `						return SXERR_ABORT;` |
|       - |  2576 | `					}` |
|       5 |  2577 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2578 | `					/* Emit the load constant instruction */` |
|       5 |  2579 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2580 | `				}` |
|       - |  2581 | `			}` |
|      11 |  2582 | `			return SXRET_OK;` |
|       - |  2583 | `	}` |
|       - |  2584 | `	/* Query literal table */` |
|  463666 |  2585 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2586 | `		ph7_value *pLitObj;` |
|       - |  2587 | `		/* Unknown literal,install it in the literal table */` |
|  217026 |  2588 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  217026 |  2589 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2590 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2591 | `			return SXERR_ABORT;` |
|       - |  2592 | `		}` |
|  217026 |  2593 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  217026 |  2594 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  108512 |  2595 | `	}` |
|       - |  2596 | `	/* Emit the load constant instruction */` |
|  463666 |  2597 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  463666 |  2598 | `	return SXRET_OK;` |
|  274621 |  2599 |  |
|       - |  2600 | `/*` |
|       - |  2601 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2602 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2603 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2604 | ` * Otherwise, load the simple literal directly.` |
|       - |  2605 | ` */` |
|  549264 |  2606 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2607 |  |
|       - |  2608 | `	sxi32 rc;` |
|  549266 |  2609 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2610 | `		return SXRET_OK;` |
|       - |  2611 | `	}` |
|       - |  2612 | `	/* Check if this is a multi-token namespace path */` |
|  549266 |  2613 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2614 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      28 |  2615 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      28 |  2616 | `		int isAbsolute = 0;` |
|      28 |  2617 | `		SyBlobReset(pWorker);` |
|       - |  2618 | `		/* Check for leading backslash (absolute path) */` |
|      28 |  2619 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      26 |  2620 | `			isAbsolute = 1;` |
|      26 |  2621 | `			pGen->pIn++; /* Skip leading backslash */` |
|      12 |  2622 | `		}` |
|       - |  2623 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      28 |  2624 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2625 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2626 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2627 | `		}` |
|       - |  2628 | `		/* Collect all path components */` |
|     108 |  2629 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     108 |  2630 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      42 |  2631 | `				SyBlobAppend(pWorker,"\\",1);` |
|      22 |  2632 | `			}else{` |
|      68 |  2633 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2634 | `			}` |
|     108 |  2635 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      28 |  2636 | `				pGen->pIn++;` |
|      28 |  2637 | `				break;` |
|       - |  2638 | `			}` |
|      82 |  2639 | `			pGen->pIn++;` |
|       2 |  2640 | `		}` |
|      28 |  2641 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2642 | `			ph7_value *pObj;` |
|       - |  2643 | `			SyString sPath;` |
|       - |  2644 | `			sxu32 nIdx;` |
|      28 |  2645 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2646 | `			/* Install in the literal table */` |
|      28 |  2647 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      16 |  2648 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      16 |  2649 | `				if( pObj == 0 ){` |
|     ! 0 |  2650 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2651 | `					return SXERR_ABORT;` |
|       - |  2652 | `				}` |
|      16 |  2653 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      16 |  2654 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       7 |  2655 | `			}` |
|       - |  2656 | `			/* Emit the load constant instruction.` |
|       - |  2657 | `			 * P1=1 means candidate for constant/function/class expansion. */` |
|      28 |  2658 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      28 |  2659 | `			return SXRET_OK;` |
|       - |  2660 | `		}` |
|     ! 0 |  2661 | `	}` |
|       - |  2662 | `	/* Single-token literal: load directly */` |
|  549240 |  2663 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  549240 |  2664 | `	return rc;` |
|  274634 |  2665 |  |
|       - |  2666 | `/*` |
|       - |  2667 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2668 | ` */` |
|  549264 |  2669 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2670 |  |
|       - |  2671 | `	sxi32 rc;` |
|  549266 |  2672 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  549266 |  2673 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2674 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2675 | `		return rc;` |
|       - |  2676 | `	}` |
|       - |  2677 | `	/* Node successfully compiled */` |
|  549266 |  2678 | `	return SXRET_OK;` |
|  274634 |  2679 |  |
|       - |  2680 | `/*` |
|       - |  2681 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  2682 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  2683 | ` */` |
|       8 |  2684 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  2685 |  |
|       - |  2686 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  2687 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  2688 | `		pGen->pIn++;` |
|       1 |  2689 | `	}` |
|       9 |  2690 | `	return SXRET_OK;` |
|       1 |  2691 |  |
|       - |  2692 | `/*` |
|       - |  2693 | ` * Check if the given identifier name is reserved or not.` |
|       - |  2694 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  2695 | ` */` |
|      56 |  2696 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 |  2697 |  |
|      58 |  2698 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 |  2699 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  2700 | `			return TRUE;` |
|      24 |  2701 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  2702 | `			return TRUE;` |
|       2 |  2703 | `		}` |
|      43 |  2704 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  2705 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  2706 | `			return TRUE;` |
|       - |  2707 | `		}` |
|     ! 0 |  2708 | `	}` |
|       - |  2709 | `	/* Not a reserved constant */` |
|      50 |  2710 | `	return FALSE;` |
|      30 |  2711 |  |
|       - |  2712 | `/*` |
|       - |  2713 | ` * Compile the 'const' statement.` |
|       - |  2714 | ` * According to the PHP language reference` |
|       - |  2715 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  2716 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  2717 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  2718 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  2719 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2720 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  2721 | ` *  Syntax` |
|       - |  2722 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  2723 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  2724 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  2725 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  2726 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  2727 | ` *  to get a list of all defined constants.` |
|       - |  2728 | ` *` |
|       - |  2729 | ` * Symisc eXtension.` |
|       - |  2730 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  2731 | ` *  would allow only simple scalar value.` |
|       - |  2732 | ` *  Example` |
|       - |  2733 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  2734 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  2735 | ` */` |
|      32 |  2736 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 |  2737 |  |
|       - |  2738 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 |  2739 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2740 | `	SyString *pName;` |
|       - |  2741 | `	sxi32 rc;` |
|      34 |  2742 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 |  2743 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  2744 | `		/* Invalid constant name */` |
|       7 |  2745 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  2746 | `		if( rc == SXERR_ABORT ){` |
|       - |  2747 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2748 | `			return SXERR_ABORT;` |
|       - |  2749 | `		}` |
|       7 |  2750 | `		goto Synchronize;` |
|       - |  2751 | `	}` |
|       - |  2752 | `	/* Peek constant name */` |
|      28 |  2753 | `	pName = &pGen->pIn->sData;` |
|       - |  2754 | `	/* Make sure the constant name isn't reserved */` |
|      28 |  2755 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  2756 | `		/* Reserved constant */` |
|       9 |  2757 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  2758 | `		if( rc == SXERR_ABORT ){` |
|       - |  2759 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2760 | `			return SXERR_ABORT;` |
|       - |  2761 | `		}` |
|       9 |  2762 | `		goto Synchronize;` |
|       - |  2763 | `	}` |
|      20 |  2764 | `	pGen->pIn++;` |
|      20 |  2765 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  2766 | `		/* Invalid statement*/` |
|       5 |  2767 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 |  2768 | `		if( rc == SXERR_ABORT ){` |
|       - |  2769 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2770 | `			return SXERR_ABORT;` |
|       - |  2771 | `		}` |
|       5 |  2772 | `		goto Synchronize;` |
|       - |  2773 | `	}` |
|      15 |  2774 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  2775 | `	/* Allocate a new constant value container */` |
|      15 |  2776 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  2777 | `	if( pConsCode == 0 ){` |
|     ! 0 |  2778 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2779 | `		return SXERR_ABORT;` |
|       - |  2780 | `	}` |
|      15 |  2781 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2782 | `	/* Swap bytecode container */` |
|      15 |  2783 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  2784 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  2785 | `	/* Compile constant value */` |
|      15 |  2786 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  2787 | `	/* Emit the done instruction */` |
|      15 |  2788 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  2789 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  2790 | `	if( rc == SXERR_ABORT ){` |
|       - |  2791 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  2792 | `		return SXERR_ABORT;` |
|       - |  2793 | `	}` |
|      15 |  2794 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  2795 | `	/* Register the constant with namespace-qualified name */` |
|       - |  2796 | `	{` |
|       - |  2797 | `		SyBlob sFQN;` |
|       - |  2798 | `		SyString sFQNStr;` |
|      15 |  2799 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  2800 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  2801 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  2802 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  2803 | `		SyBlobRelease(&sFQN);` |
|       - |  2804 | `	}` |
|      15 |  2805 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2806 | `		SySetRelease(pConsCode);` |
|     ! 0 |  2807 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  2808 | `	}` |
|      15 |  2809 | `	return SXRET_OK;` |
|       9 |  2810 | `Synchronize:` |
|       - |  2811 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 |  2812 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 |  2813 | `		pGen->pIn++;` |
|       1 |  2814 | `	}` |
|      19 |  2815 | `	return SXRET_OK;` |
|      18 |  2816 |  |
|       - |  2817 | `/*` |
|       - |  2818 | ` * Compile the 'continue' statement.` |
|       - |  2819 | ` * According to the PHP language reference` |
|       - |  2820 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  2821 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  2822 | ` *  iteration.` |
|       - |  2823 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  2824 | ` *  the purposes of continue.` |
|       - |  2825 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  2826 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  2827 | ` *  Note:` |
|       - |  2828 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  2829 | ` */` |
|       - |  2830 | `/*` |
|       - |  2831 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  2832 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  2833 | ` * break/continue crosses a try boundary.` |
|       - |  2834 | ` *` |
|       - |  2835 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  2836 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  2837 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  2838 | ` */` |
|    2930 |  2839 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  2840 |  |
|    2932 |  2841 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17126 |  2842 | `	while( pBlock && pBlock != pTarget ){` |
|   14196 |  2843 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  2844 | `			if( pBlock->pUserData ){` |
|       - |  2845 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  2846 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  2847 | `			}else{` |
|       - |  2848 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  2849 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  2850 | `				 * exception context from a sub-execution.` |
|       - |  2851 | `				 */` |
|     ! 0 |  2852 | `				break;` |
|       - |  2853 | `			}` |
|       1 |  2854 | `		}` |
|   14196 |  2855 | `		pBlock = pBlock->pParent;` |
|       2 |  2856 | `	}` |
|    2932 |  2857 |  |
|    2846 |  2858 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  2859 |  |
|       - |  2860 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  2861 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  2862 | `	sxu32 nLineLocal;` |
|       - |  2863 | `	sxi32 rc;` |
|    2848 |  2864 | `	nLineLocal = pGen->pIn->nLine;` |
|    2848 |  2865 | `	iLevel = 0;` |
|       - |  2866 | `	/* Jump the 'continue' keyword */` |
|    2848 |  2867 | `	pGen->pIn++;` |
|    2848 |  2868 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  2869 | `		/* optional numeric argument which tells us how many levels` |
|       - |  2870 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  2871 | `		 */` |
|       - |  2872 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  2873 | `		char *zAlloc = 0;` |
|       - |  2874 | `		SyString sNum;` |
|      16 |  2875 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  2876 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2877 | `			return SXERR_ABORT;` |
|       - |  2878 | `		}` |
|      16 |  2879 | `		if( rc == SXRET_OK ){` |
|      20 |  2880 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  2881 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  2882 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  2883 | `				return SXERR_ABORT;` |
|       - |  2884 | `			}` |
|      14 |  2885 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  2886 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  2887 | `		}` |
|      16 |  2888 | `		if( iLevel < 2 ){` |
|       3 |  2889 | `			iLevel = 0;` |
|       1 |  2890 | `		}` |
|      16 |  2891 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  2892 | `	}` |
|       - |  2893 | `	/* Point to the target loop */` |
|    2848 |  2894 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2848 |  2895 | `	if( pLoop == 0 ){` |
|       - |  2896 | `		/* Illegal continue */` |
|      11 |  2897 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  2898 | `		if( rc == SXERR_ABORT ){` |
|       - |  2899 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2900 | `			return SXERR_ABORT;` |
|       - |  2901 | `		}` |
|       6 |  2902 | `	}else{` |
|    2838 |  2903 | `		sxu32 nInstrIdx = 0;` |
|       - |  2904 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2838 |  2905 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2838 |  2906 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  2907 | `			/* According to the PHP language reference manual` |
|       - |  2908 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  2909 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  2910 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  2911 | `			 */` |
|       5 |  2912 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  2913 | `			if( rc == SXRET_OK ){` |
|       5 |  2914 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  2915 | `			}` |
|       3 |  2916 | `		}else{` |
|       - |  2917 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2834 |  2918 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2834 |  2919 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  2920 | `				JumpFixup sJumpFix;` |
|       - |  2921 | `				/* Post-continue */` |
|      14 |  2922 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  2923 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  2924 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  2925 | `			}` |
|       - |  2926 | `		}` |
|       - |  2927 | `	}` |
|    2848 |  2928 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  2929 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  2930 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  2931 | `	}` |
|       - |  2932 | `	/* Statement successfully compiled */` |
|    2848 |  2933 | `	return SXRET_OK;` |
|    1425 |  2934 |  |
|       - |  2935 | `/*` |
|       - |  2936 | ` * Compile the 'break' statement.` |
|       - |  2937 | ` * According to the PHP language reference` |
|       - |  2938 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  2939 | ` *  structure.` |
|       - |  2940 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  2941 | ` *  enclosing structures are to be broken out of.` |
|       - |  2942 | ` */` |
|     110 |  2943 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 |  2944 |  |
|       - |  2945 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  2946 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  2947 | `	sxi32 rc;` |
|     112 |  2948 | `	iLevel = 0;` |
|       - |  2949 | `	/* Jump the 'break' keyword */` |
|     112 |  2950 | `	pGen->pIn++;` |
|     112 |  2951 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  2952 | `		/* optional numeric argument which tells us how many levels` |
|       - |  2953 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  2954 | `		 */` |
|       - |  2955 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  2956 | `		char *zAlloc = 0;` |
|       - |  2957 | `		SyString sNum;` |
|      16 |  2958 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  2959 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2960 | `			return SXERR_ABORT;` |
|       - |  2961 | `		}` |
|      16 |  2962 | `		if( rc == SXRET_OK ){` |
|      20 |  2963 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  2964 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  2965 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  2966 | `				return SXERR_ABORT;` |
|       - |  2967 | `			}` |
|      14 |  2968 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  2969 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  2970 | `		}` |
|      16 |  2971 | `		if( iLevel < 2 ){` |
|       3 |  2972 | `			iLevel = 0;` |
|       1 |  2973 | `		}` |
|      16 |  2974 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  2975 | `	}` |
|       - |  2976 | `	/* Extract the target loop */` |
|     112 |  2977 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 |  2978 | `	if( pLoop == 0 ){` |
|       - |  2979 | `		/* Illegal break */` |
|      17 |  2980 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 |  2981 | `		if( rc == SXERR_ABORT ){` |
|       - |  2982 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2983 | `			return SXERR_ABORT;` |
|       - |  2984 | `		}` |
|       9 |  2985 | `	}else{` |
|       - |  2986 | `		sxu32 nInstrIdx;` |
|       - |  2987 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 |  2988 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 |  2989 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 |  2990 | `		if( rc == SXRET_OK ){` |
|       - |  2991 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 |  2992 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 |  2993 | `		}` |
|       - |  2994 | `	}` |
|     112 |  2995 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  2996 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  2997 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  2998 | `	}` |
|       - |  2999 | `	/* Statement successfully compiled */` |
|     112 |  3000 | `	return SXRET_OK;` |
|      57 |  3001 |  |
|       - |  3002 | `/*` |
|       - |  3003 | ` * Compile or record a label.` |
|       - |  3004 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3005 | ` * Example` |
|       - |  3006 | ` *  goto LABEL;` |
|       - |  3007 | ` *   echo 'Foo';` |
|       - |  3008 | ` *  LABEL:` |
|       - |  3009 | ` *   echo 'Bar';` |
|       - |  3010 | ` */` |
|     112 |  3011 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 |  3012 |  |
|       - |  3013 | `	GenBlock *pBlock;` |
|       - |  3014 | `	Label sLabel;` |
|       - |  3015 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 |  3016 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 |  3017 | `	if( pBlock ){` |
|       - |  3018 | `		sxi32 rc;` |
|       7 |  3019 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3020 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 |  3021 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3022 | `			return SXERR_ABORT;` |
|       - |  3023 | `		}` |
|       3 |  3024 | `	}else{` |
|     110 |  3025 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3026 | `		char *zDup;` |
|       - |  3027 | `		/* Initialize label fields */` |
|     110 |  3028 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3029 | `		/* Duplicate label name */` |
|     110 |  3030 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 |  3031 | `		if( zDup == 0 ){` |
|     ! 0 |  3032 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3033 | `			return SXERR_ABORT;` |
|       - |  3034 | `		}` |
|     110 |  3035 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 |  3036 | `		sLabel.bRef  = FALSE;` |
|     110 |  3037 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 |  3038 | `		pBlock = pGen->pCurrent;` |
|     218 |  3039 | `		while( pBlock ){` |
|     130 |  3040 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3041 | `				break;` |
|       - |  3042 | `			}` |
|       - |  3043 | `			/* Point to the upper block */` |
|     110 |  3044 | `			pBlock = pBlock->pParent;` |
|       2 |  3045 | `		}` |
|     110 |  3046 | `		if( pBlock ){` |
|      22 |  3047 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3048 | `		}else{` |
|      90 |  3049 | `			sLabel.pFunc = 0;` |
|       - |  3050 | `		}` |
|       - |  3051 | `		/* Insert in label set */` |
|     110 |  3052 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3053 | `	}` |
|     114 |  3054 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 |  3055 | `	return SXRET_OK;` |
|      58 |  3056 |  |
|       - |  3057 | `/*` |
|       - |  3058 | ` * Compile the so hated 'goto' statement.` |
|       - |  3059 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3060 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3061 | ` * a compiler it has to do this.` |
|       - |  3062 | ` * According to the PHP language reference manual` |
|       - |  3063 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3064 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3065 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3066 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3067 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3068 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3069 | ` *   of a multi-level break` |
|       - |  3070 | ` */` |
|     152 |  3071 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 |  3072 |  |
|       - |  3073 | `	JumpFixup sJump;` |
|       - |  3074 | `	sxi32 rc;` |
|     154 |  3075 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 |  3076 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3077 | `		/* Missing label */` |
|     ! 0 |  3078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3079 | `		if( rc == SXERR_ABORT ){` |
|       - |  3080 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3081 | `			return SXERR_ABORT;` |
|       - |  3082 | `		}` |
|     ! 0 |  3083 | `		return SXRET_OK;` |
|       - |  3084 | `	}` |
|     154 |  3085 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3086 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3087 | `		if( rc == SXERR_ABORT ){` |
|       - |  3088 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3089 | `			return SXERR_ABORT;` |
|       - |  3090 | `		}` |
|       3 |  3091 | `	}else{` |
|     150 |  3092 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3093 | `		GenBlock *pBlock;` |
|       - |  3094 | `		char *zDup;` |
|       - |  3095 | `		/* Prepare the jump destination */` |
|     150 |  3096 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 |  3097 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3098 | `		/* Duplicate label name */` |
|     150 |  3099 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 |  3100 | `		if( zDup == 0 ){` |
|     ! 0 |  3101 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3102 | `			return SXERR_ABORT;` |
|       - |  3103 | `		}` |
|     150 |  3104 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 |  3105 | `		pBlock = pGen->pCurrent;` |
|     312 |  3106 | `		while( pBlock ){` |
|     196 |  3107 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 |  3108 | `				break;` |
|       - |  3109 | `			}` |
|       - |  3110 | `			/* Point to the upper block */` |
|     164 |  3111 | `			pBlock = pBlock->pParent;` |
|       2 |  3112 | `		}` |
|     150 |  3113 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 |  3114 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 |  3115 | `			if( rc == SXERR_ABORT ){` |
|       - |  3116 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3117 | `				return SXERR_ABORT;` |
|       - |  3118 | `			}` |
|       3 |  3119 | `		}` |
|     150 |  3120 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 |  3121 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3122 | `		}else{` |
|     124 |  3123 | `			sJump.pFunc = 0;` |
|       - |  3124 | `		}` |
|       - |  3125 | `		/* Emit the unconditional jump */` |
|     150 |  3126 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 |  3127 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3128 | `		}` |
|       - |  3129 | `	}` |
|     154 |  3130 | `	pGen->pIn++; /* Jump the label name */` |
|     154 |  3131 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3132 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3133 | `	}` |
|       - |  3134 | `	/* Statement successfully compiled */` |
|     154 |  3135 | `	return SXRET_OK;` |
|      78 |  3136 |  |
|       - |  3137 | `/*` |
|       - |  3138 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3139 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3140 | ` * failure.` |
|       - |  3141 | ` */` |
|      20 |  3142 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3143 |  |
|       - |  3144 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3145 | `	sxu32 nRawObj;` |
|      10 |  3146 | `	sxu32 nObjIdx;` |
|       - |  3147 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3148 | `	 * a PHP block.` |
|       - |  3149 | `	 */` |
|      10 |  3150 | `Consume:` |
|      21 |  3151 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3152 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3153 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3154 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3155 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3156 | `			return SXERR_ABORT;` |
|       - |  3157 | `		}` |
|       - |  3158 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3159 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3160 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3161 | `		++nRawObj;` |
|     ! 0 |  3162 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3163 | `	}` |
|      21 |  3164 | `	if( nRawObj > 0 ){` |
|       - |  3165 | `		/* Emit the consume instruction */` |
|     ! 0 |  3166 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3167 | `	}` |
|      21 |  3168 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3169 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3170 | `		/* Reset the token set */` |
|     ! 0 |  3171 | `		SySetReset(pTokenSet);` |
|       - |  3172 | `		/* Tokenize input */` |
|     ! 0 |  3173 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3174 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3175 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3176 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3177 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3178 | `		/* Advance the stream cursor */` |
|     ! 0 |  3179 | `		pGen->pRawIn++;` |
|       - |  3180 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3181 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3182 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3183 | `			sxi32 rc;` |
|       - |  3184 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3185 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3186 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3187 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3188 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3189 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3190 | `				return SXERR_ABORT;` |
|     ! 0 |  3191 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3192 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3193 | `			}` |
|     ! 0 |  3194 | `			goto Consume;` |
|       - |  3195 | `		}` |
|     ! 0 |  3196 | `	}else{` |
|       - |  3197 | `		/* No more chunks to process */` |
|      21 |  3198 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3199 | `		return SXERR_EOF;` |
|       - |  3200 | `	}` |
|     ! 0 |  3201 | `	return SXRET_OK;` |
|      11 |  3202 |  |
|       - |  3203 | `/*` |
|       - |  3204 | ` * Compile a PHP block.` |
|       - |  3205 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3206 | ` * optionally delimited by braces {}.` |
|       - |  3207 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3208 | ` * and this function takes care of generating the appropriate error` |
|       - |  3209 | ` * message.` |
|       - |  3210 | ` */` |
|  308698 |  3211 | `static sxi32 PH7_CompileBlock(` |
|       - |  3212 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3213 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3214 | `	)` |
|       2 |  3215 |  |
|       - |  3216 | `	sxi32 rc;` |
|       - |  3217 | `	sxu32 nLine;` |
|  308700 |  3218 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  307292 |  3219 | `		nLine = pGen->pIn->nLine;` |
|  307292 |  3220 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  307292 |  3221 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3222 | `			return SXERR_ABORT;` |
|       - |  3223 | `		}` |
|  307292 |  3224 | `		pGen->pIn++;` |
|       - |  3225 | `		/* Compile until we hit the closing braces '}' */` |
|  424177 |  3226 | `		for(;;){` |
|  848356 |  3227 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3228 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3229 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3230 | `			 	   return SXERR_ABORT;` |
|       - |  3231 | `				}` |
|      21 |  3232 | `				if( rc == SXERR_EOF ){` |
|       - |  3233 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3234 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3235 | `					break;` |
|       - |  3236 | `				}` |
|     ! 0 |  3237 | `			}` |
|  848336 |  3238 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3239 | `				/* Closing braces found,break immediately*/` |
|  307272 |  3240 | `				pGen->pIn++;` |
|  307272 |  3241 | `				break;` |
|       - |  3242 | `			}` |
|       - |  3243 | `			/* Compile a single statement */` |
|  541066 |  3244 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  541066 |  3245 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3246 | `				return SXERR_ABORT;` |
|       - |  3247 | `			}` |
|       2 |  3248 | `		}` |
|  307292 |  3249 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  155055 |  3250 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3251 | `		pGen->pIn++;` |
|     ! 0 |  3252 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3253 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3254 | `			return SXERR_ABORT;` |
|       - |  3255 | `		}` |
|       - |  3256 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3257 | `		for(;;){` |
|     ! 0 |  3258 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3259 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3260 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3261 | `			 	   return SXERR_ABORT;` |
|       - |  3262 | `				}` |
|     ! 0 |  3263 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3264 | `					/* No more token to process */` |
|     ! 0 |  3265 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3266 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3267 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3268 | `					}` |
|     ! 0 |  3269 | `					break;` |
|       - |  3270 | `				}` |
|     ! 0 |  3271 | `			}` |
|     ! 0 |  3272 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3273 | `				sxi32 nKwrd;` |
|       - |  3274 | `				/* Keyword found */` |
|     ! 0 |  3275 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3276 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3277 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3278 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3279 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3280 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3281 | `						}` |
|     ! 0 |  3282 | `						break;` |
|       - |  3283 | `				}` |
|     ! 0 |  3284 | `			}` |
|       - |  3285 | `			/* Compile a single statement */` |
|     ! 0 |  3286 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3287 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3288 | `				return SXERR_ABORT;` |
|       - |  3289 | `			}` |
|     ! 0 |  3290 | `		}` |
|     ! 0 |  3291 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3292 | `	}else{` |
|       - |  3293 | `		/* Compile a single statement */` |
|    1410 |  3294 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1410 |  3295 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3296 | `			return SXERR_ABORT;` |
|       - |  3297 | `		}` |
|       - |  3298 | `	}` |
|       - |  3299 | `	/* Jump trailing semi-colons ';' */` |
|  308700 |  3300 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3301 | `		pGen->pIn++;` |
|     ! 0 |  3302 | `	}` |
|  308700 |  3303 | `	return SXRET_OK;` |
|  154351 |  3304 |  |
|       - |  3305 | `/*` |
|       - |  3306 | ` * Compile the gentle 'while' statement.` |
|       - |  3307 | ` * According to the PHP language reference` |
|       - |  3308 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3309 | ` *  The basic form of a while statement is:` |
|       - |  3310 | ` *  while (expr)` |
|       - |  3311 | ` *   statement` |
|       - |  3312 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3313 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3314 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3315 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3316 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3317 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3318 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3319 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3320 | ` *  while (expr):` |
|       - |  3321 | ` *    statement` |
|       - |  3322 | ` *   endwhile;` |
|       - |  3323 | ` */` |
|   11310 |  3324 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3325 |  |
|   11312 |  3326 | `	GenBlock *pWhileBlock = 0;` |
|   11312 |  3327 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3328 | `	sxu32 nFalseJump;` |
|       - |  3329 | `	sxu32 nLine;` |
|       - |  3330 | `	sxi32 rc;` |
|   11312 |  3331 | `	nLine = pGen->pIn->nLine;` |
|       - |  3332 | `	/* Jump the 'while' keyword */` |
|   11312 |  3333 | `	pGen->pIn++;` |
|   11312 |  3334 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3335 | `		/* Syntax error */` |
|     ! 0 |  3336 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3337 | `		if( rc == SXERR_ABORT ){` |
|       - |  3338 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3339 | `			return SXERR_ABORT;` |
|       - |  3340 | `		}` |
|     ! 0 |  3341 | `		goto Synchronize;` |
|       - |  3342 | `	}` |
|       - |  3343 | `	/* Jump the left parenthesis '(' */` |
|   11312 |  3344 | `	pGen->pIn++;` |
|       - |  3345 | `	/* Create the loop block */` |
|   11312 |  3346 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11312 |  3347 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3348 | `		return SXERR_ABORT;` |
|       - |  3349 | `	}` |
|       - |  3350 | `	/* Delimit the condition */` |
|   11312 |  3351 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11312 |  3352 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3353 | `		/* Empty expression */` |
|       3 |  3354 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3355 | `		if( rc == SXERR_ABORT ){` |
|       - |  3356 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3357 | `			return SXERR_ABORT;` |
|       - |  3358 | `		}` |
|       1 |  3359 | `	}` |
|       - |  3360 | `	/* Swap token streams */` |
|   11312 |  3361 | `	pTmp = pGen->pEnd;` |
|   11312 |  3362 | `	pGen->pEnd = pEnd;` |
|       - |  3363 | `	/* Compile the expression */` |
|   11312 |  3364 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11312 |  3365 | `	if( rc == SXERR_ABORT ){` |
|       - |  3366 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3367 | `		return SXERR_ABORT;` |
|       - |  3368 | `	}` |
|       - |  3369 | `	/* Update token stream */` |
|   11312 |  3370 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3371 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3372 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3373 | `			return SXERR_ABORT;` |
|       - |  3374 | `		}` |
|     ! 0 |  3375 | `		pGen->pIn++;` |
|     ! 0 |  3376 | `	}` |
|       - |  3377 | `	/* Synchronize pointers */` |
|   11312 |  3378 | `	pGen->pIn  = &pEnd[1];` |
|   11312 |  3379 | `	pGen->pEnd = pTmp;` |
|       - |  3380 | `	/* Emit the false jump */` |
|   11312 |  3381 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3382 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11312 |  3383 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3384 | `	/* Compile the loop body */` |
|   11312 |  3385 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11312 |  3386 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3387 | `		return SXERR_ABORT;` |
|       - |  3388 | `	}` |
|       - |  3389 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11312 |  3390 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3391 | `	/* Fix all jumps now the destination is resolved */` |
|   11312 |  3392 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3393 | `	/* Release the loop block */` |
|   11312 |  3394 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3395 | `	/* Statement successfully compiled */` |
|   11312 |  3396 | `	return SXRET_OK;` |
|     ! 0 |  3397 | `Synchronize:` |
|       - |  3398 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3399 | `	 * compiling this erroneous block.` |
|       - |  3400 | `	 */` |
|     ! 0 |  3401 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3402 | `		pGen->pIn++;` |
|     ! 0 |  3403 | `	}` |
|     ! 0 |  3404 | `	return SXRET_OK;` |
|    5657 |  3405 |  |
|       - |  3406 | `/*` |
|       - |  3407 | ` * Compile the ugly do..while() statement.` |
|       - |  3408 | ` * According to the PHP language reference` |
|       - |  3409 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3410 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3411 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3412 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3413 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3414 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3415 | ` *  would end immediately).` |
|       - |  3416 | ` *  There is just one syntax for do-while loops:` |
|       - |  3417 | ` *  <?php` |
|       - |  3418 | ` *  $i = 0;` |
|       - |  3419 | ` *  do {` |
|       - |  3420 | ` *   echo $i;` |
|       - |  3421 | ` *  } while ($i > 0);` |
|       - |  3422 | ` * ?>` |
|       - |  3423 | ` */` |
|       2 |  3424 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3425 |  |
|       3 |  3426 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3427 | `	GenBlock *pDoBlock = 0;` |
|       - |  3428 | `	sxu32 nLine;` |
|       - |  3429 | `	sxi32 rc;` |
|       3 |  3430 | `	nLine = pGen->pIn->nLine;` |
|       - |  3431 | `	/* Jump the 'do' keyword */` |
|       3 |  3432 | `	pGen->pIn++;` |
|       - |  3433 | `	/* Create the loop block */` |
|       3 |  3434 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3435 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3436 | `		return SXERR_ABORT;` |
|       - |  3437 | `	}` |
|       - |  3438 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3439 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3440 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3441 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3442 | `		return SXERR_ABORT;` |
|       - |  3443 | `	}` |
|       3 |  3444 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3445 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3446 | `	}` |
|       3 |  3447 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3448 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3449 | `			/* Missing 'while' statement */` |
|       3 |  3450 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3451 | `			if( rc == SXERR_ABORT ){` |
|       - |  3452 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3453 | `				return SXERR_ABORT;` |
|       - |  3454 | `			}` |
|       3 |  3455 | `			goto Synchronize;` |
|       - |  3456 | `	}` |
|       - |  3457 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3458 | `	pGen->pIn++;` |
|     ! 0 |  3459 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3460 | `		/* Syntax error */` |
|     ! 0 |  3461 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3462 | `		if( rc == SXERR_ABORT ){` |
|       - |  3463 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3464 | `			return SXERR_ABORT;` |
|       - |  3465 | `		}` |
|     ! 0 |  3466 | `		goto Synchronize;` |
|       - |  3467 | `	}` |
|       - |  3468 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3469 | `	pGen->pIn++;` |
|       - |  3470 | `	/* Delimit the condition */` |
|     ! 0 |  3471 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3472 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3473 | `		/* Empty expression */` |
|     ! 0 |  3474 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3475 | `		if( rc == SXERR_ABORT ){` |
|       - |  3476 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3477 | `			return SXERR_ABORT;` |
|       - |  3478 | `		}` |
|     ! 0 |  3479 | `		goto Synchronize;` |
|       - |  3480 | `	}` |
|       - |  3481 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3482 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3483 | `		JumpFixup *aPost;` |
|       - |  3484 | `		VmInstr *pInstr;` |
|       - |  3485 | `		sxu32 nJumpDest;` |
|       - |  3486 | `		sxu32 n;` |
|     ! 0 |  3487 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3488 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3489 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3490 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3491 | `			if( pInstr ){` |
|       - |  3492 | `				/* Fix */` |
|     ! 0 |  3493 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3494 | `			}` |
|     ! 0 |  3495 | `		}` |
|     ! 0 |  3496 | `	}` |
|       - |  3497 | `	/* Swap token streams */` |
|     ! 0 |  3498 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3499 | `	pGen->pEnd = pEnd;` |
|       - |  3500 | `	/* Compile the expression */` |
|     ! 0 |  3501 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3502 | `	if( rc == SXERR_ABORT ){` |
|       - |  3503 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3504 | `		return SXERR_ABORT;` |
|       - |  3505 | `	}` |
|       - |  3506 | `	/* Update token stream */` |
|     ! 0 |  3507 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3508 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3509 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3510 | `			return SXERR_ABORT;` |
|       - |  3511 | `		}` |
|     ! 0 |  3512 | `		pGen->pIn++;` |
|     ! 0 |  3513 | `	}` |
|     ! 0 |  3514 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3515 | `	pGen->pEnd = pTmp;` |
|       - |  3516 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3517 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3518 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3519 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3520 | `	/* Release the loop block */` |
|     ! 0 |  3521 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3522 | `	/* Statement successfully compiled */` |
|     ! 0 |  3523 | `	return SXRET_OK;` |
|       1 |  3524 | `Synchronize:` |
|       - |  3525 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3526 | `	 * compiling this erroneous block.` |
|       - |  3527 | `	 */` |
|       3 |  3528 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3529 | `		pGen->pIn++;` |
|     ! 0 |  3530 | `	}` |
|       3 |  3531 | `	return SXRET_OK;` |
|       2 |  3532 |  |
|       - |  3533 | `/*` |
|       - |  3534 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3535 | ` * According to the PHP language reference` |
|       - |  3536 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3537 | ` *  The syntax of a for loop is:` |
|       - |  3538 | ` *  for (expr1; expr2; expr3)` |
|       - |  3539 | ` *   statement` |
|       - |  3540 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3541 | ` *  the beginning of the loop.` |
|       - |  3542 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3543 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3544 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3545 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3546 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3547 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3548 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3549 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3550 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3551 | ` *  of using the for truth expression.` |
|       - |  3552 | ` */` |
|   11312 |  3553 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3554 |  |
|   11314 |  3555 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11314 |  3556 | `	GenBlock *pForBlock = 0;` |
|       - |  3557 | `	sxu32 nFalseJump;` |
|       - |  3558 | `	sxu32 nLine;` |
|       - |  3559 | `	sxi32 rc;` |
|   11314 |  3560 | `	nLine = pGen->pIn->nLine;` |
|       - |  3561 | `	/* Jump the 'for' keyword */` |
|   11314 |  3562 | `	pGen->pIn++;` |
|   11314 |  3563 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3564 | `		/* Syntax error */` |
|     ! 0 |  3565 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3566 | `		if( rc == SXERR_ABORT ){` |
|       - |  3567 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3568 | `			return SXERR_ABORT;` |
|       - |  3569 | `		}` |
|     ! 0 |  3570 | `		return SXRET_OK;` |
|       - |  3571 | `	}` |
|       - |  3572 | `	/* Jump the left parenthesis '(' */` |
|   11314 |  3573 | `	pGen->pIn++;` |
|       - |  3574 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11314 |  3575 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11314 |  3576 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3577 | `		/* Empty expression */` |
|     ! 0 |  3578 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3579 | `		if( rc == SXERR_ABORT ){` |
|       - |  3580 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3581 | `			return SXERR_ABORT;` |
|       - |  3582 | `		}` |
|       - |  3583 | `		/* Synchronize */` |
|     ! 0 |  3584 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3585 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3586 | `			pGen->pIn++;` |
|     ! 0 |  3587 | `		}` |
|     ! 0 |  3588 | `		return SXRET_OK;` |
|       - |  3589 | `	}` |
|       - |  3590 | `	/* Swap token streams */` |
|   11314 |  3591 | `	pTmp = pGen->pEnd;` |
|   11314 |  3592 | `	pGen->pEnd = pEnd;` |
|       - |  3593 | `	/* Compile initialization expressions if available */` |
|   11314 |  3594 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3595 | `	/* Pop operand lvalues */` |
|   11314 |  3596 | `	if( rc == SXERR_ABORT ){` |
|       - |  3597 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3598 | `		return SXERR_ABORT;` |
|   11314 |  3599 | `	}else if( rc != SXERR_EMPTY ){` |
|   11312 |  3600 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5655 |  3601 | `	}` |
|   11314 |  3602 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3603 | `		/* Syntax error */` |
|     ! 0 |  3604 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3605 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3606 | `		if( rc == SXERR_ABORT ){` |
|       - |  3607 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3608 | `			return SXERR_ABORT;` |
|       - |  3609 | `		}` |
|     ! 0 |  3610 | `		return SXRET_OK;` |
|       - |  3611 | `	}` |
|       - |  3612 | `	/* Jump the trailing ';' */` |
|   11314 |  3613 | `	pGen->pIn++;` |
|       - |  3614 | `	/* Create the loop block */` |
|   11314 |  3615 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11314 |  3616 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3617 | `		return SXERR_ABORT;` |
|       - |  3618 | `	}` |
|       - |  3619 | `	/* Deffer continue jumps */` |
|   11314 |  3620 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3621 | `	/* Compile the condition */` |
|   11314 |  3622 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11314 |  3623 | `	if( rc == SXERR_ABORT ){` |
|       - |  3624 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3625 | `		return SXERR_ABORT;` |
|   11314 |  3626 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3627 | `		/* Emit the false jump */` |
|   11312 |  3628 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3629 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11312 |  3630 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5655 |  3631 | `	}` |
|   11314 |  3632 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3633 | `		/* Syntax error */` |
|       5 |  3634 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3635 | `			"for: Expected ';' after conditionals expressions");` |
|       5 |  3636 | `		if( rc == SXERR_ABORT ){` |
|       - |  3637 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3638 | `			return SXERR_ABORT;` |
|       - |  3639 | `		}` |
|       5 |  3640 | `		return SXRET_OK;` |
|       - |  3641 | `	}` |
|       - |  3642 | `	/* Jump the trailing ';' */` |
|   11310 |  3643 | `	pGen->pIn++;` |
|       - |  3644 | `	/* Save the post condition stream */` |
|   11310 |  3645 | `	pPostStart = pGen->pIn;` |
|       - |  3646 | `	/* Compile the loop body */` |
|   11310 |  3647 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11310 |  3648 | `	pGen->pEnd = pTmp;` |
|   11310 |  3649 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11310 |  3650 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3651 | `		return SXERR_ABORT;` |
|       - |  3652 | `	}` |
|       - |  3653 | `	/* Fix post-continue jumps */` |
|   11310 |  3654 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3655 | `		JumpFixup *aPost;` |
|       - |  3656 | `		VmInstr *pInstr;` |
|       - |  3657 | `		sxu32 nJumpDest;` |
|       - |  3658 | `		sxu32 n;` |
|      14 |  3659 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3660 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3661 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3662 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3663 | `			if( pInstr ){` |
|       - |  3664 | `				/* Fix jump */` |
|      14 |  3665 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3666 | `			}` |
|       8 |  3667 | `		}` |
|       6 |  3668 | `	}` |
|       - |  3669 | `	/* compile the post-expressions if available */` |
|   11310 |  3670 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3671 | `		pPostStart++;` |
|     ! 0 |  3672 | `	}` |
|   11310 |  3673 | `	if( pPostStart < pEnd ){` |
|       - |  3674 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11310 |  3675 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11310 |  3676 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11310 |  3677 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3678 | `			/* Syntax error */` |
|     ! 0 |  3679 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  3680 | `			if( rc == SXERR_ABORT ){` |
|       - |  3681 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3682 | `				return SXERR_ABORT;` |
|       - |  3683 | `			}` |
|     ! 0 |  3684 | `			return SXRET_OK;` |
|       - |  3685 | `		}` |
|   11310 |  3686 | `		RE_SWAP_DELIMITER(pGen);` |
|   11310 |  3687 | `		if( rc == SXERR_ABORT ){` |
|       - |  3688 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3689 | `			return SXERR_ABORT;` |
|   11310 |  3690 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  3691 | `			/* Pop operand lvalue */` |
|   11310 |  3692 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5654 |  3693 | `		}` |
|    5654 |  3694 | `	}` |
|       - |  3695 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11310 |  3696 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  3697 | `	/* Fix all jumps now the destination is resolved */` |
|   11310 |  3698 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3699 | `	/* Release the loop block */` |
|   11310 |  3700 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3701 | `	/* Statement successfully compiled */` |
|   11310 |  3702 | `	return SXRET_OK;` |
|    5658 |  3703 |  |
|       - |  3704 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  3705 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  3706 | ` * are allowed.` |
|       - |  3707 | ` */` |
|    6006 |  3708 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  3709 |  |
|    6008 |  3710 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6008 |  3711 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  3712 | `		/* Unexpected expression */` |
|     ! 0 |  3713 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  3714 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  3715 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  3716 | `			rc = SXERR_INVALID;` |
|     ! 0 |  3717 | `		}` |
|     ! 0 |  3718 | `	}` |
|    6008 |  3719 | `	return rc;` |
|       2 |  3720 |  |
|       - |  3721 | `/*` |
|       - |  3722 | ` * Compile the 'foreach' statement.` |
|       - |  3723 | ` * According to the PHP language reference` |
|       - |  3724 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  3725 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  3726 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  3727 | ` *  is a minor but useful extension of the first:` |
|       - |  3728 | ` *  foreach (array_expression as $value)` |
|       - |  3729 | ` *    statement` |
|       - |  3730 | ` *  foreach (array_expression as $key => $value)` |
|       - |  3731 | ` *   statement` |
|       - |  3732 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  3733 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  3734 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  3735 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  3736 | ` *  to the variable $key on each loop.` |
|       - |  3737 | ` *  Note:` |
|       - |  3738 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  3739 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  3740 | ` *  Note:` |
|       - |  3741 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  3742 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  3743 | ` *  or after the foreach without resetting it.` |
|       - |  3744 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  3745 | ` *  of copying the value.` |
|       - |  3746 | ` */` |
|    3056 |  3747 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  3748 |  |
|    3058 |  3749 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3058 |  3750 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3058 |  3751 | `	GenBlock *pForeachBlock = 0;` |
|       - |  3752 | `	ph7_foreach_info *pInfo;` |
|       - |  3753 | `	sxu32 nFalseJump;` |
|       - |  3754 | `	VmInstr *pInstr;` |
|       - |  3755 | `	sxu32 nLine;` |
|       - |  3756 | `	sxi32 rc;` |
|    3058 |  3757 | `	nLine = pGen->pIn->nLine;` |
|       - |  3758 | `	/* Jump the 'foreach' keyword */` |
|    3058 |  3759 | `	pGen->pIn++;` |
|    3058 |  3760 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3761 | `		/* Syntax error */` |
|     ! 0 |  3762 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  3763 | `		if( rc == SXERR_ABORT ){` |
|       - |  3764 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3765 | `			return SXERR_ABORT;` |
|       - |  3766 | `		}` |
|     ! 0 |  3767 | `		goto Synchronize;` |
|       - |  3768 | `	}` |
|       - |  3769 | `	/* Jump the left parenthesis '(' */` |
|    3058 |  3770 | `	pGen->pIn++;` |
|       - |  3771 | `	/* Create the loop block */` |
|    3058 |  3772 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3058 |  3773 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3774 | `		return SXERR_ABORT;` |
|       - |  3775 | `	}` |
|       - |  3776 | `	/* Delimit the expression */` |
|    3058 |  3777 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3058 |  3778 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3779 | `		/* Empty expression */` |
|     ! 0 |  3780 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  3781 | `		if( rc == SXERR_ABORT ){` |
|       - |  3782 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3783 | `			return SXERR_ABORT;` |
|       - |  3784 | `		}` |
|       - |  3785 | `		/* Synchronize */` |
|     ! 0 |  3786 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3787 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3788 | `			pGen->pIn++;` |
|     ! 0 |  3789 | `		}` |
|     ! 0 |  3790 | `		return SXRET_OK;` |
|       - |  3791 | `	}` |
|       - |  3792 | `	/* Compile the array expression */` |
|    3058 |  3793 | `	pCur = pGen->pIn;` |
|   20484 |  3794 | `	while( pCur < pEnd ){` |
|   20484 |  3795 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3068 |  3796 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3068 |  3797 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  3798 | `				/* Break with the first 'as' found */` |
|    3058 |  3799 | `				break;` |
|       - |  3800 | `			}` |
|       5 |  3801 | `		}` |
|       - |  3802 | `		/* Advance the stream cursor */` |
|   17428 |  3803 | `		pCur++;` |
|       2 |  3804 | `	}` |
|    3058 |  3805 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  3806 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  3807 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  3808 | `		if( rc == SXERR_ABORT ){` |
|       - |  3809 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3810 | `			return SXERR_ABORT;` |
|       - |  3811 | `		}` |
|     ! 0 |  3812 | `		goto Synchronize;` |
|       - |  3813 | `	}` |
|       - |  3814 | `	/* Swap token streams */` |
|    3058 |  3815 | `	pTmp = pGen->pEnd;` |
|    3058 |  3816 | `	pGen->pEnd = pCur;` |
|    3058 |  3817 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3058 |  3818 | `	if( rc == SXERR_ABORT ){` |
|       - |  3819 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3820 | `		return SXERR_ABORT;` |
|       - |  3821 | `	}` |
|       - |  3822 | `	/* Update token stream */` |
|    3058 |  3823 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  3824 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3825 | `		if( rc == SXERR_ABORT ){` |
|       - |  3826 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3827 | `			return SXERR_ABORT;` |
|       - |  3828 | `		}` |
|     ! 0 |  3829 | `		pGen->pIn++;` |
|     ! 0 |  3830 | `	}` |
|    3058 |  3831 | `	pCur++; /* Jump the 'as' keyword */` |
|    3058 |  3832 | `	pGen->pIn = pCur;` |
|    3058 |  3833 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  3834 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  3835 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3836 | `			return SXERR_ABORT;` |
|       - |  3837 | `		}` |
|     ! 0 |  3838 | `	}` |
|       - |  3839 | `	/* Create the foreach context */` |
|    3058 |  3840 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3058 |  3841 | `	if( pInfo == 0 ){` |
|     ! 0 |  3842 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  3843 | `		return SXERR_ABORT;` |
|       - |  3844 | `	}` |
|       - |  3845 | `	/* Zero the structure */` |
|    3058 |  3846 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  3847 | `	/* Initialize structure fields */` |
|    3058 |  3848 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  3849 | `	/* Check if we have a key field */` |
|    9220 |  3850 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6164 |  3851 | `		pCur++;` |
|       2 |  3852 | `	}` |
|    3058 |  3853 | `	if( pCur < pEnd ){` |
|       - |  3854 | `		/* Compile the expression holding the key name */` |
|    2962 |  3855 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  3856 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  3857 | `			if( rc == SXERR_ABORT ){` |
|       - |  3858 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3859 | `				return SXERR_ABORT;` |
|       - |  3860 | `			}` |
|     ! 0 |  3861 | `		}else{` |
|    2962 |  3862 | `			pGen->pEnd = pCur;` |
|    2962 |  3863 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    2962 |  3864 | `			if( rc == SXERR_ABORT ){` |
|       - |  3865 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3866 | `				return SXERR_ABORT;` |
|       - |  3867 | `			}` |
|    2962 |  3868 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    2962 |  3869 | `			if( pInstr->p3 ){` |
|       - |  3870 | `				/* Record key name */` |
|    2962 |  3871 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1480 |  3872 | `			}` |
|    2962 |  3873 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  3874 | `		}` |
|    2962 |  3875 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1480 |  3876 | `	}` |
|    3058 |  3877 | `	pGen->pEnd = pEnd;` |
|    3058 |  3878 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  3879 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  3880 | `		if( rc == SXERR_ABORT ){` |
|       - |  3881 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3882 | `			return SXERR_ABORT;` |
|       - |  3883 | `		}` |
|     ! 0 |  3884 | `		goto Synchronize;` |
|       - |  3885 | `	}` |
|    3058 |  3886 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  3887 | `		pGen->pIn++;` |
|       - |  3888 | `		/* Pass by reference  */` |
|      11 |  3889 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  3890 | `	}` |
|       - |  3891 | `	/* Check if the value target is list() */` |
|    3058 |  3892 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  3893 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  3894 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  3895 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  3896 | `		 */` |
|       - |  3897 | `		static int iForeachListCnt = 0;` |
|       - |  3898 | `		char zTmp[128];` |
|       - |  3899 | `		sxu32 nLen;` |
|       - |  3900 | `		char *zDup;` |
|      10 |  3901 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  3902 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  3903 | `		if( zDup == 0 ){` |
|     ! 0 |  3904 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3905 | `			return SXERR_ABORT;` |
|       - |  3906 | `		}` |
|      10 |  3907 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  3908 | `		/* Save list() token boundaries */` |
|      10 |  3909 | `		pListStart = pGen->pIn;` |
|       - |  3910 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  3911 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  3912 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  3913 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  3914 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  3915 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3916 | `				return SXERR_ABORT;` |
|       - |  3917 | `			}` |
|       3 |  3918 | `			goto Synchronize;` |
|       - |  3919 | `		}` |
|       7 |  3920 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  3921 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  3922 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  3923 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  3924 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  3925 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3926 | `				return SXERR_ABORT;` |
|       - |  3927 | `			}` |
|     ! 0 |  3928 | `			goto Synchronize;` |
|       - |  3929 | `		}` |
|       7 |  3930 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  3931 | `		pListEnd = pGen->pIn;` |
|       7 |  3932 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3053 |  3933 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  3934 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  3935 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  3936 | `		 */` |
|       - |  3937 | `		static int iForeachShortListCnt = 0;` |
|       - |  3938 | `		char zTmp[128];` |
|       - |  3939 | `		sxu32 nLen;` |
|       - |  3940 | `		char *zDup;` |
|       3 |  3941 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  3942 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  3943 | `		if( zDup == 0 ){` |
|     ! 0 |  3944 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3945 | `			return SXERR_ABORT;` |
|       - |  3946 | `		}` |
|       3 |  3947 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  3948 | `		/* Save [...] token boundaries */` |
|       3 |  3949 | `		pListStart = pGen->pIn;` |
|       - |  3950 | `		/* Advance past [...] */` |
|       3 |  3951 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  3952 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  3953 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  3954 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  3955 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  3956 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3957 | `				return SXERR_ABORT;` |
|       - |  3958 | `			}` |
|     ! 0 |  3959 | `			goto Synchronize;` |
|       - |  3960 | `		}` |
|       3 |  3961 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  3962 | `		pListEnd = pGen->pIn;` |
|       3 |  3963 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  3964 | `	}else{` |
|       - |  3965 | `		/* Compile the expression holding the value name */` |
|    3048 |  3966 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3048 |  3967 | `		if( rc == SXERR_ABORT ){` |
|       - |  3968 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3969 | `			return SXERR_ABORT;` |
|       - |  3970 | `		}` |
|    3048 |  3971 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3048 |  3972 | `		if( pInstr->p3 ){` |
|       - |  3973 | `			/* Record value name */` |
|    3048 |  3974 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1523 |  3975 | `		}` |
|       - |  3976 | `	}` |
|       - |  3977 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3056 |  3978 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  3979 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3056 |  3980 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  3981 | `	/* Record the first instruction to execute */` |
|    3056 |  3982 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3983 | `	/* Emit the FOREACH_STEP instruction */` |
|    3056 |  3984 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  3985 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3056 |  3986 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  3987 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3056 |  3988 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  3989 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  3990 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  3991 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  3992 | `		 */` |
|       9 |  3993 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  3994 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  3995 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  3996 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  3997 | `		 */` |
|       9 |  3998 | `		pSavedIn = pGen->pIn;` |
|       9 |  3999 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4000 | `		pGen->pIn = pListStart;` |
|       9 |  4001 | `		pGen->pEnd = pListEnd;` |
|       9 |  4002 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4003 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4004 | `		}else{` |
|       7 |  4005 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4006 | `		}` |
|       9 |  4007 | `		pGen->pIn = pSavedIn;` |
|       9 |  4008 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4009 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4010 | `			return SXERR_ABORT;` |
|       - |  4011 | `		}` |
|       - |  4012 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4013 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4014 | `	}` |
|       - |  4015 | `	/* Compile the loop body */` |
|    3056 |  4016 | `	pGen->pIn = &pEnd[1];` |
|    3056 |  4017 | `	pGen->pEnd = pTmp;` |
|    3056 |  4018 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3056 |  4019 | `	if( rc == SXERR_ABORT ){` |
|       - |  4020 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4021 | `		return SXERR_ABORT;` |
|       - |  4022 | `	}` |
|       - |  4023 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3056 |  4024 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4025 | `	/* Fix all jumps now the destination is resolved */` |
|    3056 |  4026 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4027 | `	/* Release the loop block */` |
|    3056 |  4028 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4029 | `	/* Statement successfully compiled */` |
|    3056 |  4030 | `	return SXRET_OK;` |
|       1 |  4031 | `Synchronize:` |
|       - |  4032 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4033 | `	 * compiling this erroneous block.` |
|       - |  4034 | `	 */` |
|       3 |  4035 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4036 | `		pGen->pIn++;` |
|     ! 0 |  4037 | `	}` |
|       3 |  4038 | `	return SXRET_OK;` |
|    1530 |  4039 |  |
|       - |  4040 | `/*` |
|       - |  4041 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4042 | ` * According to the PHP language reference` |
|       - |  4043 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4044 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4045 | ` *  that is similar to that of C:` |
|       - |  4046 | ` *  if (expr)` |
|       - |  4047 | ` *   statement` |
|       - |  4048 | ` *  else construct:` |
|       - |  4049 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4050 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4051 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4052 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4053 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4054 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4055 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4056 | ` *  elseif` |
|       - |  4057 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4058 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4059 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4060 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4061 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4062 | ` *   <?php` |
|       - |  4063 | ` *    if ($a > $b) {` |
|       - |  4064 | ` *     echo "a is bigger than b";` |
|       - |  4065 | ` *    } elseif ($a == $b) {` |
|       - |  4066 | ` *     echo "a is equal to b";` |
|       - |  4067 | ` *    } else {` |
|       - |  4068 | ` *     echo "a is smaller than b";` |
|       - |  4069 | ` *    }` |
|       - |  4070 | ` *    ?>` |
|       - |  4071 | ` */` |
|  112324 |  4072 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4073 |  |
|  112326 |  4074 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  112326 |  4075 | `	GenBlock *pCondBlock = 0;` |
|       - |  4076 | `	sxu32 nJumpIdx;` |
|       - |  4077 | `	sxu32 nKeyID;` |
|       - |  4078 | `	sxi32 rc;` |
|       - |  4079 | `	/* Jump the 'if' keyword */` |
|  112326 |  4080 | `	pGen->pIn++;` |
|  112326 |  4081 | `	pToken = pGen->pIn;` |
|       - |  4082 | `	/* Create the conditional block */` |
|  112326 |  4083 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  112326 |  4084 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4085 | `		return SXERR_ABORT;` |
|       - |  4086 | `	}` |
|       - |  4087 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   61779 |  4088 | `	for(;;){` |
|  123560 |  4089 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4090 | `			/* Syntax error */` |
|     ! 0 |  4091 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4092 | `				pToken--;` |
|     ! 0 |  4093 | `			}` |
|     ! 0 |  4094 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4095 | `			if( rc == SXERR_ABORT ){` |
|       - |  4096 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4097 | `				return SXERR_ABORT;` |
|       - |  4098 | `			}` |
|     ! 0 |  4099 | `			goto Synchronize;` |
|       - |  4100 | `		}` |
|       - |  4101 | `		/* Jump the left parenthesis '(' */` |
|  123560 |  4102 | `		pToken++;` |
|       - |  4103 | `		/* Delimit the condition */` |
|  123560 |  4104 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  123560 |  4105 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4106 | `			/* Syntax error */` |
|     ! 0 |  4107 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4108 | `				pToken--;` |
|     ! 0 |  4109 | `			}` |
|     ! 0 |  4110 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4111 | `			if( rc == SXERR_ABORT ){` |
|       - |  4112 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4113 | `				return SXERR_ABORT;` |
|       - |  4114 | `			}` |
|     ! 0 |  4115 | `			goto Synchronize;` |
|       - |  4116 | `		}` |
|       - |  4117 | `		/* Swap token streams */` |
|  123560 |  4118 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4119 | `		/* Compile the condition */` |
|  123560 |  4120 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4121 | `		/* Update token stream */` |
|  123560 |  4122 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4123 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4124 | `			pGen->pIn++;` |
|     ! 0 |  4125 | `		}` |
|  123560 |  4126 | `		pGen->pIn  = &pEnd[1];` |
|  123560 |  4127 | `		pGen->pEnd = pTmp;` |
|  123560 |  4128 | `		if( rc == SXERR_ABORT ){` |
|       - |  4129 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4130 | `			return SXERR_ABORT;` |
|       - |  4131 | `		}` |
|       - |  4132 | `		/* Emit the false jump */` |
|  123560 |  4133 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4134 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  123560 |  4135 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4136 | `		/* Compile the body */` |
|  123560 |  4137 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  123560 |  4138 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4139 | `			return SXERR_ABORT;` |
|       - |  4140 | `		}` |
|  123560 |  4141 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   33244 |  4142 | `			break;` |
|       - |  4143 | `		}` |
|       - |  4144 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   57076 |  4145 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   57076 |  4146 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   36712 |  4147 | `			break;` |
|       - |  4148 | `		}` |
|       - |  4149 | `		/* Emit the unconditional jump */` |
|   20366 |  4150 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4151 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   20366 |  4152 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   20366 |  4153 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   14736 |  4154 | `			pToken = &pGen->pIn[1];` |
|   14736 |  4155 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5634 |  4156 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4567 |  4157 | `					break;` |
|       - |  4158 | `			}` |
|    5606 |  4159 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2802 |  4160 | `		}` |
|   11236 |  4161 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4162 | `		/* Synchronize cursors */` |
|   11236 |  4163 | `		pToken = pGen->pIn;` |
|       - |  4164 | `		/* Fix the false jump */` |
|   11236 |  4165 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4166 | `	} /* For(;;) */` |
|       - |  4167 | `	/* Fix the false jump */` |
|  112326 |  4168 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  112326 |  4169 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   45840 |  4170 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4171 | `			/* Compile the else block */` |
|    9132 |  4172 | `			pGen->pIn++;` |
|    9132 |  4173 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9132 |  4174 | `			if( rc == SXERR_ABORT ){` |
|       - |  4175 |  |
|     ! 0 |  4176 | `				return SXERR_ABORT;` |
|       - |  4177 | `			}` |
|    4565 |  4178 | `	}` |
|  112326 |  4179 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4180 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  112326 |  4181 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4182 | `	/* Release the conditional block */` |
|  112326 |  4183 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4184 | `	/* Statement successfully compiled */` |
|  112326 |  4185 | `	return SXRET_OK;` |
|     ! 0 |  4186 | `Synchronize:` |
|       - |  4187 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4188 | `	 */` |
|     ! 0 |  4189 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4190 | `		pGen->pIn++;` |
|     ! 0 |  4191 | `	}` |
|     ! 0 |  4192 | `	return SXRET_OK;` |
|   56164 |  4193 |  |
|       - |  4194 | `/*` |
|       - |  4195 | ` * Compile the global construct.` |
|       - |  4196 | ` * According to the PHP language reference` |
|       - |  4197 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4198 | ` *  to be used in that function.` |
|       - |  4199 | ` *  Example #1 Using global` |
|       - |  4200 | ` *  <?php` |
|       - |  4201 | ` *   $a = 1;` |
|       - |  4202 | ` *   $b = 2;` |
|       - |  4203 | ` *   function Sum()` |
|       - |  4204 | ` *   {` |
|       - |  4205 | ` *    global $a, $b;` |
|       - |  4206 | ` *    $b = $a + $b;` |
|       - |  4207 | ` *   }` |
|       - |  4208 | ` *   Sum();` |
|       - |  4209 | ` *   echo $b;` |
|       - |  4210 | ` *  ?>` |
|       - |  4211 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4212 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4213 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4214 | ` */` |
|      26 |  4215 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4216 |  |
|      28 |  4217 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4218 | `	sxi32 nExpr;` |
|       - |  4219 | `	sxi32 rc;` |
|       - |  4220 | `	/* Jump the 'global' keyword */` |
|      28 |  4221 | `	pGen->pIn++;` |
|      28 |  4222 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4223 | `		/* Nothing to process */` |
|     ! 0 |  4224 | `		return SXRET_OK;` |
|       - |  4225 | `	}` |
|      28 |  4226 | `	pTmp = pGen->pEnd;` |
|      28 |  4227 | `	nExpr = 0;` |
|      56 |  4228 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      30 |  4229 | `		if( pGen->pIn < pNext ){` |
|      30 |  4230 | `			pGen->pEnd = pNext;` |
|      30 |  4231 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4232 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4233 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4234 | `					return SXERR_ABORT;` |
|       - |  4235 | `				}` |
|     ! 0 |  4236 | `			}else{` |
|      30 |  4237 | `				pGen->pIn++;` |
|      30 |  4238 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4239 | `					/* Emit a warning */` |
|     ! 0 |  4240 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4241 | `				}else{` |
|      30 |  4242 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  4243 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4244 | `						return SXERR_ABORT;` |
|      30 |  4245 | `					}else if(rc != SXERR_EMPTY ){` |
|      30 |  4246 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      30 |  4247 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4248 | `							/* Variable name, not a constant */` |
|      30 |  4249 | `							pLast->iP1 = 0;` |
|      14 |  4250 | `						}` |
|      30 |  4251 | `						nExpr++;` |
|      14 |  4252 | `					}` |
|       - |  4253 | `				}` |
|       - |  4254 | `			}` |
|      14 |  4255 | `		}` |
|       - |  4256 | `		/* Next expression in the stream */` |
|      30 |  4257 | `		pGen->pIn = pNext;` |
|       - |  4258 | `		/* Jump trailing commas */` |
|      32 |  4259 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4260 | `			pGen->pIn++;` |
|       1 |  4261 | `		}` |
|       2 |  4262 | `	}` |
|       - |  4263 | `	/* Restore token stream */` |
|      28 |  4264 | `	pGen->pEnd = pTmp;` |
|      28 |  4265 | `	if( nExpr > 0 ){` |
|       - |  4266 | `		/* Emit the uplink instruction */` |
|      28 |  4267 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      13 |  4268 | `	}` |
|      28 |  4269 | `	return SXRET_OK;` |
|      15 |  4270 |  |
|       - |  4271 | `/*` |
|       - |  4272 | ` * Compile the return statement.` |
|       - |  4273 | ` * According to the PHP language reference` |
|       - |  4274 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4275 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4276 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4277 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4278 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4279 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4280 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4281 | ` *  from within the main script file, then script execution end.` |
|       - |  4282 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4283 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4284 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4285 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4286 | ` */` |
|  163228 |  4287 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4288 |  |
|  163230 |  4289 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4290 | `	sxi32 rc;` |
|       - |  4291 | `	/* Jump the 'return' keyword */` |
|  163230 |  4292 | `	pGen->pIn++;` |
|  163230 |  4293 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4294 | `		/* Compile the expression */` |
|  163208 |  4295 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  163208 |  4296 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4297 | `			return SXERR_ABORT;` |
|  163208 |  4298 | `		}else if(rc != SXERR_EMPTY ){` |
|  163208 |  4299 | `			nRet = 1;` |
|   81603 |  4300 | `		}` |
|   81603 |  4301 | `	}` |
|       - |  4302 | `	/* Emit the done instruction */` |
|  163230 |  4303 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  163230 |  4304 | `	return SXRET_OK;` |
|   81616 |  4305 |  |
|       - |  4306 | `/*` |
|       - |  4307 | ` * Compile a yield expression.` |
|       - |  4308 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4309 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4310 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4311 | ` */` |
|      32 |  4312 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  4313 |  |
|       - |  4314 | `	SyToken *pTmp, *pSplit;` |
|      34 |  4315 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      34 |  4316 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4317 | `	sxi32 rc;` |
|      16 |  4318 | `	(void)iCompileFlag;` |
|       - |  4319 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      34 |  4320 | `	pGen->pIn++;` |
|       - |  4321 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4322 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      34 |  4323 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4324 | `		/* Bare yield — no value */` |
|     ! 0 |  4325 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4326 | `		return SXRET_OK;` |
|       - |  4327 | `	}` |
|       - |  4328 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      34 |  4329 | `	pSplit = 0;` |
|       - |  4330 | `	{` |
|      34 |  4331 | `		SyToken *pCur = pGen->pIn;` |
|      34 |  4332 | `		sxi32 nNest = 0;` |
|      78 |  4333 | `		while( pCur < pGen->pEnd ){` |
|      52 |  4334 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4335 | `				nNest++;` |
|      52 |  4336 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4337 | `				nNest--;` |
|      52 |  4338 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 |  4339 | `				pSplit = pCur;` |
|       7 |  4340 | `				break;` |
|       - |  4341 | `			}` |
|      46 |  4342 | `			pCur++;` |
|       2 |  4343 | `		}` |
|       - |  4344 | `	}` |
|      34 |  4345 | `	pTmp = pGen->pEnd;` |
|      34 |  4346 | `	if( pSplit ){` |
|       - |  4347 | `		/* yield $key => $value */` |
|       7 |  4348 | `		pGen->pEnd = pSplit;` |
|       7 |  4349 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4350 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4351 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 |  4352 | `		pGen->pEnd = pTmp;` |
|       7 |  4353 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4354 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4355 | `		iP1 = 1;` |
|       7 |  4356 | `		iP2 = 1;` |
|       4 |  4357 | `	}else{` |
|       - |  4358 | `		/* yield $value */` |
|      28 |  4359 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      28 |  4360 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      28 |  4361 | `		if( rc != SXERR_EMPTY ){` |
|      28 |  4362 | `			iP1 = 1;` |
|      13 |  4363 | `		}` |
|       - |  4364 | `	}` |
|      34 |  4365 | `	pGen->pEnd = pTmp;` |
|      34 |  4366 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      34 |  4367 | `	return SXRET_OK;` |
|      18 |  4368 |  |
|       - |  4369 | `/*` |
|       - |  4370 | ` * Compile the die/exit language construct.` |
|       - |  4371 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4372 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4373 | ` */` |
|      88 |  4374 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 |  4375 |  |
|      90 |  4376 | `	sxi32 nExpr = 0;` |
|       - |  4377 | `	sxi32 rc;` |
|       - |  4378 | `	/* Jump the die/exit keyword */` |
|      90 |  4379 | `	pGen->pIn++;` |
|      90 |  4380 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4381 | `		/* Compile the expression */` |
|      90 |  4382 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 |  4383 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4384 | `			return SXERR_ABORT;` |
|      90 |  4385 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 |  4386 | `			nExpr = 1;` |
|      44 |  4387 | `		}` |
|      44 |  4388 | `	}` |
|       - |  4389 | `	/* Emit the HALT instruction */` |
|      90 |  4390 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 |  4391 | `	return SXRET_OK;` |
|      46 |  4392 |  |
|       - |  4393 | `/*` |
|       - |  4394 | ` * Compile the 'echo' language construct.` |
|       - |  4395 | ` */` |
|   11418 |  4396 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4397 |  |
|   11420 |  4398 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4399 | `	sxi32 rc;` |
|       - |  4400 | `	/* Jump the 'echo' keyword */` |
|   11420 |  4401 | `	pGen->pIn++;` |
|       - |  4402 | `	/* Compile arguments one after one */` |
|   11420 |  4403 | `	pTmp = pGen->pEnd;` |
|   23598 |  4404 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   12180 |  4405 | `		if( pGen->pIn < pNext ){` |
|   12180 |  4406 | `			pGen->pEnd = pNext;` |
|   12180 |  4407 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   12180 |  4408 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4409 | `				return SXERR_ABORT;` |
|   12180 |  4410 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4411 | `				/* Emit the consume instruction */` |
|   12156 |  4412 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6077 |  4413 | `			}` |
|    6089 |  4414 | `		}` |
|       - |  4415 | `		/* Jump trailing commas */` |
|   12940 |  4416 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     762 |  4417 | `			pNext++;` |
|       2 |  4418 | `		}` |
|   12180 |  4419 | `		pGen->pIn = pNext;` |
|       2 |  4420 | `	}` |
|       - |  4421 | `	/* Restore token stream */` |
|   11420 |  4422 | `	pGen->pEnd = pTmp;` |
|   11420 |  4423 | `	return SXRET_OK;` |
|    5711 |  4424 |  |
|       - |  4425 | `/*` |
|       - |  4426 | ` * Compile the static statement.` |
|       - |  4427 | ` * According to the PHP language reference` |
|       - |  4428 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4429 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4430 | ` *  when program execution leaves this scope.` |
|       - |  4431 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4432 | ` * Symisc eXtension.` |
|       - |  4433 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4434 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4435 | ` *  Example` |
|       - |  4436 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4437 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4438 | ` */` |
|       2 |  4439 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 |  4440 |  |
|       - |  4441 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4442 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4443 | `	GenBlock *pBlock;` |
|       - |  4444 | `	SyString *pName;` |
|       - |  4445 | `	char *zDup;` |
|       - |  4446 | `	sxu32 nLine;` |
|       - |  4447 | `	sxi32 rc;` |
|       - |  4448 | `	/* Jump the static keyword */` |
|       3 |  4449 | `	nLine = pGen->pIn->nLine;` |
|       3 |  4450 | `	pGen->pIn++;` |
|       - |  4451 | `	/* Extract the enclosing function if any */` |
|       3 |  4452 | `	pBlock = pGen->pCurrent;` |
|       5 |  4453 | `	while( pBlock ){` |
|       5 |  4454 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 |  4455 | `			break;` |
|       - |  4456 | `		}` |
|       - |  4457 | `		/* Point to the upper block */` |
|       3 |  4458 | `		pBlock = pBlock->pParent;` |
|       1 |  4459 | `	}` |
|       3 |  4460 | `	if( pBlock == 0 ){` |
|       - |  4461 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4462 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4463 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4464 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4465 | `				return SXERR_ABORT;` |
|       - |  4466 | `			}` |
|     ! 0 |  4467 | `			goto Synchronize;` |
|       - |  4468 | `		}` |
|       - |  4469 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4470 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4471 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4472 | `			return SXERR_ABORT;` |
|     ! 0 |  4473 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4474 | `			/* Emit the POP instruction */` |
|     ! 0 |  4475 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4476 | `		}` |
|     ! 0 |  4477 | `		return SXRET_OK;` |
|       - |  4478 | `	}` |
|       3 |  4479 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4480 | `	/* Make sure we are dealing with a valid statement */` |
|       3 |  4481 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 |  4482 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4483 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4484 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4485 | `				return SXERR_ABORT;` |
|       - |  4486 | `			}` |
|       3 |  4487 | `			goto Synchronize;` |
|       - |  4488 | `	}` |
|     ! 0 |  4489 | `	pGen->pIn++;` |
|       - |  4490 | `	/* Extract variable name */` |
|     ! 0 |  4491 | `	pName = &pGen->pIn->sData;` |
|     ! 0 |  4492 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 |  4493 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4494 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4495 | `		goto Synchronize;` |
|       - |  4496 | `	}` |
|       - |  4497 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 |  4498 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 |  4499 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4500 | `	/* Duplicate variable name */` |
|     ! 0 |  4501 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 |  4502 | `	if( zDup == 0 ){` |
|     ! 0 |  4503 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4504 | `		return SXERR_ABORT;` |
|       - |  4505 | `	}` |
|     ! 0 |  4506 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4507 | `	/* Check if we have an expression to compile */` |
|     ! 0 |  4508 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4509 | `		SySet *pInstrContainer;` |
|       - |  4510 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4511 | `		 * Static variable can take any complex expression including function` |
|       - |  4512 | `		 * call as their initialization value.` |
|       - |  4513 | `		 * Example:` |
|       - |  4514 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4515 | `		 */` |
|     ! 0 |  4516 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4517 | `		/* Swap bytecode container */` |
|     ! 0 |  4518 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 |  4519 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4520 | `		/* Compile the expression */` |
|     ! 0 |  4521 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4522 | `		/* Emit the done instruction */` |
|     ! 0 |  4523 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4524 | `		/* Restore default bytecode container */` |
|     ! 0 |  4525 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  4526 | `	}` |
|       - |  4527 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 |  4528 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 |  4529 | `	return SXRET_OK;` |
|       1 |  4530 | `Synchronize:` |
|       - |  4531 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4532 | `	 * statement.` |
|       - |  4533 | `	 */` |
|       5 |  4534 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4535 | `		pGen->pIn++;` |
|       1 |  4536 | `	}` |
|       3 |  4537 | `	return SXRET_OK;` |
|       2 |  4538 |  |
|       - |  4539 | `/*` |
|       - |  4540 | ` * Compile the var statement.` |
|       - |  4541 | ` * Symisc Extension:` |
|       - |  4542 | ` *      var statement can be used outside of a class definition.` |
|       - |  4543 | ` */` |
|       4 |  4544 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4545 |  |
|       - |  4546 | `	sxu32 nLine;` |
|       - |  4547 | `	sxi32 rc;` |
|       5 |  4548 | `	nLine = pGen->pIn->nLine;` |
|       - |  4549 | `	/* Jump the 'var' keyword */` |
|       5 |  4550 | `	pGen->pIn++;` |
|       5 |  4551 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4552 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4553 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4554 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4555 | `			pGen->pIn++;` |
|     ! 0 |  4556 | `		}` |
|     ! 0 |  4557 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4558 | `			return SXERR_ABORT;` |
|       - |  4559 | `		}` |
|     ! 0 |  4560 | `	}else{` |
|       - |  4561 | `		/* Compile the expression */` |
|       5 |  4562 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4563 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4564 | `			return SXERR_ABORT;` |
|       5 |  4565 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4566 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4567 | `		}` |
|       - |  4568 | `	}` |
|       5 |  4569 | `	return SXRET_OK;` |
|       3 |  4570 |  |
|       - |  4571 | `/*` |
|       - |  4572 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4573 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4574 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4575 | ` */` |
|       - |  4576 | `/*` |
|       - |  4577 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4578 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4579 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4580 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4581 | ` *` |
|       - |  4582 | ` * Resolution order:` |
|       - |  4583 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4584 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4585 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4586 | ` *` |
|       - |  4587 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4588 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4589 | ` * Returns the (possibly new) literal index.` |
|       - |  4590 | ` */` |
|  334958 |  4591 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 |  4592 |  |
|       - |  4593 | `	ph7_value *pLit;` |
|       - |  4594 | `	const char *zLit;` |
|       - |  4595 | `	SyString sQualified;` |
|       - |  4596 | `	sxu32 nLit;` |
|       - |  4597 | `	sxu32 k;` |
|       - |  4598 | `	sxu32 nNewIdx;` |
|       - |  4599 | `	int hasNsSep;` |
|       - |  4600 | `	SyHashEntry *pImport;` |
|       - |  4601 | `	ph7_value *pNew;` |
|  334960 |  4602 | `	if( pFromImport ){` |
|  320218 |  4603 | `		*pFromImport = 0;` |
|  160108 |  4604 | `	}` |
|  334960 |  4605 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  334960 |  4606 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4607 | `		return nOrigIdx;` |
|       - |  4608 | `	}` |
|  334960 |  4609 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  334960 |  4610 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4611 | `	/* Skip if already qualified (contains backslash) */` |
|  334960 |  4612 | `	hasNsSep = 0;` |
| 3601422 |  4613 | `	for( k = 0; k < nLit; k++ ){` |
| 3266500 |  4614 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1633233 |  4615 | `	}` |
|  334960 |  4616 | `	if( hasNsSep ){` |
|      38 |  4617 | `		return nOrigIdx;` |
|       - |  4618 | `	}` |
|       - |  4619 | `	/* Check use imports first (works even outside namespaces) */` |
|  334924 |  4620 | `	SyBlobReset(&pGen->sWorker);` |
|  334924 |  4621 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  334924 |  4622 | `	if( pImport ){` |
|      38 |  4623 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4624 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4625 | `		if( pFromImport ){` |
|      18 |  4626 | `			*pFromImport = 1;` |
|       8 |  4627 | `		}` |
|      20 |  4628 | `	}else{` |
|  334888 |  4629 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  334800 |  4630 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4631 | `		}` |
|       - |  4632 | `		/* Prepend current namespace */` |
|      90 |  4633 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      90 |  4634 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      90 |  4635 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4636 | `	}` |
|       - |  4637 | `	/* Look up or create a new literal for the qualified name */` |
|     126 |  4638 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     126 |  4639 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 |  4640 | `		return nNewIdx; /* Already interned */` |
|       - |  4641 | `	}` |
|      74 |  4642 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      74 |  4643 | `	if( pNew == 0 ){` |
|     ! 0 |  4644 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4645 | `	}` |
|      74 |  4646 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      74 |  4647 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      74 |  4648 | `	return nNewIdx;` |
|  167481 |  4649 |  |
|       - |  4650 | `/*` |
|       - |  4651 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4652 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4653 | ` */` |
|   28402 |  4654 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4655 |  |
|       - |  4656 | `	SyHashEntry *pImport;` |
|       - |  4657 | `	/* Check use imports first */` |
|   28404 |  4658 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   28404 |  4659 | `	if( pImport ){` |
|      12 |  4660 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 |  4661 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 |  4662 | `		return;` |
|       - |  4663 | `	}` |
|       - |  4664 | `	/* Prepend current namespace if active */` |
|   28394 |  4665 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4666 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4667 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4668 | `	}` |
|   28394 |  4669 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   14203 |  4670 |  |
|       - |  4671 | `/*` |
|       - |  4672 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4673 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4674 | ` * The caller must release pOut when done.` |
|       - |  4675 | ` */` |
|   48354 |  4676 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4677 |  |
|   48356 |  4678 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      54 |  4679 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      54 |  4680 | `		SyBlobAppend(pOut,"\\",1);` |
|      26 |  4681 | `	}` |
|   48356 |  4682 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   48356 |  4683 |  |
|       - |  4684 | `/*` |
|       - |  4685 | ` * Compile a namespace statement` |
|       - |  4686 | ` * According to the PHP language reference manual` |
|       - |  4687 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  4688 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  4689 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  4690 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  4691 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  4692 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  4693 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  4694 | ` *  programming world.` |
|       - |  4695 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  4696 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  4697 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  4698 | ` *  classes/functions/constants.` |
|       - |  4699 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  4700 | ` *  readability of source code.` |
|       - |  4701 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  4702 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  4703 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  4704 | ` *       class MyClass {}` |
|       - |  4705 | ` *       function myfunction() {}` |
|       - |  4706 | ` *       const MYCONST = 1;` |
|       - |  4707 | ` *       $a = new MyClass;` |
|       - |  4708 | ` *       $c = new \my\name\MyClass;` |
|       - |  4709 | ` *       $a = strlen('hi');` |
|       - |  4710 | ` *       $d = namespace\MYCONST;` |
|       - |  4711 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  4712 | ` *       echo constant($d);` |
|       - |  4713 | ` * NOTE` |
|       - |  4714 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  4715 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  4716 | ` */` |
|       - |  4717 | `/*` |
|       - |  4718 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  4719 | ` */` |
|      14 |  4720 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 |  4721 |  |
|      15 |  4722 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 |  4723 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 |  4724 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 |  4725 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 |  4726 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 |  4727 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  4728 | `	return "token";` |
|       8 |  4729 |  |
|     100 |  4730 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 |  4731 |  |
|       - |  4732 | `	sxu32 nLine;` |
|       - |  4733 | `	sxi32 rc;` |
|     102 |  4734 | `	nLine = pGen->pIn->nLine;` |
|     102 |  4735 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  4736 | `	/* Reset namespace and clear previous use imports */` |
|     102 |  4737 | `	SyBlobReset(&pGen->sNamespace);` |
|     102 |  4738 | `	SyHashRelease(&pGen->hUseImports);` |
|     102 |  4739 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  4740 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     102 |  4741 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  4742 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     102 |  4743 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     102 |  4744 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4745 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  4746 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  4747 | `		return SXRET_OK;` |
|       - |  4748 | `	}` |
|     102 |  4749 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  4750 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  4751 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  4752 | `		return SXRET_OK;` |
|       - |  4753 | `	}` |
|     102 |  4754 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  4755 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  4756 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  4757 | `		return SXRET_OK;` |
|       - |  4758 | `	}` |
|       - |  4759 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     240 |  4760 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     140 |  4761 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  4762 | `			/* Append backslash separator */` |
|      21 |  4763 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 |  4764 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      10 |  4765 | `			}` |
|      11 |  4766 | `		}else{` |
|       - |  4767 | `			/* Append identifier */` |
|     120 |  4768 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  4769 | `		}` |
|     140 |  4770 | `		pGen->pIn++;` |
|       2 |  4771 | `	}` |
|       - |  4772 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  4773 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  4774 | `	{` |
|     102 |  4775 | `		char *zNsDup = 0;` |
|     102 |  4776 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     149 |  4777 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      98 |  4778 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      49 |  4779 | `		}` |
|     102 |  4780 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  4781 | `	}` |
|     102 |  4782 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 |  4783 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  4784 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  4785 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 |  4786 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4787 | `			return SXERR_ABORT;` |
|       - |  4788 | `		}` |
|       2 |  4789 | `	}` |
|     102 |  4790 | `	return SXRET_OK;` |
|      52 |  4791 |  |
|       - |  4792 | `/*` |
|       - |  4793 | ` * Compile the 'use' statement` |
|       - |  4794 | ` * According to the PHP language reference manual` |
|       - |  4795 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  4796 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  4797 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  4798 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  4799 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  4800 | ` *  a function or constant is not supported.` |
|       - |  4801 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  4802 | ` * NOTE` |
|       - |  4803 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  4804 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  4805 | ` */` |
|      66 |  4806 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 |  4807 |  |
|       - |  4808 | `	sxu32 nLine;` |
|       - |  4809 | `	sxi32 rc;` |
|       - |  4810 | `	SyBlob sPath;` |
|       - |  4811 | `	SyString sAlias;` |
|       - |  4812 | `	SyToken *pLast;` |
|       - |  4813 | `	char *zDup;` |
|       - |  4814 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  4815 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  4816 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      68 |  4817 | `	nLine = pGen->pIn->nLine;` |
|      68 |  4818 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  4819 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      68 |  4820 | `	iUseType = 0;` |
|      68 |  4821 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  4822 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  4823 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  4824 | `			iUseType = 1;` |
|      16 |  4825 | `			pGen->pIn++;` |
|      23 |  4826 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  4827 | `			iUseType = 2;` |
|      16 |  4828 | `			pGen->pIn++;` |
|       7 |  4829 | `		}` |
|      14 |  4830 | `	}` |
|       - |  4831 | `	/* Select target hash tables based on import type */` |
|      68 |  4832 | `	switch( iUseType ){` |
|       7 |  4833 | `		case 1:` |
|      16 |  4834 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  4835 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  4836 | `			break;` |
|       7 |  4837 | `		case 2:` |
|      16 |  4838 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  4839 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  4840 | `			break;` |
|      19 |  4841 | `		default:` |
|      40 |  4842 | `			pGenHash = &pGen->hUseImports;` |
|      40 |  4843 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      38 |  4844 | `			break;` |
|       - |  4845 | `	}` |
|      68 |  4846 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  4847 | `	/* Process one or more use declarations separated by commas */` |
|      34 |  4848 | `	for(;;){` |
|      70 |  4849 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  4850 | `			break;` |
|       - |  4851 | `		}` |
|      70 |  4852 | `		SyBlobReset(&sPath);` |
|      70 |  4853 | `		pLast = 0;` |
|       - |  4854 | `		/* Collect the full namespace path */` |
|     254 |  4855 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     186 |  4856 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     126 |  4857 | `				pLast = pGen->pIn;` |
|     126 |  4858 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 |  4859 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  4860 | `				}` |
|     126 |  4861 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      62 |  4862 | `			}` |
|     186 |  4863 | `			pGen->pIn++;` |
|       2 |  4864 | `		}` |
|      70 |  4865 | `		if( pLast == 0 ){` |
|       - |  4866 | `			/* Empty path */` |
|       5 |  4867 | `			break;` |
|       - |  4868 | `		}` |
|       - |  4869 | `		/* Default alias is the last component of the path */` |
|      66 |  4870 | `		sAlias = pLast->sData;` |
|       - |  4871 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      64 |  4872 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      42 |  4873 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  4874 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  4875 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  4876 | `				sAlias = pGen->pIn->sData;` |
|      18 |  4877 | `				pGen->pIn++;` |
|       8 |  4878 | `			}` |
|       8 |  4879 | `		}` |
|       - |  4880 | `		/* Check for duplicate import alias (per-type) */` |
|      66 |  4881 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 |  4882 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  4883 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  4884 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 |  4885 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4886 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  4887 | `				return SXERR_ABORT;` |
|       - |  4888 | `			}` |
|       2 |  4889 | `		}` |
|       - |  4890 | `		/* Register the import: alias -> FQN.` |
|       - |  4891 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  4892 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  4893 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      98 |  4894 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      64 |  4895 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      66 |  4896 | `		if( zDup ){` |
|      66 |  4897 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      66 |  4898 | `			if( pVmHash ){` |
|       - |  4899 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  4900 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      38 |  4901 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      38 |  4902 | `				if( zAliasDup ){` |
|      38 |  4903 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      18 |  4904 | `				}` |
|      18 |  4905 | `			}` |
|      66 |  4906 | `			if( iUseType == 2 ){` |
|       - |  4907 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  4908 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  4909 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  4910 | `				if( zAliasDup ){` |
|       - |  4911 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  4912 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  4913 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  4914 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  4915 | `					if( azPair ){` |
|      16 |  4916 | `						azPair[0] = zAliasDup;` |
|      16 |  4917 | `						azPair[1] = zDup;` |
|      16 |  4918 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  4919 | `					}` |
|       7 |  4920 | `				}` |
|       7 |  4921 | `			}` |
|      32 |  4922 | `		}` |
|       - |  4923 | `		/* Check for comma (multiple use declarations) */` |
|      66 |  4924 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4925 | `			pGen->pIn++;` |
|       2 |  4926 | `		}else{` |
|      33 |  4927 | `			break;` |
|       - |  4928 | `		}` |
|       1 |  4929 | `	}` |
|      68 |  4930 | `	SyBlobRelease(&sPath);` |
|      68 |  4931 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  4932 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  4933 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  4934 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4935 | `			return SXERR_ABORT;` |
|       - |  4936 | `		}` |
|       1 |  4937 | `	}` |
|      68 |  4938 | `	return SXRET_OK;` |
|      35 |  4939 |  |
|       - |  4940 | `/*` |
|       - |  4941 | ` * Compile the stupid 'declare' language construct.` |
|       - |  4942 | ` *` |
|       - |  4943 | ` * According to the PHP language reference manual.` |
|       - |  4944 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  4945 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  4946 | ` *  declare (directive)` |
|       - |  4947 | ` *   statement` |
|       - |  4948 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  4949 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  4950 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  4951 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  4952 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  4953 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  4954 | ` * <?php` |
|       - |  4955 | ` * // these are the same:` |
|       - |  4956 | ` * // you can use this:` |
|       - |  4957 | ` * declare(ticks=1) {` |
|       - |  4958 | ` *   // entire script here` |
|       - |  4959 | ` * }` |
|       - |  4960 | ` * // or you can use this:` |
|       - |  4961 | ` * declare(ticks=1);` |
|       - |  4962 | ` * // entire script here` |
|       - |  4963 | ` * ?>` |
|       - |  4964 | ` *` |
|       - |  4965 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  4966 | ` */` |
|       8 |  4967 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       1 |  4968 |  |
|       9 |  4969 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       9 |  4970 | `	SyToken *pEnd = 0; /* cc warning */` |
|       - |  4971 | `	sxi32 rc;` |
|       9 |  4972 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       9 |  4973 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  4974 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  4975 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4976 | `			return SXERR_ABORT;` |
|       - |  4977 | `		}` |
|       5 |  4978 | `		goto Synchro;` |
|       - |  4979 | `	}` |
|       5 |  4980 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       - |  4981 | `	/* Delimit the directive */` |
|       5 |  4982 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pEnd);` |
|       5 |  4983 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  4984 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  4985 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4986 | `			return SXERR_ABORT;` |
|       - |  4987 | `		}` |
|     ! 0 |  4988 | `		return SXRET_OK;` |
|       - |  4989 | `	}` |
|       - |  4990 | `	/* Update the cursor */` |
|       5 |  4991 | `	pGen->pIn = &pEnd[1];` |
|       5 |  4992 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0  ){` |
|     ! 0 |  4993 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  4994 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4995 | `			return SXERR_ABORT;` |
|       - |  4996 | `		}` |
|     ! 0 |  4997 | `	}` |
|       - |  4998 | `	/* TICKET 1433-81: This construct is disabled in the current release of the PH7 engine. */` |
|       7 |  4999 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,nLine, /* Emit a notice */` |
|       - |  5000 | `		"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5001 | `		ph7_lib_version()` |
|       - |  5002 | `		);` |
|       - |  5003 | `	/*All done */` |
|       5 |  5004 | `	return SXRET_OK;` |
|       2 |  5005 | `Synchro:` |
|       - |  5006 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5007 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5008 | `		pGen->pIn++;` |
|       1 |  5009 | `	}` |
|       5 |  5010 | `	return SXRET_OK;` |
|       5 |  5011 |  |
|       - |  5012 | `/*` |
|       - |  5013 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5014 | ` * as follows:` |
|       - |  5015 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5016 | ` * {` |
|       - |  5017 | ` *   return "Making a cup of $type.\n";` |
|       - |  5018 | ` * }` |
|       - |  5019 | ` * Symisc eXtension.` |
|       - |  5020 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5021 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5022 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5023 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5024 | ` *      {` |
|       - |  5025 | ` *       var_dump($a);` |
|       - |  5026 | ` *      }` |
|       - |  5027 | ` *     //call test without args` |
|       - |  5028 | ` *      test();` |
|       - |  5029 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5030 | ` *      Example:` |
|       - |  5031 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5032 | ` * 3 -) Function overloading!!` |
|       - |  5033 | ` *      Example:` |
|       - |  5034 | ` *      function foo($a) {` |
|       - |  5035 | ` *   	  return $a.PHP_EOL;` |
|       - |  5036 | ` *	    }` |
|       - |  5037 | ` *	    function foo($a, $b) {` |
|       - |  5038 | ` *   	  return $a + $b;` |
|       - |  5039 | ` *	    }` |
|       - |  5040 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5041 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5042 | ` *      // Same arg` |
|       - |  5043 | ` *	   function foo(string $a)` |
|       - |  5044 | ` *	   {` |
|       - |  5045 | ` *	     echo "a is a string\n";` |
|       - |  5046 | ` *	     var_dump($a);` |
|       - |  5047 | ` *	   }` |
|       - |  5048 | ` *	  function foo(int $a)` |
|       - |  5049 | ` *	  {` |
|       - |  5050 | ` *	    echo "a is integer\n";` |
|       - |  5051 | ` *	    var_dump($a);` |
|       - |  5052 | ` *	  }` |
|       - |  5053 | ` *	  function foo(array $a)` |
|       - |  5054 | ` *	  {` |
|       - |  5055 | ` * 	    echo "a is an array\n";` |
|       - |  5056 | ` * 	    var_dump($a);` |
|       - |  5057 | ` *	  }` |
|       - |  5058 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5059 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5060 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5061 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5062 | ` * introduced by the PH7 engine.` |
|       - |  5063 | ` */` |
|   44852 |  5064 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5065 |  |
|       - |  5066 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5067 | `	SySet *pInstrContainer;` |
|       - |  5068 | `	sxi32 rc;` |
|       - |  5069 | `	/* Swap token stream */` |
|   44854 |  5070 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   44854 |  5071 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   44854 |  5072 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5073 | `	/* Compile the expression holding the argument value */` |
|   44854 |  5074 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5075 | `	/* Emit the done instruction */` |
|   44854 |  5076 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   44854 |  5077 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   44854 |  5078 | `	RE_SWAP_DELIMITER(pGen);` |
|   44854 |  5079 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5080 | `		return SXERR_ABORT;` |
|       - |  5081 | `	}` |
|   44854 |  5082 | `	return SXRET_OK;` |
|   22428 |  5083 |  |
|       - |  5084 | `/*` |
|       - |  5085 | ` * Collect function arguments one after one.` |
|       - |  5086 | ` * According to the PHP language reference manual.` |
|       - |  5087 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5088 | ` * list of expressions.` |
|       - |  5089 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5090 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5091 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5092 | ` * for more information.` |
|       - |  5093 | ` * Example #1 Passing arrays to functions` |
|       - |  5094 | ` * <?php` |
|       - |  5095 | ` * function takes_array($input)` |
|       - |  5096 | ` * {` |
|       - |  5097 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5098 | ` * }` |
|       - |  5099 | ` * ?>` |
|       - |  5100 | ` * Making arguments be passed by reference` |
|       - |  5101 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5102 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5103 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5104 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5105 | ` * to the argument name in the function definition:` |
|       - |  5106 | ` * Example #2 Passing function parameters by reference` |
|       - |  5107 | ` * <?php` |
|       - |  5108 | ` * function add_some_extra(&$string)` |
|       - |  5109 | ` * {` |
|       - |  5110 | ` *   $string .= 'and something extra.';` |
|       - |  5111 | ` * }` |
|       - |  5112 | ` * $str = 'This is a string, ';` |
|       - |  5113 | ` * add_some_extra($str);` |
|       - |  5114 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5115 | ` * ?>` |
|       - |  5116 | ` *` |
|       - |  5117 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5118 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5119 | ` * on these extension.` |
|       - |  5120 | ` */` |
|   53958 |  5121 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd)` |
|       2 |  5122 |  |
|       - |  5123 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5124 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5125 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5126 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5127 | `	sxi32 rc;` |
|       - |  5128 |  |
|   53960 |  5129 | `	pIn = pGen->pIn;` |
|   53960 |  5130 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5131 | `	/* Process arguments one after one */` |
|   68230 |  5132 | `	for(;;){` |
|  136462 |  5133 | `		if( pIn >= pEnd ){` |
|       - |  5134 | `			/* No more arguments to process */` |
|   53952 |  5135 | `			break;` |
|       - |  5136 | `		}` |
|   82512 |  5137 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   82512 |  5138 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   82512 |  5139 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   82512 |  5140 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5141 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  110590 |  5142 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   70747 |  5143 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   57574 |  5144 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   56148 |  5145 | `			sxu32 nLineLocal = pIn->nLine;` |
|   56148 |  5146 | `			sxi32 iTFlags = 0;` |
|   56148 |  5147 | `			pGen->pIn = pIn;` |
|   56148 |  5148 | `			rc = GenStateParseUnionTypeDecl(` |
|   28073 |  5149 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   28073 |  5150 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5151 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5152 | `				/* bAllowVoid */ 0,` |
|   28073 |  5153 | `						nLineLocal);` |
|   56148 |  5154 | `			pIn = pGen->pIn;` |
|   56148 |  5155 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5156 | `				return SXERR_ABORT;` |
|   56148 |  5157 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5158 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5159 | `				return SXERR_SYNTAX;` |
|   56146 |  5160 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5161 | `				if( pIn < pEnd ){` |
|       7 |  5162 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5163 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5164 | `						&pIn->sData);` |
|       3 |  5165 | `				}else{` |
|     ! 0 |  5166 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5167 | `						"syntax error, unexpected end of file");` |
|       - |  5168 | `				}` |
|       5 |  5169 | `				return SXERR_SYNTAX;` |
|       - |  5170 | `			}` |
|   56142 |  5171 | `			sArg.iFlags \|= iTFlags;` |
|   28070 |  5172 | `		}` |
|   82506 |  5173 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5174 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5175 | `			return rc;` |
|       - |  5176 | `		}` |
|   82506 |  5177 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5178 | `			/* Pass by reference,record that */` |
|    2828 |  5179 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2828 |  5180 | `			pIn++;` |
|    1413 |  5181 | `		}` |
|   82506 |  5182 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5183 | `			/* Variadic parameter: ...$args */` |
|      34 |  5184 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      34 |  5185 | `			pIn++;` |
|      16 |  5186 | `		}` |
|   82506 |  5187 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5188 | `			/* Invalid argument */` |
|     ! 0 |  5189 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5190 | `			return rc;` |
|       - |  5191 | `		}` |
|   82506 |  5192 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5193 | `		/* Copy argument name */` |
|   82506 |  5194 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   82506 |  5195 | `		if( zDup == 0 ){` |
|     ! 0 |  5196 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5197 | `			return SXERR_ABORT;` |
|       - |  5198 | `		}` |
|   82506 |  5199 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   82506 |  5200 | `		pIn++;` |
|   82506 |  5201 | `		if( pIn < pEnd ){` |
|   50994 |  5202 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5203 | `				SyToken *pDefend;` |
|   44856 |  5204 | `				sxi32 iNest = 0;` |
|   44856 |  5205 | `				pIn++; /* Jump the equal sign */` |
|   44856 |  5206 | `				pDefend = pIn;` |
|       - |  5207 | `				/* Process the default value associated with this argument */` |
|   95312 |  5208 | `				while( pDefend < pEnd ){` |
|   72876 |  5209 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   22420 |  5210 | `						break;` |
|       - |  5211 | `					}` |
|   50458 |  5212 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5213 | `						/* Increment nesting level */` |
|    2804 |  5214 | `						iNest++;` |
|   49057 |  5215 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5216 | `						/* Decrement nesting level */` |
|    2804 |  5217 | `						iNest--;` |
|    1401 |  5218 | `					}` |
|   50458 |  5219 | `					pDefend++;` |
|       2 |  5220 | `				}` |
|   44856 |  5221 | `				if( pIn >= pDefend ){` |
|       3 |  5222 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5223 | `					return rc;` |
|       - |  5224 | `				}` |
|       - |  5225 | `				/* Process default value */` |
|   44854 |  5226 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   44854 |  5227 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5228 | `					return rc;` |
|       - |  5229 | `				}` |
|       - |  5230 | `				/* Point beyond the default value */` |
|   44854 |  5231 | `				pIn = pDefend;` |
|   22426 |  5232 | `			}` |
|   50992 |  5233 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5234 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5235 | `				return rc;` |
|       - |  5236 | `			}` |
|   50992 |  5237 | `			pIn++; /* Jump the trailing comma */` |
|   25495 |  5238 | `		}` |
|       - |  5239 | `		/* Append argument signature */` |
|   82504 |  5240 | `		if( sArg.nType > 0 ){` |
|   56110 |  5241 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5242 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5618 |  5243 | `				int marker = 'o';` |
|    5618 |  5244 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5618 |  5245 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2810 |  5246 | `			}else{` |
|       - |  5247 | `				int c;` |
|   50494 |  5248 | `				c = 'n'; /* cc warning */` |
|       - |  5249 | `				/* Type leading character */` |
|   50494 |  5250 | `				switch(sArg.nType){` |
|     ! 0 |  5251 | `				case MEMOBJ_HASHMAP:` |
|       - |  5252 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5253 | `					c = 'h';` |
|     ! 0 |  5254 | `					break;` |
|    7016 |  5255 | `				case MEMOBJ_INT:` |
|       - |  5256 | `					/* Integer */` |
|   14034 |  5257 | `					c = 'i';` |
|   14034 |  5258 | `					break;` |
|     ! 0 |  5259 | `				case MEMOBJ_BOOL:` |
|       - |  5260 | `					/* Bool */` |
|     ! 0 |  5261 | `					c = 'b';` |
|     ! 0 |  5262 | `					break;` |
|     ! 0 |  5263 | `				case MEMOBJ_REAL:` |
|       - |  5264 | `					/* Float */` |
|     ! 0 |  5265 | `					c = 'f';` |
|     ! 0 |  5266 | `					break;` |
|   18223 |  5267 | `				case MEMOBJ_STRING:` |
|       - |  5268 | `					/* String */` |
|   36448 |  5269 | `					c = 's';` |
|   36448 |  5270 | `					break;` |
|       7 |  5271 | `				case MEMOBJ_OBJ:` |
|       - |  5272 | `					/* Object */` |
|      16 |  5273 | `					c = 'o';` |
|      14 |  5274 | `					break;` |
|     ! 0 |  5275 | `				default:` |
|     ! 0 |  5276 | `					break;` |
|       - |  5277 | `				}` |
|   50494 |  5278 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5279 | `			}` |
|   28056 |  5280 | `		}else{` |
|       - |  5281 | `			/* No type is associated with this parameter which mean` |
|       - |  5282 | `			 * that this function is not condidate for overloading.` |
|       - |  5283 | `			 */` |
|   26396 |  5284 | `			SyBlobRelease(&sSig);` |
|       - |  5285 | `		}` |
|       - |  5286 | `		/* Save in the argument set */` |
|   82504 |  5287 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5288 | `	}` |
|   53952 |  5289 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5290 | `		/* Save function signature */` |
|   33688 |  5291 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   16843 |  5292 | `	}` |
|   53952 |  5293 | `	return SXRET_OK;` |
|   26981 |  5294 |  |
|       - |  5295 | `/*` |
|       - |  5296 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5297 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5298 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5299 | ` */` |
|  149716 |  5300 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5301 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5302 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5303 | `	)` |
|       2 |  5304 |  |
|       - |  5305 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5306 | `	GenBlock *pBlock;` |
|       - |  5307 | `	sxu32 nGotoOfft;` |
|       - |  5308 | `	sxi32 rc;` |
|       - |  5309 | `	/* Attach the new function */` |
|  149718 |  5310 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  149718 |  5311 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5312 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5313 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5314 | `		return SXERR_ABORT;` |
|       - |  5315 | `	}` |
|  149718 |  5316 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5317 | `	/* Swap bytecode containers */` |
|  149718 |  5318 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  149718 |  5319 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5320 | `	/* Compile the body */` |
|  149718 |  5321 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5322 | `	/* Fix exception jumps now the destination is resolved */` |
|  149718 |  5323 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5324 | `	/* Emit the final return if not yet done */` |
|  149718 |  5325 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5326 | `	/* Fix gotos jumps now the destination is resolved */` |
|  149718 |  5327 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5328 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5329 | `	}` |
|  149718 |  5330 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5331 | `	/* Restore the default container */` |
|  149718 |  5332 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5333 | `	/* Leave function block */` |
|  149718 |  5334 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  149718 |  5335 | `	if( rc == SXERR_ABORT ){` |
|       - |  5336 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5337 | `		return SXERR_ABORT;` |
|       - |  5338 | `	}` |
|       - |  5339 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5340 | `	{` |
|  149718 |  5341 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5342 | `		sxu32 i;` |
| 3107984 |  5343 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 2958284 |  5344 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      18 |  5345 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      18 |  5346 | `				break;` |
|       - |  5347 | `			}` |
| 1479135 |  5348 | `		}` |
|       - |  5349 | `	}` |
|       - |  5350 | `	/* All done, function body compiled */` |
|  149718 |  5351 | `	return SXRET_OK;` |
|   74860 |  5352 |  |
|       - |  5353 | `/*` |
|       - |  5354 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5355 | ` * According to the PHP language reference manual.` |
|       - |  5356 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5357 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5358 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5359 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5360 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5361 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5362 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5363 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5364 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5365 | ` *` |
|       - |  5366 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5367 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5368 | ` * on these extension.` |
|       - |  5369 | ` */` |
|       - |  5370 | `/*` |
|       - |  5371 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5372 | ` */` |
|      62 |  5373 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5374 |  |
|       - |  5375 | `	sxu32 i;` |
|     190 |  5376 | `	for( i = 0; i < n; i++ ){` |
|     168 |  5377 | `		int a = zA[i], b = zB[i];` |
|     168 |  5378 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     168 |  5379 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     168 |  5380 | `		if( a != b ) return a - b;` |
|      65 |  5381 | `	}` |
|      24 |  5382 | `	return 0;` |
|      33 |  5383 |  |
|       - |  5384 | `/*` |
|       - |  5385 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5386 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5387 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5388 | ` */` |
|       - |  5389 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5390 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5391 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5392 |  |
|       - |  5393 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5394 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5395 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5396 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5397 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5398 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5399 |  |
|       - |  5400 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5401 | `struct PhlTypeAtom {` |
|       - |  5402 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5403 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5404 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5405 | `	sxu32 nCanon;` |
|       - |  5406 | `};` |
|       - |  5407 |  |
|       - |  5408 | `/*` |
|       - |  5409 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5410 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5411 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5412 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5413 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5414 | ` * already be consumed by the caller.` |
|       - |  5415 | ` */` |
|   56414 |  5416 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5417 |  |
|   56416 |  5418 | `	SyToken *pIn = pGen->pIn;` |
|   56416 |  5419 | `	SyZero(pOut, sizeof(*pOut));` |
|   56416 |  5420 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   56416 |  5421 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5422 | `		return SXERR_SYNTAX;` |
|       - |  5423 | `	}` |
|       - |  5424 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   56416 |  5425 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5426 | `		pIn++;` |
|       8 |  5427 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5428 | `			return SXERR_SYNTAX;` |
|       - |  5429 | `		}` |
|       3 |  5430 | `	}` |
|   56416 |  5431 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5432 | `		return SXERR_SYNTAX;` |
|       - |  5433 | `	}` |
|   56416 |  5434 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   50744 |  5435 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   50744 |  5436 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5437 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   50737 |  5438 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       8 |  5439 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   50727 |  5440 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   14152 |  5441 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   43649 |  5442 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   36526 |  5443 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   18312 |  5444 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      20 |  5445 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      41 |  5446 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5447 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5448 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5449 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5450 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5451 | `			pOut->sClass = pIn->sData;` |
|       4 |  5452 | `		}else{` |
|       3 |  5453 | `			return SXERR_SYNTAX;` |
|       - |  5454 | `		}` |
|   50742 |  5455 | `		pIn++;` |
|   25372 |  5456 | `	}else{` |
|       - |  5457 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5458 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    5674 |  5459 | `		SyString *pT = &pIn->sData;` |
|    5674 |  5460 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5461 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5462 | `			pIn++;` |
|    5669 |  5463 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      12 |  5464 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      12 |  5465 | `			pIn++;` |
|    5659 |  5466 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5467 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5468 | `			pIn++;` |
|       2 |  5469 | `		}else{` |
|       - |  5470 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    5652 |  5471 | `			SyToken *pFirst = pIn;` |
|    5652 |  5472 | `			SyToken *pLast = pIn;` |
|    5652 |  5473 | `			pOut->nType = SXU32_HIGH;` |
|    5652 |  5474 | `			pOut->sClass = pIn->sData;` |
|    5652 |  5475 | `			pIn++;` |
|    8478 |  5476 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    5655 |  5477 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  5478 | `				pLast = &pIn[1];` |
|       3 |  5479 | `				pIn += 2;` |
|       1 |  5480 | `			}` |
|    5652 |  5481 | `			if( pLast != pFirst ){` |
|       3 |  5482 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  5483 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  5484 | `				pOut->sClass.zString = zFirst;` |
|       3 |  5485 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  5486 | `			}` |
|       - |  5487 | `		}` |
|       - |  5488 | `	}` |
|   56414 |  5489 | `	pGen->pIn = pIn;` |
|   56414 |  5490 | `	return SXRET_OK;` |
|   28209 |  5491 |  |
|       - |  5492 |  |
|       - |  5493 | `/*` |
|       - |  5494 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  5495 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  5496 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  5497 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  5498 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  5499 | ` */` |
|   56326 |  5500 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  5501 |  |
|       - |  5502 | `	int i;` |
|   56328 |  5503 | `	int nNonNull = 0;` |
|  112726 |  5504 | `	for( i = 0; i < nAtoms; i++ ){` |
|   56400 |  5505 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   56390 |  5506 | `			nNonNull++;` |
|   28194 |  5507 | `		}` |
|   28201 |  5508 | `	}` |
|   56328 |  5509 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  5510 | `		/* Shorthand: ?T */` |
|      48 |  5511 | `		for( i = 0; i < nAtoms; i++ ){` |
|      48 |  5512 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      48 |  5513 | `			SyBlobAppend(pBlob, "?", 1);` |
|      48 |  5514 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5515 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  5516 | `			}else{` |
|      38 |  5517 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  5518 | `			}` |
|      48 |  5519 | `			return;` |
|     ! 0 |  5520 | `		}` |
|     ! 0 |  5521 | `	}` |
|       - |  5522 | `	{` |
|   56282 |  5523 | `		int bFirst = 1;` |
|       - |  5524 | `		/* 1) Classes in declaration order */` |
|  112628 |  5525 | `		for( i = 0; i < nAtoms; i++ ){` |
|   56348 |  5526 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    5646 |  5527 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    5646 |  5528 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    5646 |  5529 | `				bFirst = 0;` |
|    2822 |  5530 | `			}` |
|   28175 |  5531 | `		}` |
|       - |  5532 | `		/* 2) Built-ins in canonical order */` |
|       - |  5533 | `		{` |
|       - |  5534 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  5535 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  5536 | `			int k;` |
|  393962 |  5537 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  624986 |  5538 | `				for( i = 0; i < nAtoms; i++ ){` |
|  337996 |  5539 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   50692 |  5540 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   50692 |  5541 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   50692 |  5542 | `						bFirst = 0;` |
|   50692 |  5543 | `						break;` |
|       - |  5544 | `					}` |
|  143654 |  5545 | `				}` |
|  168842 |  5546 | `			}` |
|       - |  5547 | `		}` |
|       - |  5548 | `		/* 3) null suffix */` |
|   56282 |  5549 | `		if( bNullable ){` |
|       6 |  5550 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  5551 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  5552 | `		}` |
|       - |  5553 | `	}` |
|   28165 |  5554 |  |
|       - |  5555 |  |
|       - |  5556 | `/*` |
|       - |  5557 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  5558 | ` *` |
|       - |  5559 | ` * Outputs:` |
|       - |  5560 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  5561 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  5562 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  5563 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  5564 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  5565 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  5566 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  5567 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  5568 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  5569 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  5570 | ` *` |
|       - |  5571 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  5572 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  5573 | ` */` |
|   56336 |  5574 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  5575 | `	ph7_gen_state *pGen,` |
|       - |  5576 | `	sxu32 *pnType,` |
|       - |  5577 | `	SyString *pClass,` |
|       - |  5578 | `	SySet *pAlts,` |
|       - |  5579 | `	sxi32 *piTypeFlags,` |
|       - |  5580 | `	SyString *pTypeText,` |
|       - |  5581 | `	int iNullableFlag,` |
|       - |  5582 | `	int iUnionFlag,` |
|       - |  5583 | `	int bAllowVoid,` |
|       - |  5584 | `	sxu32 nLine` |
|       2 |  5585 | `){` |
|       - |  5586 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   56338 |  5587 | `	int nAtoms = 0;` |
|   56338 |  5588 | `	int bShortNullable = 0;` |
|   56338 |  5589 | `	int bExplicitNull = 0;` |
|       - |  5590 | `	sxi32 rc;` |
|   56338 |  5591 | `	*pnType = 0;` |
|   56338 |  5592 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   56338 |  5593 | `	*piTypeFlags = 0;` |
|   56338 |  5594 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  5595 |  |
|   56338 |  5596 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5597 | `		return SXRET_OK;` |
|       - |  5598 | `	}` |
|       - |  5599 | ``	/* Optional `?` shorthand prefix */`` |
|   56336 |  5600 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      44 |  5601 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      44 |  5602 | `		bShortNullable = 1;` |
|      44 |  5603 | `		pGen->pIn++;` |
|      44 |  5604 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5605 | `			return SXERR_SYNTAX;` |
|       - |  5606 | `		}` |
|      21 |  5607 | `	}` |
|       - |  5608 | `	/* First atom is mandatory */` |
|   56338 |  5609 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   56338 |  5610 | `	if( rc != SXRET_OK ){` |
|       3 |  5611 | `		return rc;` |
|       - |  5612 | `	}` |
|   56336 |  5613 | `	nAtoms = 1;` |
|       - |  5614 | ``	/* Subsequent atoms separated by `\|` */`` |
|   84620 |  5615 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   56455 |  5616 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      82 |  5617 | `		if( bShortNullable ){` |
|       - |  5618 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  5619 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  5620 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  5621 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  5622 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  5623 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  5624 | `		}` |
|      80 |  5625 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  5626 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5627 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  5628 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  5629 | `		}` |
|      80 |  5630 | ``		pGen->pIn++; /* skip `\|` */`` |
|      80 |  5631 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      80 |  5632 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  5633 | `			return rc;` |
|       - |  5634 | `		}` |
|      80 |  5635 | `		nAtoms++;` |
|       2 |  5636 | `	}` |
|       - |  5637 | `	/* Validation pass.` |
|       - |  5638 | `	 *` |
|       - |  5639 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  5640 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  5641 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  5642 | `	 */` |
|       - |  5643 | `	{` |
|       - |  5644 | `		int i, j;` |
|   56334 |  5645 | `		int bHasNonNull = 0;` |
|  112738 |  5646 | `		for( i = 0; i < nAtoms; i++ ){` |
|   56412 |  5647 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      12 |  5648 | `				if( nAtoms > 1 ){` |
|       3 |  5649 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5650 | `						"Void can only be used as a standalone type");` |
|       3 |  5651 | `					return SXERR_SYNTAX;` |
|       - |  5652 | `				}` |
|      10 |  5653 | `				if( !bAllowVoid ){` |
|     ! 0 |  5654 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5655 | `						"void cannot be used here");` |
|     ! 0 |  5656 | `					return SXERR_SYNTAX;` |
|       - |  5657 | `				}` |
|      10 |  5658 | `				if( bShortNullable ){` |
|     ! 0 |  5659 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5660 | `						"Void type cannot be nullable");` |
|     ! 0 |  5661 | `					return SXERR_SYNTAX;` |
|       - |  5662 | `				}` |
|       4 |  5663 | `			}` |
|   56410 |  5664 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  5665 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  5666 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  5667 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  5668 | `				 * does not return), and folding them would mislead any` |
|       - |  5669 | `				 * future return-enforcement work. */` |
|       3 |  5670 | `				if( nAtoms > 1 ){` |
|       3 |  5671 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5672 | `						"never can only be used as a standalone type");` |
|       3 |  5673 | `					return SXERR_SYNTAX;` |
|       - |  5674 | `				}` |
|     ! 0 |  5675 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5676 | `					"never type is not yet implemented");` |
|     ! 0 |  5677 | `				return SXERR_SYNTAX;` |
|       - |  5678 | `			}` |
|   56408 |  5679 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  5680 | `				bExplicitNull = 1;` |
|       7 |  5681 | `			}else{` |
|   56398 |  5682 | `				bHasNonNull = 1;` |
|       - |  5683 | `			}` |
|       - |  5684 | `			/* Duplicate detection */` |
|   56516 |  5685 | `			for( j = 0; j < i; j++ ){` |
|     112 |  5686 | `				int bDup = 0;` |
|     112 |  5687 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  5688 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5689 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  5690 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  5691 | `								aAtoms[j].sClass.zString,` |
|      12 |  5692 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  5693 | `							bDup = 1;` |
|     ! 0 |  5694 | `						}` |
|       8 |  5695 | `					}else{` |
|       3 |  5696 | `						bDup = 1;` |
|       - |  5697 | `					}` |
|       7 |  5698 | `				}` |
|     112 |  5699 | `				if( bDup ){` |
|       - |  5700 | `					const char *zName;` |
|       - |  5701 | `					sxu32 nName;` |
|       3 |  5702 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  5703 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  5704 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  5705 | `					}else{` |
|       3 |  5706 | `						zName = aAtoms[i].zCanon;` |
|       3 |  5707 | `						nName = aAtoms[i].nCanon;` |
|       - |  5708 | `					}` |
|       4 |  5709 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  5710 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  5711 | `					return SXERR_SYNTAX;` |
|       - |  5712 | `				}` |
|      56 |  5713 | `			}` |
|   28204 |  5714 | `		}` |
|   56328 |  5715 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  5716 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5717 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  5718 | `			return SXERR_SYNTAX;` |
|       - |  5719 | `		}` |
|       - |  5720 | `	}` |
|       - |  5721 | `	/* Compute nullability flag */` |
|   56328 |  5722 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      52 |  5723 | `		*piTypeFlags \|= iNullableFlag;` |
|      25 |  5724 | `	}` |
|       - |  5725 | `	/* Build canonical type text */` |
|   56328 |  5726 | `	if( pTypeText ){` |
|       - |  5727 | `		SyBlob sBlob;` |
|   56328 |  5728 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   84471 |  5729 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   28163 |  5730 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   56328 |  5731 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   84479 |  5732 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   56318 |  5733 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   56320 |  5734 | `			if( zDup ){` |
|   56320 |  5735 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   28159 |  5736 | `			}` |
|   28159 |  5737 | `		}` |
|   56328 |  5738 | `		SyBlobRelease(&sBlob);` |
|   28163 |  5739 | `	}` |
|       - |  5740 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  5741 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  5742 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  5743 | `	{` |
|   56328 |  5744 | `		int nNonNull = 0;` |
|   56328 |  5745 | `		int iNonNullIdx = -1;` |
|       - |  5746 | `		int i;` |
|  112726 |  5747 | `		for( i = 0; i < nAtoms; i++ ){` |
|   56400 |  5748 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   56390 |  5749 | `				nNonNull++;` |
|   56390 |  5750 | `				iNonNullIdx = i;` |
|   28194 |  5751 | `			}` |
|   28201 |  5752 | `		}` |
|   56328 |  5753 | `		if( nNonNull <= 1 ){` |
|       - |  5754 | `			/* Fast path: store as single type. */` |
|   56282 |  5755 | `			if( iNonNullIdx >= 0 ){` |
|   56282 |  5756 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   56282 |  5757 | `				if( pA->nType == SXU32_HIGH ){` |
|    8447 |  5758 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    2815 |  5759 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    5632 |  5760 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    5632 |  5761 | `					*pnType = SXU32_HIGH;` |
|    5632 |  5762 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   53467 |  5763 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      10 |  5764 | `					*pnType = MEMOBJ_VOID;` |
|       6 |  5765 | `				}else{` |
|       - |  5766 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  5767 | `					 * pass above rejects it as not-yet-implemented. */` |
|   50644 |  5768 | `					*pnType = pA->nType;` |
|       - |  5769 | `				}` |
|   28140 |  5770 | `			}` |
|   28142 |  5771 | `		}else{` |
|       - |  5772 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      48 |  5773 | `			*piTypeFlags \|= iUnionFlag;` |
|     160 |  5774 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  5775 | `				ph7_type_alt sAlt;` |
|     114 |  5776 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     110 |  5777 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     110 |  5778 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      38 |  5779 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      12 |  5780 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      26 |  5781 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      26 |  5782 | `					sAlt.nType = SXU32_HIGH;` |
|      26 |  5783 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      14 |  5784 | `				}else{` |
|      86 |  5785 | `					sAlt.nType = aAtoms[i].nType;` |
|      86 |  5786 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  5787 | `				}` |
|     110 |  5788 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      56 |  5789 | `			}` |
|       - |  5790 | `		}` |
|       - |  5791 | `	}` |
|   56328 |  5792 | `	return SXRET_OK;` |
|   28170 |  5793 |  |
|       - |  5794 |  |
|       - |  5795 | `/*` |
|       - |  5796 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  5797 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  5798 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  5799 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  5800 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  5801 | `` *          and union types `: T\|U`.`` |
|       - |  5802 | ` */` |
|  172264 |  5803 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  5804 |  |
|  172266 |  5805 | `	sxi32 iFlags = 0;` |
|       - |  5806 | `	sxi32 rc;` |
|       - |  5807 | `	sxu32 nLine;` |
|  172266 |  5808 | `	pFunc->nReturnType = 0;` |
|  172266 |  5809 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  172266 |  5810 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  172266 |  5811 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  172180 |  5812 | `		return SXRET_OK;` |
|       - |  5813 | `	}` |
|      88 |  5814 | `	pGen->pIn++; /* Skip ':' */` |
|      88 |  5815 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5816 | `		return SXRET_OK;` |
|       - |  5817 | `	}` |
|      88 |  5818 | `	nLine = pGen->pIn->nLine;` |
|      88 |  5819 | `	rc = GenStateParseUnionTypeDecl(` |
|      43 |  5820 | `		pGen,` |
|      43 |  5821 | `		&pFunc->nReturnType,` |
|      43 |  5822 | `		&pFunc->sReturnClass,` |
|      43 |  5823 | `		&pFunc->aReturnUnion,` |
|       - |  5824 | `		&iFlags,` |
|      43 |  5825 | `		&pFunc->sReturnTypeName,` |
|       - |  5826 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  5827 | `		/* iUnionFlag */ 0,` |
|       - |  5828 | `		/* bAllowVoid */ 1,` |
|      43 |  5829 | `		nLine);` |
|      43 |  5830 | `	(void)iFlags;` |
|      88 |  5831 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5832 | `		return SXERR_ABORT;` |
|       - |  5833 | `	}` |
|      88 |  5834 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  5835 | `		/* Error already reported */` |
|     ! 0 |  5836 | `		return SXERR_SYNTAX;` |
|       - |  5837 | `	}` |
|      88 |  5838 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  5839 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  5840 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  5841 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  5842 | `				&pGen->pIn->sData);` |
|       3 |  5843 | `		}else{` |
|     ! 0 |  5844 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  5845 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  5846 | `		}` |
|       5 |  5847 | `		return SXERR_SYNTAX;` |
|       - |  5848 | `	}` |
|      84 |  5849 | `	return SXRET_OK;` |
|   86134 |  5850 |  |
|       - |  5851 |  |
|   37198 |  5852 | `static sxi32 GenStateCompileFunc(` |
|       - |  5853 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  5854 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  5855 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  5856 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  5857 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  5858 | `	)` |
|       2 |  5859 |  |
|       - |  5860 | `	ph7_vm_func *pFunc;` |
|       - |  5861 | `	SyToken *pEnd;` |
|       - |  5862 | `	sxu32 nLine;` |
|       - |  5863 | `	char *zName;` |
|       - |  5864 | `	sxi32 rc;` |
|       - |  5865 | `	/* Extract line number */` |
|   37200 |  5866 | `	nLine = pGen->pIn->nLine;` |
|       - |  5867 | `	/* Jump the left parenthesis '(' */` |
|   37200 |  5868 | `	pGen->pIn++;` |
|       - |  5869 | `	/* Delimit the function signature */` |
|   37200 |  5870 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   37200 |  5871 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  5872 | `		/* Syntax error */` |
|       7 |  5873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  5874 | `		if( rc == SXERR_ABORT ){` |
|       - |  5875 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  5876 | `			return SXERR_ABORT;` |
|       - |  5877 | `		}` |
|       7 |  5878 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  5879 | `		return SXRET_OK;` |
|       - |  5880 | `	}` |
|       - |  5881 | `	/* Create the function state */` |
|   37194 |  5882 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   37194 |  5883 | `	if( pFunc == 0 ){` |
|     ! 0 |  5884 | `		goto OutOfMem;` |
|       - |  5885 | `	}` |
|       - |  5886 | `	/* Build the function name, prepending namespace if active */` |
|   37201 |  5887 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  5888 | `		SyBlob sFQN;` |
|       - |  5889 | `		sxu32 nLen;` |
|      16 |  5890 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  5891 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  5892 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  5893 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  5894 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  5895 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  5896 | `		SyBlobRelease(&sFQN);` |
|      16 |  5897 | `		if( zName == 0 ){` |
|     ! 0 |  5898 | `			goto OutOfMem;` |
|       - |  5899 | `		}` |
|      16 |  5900 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  5901 | `	}else{` |
|   37180 |  5902 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   37180 |  5903 | `		if( zName == 0 ){` |
|     ! 0 |  5904 | `			goto OutOfMem;` |
|       - |  5905 | `		}` |
|   37180 |  5906 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  5907 | `	}` |
|   37194 |  5908 | `	if( pGen->pIn < pEnd ){` |
|       - |  5909 | `		/* Collect function arguments */` |
|   25796 |  5910 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd);` |
|   25796 |  5911 | `		if( rc == SXERR_ABORT ){` |
|       - |  5912 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5913 | `			return SXERR_ABORT;` |
|       - |  5914 | `		}` |
|   12897 |  5915 | `	}` |
|       - |  5916 | `	/* Point past ')' and parse optional return type ': type' */` |
|   37194 |  5917 | `	pGen->pIn = &pEnd[1];` |
|       - |  5918 | `	{` |
|   37194 |  5919 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   37194 |  5920 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  5921 | `			return SXERR_ABORT;` |
|   37194 |  5922 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  5923 | `			return SXERR_SYNTAX;` |
|       - |  5924 | `		}` |
|       - |  5925 | `	}` |
|   37190 |  5926 | `	if( bHandleClosure ){` |
|       - |  5927 | `		ph7_vm_func_closure_env sEnv;` |
|     170 |  5928 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     168 |  5929 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      93 |  5930 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 |  5931 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  5932 | `				/* Closure,record environment variable */` |
|      16 |  5933 | `				pGen->pIn++;` |
|      16 |  5934 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  5935 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  5936 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5937 | `						return SXERR_ABORT;` |
|       - |  5938 | `					}` |
|     ! 0 |  5939 | `				}` |
|      16 |  5940 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  5941 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 |  5942 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 |  5943 | `					int iFlagsLocal = 0;` |
|      34 |  5944 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 |  5945 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 |  5946 | `						break;` |
|       - |  5947 | `					}` |
|      20 |  5948 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 |  5949 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  5950 | `						/* Pass by reference,record that */` |
|     ! 0 |  5951 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  5952 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  5953 | `							);` |
|     ! 0 |  5954 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  5955 | `						pGen->pIn++;` |
|     ! 0 |  5956 | `					}` |
|      18 |  5957 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 |  5958 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5959 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  5960 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  5961 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  5962 | `								return SXERR_ABORT;` |
|       - |  5963 | `							}` |
|       - |  5964 | `							/* Find the closing parenthesis */` |
|     ! 0 |  5965 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  5966 | `								pGen->pIn++;` |
|     ! 0 |  5967 | `							}` |
|     ! 0 |  5968 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  5969 | `								pGen->pIn++;` |
|     ! 0 |  5970 | `							}` |
|     ! 0 |  5971 | `							break;` |
|       - |  5972 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  5973 | `					}else{` |
|       - |  5974 | `						SyString *pNameLocal;` |
|       - |  5975 | `						char *zDup;` |
|       - |  5976 | `						/* Duplicate variable name */` |
|      20 |  5977 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 |  5978 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 |  5979 | `						if( zDup ){` |
|       - |  5980 | `							/* Zero the structure */` |
|      20 |  5981 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 |  5982 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 |  5983 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 |  5984 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 |  5985 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  5986 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  5987 | `									got_this = 1;` |
|     ! 0 |  5988 | `							}` |
|       - |  5989 | `							/* Save imported variable */` |
|      20 |  5990 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 |  5991 | `						}else{` |
|     ! 0 |  5992 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  5993 | `							 return SXERR_ABORT;` |
|       - |  5994 | `						}` |
|       - |  5995 | `					}` |
|      20 |  5996 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 |  5997 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  5998 | `						/* Ignore trailing commas */` |
|       7 |  5999 | `						pGen->pIn++;` |
|       1 |  6000 | `					}` |
|       2 |  6001 | `				}` |
|      16 |  6002 | `				if( !got_this ){` |
|       - |  6003 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6004 | `					 * available to the closure environment.` |
|       - |  6005 | `					 */` |
|      16 |  6006 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 |  6007 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 |  6008 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 |  6009 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 |  6010 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 |  6011 | `				}` |
|      16 |  6012 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6013 | `					/* Mark as closure */` |
|      16 |  6014 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 |  6015 | `				}` |
|       7 |  6016 | `		}` |
|      84 |  6017 | `	}` |
|       - |  6018 | `	/* Compile the body */` |
|   37190 |  6019 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   37190 |  6020 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6021 | `		return SXERR_ABORT;` |
|       - |  6022 | `	}` |
|   37190 |  6023 | `	if( ppFunc ){` |
|     170 |  6024 | `		*ppFunc = pFunc;` |
|      84 |  6025 | `	}` |
|   37190 |  6026 | `	rc = SXRET_OK;` |
|   37190 |  6027 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6028 | `		/* Finally register the function */` |
|   37176 |  6029 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   18587 |  6030 | `	}` |
|   37190 |  6031 | `	if( rc == SXRET_OK ){` |
|   37190 |  6032 | `		return SXRET_OK;` |
|       - |  6033 | `	}` |
|       - |  6034 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6035 | `OutOfMem:` |
|       - |  6036 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6037 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6038 | `	 */` |
|     ! 0 |  6039 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6040 | `	return SXERR_ABORT;` |
|   18601 |  6041 |  |
|       - |  6042 | `/*` |
|       - |  6043 | ` * Compile a standard PHP function.` |
|       - |  6044 | ` *  Refer to the block-comment above for more information.` |
|       - |  6045 | ` */` |
|   37036 |  6046 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6047 |  |
|       - |  6048 | `	SyString *pName;` |
|       - |  6049 | `	sxi32 iFlags;` |
|       - |  6050 | `	sxu32 nLine;` |
|       - |  6051 | `	sxi32 rc;` |
|       - |  6052 |  |
|   37038 |  6053 | `	nLine = pGen->pIn->nLine;` |
|   37038 |  6054 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   37038 |  6055 | `	iFlags = 0;` |
|   37038 |  6056 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6057 | `		/* Return by reference,remember that */` |
|       7 |  6058 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6059 | `		/* Jump the '&' token */` |
|       7 |  6060 | `		pGen->pIn++;` |
|       3 |  6061 | `	}` |
|   37038 |  6062 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6063 | `		/* Invalid function name */` |
|       5 |  6064 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6065 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6066 | `			return SXERR_ABORT;` |
|       - |  6067 | `		}` |
|       - |  6068 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6069 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6070 | `			pGen->pIn++;` |
|       1 |  6071 | `		}` |
|       5 |  6072 | `		return SXRET_OK;` |
|       - |  6073 | `	}` |
|   37034 |  6074 | `	pName = &pGen->pIn->sData;` |
|   37034 |  6075 | `	nLine = pGen->pIn->nLine;` |
|       - |  6076 | `	/* Jump the function name */` |
|   37034 |  6077 | `	pGen->pIn++;` |
|   37034 |  6078 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6079 | `		/* Syntax error */` |
|       3 |  6080 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6081 | `		if( rc == SXERR_ABORT ){` |
|       - |  6082 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6083 | `			return SXERR_ABORT;` |
|       - |  6084 | `		}` |
|       - |  6085 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6086 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6087 | `			pGen->pIn++;` |
|     ! 0 |  6088 | `		}` |
|       3 |  6089 | `		return SXRET_OK;` |
|       - |  6090 | `	}` |
|       - |  6091 | `	/* Compile function body */` |
|   37032 |  6092 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   37032 |  6093 | `	return rc;` |
|   18520 |  6094 |  |
|       - |  6095 | `/*` |
|       - |  6096 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6097 | ` * According to the PHP language reference manual` |
|       - |  6098 | ` *  Visibility:` |
|       - |  6099 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6100 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6101 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6102 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6103 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6104 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6105 | ` */` |
|  171796 |  6106 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6107 |  |
|  171798 |  6108 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8476 |  6109 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  163324 |  6110 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   19658 |  6111 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6112 | `	}` |
|       - |  6113 | `	/* Assume public by default */` |
|  143668 |  6114 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   85900 |  6115 |  |
|       - |  6116 | `/*` |
|       - |  6117 | ` * Compile a class constant.` |
|       - |  6118 | ` * According to the PHP language reference manual` |
|       - |  6119 | ` *  Class Constants` |
|       - |  6120 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6121 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6122 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6123 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6124 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6125 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6126 | ` * Symisc eXtension.` |
|       - |  6127 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6128 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6129 | ` *  Example:` |
|       - |  6130 | ` *   class Test{` |
|       - |  6131 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6132 | ` *   };` |
|       - |  6133 | ` *   var_dump(TEST::MyConst);` |
|       - |  6134 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6135 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6136 | ` */` |
|      30 |  6137 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6138 |  |
|      32 |  6139 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6140 | `	SySet *pInstrContainer;` |
|       - |  6141 | `	ph7_class_attr *pCons;` |
|       - |  6142 | `	SyString *pName;` |
|       - |  6143 | `	sxi32 rc;` |
|       - |  6144 | `	/* Extract visibility level */` |
|      32 |  6145 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6146 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6147 | `loop:` |
|       - |  6148 | `	/* Mark as constant */` |
|      32 |  6149 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6150 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6151 | `		/* Invalid constant name */` |
|     ! 0 |  6152 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6153 | `		if( rc == SXERR_ABORT ){` |
|       - |  6154 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6155 | `			return SXERR_ABORT;` |
|       - |  6156 | `		}` |
|     ! 0 |  6157 | `		goto Synchronize;` |
|       - |  6158 | `	}` |
|       - |  6159 | `	/* Peek constant name */` |
|      32 |  6160 | `	pName = &pGen->pIn->sData;` |
|       - |  6161 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6162 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6163 | `		/* Reserved constant name */` |
|     ! 0 |  6164 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6165 | `		if( rc == SXERR_ABORT ){` |
|       - |  6166 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6167 | `			return SXERR_ABORT;` |
|       - |  6168 | `		}` |
|     ! 0 |  6169 | `		goto Synchronize;` |
|       - |  6170 | `	}` |
|       - |  6171 | `	/* Advance the stream cursor */` |
|      32 |  6172 | `	pGen->pIn++;` |
|      32 |  6173 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6174 | `		/* Invalid declaration */` |
|     ! 0 |  6175 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6176 | `		if( rc == SXERR_ABORT ){` |
|       - |  6177 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6178 | `			return SXERR_ABORT;` |
|       - |  6179 | `		}` |
|     ! 0 |  6180 | `		goto Synchronize;` |
|       - |  6181 | `	}` |
|      32 |  6182 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6183 | `	/* Allocate a new class attribute */` |
|      32 |  6184 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6185 | `	if( pCons == 0 ){` |
|     ! 0 |  6186 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6187 | `		return SXERR_ABORT;` |
|       - |  6188 | `	}` |
|       - |  6189 | `	/* Swap bytecode container */` |
|      32 |  6190 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6191 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6192 | `	/* Compile constant value.` |
|       - |  6193 | `	 */` |
|      32 |  6194 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6195 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6196 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6197 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6198 | `			return SXERR_ABORT;` |
|       - |  6199 | `		}` |
|       1 |  6200 | `	}` |
|       - |  6201 | `	/* Emit the done instruction */` |
|      32 |  6202 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6203 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6204 | `	if( rc == SXERR_ABORT ){` |
|       - |  6205 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6206 | `		return SXERR_ABORT;` |
|       - |  6207 | `	}` |
|       - |  6208 | `	/* All done,install the constant */` |
|      32 |  6209 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6210 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6211 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6212 | `		return SXERR_ABORT;` |
|       - |  6213 | `	}` |
|      32 |  6214 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6215 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6216 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6217 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6218 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6219 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6220 | `				pTok--;` |
|     ! 0 |  6221 | `			}` |
|     ! 0 |  6222 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6223 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6224 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6225 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6226 | `				return SXERR_ABORT;` |
|       - |  6227 | `			}` |
|     ! 0 |  6228 | `		}else{` |
|     ! 0 |  6229 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6230 | `				goto loop;` |
|       - |  6231 | `			}` |
|       - |  6232 | `		}` |
|     ! 0 |  6233 | `	}` |
|      32 |  6234 | `	return SXRET_OK;` |
|     ! 0 |  6235 | `Synchronize:` |
|       - |  6236 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6237 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6238 | `		pGen->pIn++;` |
|     ! 0 |  6239 | `	}` |
|     ! 0 |  6240 | `	return SXERR_CORRUPT;` |
|      17 |  6241 |  |
|       - |  6242 | `/*` |
|       - |  6243 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6244 | ` * According to the PHP language reference manual` |
|       - |  6245 | ` *  Properties` |
|       - |  6246 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6247 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6248 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6249 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6250 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6251 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6252 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6253 | ` * Symisc eXtension.` |
|       - |  6254 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6255 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6256 | ` *  Example:` |
|       - |  6257 | ` *   class Test{` |
|       - |  6258 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6259 | ` *   };` |
|       - |  6260 | ` *   var_dump(TEST::myVar);` |
|       - |  6261 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6262 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6263 | ` */` |
|       - |  6264 | `/*` |
|       - |  6265 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6266 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6267 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6268 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6269 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6270 | ` */` |
|  112604 |  6271 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6272 |  |
|  112606 |  6273 | `	SyToken *p = pStart;` |
|  112606 |  6274 | `	if( p >= pEnd ) return 0;` |
|  112606 |  6275 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6276 | `		p++;` |
|      16 |  6277 | `		if( p >= pEnd ) return 0;` |
|       7 |  6278 | `	}` |
|  112606 |  6279 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6280 | `		p++;` |
|       3 |  6281 | `		if( p >= pEnd ) return 0;` |
|       1 |  6282 | `	}` |
|  112606 |  6283 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6284 | `		return 0;` |
|       - |  6285 | `	}` |
|       - |  6286 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6287 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6288 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6289 | `	 * dispatch site. */` |
|  112606 |  6290 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  112592 |  6291 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  112635 |  6292 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    2941 |  6293 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  112502 |  6294 | `			return 0;` |
|       - |  6295 | `		}` |
|      45 |  6296 | `	}` |
|     106 |  6297 | `	p++;` |
|       - |  6298 | `	/* Consume optional namespace path */` |
|     108 |  6299 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6300 | `		p += 2;` |
|       1 |  6301 | `	}` |
|       - |  6302 | ``	/* Consume any `\| Type` union alternatives */`` |
|     174 |  6303 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      72 |  6304 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6305 | `		p++;` |
|      14 |  6306 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6307 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6308 | `		p++;` |
|      14 |  6309 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6310 | `			p += 2;` |
|     ! 0 |  6311 | `		}` |
|       2 |  6312 | `	}` |
|     106 |  6313 | `	if( p >= pEnd ) return 0;` |
|     106 |  6314 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   56304 |  6315 |  |
|       - |  6316 |  |
|       - |  6317 | `/*` |
|       - |  6318 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6319 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6320 | ` * if not). Recognized forms:` |
|       - |  6321 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6322 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6323 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6324 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6325 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6326 | ` * on unrecoverable error.` |
|       - |  6327 | ` *` |
|       - |  6328 | ` * When a type is parsed:` |
|       - |  6329 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6330 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6331 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6332 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6333 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6334 | ` */` |
|     104 |  6335 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6336 | `	ph7_gen_state *pGen,` |
|       - |  6337 | `	sxu32 *pnType,` |
|       - |  6338 | `	SyString *pClass,` |
|       - |  6339 | `	sxi32 *piTypeFlags,` |
|       - |  6340 | `	SyString *pTypeText,` |
|       - |  6341 | `	SySet *pAlts` |
|       2 |  6342 | `){` |
|     106 |  6343 | `	sxi32 iFlags = 0;` |
|       - |  6344 | `	sxi32 rc;` |
|     106 |  6345 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6346 | `		return SXRET_OK;` |
|       - |  6347 | `	}` |
|       - |  6348 | `	/* If the first token is '$', there's no type */` |
|     106 |  6349 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6350 | `		return SXRET_OK;` |
|       - |  6351 | `	}` |
|     106 |  6352 | `	rc = GenStateParseUnionTypeDecl(` |
|      52 |  6353 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6354 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6355 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6356 | `		/* bAllowVoid */ 0,` |
|     104 |  6357 | `		pGen->pIn->nLine);` |
|     106 |  6358 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6359 | `		return rc;` |
|       - |  6360 | `	}` |
|       - |  6361 | `	/* Verify next token is '$' (start of property name) */` |
|     106 |  6362 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6363 | `		return SXERR_SYNTAX;` |
|       - |  6364 | `	}` |
|     106 |  6365 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     106 |  6366 | `	return SXRET_OK;` |
|      54 |  6367 |  |
|       - |  6368 |  |
|   36772 |  6369 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6370 |  |
|   36774 |  6371 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6372 | `	ph7_class_attr *pAttr;` |
|       - |  6373 | `	SyString *pName;` |
|       - |  6374 | `	sxi32 rc;` |
|   36774 |  6375 | `	sxu32 nType = 0;` |
|       - |  6376 | `	SyString sTypeClass;` |
|       - |  6377 | `	SyString sTypeText;` |
|       - |  6378 | `	SySet aUnionAlts;` |
|   36774 |  6379 | `	sxi32 iTypeFlags = 0;` |
|   36774 |  6380 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   36774 |  6381 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   36774 |  6382 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6383 | `	/* Extract visibility level */` |
|   36774 |  6384 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6385 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   36826 |  6386 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     106 |  6387 | `		SyToken *pTypeTok = pGen->pIn;` |
|       - |  6388 | `		/* A leading '?' is part of the type, look past it when sniffing the` |
|       - |  6389 | `		 * type keyword for the disallowed list. */` |
|     111 |  6390 | `		if( (pTypeTok->nType & PH7_TK_OP) && pTypeTok->sData.nByte == 1` |
|      16 |  6391 | `		 && pTypeTok->sData.zString[0] == '?' && pTypeTok + 1 < pGen->pEnd ){` |
|      16 |  6392 | `			pTypeTok = pTypeTok + 1;` |
|       7 |  6393 | `		}` |
|       - |  6394 | `		/* Reject disallowed standalone property types up front (when there's` |
|       - |  6395 | ``		 * no `\|` ahead): void, callable, never, mixed, iterable. The union`` |
|       - |  6396 | `		 * parser also rejects void/never inside unions; here we only catch` |
|       - |  6397 | `		 * the simple single-token form so the existing single-type error` |
|       - |  6398 | `		 * messages stay intact. */` |
|     106 |  6399 | `		if( pTypeTok->nType & PH7_TK_ID ){` |
|      14 |  6400 | `			SyString *pT = &pTypeTok->sData;` |
|      14 |  6401 | `			SyToken *pAfter = pTypeTok + 1;` |
|      26 |  6402 | `			int bSingle = (pAfter >= pGen->pEnd` |
|      12 |  6403 | `				\|\| (pAfter->nType & PH7_TK_DOLLAR)` |
|      22 |  6404 | `				\|\| !((pAfter->nType & PH7_TK_OP) && pAfter->sData.nByte == 1` |
|       4 |  6405 | `					&& pAfter->sData.zString[0] == '\|'));` |
|      15 |  6406 | `			if( bSingle && (` |
|       8 |  6407 | `				 (pT->nByte == 4 && SyMemcmpNoCase(pT->zString,"void",4) == 0)` |
|       8 |  6408 | `			 \|\| (pT->nByte == 5 && SyMemcmpNoCase(pT->zString,"never",5) == 0)` |
|       8 |  6409 | `			 \|\| (pT->nByte == 5 && SyMemcmpNoCase(pT->zString,"mixed",5) == 0)` |
|       8 |  6410 | `			 \|\| (pT->nByte == 8 && SyMemcmpNoCase(pT->zString,"callable",8) == 0)` |
|       8 |  6411 | `			 \|\| (pT->nByte == 8 && SyMemcmpNoCase(pT->zString,"iterable",8) == 0)) ){` |
|     ! 0 |  6412 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  6413 | `					"Property cannot have type %z",pT);` |
|     ! 0 |  6414 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  6415 | `					return SXERR_ABORT;` |
|       - |  6416 | `				}` |
|     ! 0 |  6417 | `				goto Synchronize;` |
|       - |  6418 | `			}` |
|       6 |  6419 | `		}` |
|     106 |  6420 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     106 |  6421 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6422 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6423 | `			goto Synchronize;` |
|     106 |  6424 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  6425 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6426 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  6427 | `				&pGen->pIn->sData);` |
|     ! 0 |  6428 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6429 | `				return SXERR_ABORT;` |
|       - |  6430 | `			}` |
|     ! 0 |  6431 | `			goto Synchronize;` |
|     106 |  6432 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6433 | `			return SXERR_ABORT;` |
|       - |  6434 | `		}` |
|      52 |  6435 | `	}` |
|     ! 0 |  6436 | `loop:` |
|   36778 |  6437 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6438 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  6439 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6440 | `			return SXERR_ABORT;` |
|       - |  6441 | `		}` |
|     ! 0 |  6442 | `		goto Synchronize;` |
|       - |  6443 | `	}` |
|   36778 |  6444 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   36778 |  6445 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  6446 | `		/* Invalid attribute name */` |
|     ! 0 |  6447 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  6448 | `		if( rc == SXERR_ABORT ){` |
|       - |  6449 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6450 | `			return SXERR_ABORT;` |
|       - |  6451 | `		}` |
|     ! 0 |  6452 | `		goto Synchronize;` |
|       - |  6453 | `	}` |
|       - |  6454 | `	/* Peek attribute name */` |
|   36778 |  6455 | `	pName = &pGen->pIn->sData;` |
|       - |  6456 | `	/* Advance the stream cursor */` |
|   36778 |  6457 | `	pGen->pIn++;` |
|   36778 |  6458 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  6459 | `		/* Invalid declaration */` |
|       3 |  6460 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  6461 | `		if( rc == SXERR_ABORT ){` |
|       - |  6462 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6463 | `			return SXERR_ABORT;` |
|       - |  6464 | `		}` |
|       3 |  6465 | `		goto Synchronize;` |
|       - |  6466 | `	}` |
|       - |  6467 | `	/* Allocate a new class attribute */` |
|   36776 |  6468 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   36776 |  6469 | `	if( pAttr == 0 ){` |
|     ! 0 |  6470 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  6471 | `		return SXERR_ABORT;` |
|       - |  6472 | `	}` |
|   36776 |  6473 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     110 |  6474 | `		pAttr->nType = nType;` |
|     110 |  6475 | `		pAttr->sClass = sTypeClass;` |
|     110 |  6476 | `		pAttr->sTypeName = sTypeText;` |
|     110 |  6477 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6478 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  6479 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  6480 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  6481 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  6482 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  6483 | `			sxu32 i;` |
|      32 |  6484 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  6485 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  6486 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  6487 | `			}` |
|       5 |  6488 | `		}` |
|      54 |  6489 | `	}` |
|   36776 |  6490 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  6491 | `		SySet *pInstrContainer;` |
|   11480 |  6492 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  6493 | `		/* Swap bytecode container */` |
|   11480 |  6494 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   11480 |  6495 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  6496 | `		/* Compile attribute value.` |
|       - |  6497 | `		 */` |
|   11480 |  6498 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   11480 |  6499 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  6500 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  6501 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6502 | `				return SXERR_ABORT;` |
|       - |  6503 | `			}` |
|     ! 0 |  6504 | `		}` |
|       - |  6505 | `		/* Emit the done instruction */` |
|   11480 |  6506 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   11480 |  6507 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5739 |  6508 | `	}` |
|       - |  6509 | `	/* All done,install the attribute */` |
|   36776 |  6510 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   36776 |  6511 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6512 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6513 | `		return SXERR_ABORT;` |
|       - |  6514 | `	}` |
|   36776 |  6515 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6516 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  6517 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  6518 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  6519 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6520 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6521 | `				pTok--;` |
|     ! 0 |  6522 | `			}` |
|     ! 0 |  6523 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6524 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  6525 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6526 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6527 | `				return SXERR_ABORT;` |
|       - |  6528 | `			}` |
|     ! 0 |  6529 | `		}else{` |
|       5 |  6530 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  6531 | `				goto loop;` |
|       - |  6532 | `			}` |
|       - |  6533 | `		}` |
|     ! 0 |  6534 | `	}` |
|   36772 |  6535 | `	SySetRelease(&aUnionAlts);` |
|   36772 |  6536 | `	return SXRET_OK;` |
|       1 |  6537 | `Synchronize:` |
|       - |  6538 | `	/* Synchronize with the first semi-colon */` |
|       5 |  6539 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       3 |  6540 | `		pGen->pIn++;` |
|       1 |  6541 | `	}` |
|       3 |  6542 | `	SySetRelease(&aUnionAlts);` |
|       3 |  6543 | `	return SXERR_CORRUPT;` |
|   18388 |  6544 |  |
|       - |  6545 | `/*` |
|       - |  6546 | ` * Compile a class method.` |
|       - |  6547 | ` *` |
|       - |  6548 | ` * Refer to the official documentation for more information` |
|       - |  6549 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  6550 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  6551 | ` * overloading and many more.` |
|       - |  6552 | ` */` |
|  134994 |  6553 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  6554 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6555 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  6556 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  6557 | `	int doBody,          /* TRUE to process method body */` |
|       - |  6558 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  6559 | `	)` |
|       2 |  6560 |  |
|  134996 |  6561 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6562 | `	ph7_class_method *pMeth;` |
|       - |  6563 | `	sxi32 iFuncFlags;` |
|       - |  6564 | `	SyString *pName;` |
|       - |  6565 | `	SyToken *pEnd;` |
|       - |  6566 | `	sxi32 rc;` |
|       - |  6567 | `	/* Extract visibility level */` |
|  134996 |  6568 | `	iProtection = GetProtectionLevel(iProtection);` |
|  134996 |  6569 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  134996 |  6570 | `	iFuncFlags = 0;` |
|  134996 |  6571 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  6572 | `		/* Invalid method name */` |
|     ! 0 |  6573 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  6574 | `		if( rc == SXERR_ABORT ){` |
|       - |  6575 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6576 | `			return SXERR_ABORT;` |
|       - |  6577 | `		}` |
|     ! 0 |  6578 | `		goto Synchronize;` |
|       - |  6579 | `	}` |
|  134996 |  6580 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6581 | `		/* Return by reference,remember that */` |
|     ! 0 |  6582 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6583 | `		/* Jump the '&' token */` |
|     ! 0 |  6584 | `		pGen->pIn++;` |
|     ! 0 |  6585 | `	}` |
|  134996 |  6586 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6587 | `		/* Invalid method name */` |
|     ! 0 |  6588 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  6589 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6590 | `			return SXERR_ABORT;` |
|       - |  6591 | `		}` |
|     ! 0 |  6592 | `		goto Synchronize;` |
|       - |  6593 | `	}` |
|       - |  6594 | `	/* Peek method name */` |
|  134996 |  6595 | `	pName = &pGen->pIn->sData;` |
|  134996 |  6596 | `	nLine = pGen->pIn->nLine;` |
|       - |  6597 | `	/* Jump the method name */` |
|  134996 |  6598 | `	pGen->pIn++;` |
|  134996 |  6599 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  6600 | `		/* Abstract method */` |
|   22466 |  6601 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  6602 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6603 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  6604 | `				&pClass->sName,pName);` |
|     ! 0 |  6605 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6606 | `				return SXERR_ABORT;` |
|       - |  6607 | `			}` |
|     ! 0 |  6608 | `		}` |
|       - |  6609 | `		/* Assemble method signature only */` |
|   22466 |  6610 | `		doBody = FALSE;` |
|   11232 |  6611 | `	}` |
|  134996 |  6612 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6613 | `		/* Syntax error */` |
|     ! 0 |  6614 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  6615 | `		if( rc == SXERR_ABORT ){` |
|       - |  6616 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6617 | `			return SXERR_ABORT;` |
|       - |  6618 | `		}` |
|     ! 0 |  6619 | `		goto Synchronize;` |
|       - |  6620 | `	}` |
|       - |  6621 | `	/* Allocate a new class_method instance */` |
|  134996 |  6622 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  134996 |  6623 | `	if( pMeth == 0 ){` |
|     ! 0 |  6624 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6625 | `		return SXERR_ABORT;` |
|       - |  6626 | `	}` |
|       - |  6627 | `	/* Jump the left parenthesis '(' */` |
|  134996 |  6628 | `	pGen->pIn++;` |
|  134996 |  6629 | `	pEnd = 0; /* cc warning */` |
|       - |  6630 | `	/* Delimit the method signature */` |
|  134996 |  6631 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  134996 |  6632 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6633 | `		/* Syntax error */` |
|       3 |  6634 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  6635 | `		if( rc == SXERR_ABORT ){` |
|       - |  6636 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6637 | `			return SXERR_ABORT;` |
|       - |  6638 | `		}` |
|       3 |  6639 | `		goto Synchronize;` |
|       - |  6640 | `	}` |
|  134994 |  6641 | `	if( pGen->pIn < pEnd ){` |
|       - |  6642 | `		/* Collect method arguments */` |
|   28116 |  6643 | `		rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd);` |
|   28116 |  6644 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6645 | `			return SXERR_ABORT;` |
|       - |  6646 | `		}` |
|   14057 |  6647 | `	}` |
|       - |  6648 | `	/* Point past ')' and parse optional return type ': type' */` |
|  134994 |  6649 | `	pGen->pIn = &pEnd[1];` |
|       - |  6650 | `	{` |
|  134994 |  6651 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  134994 |  6652 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6653 | `			return SXERR_ABORT;` |
|  134994 |  6654 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  6655 | `			goto Synchronize;` |
|       - |  6656 | `		}` |
|       - |  6657 | `	}` |
|  134994 |  6658 | `	if( doBody ){` |
|       - |  6659 | `		/* Compile method body */` |
|  112530 |  6660 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  112530 |  6661 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6662 | `			return SXERR_ABORT;` |
|       - |  6663 | `		}` |
|   56266 |  6664 | `	}else{` |
|       - |  6665 | `		/* Only method signature is allowed */` |
|   22466 |  6666 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  6667 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  6668 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  6669 | `				if( rc == SXERR_ABORT ){` |
|       - |  6670 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  6671 | `					return SXERR_ABORT;` |
|       - |  6672 | `				}` |
|     ! 0 |  6673 | `				return SXERR_CORRUPT;` |
|       - |  6674 | `			}` |
|       - |  6675 | `	}` |
|       - |  6676 | `	/* All done,install the method */` |
|  134994 |  6677 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  134994 |  6678 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6679 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6680 | `		return SXERR_ABORT;` |
|       - |  6681 | `	}` |
|  134994 |  6682 | `	return SXRET_OK;` |
|       1 |  6683 | `Synchronize:` |
|       - |  6684 | `	/* Synchronize with the first semi-colon */` |
|       7 |  6685 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       5 |  6686 | `		pGen->pIn++;` |
|       1 |  6687 | `	}` |
|       3 |  6688 | `	return SXERR_CORRUPT;` |
|   67499 |  6689 |  |
|       - |  6690 | `/*` |
|       - |  6691 | ` * Compile an object interface.` |
|       - |  6692 | ` *  According to the PHP language reference manual` |
|       - |  6693 | ` *   Object Interfaces:` |
|       - |  6694 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  6695 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  6696 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  6697 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  6698 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  6699 | ` */` |
|    8444 |  6700 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  6701 |  |
|    8446 |  6702 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6703 | `	ph7_class *pClass,*pBase;` |
|       - |  6704 | `	SyToken *pEnd,*pTmp;` |
|       - |  6705 | `	SyString *pName;` |
|       - |  6706 | `	sxi32 nKwrd;` |
|       - |  6707 | `	sxi32 rc;` |
|       - |  6708 | `	/* Jump the 'interface' keyword */` |
|    8446 |  6709 | `	pGen->pIn++;` |
|       - |  6710 | `	/* Extract interface name */` |
|    8446 |  6711 | `	pName = &pGen->pIn->sData;` |
|       - |  6712 | `	/* Advance the stream cursor */` |
|    8446 |  6713 | `	pGen->pIn++;` |
|       - |  6714 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  6715 | `		SyBlob sFQN;` |
|       - |  6716 | `		SyString sFQNStr;` |
|    8446 |  6717 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8446 |  6718 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8446 |  6719 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8446 |  6720 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8446 |  6721 | `		SyBlobRelease(&sFQN);` |
|       - |  6722 | `	}` |
|    8446 |  6723 | `	if( pClass == 0 ){` |
|     ! 0 |  6724 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6725 | `		return SXERR_ABORT;` |
|       - |  6726 | `	}` |
|       - |  6727 | `	/* Mark as an interface */` |
|    8446 |  6728 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  6729 | `	/* Assume no base class is given */` |
|    8446 |  6730 | `	pBase = 0;` |
|    8446 |  6731 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  6732 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  6733 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  6734 | `			SyString *pBaseName;` |
|       - |  6735 | `			/* Extract base interface */` |
|       3 |  6736 | `			pGen->pIn++;` |
|       3 |  6737 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6738 | `				/* Syntax error */` |
|     ! 0 |  6739 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6740 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  6741 | `					pName);` |
|     ! 0 |  6742 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  6743 | `				if( rc == SXERR_ABORT ){` |
|       - |  6744 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  6745 | `					return SXERR_ABORT;` |
|       - |  6746 | `				}` |
|     ! 0 |  6747 | `				return SXRET_OK;` |
|       - |  6748 | `			}` |
|       3 |  6749 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  6750 | `			{` |
|       - |  6751 | `				SyBlob sResolved;` |
|       3 |  6752 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 |  6753 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 |  6754 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 |  6755 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 |  6756 | `				SyBlobRelease(&sResolved);` |
|       - |  6757 | `			}` |
|       - |  6758 | `			/* Only interfaces is allowed */` |
|       3 |  6759 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  6760 | `				pBase = pBase->pNextName;` |
|     ! 0 |  6761 | `			}` |
|       3 |  6762 | `			if( pBase == 0 ){` |
|       - |  6763 | `				/* Inexistant interface */` |
|     ! 0 |  6764 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 |  6765 | `				if( rc == SXERR_ABORT ){` |
|       - |  6766 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  6767 | `					return SXERR_ABORT;` |
|       - |  6768 | `				}` |
|     ! 0 |  6769 | `			}` |
|       - |  6770 | `			/* Advance the stream cursor */` |
|       3 |  6771 | `			pGen->pIn++;` |
|       1 |  6772 | `		}` |
|       1 |  6773 | `	}` |
|    8446 |  6774 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  6775 | `		/* Syntax error */` |
|     ! 0 |  6776 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  6777 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  6778 | `		if( rc == SXERR_ABORT ){` |
|       - |  6779 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6780 | `			return SXERR_ABORT;` |
|       - |  6781 | `		}` |
|     ! 0 |  6782 | `		return SXRET_OK;` |
|       - |  6783 | `	}` |
|    8446 |  6784 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8446 |  6785 | `	pEnd = 0; /* cc warning */` |
|       - |  6786 | `	/* Delimit the interface body */` |
|    8446 |  6787 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8446 |  6788 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6789 | `		/* Syntax error */` |
|     ! 0 |  6790 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  6791 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  6792 | `		if( rc == SXERR_ABORT ){` |
|       - |  6793 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6794 | `			return SXERR_ABORT;` |
|       - |  6795 | `		}` |
|     ! 0 |  6796 | `		return SXRET_OK;` |
|       - |  6797 | `	}` |
|       - |  6798 | `	/* Swap token stream */` |
|    8446 |  6799 | `	pTmp = pGen->pEnd;` |
|    8446 |  6800 | `	pGen->pEnd = pEnd;` |
|       - |  6801 | `	/* Start the parse process` |
|       - |  6802 | `	 * Note (According to the PHP reference manual):` |
|       - |  6803 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  6804 | `	 *  Only 'public' visibility is allowed.` |
|       - |  6805 | `	 */` |
|   15449 |  6806 | `	for(;;){` |
|       - |  6807 | `		/* Jump leading/trailing semi-colons */` |
|   53354 |  6808 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   22456 |  6809 | `			pGen->pIn++;` |
|       2 |  6810 | `		}` |
|   30900 |  6811 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  6812 | `			/* End of interface body */` |
|    8444 |  6813 | `			break;` |
|       - |  6814 | `		}` |
|   22458 |  6815 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  6816 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6817 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  6818 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  6819 | `			if( rc == SXERR_ABORT ){` |
|       - |  6820 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  6821 | `				return SXERR_ABORT;` |
|       - |  6822 | `			}` |
|     ! 0 |  6823 | `			goto done;` |
|       - |  6824 | `		}` |
|       - |  6825 | `		/* Extract the current keyword */` |
|   22458 |  6826 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22458 |  6827 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  6828 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  6829 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  6830 | `			const char *zKind = "member";` |
|       3 |  6831 | `			SyString *pMemberName = 0;` |
|       3 |  6832 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  6833 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  6834 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  6835 | `					zKind = "constant";` |
|       3 |  6836 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  6837 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  6838 | `					}` |
|       1 |  6839 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  6840 | `					zKind = "method";` |
|     ! 0 |  6841 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  6842 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  6843 | `					}` |
|     ! 0 |  6844 | `				}` |
|       1 |  6845 | `			}` |
|       3 |  6846 | `			if( pMemberName ){` |
|       4 |  6847 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  6848 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  6849 | `			}else{` |
|     ! 0 |  6850 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  6851 | `					"Access type for interface %s must be public",zKind);` |
|       - |  6852 | `			}` |
|       3 |  6853 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6854 | `				return SXERR_ABORT;` |
|       - |  6855 | `			}` |
|       3 |  6856 | `			goto done;` |
|       - |  6857 | `		}` |
|   22456 |  6858 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  6859 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  6860 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  6861 | `			if( rc == SXERR_ABORT ){` |
|       - |  6862 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  6863 | `				return SXERR_ABORT;` |
|       - |  6864 | `			}` |
|     ! 0 |  6865 | `			goto done;` |
|       - |  6866 | `		}` |
|   22456 |  6867 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  6868 | `			/* Advance the stream cursor */` |
|   22452 |  6869 | `			pGen->pIn++;` |
|   22452 |  6870 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  6871 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  6872 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  6873 | `				if( rc == SXERR_ABORT ){` |
|       - |  6874 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  6875 | `					return SXERR_ABORT;` |
|       - |  6876 | `				}` |
|     ! 0 |  6877 | `				goto done;` |
|       - |  6878 | `			}` |
|   22452 |  6879 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   22452 |  6880 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  6881 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  6882 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  6883 | `				if( rc == SXERR_ABORT ){` |
|       - |  6884 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  6885 | `					return SXERR_ABORT;` |
|       - |  6886 | `				}` |
|     ! 0 |  6887 | `				goto done;` |
|       - |  6888 | `			}` |
|   11225 |  6889 | `		}` |
|   22456 |  6890 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  6891 | `			/* Parse constant */` |
|       3 |  6892 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  6893 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6894 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  6895 | `					return SXERR_ABORT;` |
|       - |  6896 | `				}` |
|     ! 0 |  6897 | `				goto done;` |
|       - |  6898 | `			}` |
|       2 |  6899 | `		}else{` |
|   22454 |  6900 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   22454 |  6901 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  6902 | `				/* Static method,record that */` |
|     ! 0 |  6903 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  6904 | `				/* Advance the stream cursor */` |
|     ! 0 |  6905 | `				pGen->pIn++;` |
|     ! 0 |  6906 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  6907 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  6908 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  6909 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  6910 | `						if( rc == SXERR_ABORT ){` |
|       - |  6911 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  6912 | `							return SXERR_ABORT;` |
|       - |  6913 | `						}` |
|     ! 0 |  6914 | `						goto done;` |
|       - |  6915 | `				}` |
|     ! 0 |  6916 | `			}` |
|       - |  6917 | `			/* Process method signature (no body for interface methods) */` |
|   22454 |  6918 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   22454 |  6919 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  6920 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  6921 | `					return SXERR_ABORT;` |
|       - |  6922 | `				}` |
|     ! 0 |  6923 | `				goto done;` |
|       - |  6924 | `			}` |
|       - |  6925 | `		}` |
|       2 |  6926 | `	}` |
|       - |  6927 | `	/* Install the interface */` |
|    8444 |  6928 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8444 |  6929 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  6930 | `		/* Inherit from the base interface */` |
|       3 |  6931 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 |  6932 | `	}` |
|    8444 |  6933 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6934 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6935 | `		return SXERR_ABORT;` |
|       - |  6936 | `	}` |
|    4221 |  6937 | `done:` |
|       - |  6938 | `	/* Point beyond the interface body */` |
|    8446 |  6939 | `	pGen->pIn  = &pEnd[1];` |
|    8446 |  6940 | `	pGen->pEnd = pTmp;` |
|    8446 |  6941 | `	return PH7_OK;` |
|    4224 |  6942 |  |
|       - |  6943 | `/*` |
|       - |  6944 | ` * Compile a user-defined class.` |
|       - |  6945 | ` * According to the PHP language reference manual` |
|       - |  6946 | ` *  class` |
|       - |  6947 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  6948 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  6949 | ` *  of the properties and methods belonging to the class.` |
|       - |  6950 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  6951 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  6952 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  6953 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  6954 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  6955 | ` *  (called "methods").` |
|       - |  6956 | ` */` |
|       - |  6957 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  6958 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  6959 | `struct TraitUseEntry {` |
|       - |  6960 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  6961 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  6962 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  6963 | `};` |
|       - |  6964 | `/*` |
|       - |  6965 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  6966 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  6967 | ` */` |
|   39838 |  6968 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  6969 |  |
|       - |  6970 | `	ph7_class **apIface;` |
|       - |  6971 | `	sxu32 nIface,i;` |
|       - |  6972 | `	sxi32 rc;` |
|   39840 |  6973 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  6974 | `		return SXRET_OK;` |
|       - |  6975 | `	}` |
|   39840 |  6976 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   39840 |  6977 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   42680 |  6978 | `	for(i = 0; i < nIface; i++){` |
|    2842 |  6979 | `		ph7_class *pIface = apIface[i];` |
|       - |  6980 | `		SyHashEntry *pEntry;` |
|    2842 |  6981 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   16930 |  6982 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   14090 |  6983 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  6984 | `			ph7_class_method *pImplMeth;` |
|   14090 |  6985 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  6986 | `			/* Find the implementing method in the class */` |
|   14090 |  6987 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   14090 |  6988 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  6989 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  6990 | `			}` |
|       - |  6991 | `			/* Check visibility: interface methods must be implemented as public */` |
|   14076 |  6992 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  6993 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  6994 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  6995 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  6996 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  6997 | `					return SXERR_ABORT;` |
|       - |  6998 | `				}` |
|       1 |  6999 | `			}` |
|       - |  7000 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7001 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7002 | `			 */` |
|       - |  7003 | `			{` |
|   14076 |  7004 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   14076 |  7005 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   14076 |  7006 | `				int sigError = 0;` |
|   14076 |  7007 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7008 | `					sigError = 1;` |
|   14075 |  7009 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7010 | `					/* Extra parameters must all have default values */` |
|       5 |  7011 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7012 | `					sxu32 k;` |
|       7 |  7013 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7014 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7015 | `							sigError = 1;` |
|       3 |  7016 | `							break;` |
|       - |  7017 | `						}` |
|       2 |  7018 | `					}` |
|       2 |  7019 | `				}` |
|   14076 |  7020 | `				if( sigError ){` |
|       - |  7021 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7022 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7023 | `					sxu32 j;` |
|       5 |  7024 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7025 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7026 | `					/* Build implementing method signature */` |
|       5 |  7027 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7028 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7029 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7030 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7031 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7032 | `					}` |
|       - |  7033 | `					/* Build interface method signature */` |
|       5 |  7034 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7035 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7036 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7037 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7038 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7039 | `					}` |
|       7 |  7040 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7041 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7042 | `						&pClass->sName,pMName,` |
|       4 |  7043 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7044 | `						&pIface->sName,pMName,` |
|       4 |  7045 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7046 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7047 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7048 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7049 | `						return SXERR_ABORT;` |
|       - |  7050 | `					}` |
|       2 |  7051 | `				}` |
|       - |  7052 | `			}` |
|       2 |  7053 | `		}` |
|    1422 |  7054 | `	}` |
|   39840 |  7055 | `	return SXRET_OK;` |
|   19921 |  7056 |  |
|       - |  7057 | `/*` |
|       - |  7058 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7059 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7060 | ` */` |
|   39838 |  7061 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7062 |  |
|       - |  7063 | `	ph7_class_method *pMeth;` |
|       - |  7064 | `	SyHashEntry *pEntry;` |
|       - |  7065 | `	sxu32 nAbstract;` |
|       - |  7066 | `	SyBlob sMsg;` |
|       - |  7067 | `	sxi32 rc;` |
|       - |  7068 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   39840 |  7069 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      20 |  7070 | `		return SXRET_OK;` |
|       - |  7071 | `	}` |
|       - |  7072 | `	/* Count abstract methods */` |
|   39822 |  7073 | `	nAbstract = 0;` |
|   39822 |  7074 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  376938 |  7075 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  337118 |  7076 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  337118 |  7077 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7078 | `			nAbstract++;` |
|       8 |  7079 | `		}` |
|       2 |  7080 | `	}` |
|   39822 |  7081 | `	if( nAbstract == 0 ){` |
|   39808 |  7082 | `		return SXRET_OK;` |
|       - |  7083 | `	}` |
|       - |  7084 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7085 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7086 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7087 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7088 | `		&pClass->sName,nAbstract,` |
|       7 |  7089 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7090 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7091 | `	/* Second pass: list methods with origins */` |
|       - |  7092 | `	{` |
|      15 |  7093 | `		sxu32 nListed = 0;` |
|      15 |  7094 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7095 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7096 | `			ph7_class *pOrigin = 0;` |
|       - |  7097 | `			SyString *pMName;` |
|      19 |  7098 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7099 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7100 | `				continue;` |
|       - |  7101 | `			}` |
|      17 |  7102 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7103 | `			if( nListed > 0 ){` |
|       3 |  7104 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7105 | `			}` |
|       - |  7106 | `			/* Find the origin of this abstract method.` |
|       - |  7107 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7108 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7109 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7110 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7111 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7112 | `			 * class's namespace.` |
|       - |  7113 | `			 */` |
|       - |  7114 | `			{` |
|       - |  7115 | `				ph7_class **apIface;` |
|       - |  7116 | `				ph7_class **apTrait;` |
|       - |  7117 | `				ph7_class *pWalk;` |
|       - |  7118 | `				sxu32 i;` |
|       - |  7119 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7120 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7121 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7122 | `				 */` |
|      17 |  7123 | `				if( pClass->pBase ){` |
|       9 |  7124 | `					pWalk = pClass->pBase;` |
|      17 |  7125 | `					while( pWalk ){` |
|       - |  7126 | `						ph7_class_method *pParentMeth;` |
|      11 |  7127 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7128 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7129 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7130 | `							 * in this class's ancestor chain.` |
|       - |  7131 | `							 */` |
|      11 |  7132 | `							int fromIface = 0;` |
|      11 |  7133 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7134 | `							while( pAnc ){` |
|       - |  7135 | `								ph7_class **apPI;` |
|       - |  7136 | `								sxu32 j;` |
|      13 |  7137 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7138 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7139 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7140 | `										fromIface = 1;` |
|       9 |  7141 | `										break;` |
|       - |  7142 | `									}` |
|     ! 0 |  7143 | `								}` |
|      13 |  7144 | `								if( fromIface ) break;` |
|       5 |  7145 | `								pAnc = pAnc->pBase;` |
|       1 |  7146 | `							}` |
|      11 |  7147 | `							if( !fromIface ){` |
|       3 |  7148 | `								pOrigin = pWalk;` |
|       3 |  7149 | `								break;` |
|       - |  7150 | `							}` |
|       4 |  7151 | `						}` |
|       9 |  7152 | `						pWalk = pWalk->pBase;` |
|       1 |  7153 | `					}` |
|       4 |  7154 | `				}` |
|       - |  7155 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7156 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7157 | `				 */` |
|      17 |  7158 | `				if( !pOrigin ){` |
|      15 |  7159 | `					pWalk = pClass;` |
|      37 |  7160 | `					while( pWalk && !pOrigin ){` |
|      23 |  7161 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7162 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7163 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7164 | `							ph7_class *pDeepest = 0;` |
|      25 |  7165 | `							while( pIface ){` |
|      13 |  7166 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7167 | `									pDeepest = pIface;` |
|       6 |  7168 | `								}` |
|      13 |  7169 | `								pIface = pIface->pBase;` |
|       1 |  7170 | `							}` |
|      13 |  7171 | `							if( pDeepest ){` |
|      13 |  7172 | `								pOrigin = pDeepest;` |
|      13 |  7173 | `								break;` |
|       - |  7174 | `							}` |
|     ! 0 |  7175 | `						}` |
|      23 |  7176 | `						pWalk = pWalk->pBase;` |
|       1 |  7177 | `					}` |
|       7 |  7178 | `				}` |
|       - |  7179 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7180 | `				if( !pOrigin ){` |
|       3 |  7181 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7182 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7183 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7184 | `							pOrigin = pClass;` |
|       3 |  7185 | `							break;` |
|       - |  7186 | `						}` |
|     ! 0 |  7187 | `					}` |
|       1 |  7188 | `				}` |
|       - |  7189 | `			}` |
|      17 |  7190 | `			if( pOrigin ){` |
|      17 |  7191 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7192 | `			}else{` |
|       - |  7193 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7194 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7195 | `			}` |
|      17 |  7196 | `			nListed++;` |
|       1 |  7197 | `		}` |
|       - |  7198 | `	}` |
|      15 |  7199 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7200 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7201 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7202 | `	SyBlobRelease(&sMsg);` |
|      15 |  7203 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7204 | `		return SXERR_ABORT;` |
|       - |  7205 | `	}` |
|      15 |  7206 | `	return SXRET_OK;` |
|   19921 |  7207 |  |
|   39842 |  7208 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  7209 |  |
|   39844 |  7210 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7211 | `	ph7_class *pClass,*pBase;` |
|       - |  7212 | `	SyToken *pEnd,*pTmp;` |
|       - |  7213 | `	sxi32 iProtection;` |
|       - |  7214 | `	SySet aInterfaces;` |
|       - |  7215 | `	SySet aUseEntries;` |
|       - |  7216 | `	sxi32 iAttrflags;` |
|       - |  7217 | `	SyString *pName;` |
|       - |  7218 | `	sxi32 nKwrd;` |
|       - |  7219 | `	sxi32 rc;` |
|       - |  7220 | `	/* Jump the 'class' keyword */` |
|   39844 |  7221 | `	pGen->pIn++;` |
|   39844 |  7222 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7223 | `		/* Syntax error */` |
|     ! 0 |  7224 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  7225 | `		if( rc == SXERR_ABORT ){` |
|       - |  7226 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7227 | `			return SXERR_ABORT;` |
|       - |  7228 | `		}` |
|       - |  7229 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  7230 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  7231 | `			pGen->pIn++;` |
|     ! 0 |  7232 | `		}` |
|     ! 0 |  7233 | `		return SXRET_OK;` |
|       - |  7234 | `	}` |
|       - |  7235 | `	/* Extract class name */` |
|   39844 |  7236 | `	pName = &pGen->pIn->sData;` |
|       - |  7237 | `	/* Advance the stream cursor */` |
|   39844 |  7238 | `	pGen->pIn++;` |
|       - |  7239 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7240 | `		SyBlob sFQN;` |
|       - |  7241 | `		SyString sFQNStr;` |
|   39844 |  7242 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   39844 |  7243 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   39844 |  7244 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   39844 |  7245 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   39844 |  7246 | `		SyBlobRelease(&sFQN);` |
|       - |  7247 | `	}` |
|   39844 |  7248 | `	if( pClass == 0 ){` |
|     ! 0 |  7249 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7250 | `		return SXERR_ABORT;` |
|       - |  7251 | `	}` |
|       - |  7252 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   39844 |  7253 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   39844 |  7254 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  7255 | `	/* Assume a standalone class */` |
|   39844 |  7256 | `	pBase = 0;` |
|   39844 |  7257 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7258 | `		SyString *pBaseName;` |
|   28174 |  7259 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   28174 |  7260 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   25336 |  7261 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   25336 |  7262 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7263 | `				/* Syntax error */` |
|     ! 0 |  7264 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7265 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 |  7266 | `					pName);` |
|     ! 0 |  7267 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7268 | `				if( rc == SXERR_ABORT ){` |
|       - |  7269 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7270 | `					return SXERR_ABORT;` |
|       - |  7271 | `				}` |
|     ! 0 |  7272 | `				return SXRET_OK;` |
|       - |  7273 | `			}` |
|       - |  7274 | `			/* Extract base class name and resolve through namespace/imports */` |
|   25336 |  7275 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  7276 | `			{` |
|       - |  7277 | `				SyBlob sResolved;` |
|   25336 |  7278 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   25336 |  7279 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   38003 |  7280 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   25334 |  7281 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   25336 |  7282 | `				SyBlobRelease(&sResolved);` |
|       - |  7283 | `			}` |
|       - |  7284 | `			/* Interfaces are not allowed */` |
|   25336 |  7285 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  7286 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7287 | `			}` |
|   25336 |  7288 | `			if( pBase == 0 ){` |
|       - |  7289 | `				/* Inexistant base class */` |
|     ! 0 |  7290 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 |  7291 | `				if( rc == SXERR_ABORT ){` |
|       - |  7292 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7293 | `					return SXERR_ABORT;` |
|       - |  7294 | `				}` |
|     ! 0 |  7295 | `			}else{` |
|   25336 |  7296 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  7297 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7298 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  7299 | `					if( rc == SXERR_ABORT ){` |
|       - |  7300 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7301 | `						return SXERR_ABORT;` |
|       - |  7302 | `					}` |
|     ! 0 |  7303 | `				}` |
|       - |  7304 | `			}` |
|       - |  7305 | `			/* Advance the stream cursor */` |
|   25336 |  7306 | `			pGen->pIn++;` |
|   12667 |  7307 | `		}` |
|   28174 |  7308 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  7309 | `			ph7_class *pInterface;` |
|       - |  7310 | `			SyString *pIntName;` |
|       - |  7311 | `			/* Interface implementation */` |
|    2842 |  7312 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1420 |  7313 | `			for(;;){` |
|    2842 |  7314 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7315 | `					/* Syntax error */` |
|     ! 0 |  7316 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7317 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  7318 | `						pName);` |
|     ! 0 |  7319 | `					if( rc == SXERR_ABORT ){` |
|       - |  7320 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7321 | `						return SXERR_ABORT;` |
|       - |  7322 | `					}` |
|     ! 0 |  7323 | `					break;` |
|       - |  7324 | `				}` |
|       - |  7325 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2842 |  7326 | `				pIntName = &pGen->pIn->sData;` |
|       - |  7327 | `				{` |
|       - |  7328 | `					SyBlob sResolved;` |
|    2842 |  7329 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2842 |  7330 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5682 |  7331 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2840 |  7332 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2842 |  7333 | `					SyBlobRelease(&sResolved);` |
|       - |  7334 | `				}` |
|       - |  7335 | `				/* Only interfaces are allowed */` |
|    2842 |  7336 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7337 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  7338 | `				}` |
|    2842 |  7339 | `				if( pInterface == 0 ){` |
|       - |  7340 | `					/* Inexistant interface */` |
|     ! 0 |  7341 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 |  7342 | `					if( rc == SXERR_ABORT ){` |
|       - |  7343 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7344 | `						return SXERR_ABORT;` |
|       - |  7345 | `					}` |
|     ! 0 |  7346 | `				}else{` |
|       - |  7347 | `					/* Register interface */` |
|    2842 |  7348 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  7349 | `				}` |
|       - |  7350 | `				/* Advance the stream cursor */` |
|    2842 |  7351 | `				pGen->pIn++;` |
|    2842 |  7352 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1422 |  7353 | `					break;` |
|       - |  7354 | `				}` |
|     ! 0 |  7355 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  7356 | `			}` |
|    1420 |  7357 | `		}` |
|   14086 |  7358 | `	}` |
|   39844 |  7359 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7360 | `		/* Syntax error */` |
|     ! 0 |  7361 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  7362 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7363 | `		if( rc == SXERR_ABORT ){` |
|       - |  7364 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7365 | `			return SXERR_ABORT;` |
|       - |  7366 | `		}` |
|     ! 0 |  7367 | `		return SXRET_OK;` |
|       - |  7368 | `	}` |
|   39844 |  7369 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   39844 |  7370 | `	pEnd = 0; /* cc warning */` |
|       - |  7371 | `	/* Delimit the class body */` |
|   39844 |  7372 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   39844 |  7373 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7374 | `		/* Syntax error */` |
|     ! 0 |  7375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  7376 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7377 | `		if( rc == SXERR_ABORT ){` |
|       - |  7378 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7379 | `			return SXERR_ABORT;` |
|       - |  7380 | `		}` |
|     ! 0 |  7381 | `		return SXRET_OK;` |
|       - |  7382 | `	}` |
|       - |  7383 | `	/* Swap token stream */` |
|   39844 |  7384 | `	pTmp = pGen->pEnd;` |
|   39844 |  7385 | `	pGen->pEnd = pEnd;` |
|       - |  7386 | `	/* Set the inherited flags */` |
|   39844 |  7387 | `	pClass->iFlags = iFlags;` |
|       - |  7388 | `	/* Start the parse process */` |
|   76181 |  7389 | `	for(;;){` |
|       - |  7390 | `		/* Jump leading/trailing semi-colons */` |
|  225978 |  7391 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   36826 |  7392 | `			pGen->pIn++;` |
|       2 |  7393 | `		}` |
|  189154 |  7394 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7395 | `			/* End of class body */` |
|   39840 |  7396 | `			break;` |
|       - |  7397 | `		}` |
|  149316 |  7398 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  7399 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7400 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7401 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7402 | `			if( rc == SXERR_ABORT ){` |
|       - |  7403 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7404 | `				return SXERR_ABORT;` |
|       - |  7405 | `			}` |
|     ! 0 |  7406 | `			goto done;` |
|       - |  7407 | `		}` |
|       - |  7408 | `		/* Assume public visibility */` |
|  149316 |  7409 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  149316 |  7410 | `		iAttrflags = 0;` |
|  149316 |  7411 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  7412 | `			/* Extract the current keyword */` |
|  149316 |  7413 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  149316 |  7414 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  7415 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  7416 | `				TraitUseEntry sUse;` |
|      44 |  7417 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  7418 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  7419 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  7420 | `				for(;;){` |
|       - |  7421 | `					ph7_class *pTrait;` |
|       - |  7422 | `					SyString *pTraitName;` |
|      52 |  7423 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7424 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7425 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  7426 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7427 | `							return SXERR_ABORT;` |
|       - |  7428 | `						}` |
|     ! 0 |  7429 | `						break;` |
|       - |  7430 | `					}` |
|      52 |  7431 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  7432 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  7433 | `						SyBlob sResolved;` |
|      52 |  7434 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  7435 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  7436 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  7437 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  7438 | `						SyBlobRelease(&sResolved);` |
|       - |  7439 | `					}` |
|       - |  7440 | `					/* Only traits are allowed */` |
|      52 |  7441 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  7442 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  7443 | `					}` |
|      52 |  7444 | `					if( pTrait == 0 ){` |
|     ! 0 |  7445 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7446 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  7447 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7448 | `							return SXERR_ABORT;` |
|       - |  7449 | `						}` |
|     ! 0 |  7450 | `					}else{` |
|      52 |  7451 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  7452 | `					}` |
|      52 |  7453 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  7454 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  7455 | `						break;` |
|       - |  7456 | `					}` |
|       9 |  7457 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  7458 | `				}` |
|       - |  7459 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  7460 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  7461 | `					SyToken *pBlock;` |
|       9 |  7462 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  7463 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  7464 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  7465 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  7466 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  7467 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  7468 | `					}else{` |
|     ! 0 |  7469 | `						pGen->pIn = pGen->pEnd;` |
|       - |  7470 | `					}` |
|       4 |  7471 | `				}` |
|      44 |  7472 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  7473 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  7474 | `				continue;` |
|       - |  7475 | `			}` |
|  149274 |  7476 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  146362 |  7477 | `				iProtection = nKwrd;` |
|  146362 |  7478 | `				pGen->pIn++; /* Jump the visibility token */` |
|  146360 |  7479 | `				if( pGen->pIn >= pGen->pEnd` |
|  146362 |  7480 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  7481 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7482 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7483 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  7484 | `					if( rc == SXERR_ABORT ){` |
|       - |  7485 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7486 | `						return SXERR_ABORT;` |
|       - |  7487 | `					}` |
|     ! 0 |  7488 | `					goto done;` |
|       - |  7489 | `				}` |
|  146362 |  7490 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  7491 | `					/* Attribute declaration (untyped) */` |
|   36648 |  7492 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   36648 |  7493 | `					if( rc != SXRET_OK ){` |
|       3 |  7494 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7495 | `							return SXERR_ABORT;` |
|       - |  7496 | `						}` |
|       3 |  7497 | `						goto done;` |
|       - |  7498 | `					}` |
|   36646 |  7499 | `					continue;` |
|       - |  7500 | `				}` |
|  109716 |  7501 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  7502 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|      96 |  7503 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      96 |  7504 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  7505 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7506 | `							return SXERR_ABORT;` |
|       - |  7507 | `						}` |
|     ! 0 |  7508 | `						goto done;` |
|       - |  7509 | `					}` |
|      96 |  7510 | `					continue;` |
|       - |  7511 | `				}` |
|       - |  7512 | `				/* Extract the keyword */` |
|  109622 |  7513 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   54810 |  7514 | `			}` |
|  112534 |  7515 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7516 | `				/* Process constant declaration */` |
|      30 |  7517 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  7518 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  7519 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7520 | `						return SXERR_ABORT;` |
|       - |  7521 | `					}` |
|     ! 0 |  7522 | `					goto done;` |
|       - |  7523 | `				}` |
|      16 |  7524 | `			}else{` |
|  112506 |  7525 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7526 | `					/* Static method or attribute,record that */` |
|    2838 |  7527 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2838 |  7528 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2838 |  7529 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7530 | `						/* Extract the keyword */` |
|    2834 |  7531 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2834 |  7532 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  7533 | `							iProtection = nKwrd;` |
|     ! 0 |  7534 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  7535 | `						}` |
|    1416 |  7536 | `					}` |
|    2836 |  7537 | `					if( pGen->pIn >= pGen->pEnd` |
|    2838 |  7538 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  7539 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7540 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  7541 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  7542 | `						if( rc == SXERR_ABORT ){` |
|       - |  7543 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7544 | `							return SXERR_ABORT;` |
|       - |  7545 | `						}` |
|     ! 0 |  7546 | `						goto done;` |
|       - |  7547 | `					}` |
|    2838 |  7548 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  7549 | `						/* Attribute declaration */` |
|       5 |  7550 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  7551 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  7552 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7553 | `								return SXERR_ABORT;` |
|       - |  7554 | `							}` |
|     ! 0 |  7555 | `							goto done;` |
|       - |  7556 | `						}` |
|       5 |  7557 | `						continue;` |
|       - |  7558 | `					}` |
|    2834 |  7559 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  7560 | `						/* Typed static attribute declaration */` |
|       8 |  7561 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  7562 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  7563 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  7564 | `								return SXERR_ABORT;` |
|       - |  7565 | `							}` |
|     ! 0 |  7566 | `							goto done;` |
|       - |  7567 | `						}` |
|       8 |  7568 | `						continue;` |
|       - |  7569 | `					}` |
|       - |  7570 | `					/* Extract the keyword */` |
|    2828 |  7571 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  111083 |  7572 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  7573 | `					/* Abstract method,record that */` |
|      10 |  7574 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  7575 | `					/* Mark the whole class as abstract */` |
|      10 |  7576 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  7577 | `					/* Advance the stream cursor */` |
|      10 |  7578 | `					pGen->pIn++;` |
|      10 |  7579 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      10 |  7580 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      10 |  7581 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       8 |  7582 | `							iProtection = nKwrd;` |
|       8 |  7583 | `							pGen->pIn++; /* Jump the visibility token */` |
|       3 |  7584 | `						}` |
|       4 |  7585 | `					}` |
|      10 |  7586 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  7587 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  7588 | `							/* Static method */` |
|     ! 0 |  7589 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  7590 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  7591 | `					}` |
|      10 |  7592 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       8 |  7593 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7594 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7595 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  7596 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  7597 | `							if( rc == SXERR_ABORT ){` |
|       - |  7598 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  7599 | `								return SXERR_ABORT;` |
|       - |  7600 | `							}` |
|     ! 0 |  7601 | `							goto done;` |
|       - |  7602 | `					}` |
|      10 |  7603 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  109666 |  7604 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  7605 | `					/* final method ,record that */` |
|       5 |  7606 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  7607 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  7608 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7609 | `						/* Extract the keyword */` |
|       5 |  7610 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  7611 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  7612 | `							iProtection = nKwrd;` |
|       5 |  7613 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  7614 | `						}` |
|       2 |  7615 | `					}` |
|       5 |  7616 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  7617 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  7618 | `							/* Static method */` |
|     ! 0 |  7619 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  7620 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  7621 | `					}` |
|       5 |  7622 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  7623 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7624 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7625 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  7626 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  7627 | `							if( rc == SXERR_ABORT ){` |
|       - |  7628 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  7629 | `								return SXERR_ABORT;` |
|       - |  7630 | `							}` |
|     ! 0 |  7631 | `							goto done;` |
|       - |  7632 | `					}` |
|       5 |  7633 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  7634 | `				}` |
|  112496 |  7635 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  7636 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7637 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  7638 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  7639 | `						if( rc == SXERR_ABORT ){` |
|       - |  7640 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7641 | `							return SXERR_ABORT;` |
|       - |  7642 | `						}` |
|     ! 0 |  7643 | `						goto done;` |
|       - |  7644 | `				}` |
|  112496 |  7645 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  7646 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  7647 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  7648 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7649 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  7650 | `						if( rc == SXERR_ABORT ){` |
|       - |  7651 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7652 | `							return SXERR_ABORT;` |
|       - |  7653 | `						}` |
|     ! 0 |  7654 | `						goto done;` |
|       - |  7655 | `					}` |
|       - |  7656 | `					/* Attribute declaration */` |
|       7 |  7657 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  7658 | `				}else{` |
|       - |  7659 | `					/* Process method declaration */` |
|  112490 |  7660 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  7661 | `				}` |
|  112496 |  7662 | `				if( rc != SXRET_OK ){` |
|       3 |  7663 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7664 | `						return SXERR_ABORT;` |
|       - |  7665 | `					}` |
|       3 |  7666 | `					goto done;` |
|       - |  7667 | `				}` |
|       - |  7668 | `			}` |
|   56262 |  7669 | `		}else{` |
|       - |  7670 | `			/* Attribute declaration */` |
|     ! 0 |  7671 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  7672 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7673 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7674 | `					return SXERR_ABORT;` |
|       - |  7675 | `				}` |
|     ! 0 |  7676 | `				goto done;` |
|       - |  7677 | `			}` |
|       - |  7678 | `		}` |
|       2 |  7679 | `	}` |
|       - |  7680 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  7681 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  7682 | `	 */` |
|       - |  7683 | `	{` |
|       - |  7684 | `		TraitUseEntry *apUse;` |
|       - |  7685 | `		sxu32 nU;` |
|   39840 |  7686 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   39882 |  7687 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  7688 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  7689 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  7690 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  7691 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  7692 | `			sxu32 nT;` |
|      44 |  7693 | `			if( !hasResolution ){` |
|       - |  7694 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  7695 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  7696 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  7697 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  7698 | `						break;` |
|       - |  7699 | `					}` |
|      22 |  7700 | `				}` |
|      19 |  7701 | `			}else{` |
|       - |  7702 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  7703 | `				 * then use the block to resolve method conflicts.` |
|       - |  7704 | `				 */` |
|       - |  7705 | `				SyToken *pR;` |
|      19 |  7706 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  7707 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  7708 | `					ph7_class_attr *pAR;` |
|       - |  7709 | `					SyHashEntry *pER;` |
|       - |  7710 | `					SyString *pNR;` |
|      11 |  7711 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  7712 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  7713 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  7714 | `						pNR = &pAR->sName;` |
|     ! 0 |  7715 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  7716 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  7717 | `						}` |
|     ! 0 |  7718 | `					}` |
|      11 |  7719 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  7720 | `				}` |
|       - |  7721 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  7722 | `				pR = pUse->pResolvStart;` |
|      21 |  7723 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  7724 | `					SyString sTrait,sMethod;` |
|       - |  7725 | `					ph7_class *pSrcTrait;` |
|       - |  7726 | `					ph7_class_method *pMeth;` |
|       - |  7727 | `					sxi32 nRKwrd;` |
|      33 |  7728 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  7729 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  7730 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  7731 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  7732 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  7733 | `					sMethod = pR->sData;` |
|      13 |  7734 | `					pR++;` |
|      13 |  7735 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  7736 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  7737 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  7738 | `							sTrait = sMethod;` |
|       7 |  7739 | `							pR++;` |
|       7 |  7740 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  7741 | `							sMethod = pR->sData;` |
|       7 |  7742 | `							pR++;` |
|       3 |  7743 | `						}` |
|       3 |  7744 | `					}` |
|      13 |  7745 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7746 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  7747 | `						continue;` |
|       - |  7748 | `					}` |
|      13 |  7749 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  7750 | `					pR++;` |
|      13 |  7751 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  7752 | `						pSrcTrait = 0;` |
|       7 |  7753 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  7754 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  7755 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  7756 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  7757 | `								pSrcTrait = apTrait[nT];` |
|       5 |  7758 | `								break;` |
|       - |  7759 | `							}` |
|       2 |  7760 | `						}` |
|       5 |  7761 | `						if( pSrcTrait ){` |
|       5 |  7762 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  7763 | `							if( pMeth ){` |
|       5 |  7764 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  7765 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  7766 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  7767 | `								}` |
|       2 |  7768 | `							}` |
|       2 |  7769 | `						}` |
|       2 |  7770 | `					}` |
|      29 |  7771 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  7772 | `				}` |
|       - |  7773 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  7774 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  7775 | `					ph7_class_method *pMR;` |
|       - |  7776 | `					SyHashEntry *pER;` |
|       - |  7777 | `					SyString *pNR;` |
|      11 |  7778 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  7779 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  7780 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  7781 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  7782 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  7783 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  7784 | `						}` |
|       1 |  7785 | `					}` |
|       6 |  7786 | `				}` |
|       - |  7787 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  7788 | `				pR = pUse->pResolvStart;` |
|      21 |  7789 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  7790 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  7791 | `					ph7_class *pSrcTrait;` |
|       - |  7792 | `					ph7_class_method *pMeth;` |
|      21 |  7793 | `					int hasQual = 0;` |
|       - |  7794 | `					sxi32 nRKwrd;` |
|      33 |  7795 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  7796 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  7797 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  7798 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  7799 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  7800 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  7801 | `					sMethod = pR->sData;` |
|      13 |  7802 | `					pR++;` |
|      13 |  7803 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  7804 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  7805 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  7806 | `							sTrait = sMethod;` |
|       7 |  7807 | `							hasQual = 1;` |
|       7 |  7808 | `							pR++;` |
|       7 |  7809 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  7810 | `							sMethod = pR->sData;` |
|       7 |  7811 | `							pR++;` |
|       3 |  7812 | `						}` |
|       3 |  7813 | `					}` |
|      13 |  7814 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7815 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  7816 | `						continue;` |
|       - |  7817 | `					}` |
|      13 |  7818 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  7819 | `					pR++;` |
|      13 |  7820 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  7821 | `						sxi32 iNewVis = -1;` |
|       9 |  7822 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  7823 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  7824 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  7825 | `								iNewVis = nAK;` |
|       7 |  7826 | `								pR++;` |
|       3 |  7827 | `							}` |
|       3 |  7828 | `						}` |
|       9 |  7829 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  7830 | `							sAlias = pR->sData;` |
|       7 |  7831 | `							pR++;` |
|       3 |  7832 | `						}` |
|       9 |  7833 | `						pMeth = 0;` |
|       9 |  7834 | `						if( hasQual ){` |
|       3 |  7835 | `							pSrcTrait = 0;` |
|       5 |  7836 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  7837 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  7838 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  7839 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  7840 | `									pSrcTrait = apTrait[nT];` |
|       3 |  7841 | `									break;` |
|       - |  7842 | `								}` |
|       2 |  7843 | `							}` |
|       3 |  7844 | `							if( pSrcTrait ){` |
|       3 |  7845 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  7846 | `							}` |
|       2 |  7847 | `						}else{` |
|       7 |  7848 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  7849 | `						}` |
|       9 |  7850 | `						if( pMeth ){` |
|       9 |  7851 | `							if( sAlias.nByte > 0 ){` |
|       - |  7852 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  7853 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  7854 | `								 */` |
|       - |  7855 | `								ph7_class_method *pAlias;` |
|       - |  7856 | `								char *zAliasDup;` |
|       7 |  7857 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  7858 | `								if( pAlias ){` |
|       7 |  7859 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  7860 | `									if( iNewVis >= 0 ){` |
|       5 |  7861 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  7862 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  7863 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  7864 | `									}` |
|       7 |  7865 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  7866 | `									if( zAliasDup ){` |
|       7 |  7867 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  7868 | `									}` |
|       4 |  7869 | `								}` |
|       6 |  7870 | `							}else if( iNewVis >= 0 ){` |
|       - |  7871 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  7872 | `								ph7_class_method *pCopy;` |
|       3 |  7873 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  7874 | `								if( pCopy ){` |
|       3 |  7875 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  7876 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  7877 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  7878 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  7879 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  7880 | `									/* Replace the method in the class hash */` |
|       3 |  7881 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  7882 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  7883 | `								}` |
|       1 |  7884 | `							}` |
|       4 |  7885 | `						}` |
|       4 |  7886 | `						SXUNUSED(hasQual);` |
|       4 |  7887 | `					}` |
|      17 |  7888 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  7889 | `				}` |
|       - |  7890 | `			}` |
|      44 |  7891 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  7892 | `		}` |
|       - |  7893 | `	}` |
|       - |  7894 | `	/* Install the class */` |
|   39840 |  7895 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   39840 |  7896 | `	if( rc == SXRET_OK ){` |
|       - |  7897 | `		ph7_class **apInterface;` |
|       - |  7898 | `		sxu32 n;` |
|   39840 |  7899 | `		if( pBase ){` |
|       - |  7900 | `			/* Inherit from base class and mark as a subclass */` |
|   25336 |  7901 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   12667 |  7902 | `		}` |
|   39840 |  7903 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   42680 |  7904 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  7905 | `			/* Implements one or more interface */` |
|    2842 |  7906 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2842 |  7907 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7908 | `				break;` |
|       - |  7909 | `			}` |
|    1422 |  7910 | `		}` |
|       - |  7911 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   39840 |  7912 | `		if( rc == SXRET_OK ){` |
|   39840 |  7913 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   39840 |  7914 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  7915 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  7916 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  7917 | `				return SXERR_ABORT;` |
|       - |  7918 | `			}` |
|   19919 |  7919 | `		}` |
|       - |  7920 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   39840 |  7921 | `		if( rc == SXRET_OK ){` |
|   39840 |  7922 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   39840 |  7923 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  7924 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  7925 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  7926 | `				return SXERR_ABORT;` |
|       - |  7927 | `			}` |
|   19919 |  7928 | `		}` |
|   19919 |  7929 | `	}` |
|   39840 |  7930 | `	SySetRelease(&aUseEntries);` |
|   39840 |  7931 | `	SySetRelease(&aInterfaces);` |
|   39840 |  7932 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7933 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7934 | `		return SXERR_ABORT;` |
|       - |  7935 | `	}` |
|   19919 |  7936 | `done:` |
|       - |  7937 | `	/* Point beyond the class body */` |
|   39844 |  7938 | `	pGen->pIn = &pEnd[1];` |
|   39844 |  7939 | `	pGen->pEnd = pTmp;` |
|   39844 |  7940 | `	return PH7_OK;` |
|   19923 |  7941 |  |
|       - |  7942 | `/*` |
|       - |  7943 | ` * Compile a user-defined abstract class.` |
|       - |  7944 | ` *  According to the PHP language reference manual` |
|       - |  7945 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  7946 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  7947 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  7948 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  7949 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  7950 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  7951 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  7952 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  7953 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  7954 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  7955 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  7956 | ` *   could differ.` |
|       - |  7957 | ` */` |
|      16 |  7958 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  7959 |  |
|       - |  7960 | `	sxi32 rc;` |
|      18 |  7961 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      18 |  7962 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      18 |  7963 | `	return rc;` |
|       2 |  7964 |  |
|       - |  7965 | `/*` |
|       - |  7966 | ` * Compile a user-defined final class.` |
|       - |  7967 | ` *  According to the PHP language reference manual` |
|       - |  7968 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  7969 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  7970 | ` *    final then it cannot be extended.` |
|       - |  7971 | ` */` |
|       2 |  7972 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  7973 |  |
|       - |  7974 | `	sxi32 rc;` |
|       3 |  7975 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  7976 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  7977 | `	return rc;` |
|       1 |  7978 |  |
|       - |  7979 | `/*` |
|       - |  7980 | ` * Compile a user-defined trait.` |
|       - |  7981 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  7982 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  7983 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  7984 | ` */` |
|      54 |  7985 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  7986 |  |
|      56 |  7987 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7988 | `	ph7_class *pClass;` |
|       - |  7989 | `	SyToken *pEnd,*pTmp;` |
|       - |  7990 | `	sxi32 iProtection;` |
|       - |  7991 | `	sxi32 iAttrflags;` |
|       - |  7992 | `	SyString *pName;` |
|       - |  7993 | `	sxi32 nKwrd;` |
|       - |  7994 | `	sxi32 rc;` |
|       - |  7995 | `	/* Jump the 'trait' keyword */` |
|      56 |  7996 | `	pGen->pIn++;` |
|      56 |  7997 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7998 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  7999 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8000 | `			return SXERR_ABORT;` |
|       - |  8001 | `		}` |
|     ! 0 |  8002 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8003 | `			pGen->pIn++;` |
|     ! 0 |  8004 | `		}` |
|     ! 0 |  8005 | `		return SXRET_OK;` |
|       - |  8006 | `	}` |
|       - |  8007 | `	/* Extract trait name */` |
|      56 |  8008 | `	pName = &pGen->pIn->sData;` |
|      56 |  8009 | `	pGen->pIn++;` |
|       - |  8010 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8011 | `		SyBlob sFQN;` |
|       - |  8012 | `		SyString sFQNStr;` |
|      56 |  8013 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8014 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8015 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8016 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8017 | `		SyBlobRelease(&sFQN);` |
|       - |  8018 | `	}` |
|      56 |  8019 | `	if( pClass == 0 ){` |
|     ! 0 |  8020 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8021 | `		return SXERR_ABORT;` |
|       - |  8022 | `	}` |
|       - |  8023 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8024 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8025 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8026 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8027 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8028 | `			return SXERR_ABORT;` |
|       - |  8029 | `		}` |
|     ! 0 |  8030 | `		return SXRET_OK;` |
|       - |  8031 | `	}` |
|      56 |  8032 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8033 | `	pEnd = 0;` |
|      56 |  8034 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8035 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8036 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8037 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8038 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8039 | `			return SXERR_ABORT;` |
|       - |  8040 | `		}` |
|     ! 0 |  8041 | `		return SXRET_OK;` |
|       - |  8042 | `	}` |
|       - |  8043 | `	/* Swap token stream */` |
|      56 |  8044 | `	pTmp = pGen->pEnd;` |
|      56 |  8045 | `	pGen->pEnd = pEnd;` |
|       - |  8046 | `	/* Mark as trait */` |
|      56 |  8047 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8048 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8049 | `	for(;;){` |
|     154 |  8050 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8051 | `			pGen->pIn++;` |
|       2 |  8052 | `		}` |
|     130 |  8053 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8054 | `			break;` |
|       - |  8055 | `		}` |
|      76 |  8056 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8057 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8058 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8059 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8060 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8061 | `				return SXERR_ABORT;` |
|       - |  8062 | `			}` |
|     ! 0 |  8063 | `			goto done;` |
|       - |  8064 | `		}` |
|      76 |  8065 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8066 | `		iAttrflags = 0;` |
|      76 |  8067 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8068 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8069 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8070 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8071 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8072 | `				for(;;){` |
|       - |  8073 | `					ph7_class *pUsedTrait;` |
|       - |  8074 | `					SyString *pUsedName;` |
|       5 |  8075 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8076 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8077 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8078 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8079 | `							return SXERR_ABORT;` |
|       - |  8080 | `						}` |
|     ! 0 |  8081 | `						break;` |
|       - |  8082 | `					}` |
|       5 |  8083 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8084 | `					{` |
|       - |  8085 | `						SyBlob sResolved;` |
|       5 |  8086 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8087 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8088 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8089 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8090 | `						SyBlobRelease(&sResolved);` |
|       - |  8091 | `					}` |
|       5 |  8092 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8093 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8094 | `					}` |
|       5 |  8095 | `					if( pUsedTrait == 0 ){` |
|       4 |  8096 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8097 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8098 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8099 | `							return SXERR_ABORT;` |
|       - |  8100 | `						}` |
|       2 |  8101 | `					}else{` |
|       3 |  8102 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8103 | `					}` |
|       5 |  8104 | `					pGen->pIn++;` |
|       5 |  8105 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8106 | `						break;` |
|       - |  8107 | `					}` |
|     ! 0 |  8108 | `					pGen->pIn++;` |
|     ! 0 |  8109 | `				}` |
|       5 |  8110 | `				continue;` |
|       - |  8111 | `			}` |
|      72 |  8112 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8113 | `				iProtection = nKwrd;` |
|      68 |  8114 | `				pGen->pIn++;` |
|      66 |  8115 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8116 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8117 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8118 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8119 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8120 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8121 | `						return SXERR_ABORT;` |
|       - |  8122 | `					}` |
|     ! 0 |  8123 | `					goto done;` |
|       - |  8124 | `				}` |
|      68 |  8125 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8126 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8127 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8128 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8129 | `							return SXERR_ABORT;` |
|       - |  8130 | `						}` |
|     ! 0 |  8131 | `						goto done;` |
|       - |  8132 | `					}` |
|      11 |  8133 | `					continue;` |
|       - |  8134 | `				}` |
|      58 |  8135 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8136 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8137 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8138 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8139 | `							return SXERR_ABORT;` |
|       - |  8140 | `						}` |
|     ! 0 |  8141 | `						goto done;` |
|       - |  8142 | `					}` |
|       5 |  8143 | `					continue;` |
|       - |  8144 | `				}` |
|      53 |  8145 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8146 | `			}` |
|      57 |  8147 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8148 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8149 | `					"Traits cannot have constants");` |
|     ! 0 |  8150 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8151 | `					return SXERR_ABORT;` |
|       - |  8152 | `				}` |
|     ! 0 |  8153 | `				goto done;` |
|     ! 0 |  8154 | `			}else{` |
|      57 |  8155 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8156 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8157 | `					pGen->pIn++;` |
|       5 |  8158 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8159 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8160 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8161 | `							iProtection = nKwrd;` |
|     ! 0 |  8162 | `							pGen->pIn++;` |
|     ! 0 |  8163 | `						}` |
|       1 |  8164 | `					}` |
|       4 |  8165 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8166 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8167 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8168 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8169 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8170 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8171 | `							return SXERR_ABORT;` |
|       - |  8172 | `						}` |
|     ! 0 |  8173 | `						goto done;` |
|       - |  8174 | `					}` |
|       5 |  8175 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8176 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8177 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8178 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8179 | `								return SXERR_ABORT;` |
|       - |  8180 | `							}` |
|     ! 0 |  8181 | `							goto done;` |
|       - |  8182 | `						}` |
|       3 |  8183 | `						continue;` |
|       - |  8184 | `					}` |
|       3 |  8185 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8186 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8187 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8188 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8189 | `								return SXERR_ABORT;` |
|       - |  8190 | `							}` |
|     ! 0 |  8191 | `							goto done;` |
|       - |  8192 | `						}` |
|     ! 0 |  8193 | `						continue;` |
|       - |  8194 | `					}` |
|       3 |  8195 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  8196 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  8197 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  8198 | `					pGen->pIn++;` |
|       5 |  8199 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  8200 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8201 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8202 | `							iProtection = nKwrd;` |
|       5 |  8203 | `							pGen->pIn++;` |
|       2 |  8204 | `						}` |
|       2 |  8205 | `					}` |
|       5 |  8206 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8207 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8208 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8209 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  8210 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8211 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8212 | `							return SXERR_ABORT;` |
|       - |  8213 | `						}` |
|     ! 0 |  8214 | `						goto done;` |
|       - |  8215 | `					}` |
|       5 |  8216 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8217 | `				}` |
|      55 |  8218 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8219 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8220 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  8221 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8222 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8223 | `						return SXERR_ABORT;` |
|       - |  8224 | `					}` |
|     ! 0 |  8225 | `					goto done;` |
|       - |  8226 | `				}` |
|      55 |  8227 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  8228 | `					pGen->pIn++;` |
|     ! 0 |  8229 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8230 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8231 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8232 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8233 | `							return SXERR_ABORT;` |
|       - |  8234 | `						}` |
|     ! 0 |  8235 | `						goto done;` |
|       - |  8236 | `					}` |
|     ! 0 |  8237 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8238 | `				}else{` |
|      55 |  8239 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8240 | `				}` |
|      55 |  8241 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8242 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8243 | `						return SXERR_ABORT;` |
|       - |  8244 | `					}` |
|     ! 0 |  8245 | `					goto done;` |
|       - |  8246 | `				}` |
|       - |  8247 | `			}` |
|      28 |  8248 | `		}else{` |
|     ! 0 |  8249 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8250 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8251 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8252 | `					return SXERR_ABORT;` |
|       - |  8253 | `				}` |
|     ! 0 |  8254 | `				goto done;` |
|       - |  8255 | `			}` |
|       - |  8256 | `		}` |
|       1 |  8257 | `	}` |
|       - |  8258 | `	/* Install the trait */` |
|      56 |  8259 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  8260 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8261 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8262 | `		return SXERR_ABORT;` |
|       - |  8263 | `	}` |
|      27 |  8264 | `done:` |
|       - |  8265 | `	/* Point beyond the trait body */` |
|      56 |  8266 | `	pGen->pIn = &pEnd[1];` |
|      56 |  8267 | `	pGen->pEnd = pTmp;` |
|      56 |  8268 | `	return PH7_OK;` |
|      29 |  8269 |  |
|       - |  8270 | `/*` |
|       - |  8271 | ` * Compile a user-defined class.` |
|       - |  8272 | ` *  According to the PHP language reference manual` |
|       - |  8273 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  8274 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  8275 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  8276 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  8277 | ` *   and functions (called "methods").` |
|       - |  8278 | ` */` |
|   39824 |  8279 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  8280 |  |
|       - |  8281 | `	sxi32 rc;` |
|   39826 |  8282 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   39826 |  8283 | `	return rc;` |
|       2 |  8284 |  |
|       - |  8285 | `/*` |
|       - |  8286 | ` * Exception handling.` |
|       - |  8287 | ` *  According to the PHP language reference manual` |
|       - |  8288 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  8289 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  8290 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  8291 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  8292 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  8293 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  8294 | ` *    (or re-thrown) within a catch block.` |
|       - |  8295 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  8296 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  8297 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  8298 | ` *    been defined with set_exception_handler().` |
|       - |  8299 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  8300 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  8301 | ` */` |
|       - |  8302 | `/*` |
|       - |  8303 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  8304 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  8305 | ` * indicates failure.` |
|       - |  8306 | ` */` |
|    8466 |  8307 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  8308 |  |
|    8468 |  8309 | `	sxi32 rc = SXRET_OK;` |
|    8468 |  8310 | `	if( pRoot->pOp ){` |
|    8462 |  8311 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4233 |  8312 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  8313 | `			/* Unexpected expression */` |
|     ! 0 |  8314 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8315 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  8316 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  8317 | `				rc = SXERR_INVALID;` |
|     ! 0 |  8318 | `			}` |
|       2 |  8319 | `		}` |
|    4236 |  8320 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  8321 | `		/* Unexpected expression */` |
|     ! 0 |  8322 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8323 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  8324 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  8325 | `			rc = SXERR_INVALID;` |
|     ! 0 |  8326 | `		}` |
|     ! 0 |  8327 | `	}` |
|    8468 |  8328 | `	return rc;` |
|       2 |  8329 |  |
|       - |  8330 | `/*` |
|       - |  8331 | ` * Compile a 'throw' statement.` |
|       - |  8332 | ` * throw: This is how you trigger an exception.` |
|       - |  8333 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  8334 | ` */` |
|    8466 |  8335 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  8336 |  |
|    8468 |  8337 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8338 | `	GenBlock *pBlock;` |
|       - |  8339 | `	sxu32 nIdx;` |
|       - |  8340 | `	sxi32 rc;` |
|    8468 |  8341 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  8342 | `	/* Compile the expression */` |
|    8468 |  8343 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8468 |  8344 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8345 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  8346 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8347 | `			return SXERR_ABORT;` |
|       - |  8348 | `		}` |
|     ! 0 |  8349 | `		return SXRET_OK;` |
|       - |  8350 | `	}` |
|    8468 |  8351 | `	pBlock = pGen->pCurrent;` |
|       - |  8352 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   39350 |  8353 | `	while(pBlock->pParent){` |
|   39346 |  8354 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8464 |  8355 | `			break;` |
|       - |  8356 | `		}` |
|       - |  8357 | `		/* Point to the parent block */` |
|   30884 |  8358 | `		pBlock = pBlock->pParent;` |
|       2 |  8359 | `	}` |
|       - |  8360 | `	/* Emit the throw instruction */` |
|    8468 |  8361 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  8362 | `	/* Emit the jump */` |
|    8468 |  8363 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8468 |  8364 | `	return SXRET_OK;` |
|    4235 |  8365 |  |
|       - |  8366 | `/*` |
|       - |  8367 | ` * Compile a 'catch' block.` |
|       - |  8368 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  8369 | ` * an object containing the exception information.` |
|       - |  8370 | ` */` |
|     158 |  8371 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  8372 |  |
|     160 |  8373 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8374 | `	ph7_exception_block sCatch;` |
|       - |  8375 | `	SySet *pInstrContainer;` |
|       - |  8376 | `	SyString sClassName;` |
|       - |  8377 | `	GenBlock *pCatch;` |
|       - |  8378 | `	SyToken *pToken;` |
|       - |  8379 | `	SyString *pName;` |
|       - |  8380 | `	char *zDup;` |
|       - |  8381 | `	sxi32 rc;` |
|     160 |  8382 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  8383 | `	/* Zero the structure */` |
|     160 |  8384 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  8385 | `	/* Initialize fields */` |
|     160 |  8386 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     160 |  8387 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     160 |  8388 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  8389 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  8390 | `			pToken = pGen->pIn;` |
|     ! 0 |  8391 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8392 | `				pToken--;` |
|     ! 0 |  8393 | `			}` |
|     ! 0 |  8394 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8395 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8396 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8397 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8398 | `				return SXERR_ABORT;` |
|       - |  8399 | `			}` |
|     ! 0 |  8400 | `			return SXERR_INVALID;` |
|       - |  8401 | `	}` |
|       - |  8402 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     160 |  8403 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      91 |  8404 | `	for(;;){` |
|     184 |  8405 | `		int isAbsolute = 0;` |
|       - |  8406 | `		SyBlob sName;` |
|     184 |  8407 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - |  8408 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     184 |  8409 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       7 |  8410 | `			isAbsolute = 1;` |
|       7 |  8411 | `			pGen->pIn++;` |
|       3 |  8412 | `		}` |
|     184 |  8413 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     182 |  8414 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 |  8415 | `			SyBlobRelease(&sName);` |
|       5 |  8416 | `			pToken = pGen->pIn;` |
|       5 |  8417 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8418 | `				pToken--;` |
|     ! 0 |  8419 | `			}` |
|       7 |  8420 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8421 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  8422 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  8423 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8424 | `				return SXERR_ABORT;` |
|       - |  8425 | `			}` |
|       5 |  8426 | `			return SXERR_INVALID;` |
|       - |  8427 | `		}` |
|       - |  8428 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     180 |  8429 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     180 |  8430 | `		pGen->pIn++;` |
|     273 |  8431 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      97 |  8432 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 |  8433 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 |  8434 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 |  8435 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 |  8436 | `			pGen->pIn++;` |
|       1 |  8437 | `		}` |
|       - |  8438 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     180 |  8439 | `		if( !isAbsolute ){` |
|       - |  8440 | `			SyString sRaw;` |
|       - |  8441 | `			SyBlob sResolved;` |
|     174 |  8442 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     174 |  8443 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     174 |  8444 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     260 |  8445 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     172 |  8446 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     174 |  8447 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     174 |  8448 | `			SyBlobRelease(&sResolved);` |
|      88 |  8449 | `		}else{` |
|       - |  8450 | `			/* Absolute name: use as-is without namespace prefix */` |
|      10 |  8451 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       6 |  8452 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       7 |  8453 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - |  8454 | `		}` |
|     180 |  8455 | `		SyBlobRelease(&sName);` |
|     180 |  8456 | `		if( zDup == 0 ){` |
|     ! 0 |  8457 | `			goto Mem;` |
|       - |  8458 | `		}` |
|     180 |  8459 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     180 |  8460 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  8461 | `			goto Mem;` |
|       - |  8462 | `		}` |
|       - |  8463 | `		/* Check for '\|' (multi-catch separator) */` |
|     190 |  8464 | `		if( pGen->pIn < pGen->pEnd &&` |
|     178 |  8465 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 |  8466 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 |  8467 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 |  8468 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 |  8469 | `			continue;` |
|       - |  8470 | `		}` |
|     156 |  8471 | `		break;` |
|     ! 0 |  8472 | `	}` |
|     231 |  8473 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     156 |  8474 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8475 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  8476 | `			pToken = pGen->pIn;` |
|     ! 0 |  8477 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8478 | `				pToken--;` |
|     ! 0 |  8479 | `			}` |
|     ! 0 |  8480 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8481 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8482 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8483 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8484 | `				return SXERR_ABORT;` |
|       - |  8485 | `			}` |
|     ! 0 |  8486 | `			return SXERR_INVALID;` |
|       - |  8487 | `	}` |
|     156 |  8488 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  8489 | `	/* Duplicate instance name */` |
|     156 |  8490 | `	pName = &pGen->pIn->sData;` |
|     156 |  8491 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     156 |  8492 | `	if( zDup == 0 ){` |
|     ! 0 |  8493 | `		goto Mem;` |
|       - |  8494 | `	}` |
|     156 |  8495 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     156 |  8496 | `	pGen->pIn++;` |
|     156 |  8497 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  8498 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  8499 | `		pToken = pGen->pIn;` |
|     ! 0 |  8500 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8501 | `			pToken--;` |
|     ! 0 |  8502 | `		}` |
|     ! 0 |  8503 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8504 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8505 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8506 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8507 | `			return SXERR_ABORT;` |
|       - |  8508 | `		}` |
|     ! 0 |  8509 | `		return SXERR_INVALID;` |
|       - |  8510 | `	}` |
|       - |  8511 | `	/* Compile the block */` |
|     156 |  8512 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  8513 | `	/* Create the catch block */` |
|     156 |  8514 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     156 |  8515 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8516 | `		return SXERR_ABORT;` |
|       - |  8517 | `	}` |
|       - |  8518 | `	/* Swap bytecode container */` |
|     156 |  8519 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     156 |  8520 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  8521 | `	/* Compile the block */` |
|     156 |  8522 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  8523 | `	/* Fix forward jumps now the destination is resolved  */` |
|     156 |  8524 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  8525 | `	/* Emit the DONE instruction */` |
|     156 |  8526 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  8527 | `	/* Leave the block */` |
|     156 |  8528 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  8529 | `	/* Restore the default container */` |
|     156 |  8530 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  8531 | `	/* Install the catch block */` |
|     156 |  8532 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     156 |  8533 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8534 | `		goto Mem;` |
|       - |  8535 | `	}` |
|     156 |  8536 | `	return SXRET_OK;` |
|     ! 0 |  8537 | `Mem:` |
|     ! 0 |  8538 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8539 | `	return SXERR_ABORT;` |
|      81 |  8540 |  |
|       - |  8541 | `/*` |
|       - |  8542 | ` * Compile a 'try' block.` |
|       - |  8543 | ` * A function using an exception should be in a "try" block.` |
|       - |  8544 | ` * If the exception does not trigger, the code will continue` |
|       - |  8545 | ` * as normal. However if the exception triggers, an exception` |
|       - |  8546 | ` * is "thrown".` |
|       - |  8547 | ` */` |
|     166 |  8548 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  8549 |  |
|       - |  8550 | `	ph7_exception *pException;` |
|     168 |  8551 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8552 | `	GenBlock *pTry;` |
|       - |  8553 | `	sxu32 nJmpIdx;` |
|       - |  8554 | `	sxi32 rc;` |
|       - |  8555 | `	/* Create the exception container */` |
|     168 |  8556 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     168 |  8557 | `	if( pException == 0 ){` |
|     ! 0 |  8558 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  8559 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  8560 | `		return SXERR_ABORT;` |
|       - |  8561 | `	}` |
|       - |  8562 | `	/* Zero the structure */` |
|     168 |  8563 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  8564 | `	/* Initialize fields */` |
|     168 |  8565 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     168 |  8566 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     168 |  8567 | `	pException->iHasFinally = 0;` |
|     168 |  8568 | `	pException->iFinallyDone = 0;` |
|     168 |  8569 | `	pException->pVm = pGen->pVm;` |
|       - |  8570 | `	/* Create the try block */` |
|     168 |  8571 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     168 |  8572 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8573 | `		return SXERR_ABORT;` |
|       - |  8574 | `	}` |
|       - |  8575 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     168 |  8576 | `	pTry->pUserData = pException;` |
|       - |  8577 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     168 |  8578 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  8579 | `	/* Fix the jump later when the destination is resolved */` |
|     168 |  8580 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     168 |  8581 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  8582 | `	/* Compile the block */` |
|     168 |  8583 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     168 |  8584 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8585 | `		return SXERR_ABORT;` |
|       - |  8586 | `	}` |
|       - |  8587 | `	/* Fix forward jumps now the destination is resolved */` |
|     168 |  8588 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  8589 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     168 |  8590 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  8591 | `	/* Leave the block */` |
|     168 |  8592 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  8593 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     168 |  8594 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     164 |  8595 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  8596 | `		/* Compile one or more catch blocks */` |
|     156 |  8597 | `		for(;;){` |
|     312 |  8598 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     244 |  8599 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      79 |  8600 | `					break;` |
|       - |  8601 | `			}` |
|     160 |  8602 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     160 |  8603 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8604 | `				return SXERR_ABORT;` |
|       - |  8605 | `			}` |
|       2 |  8606 | `		}` |
|      77 |  8607 | `	}` |
|       - |  8608 | `	/* Compile optional finally block */` |
|     168 |  8609 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      82 |  8610 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  8611 | `		SySet *pInstrContainer;` |
|       - |  8612 | `		GenBlock *pFinBlock;` |
|      32 |  8613 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  8614 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  8615 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  8616 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  8617 | `			return SXERR_ABORT;` |
|       - |  8618 | `		}` |
|       - |  8619 | `		/* Swap bytecode container */` |
|      32 |  8620 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  8621 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  8622 | `		/* Compile the finally body */` |
|      32 |  8623 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  8624 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8625 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  8626 | `			return SXERR_ABORT;` |
|       - |  8627 | `		}` |
|       - |  8628 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  8629 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  8630 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  8631 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  8632 | `		/* Leave the block */` |
|      32 |  8633 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  8634 | `		/* Restore the default container */` |
|      32 |  8635 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  8636 | `		pException->iHasFinally = 1;` |
|      15 |  8637 | `	}` |
|       - |  8638 | `	/* Must have at least one catch or finally */` |
|     168 |  8639 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  8640 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  8641 | `			"Cannot use try without catch or finally");` |
|       7 |  8642 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8643 | `			return SXERR_ABORT;` |
|       - |  8644 | `		}` |
|       3 |  8645 | `	}` |
|     168 |  8646 | `	return SXRET_OK;` |
|      85 |  8647 |  |
|       - |  8648 | `/*` |
|       - |  8649 | ` * Compile a switch block.` |
|       - |  8650 | ` *  (See block-comment below for more information)` |
|       - |  8651 | ` */` |
|     108 |  8652 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  8653 |  |
|     110 |  8654 | `	sxi32 rc = SXRET_OK;` |
|     110 |  8655 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  8656 | `		/* Unexpected token */` |
|     ! 0 |  8657 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  8658 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8659 | `			return SXERR_ABORT;` |
|       - |  8660 | `		}` |
|     ! 0 |  8661 | `		pGen->pIn++;` |
|     ! 0 |  8662 | `	}` |
|     110 |  8663 | `	pGen->pIn++;` |
|       - |  8664 | `	/* First instruction to execute in this block. */` |
|     110 |  8665 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  8666 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  8667 | `	 * or the '}' token */` |
|     182 |  8668 | `	for(;;){` |
|     366 |  8669 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8670 | `			/* No more input to process */` |
|     ! 0 |  8671 | `			break;` |
|       - |  8672 | `		}` |
|     366 |  8673 | `		rc = SXRET_OK;` |
|     366 |  8674 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  8675 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  8676 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  8677 | `					/* Unexpected token */` |
|     ! 0 |  8678 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  8679 | `						&pGen->pIn->sData);` |
|     ! 0 |  8680 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8681 | `						return SXERR_ABORT;` |
|       - |  8682 | `					}` |
|       - |  8683 | `					/* FALL THROUGH */` |
|     ! 0 |  8684 | `				}` |
|      28 |  8685 | `				rc = SXERR_EOF;` |
|      28 |  8686 | `				break;` |
|       - |  8687 | `			}` |
|      23 |  8688 | `		}else{` |
|       - |  8689 | `			sxi32 nKwrd;` |
|       - |  8690 | `			/* Extract the keyword */` |
|     298 |  8691 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  8692 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  8693 | `				break;` |
|       - |  8694 | `			}` |
|     218 |  8695 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  8696 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  8697 | `					/* Unexpected token */` |
|     ! 0 |  8698 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  8699 | `						&pGen->pIn->sData);` |
|     ! 0 |  8700 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8701 | `						return SXERR_ABORT;` |
|       - |  8702 | `					}` |
|       - |  8703 | `					/* FALL THROUGH */` |
|     ! 0 |  8704 | `				}` |
|       - |  8705 | `				/* Block compiled */` |
|       3 |  8706 | `				break;` |
|       - |  8707 | `			}` |
|       - |  8708 | `		}` |
|       - |  8709 | `		/* Compile block */` |
|     258 |  8710 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  8711 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8712 | `			return SXERR_ABORT;` |
|       - |  8713 | `		}` |
|       2 |  8714 | `	}` |
|     110 |  8715 | `	return rc;` |
|      56 |  8716 |  |
|       - |  8717 | `/*` |
|       - |  8718 | ` * Compile a case eXpression.` |
|       - |  8719 | ` *  (See block-comment below for more information)` |
|       - |  8720 | ` */` |
|      88 |  8721 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  8722 |  |
|       - |  8723 | `	SySet *pInstrContainer;` |
|       - |  8724 | `	SyToken *pEnd,*pTmp;` |
|      90 |  8725 | `	sxi32 iNest = 0;` |
|       - |  8726 | `	sxi32 rc;` |
|       - |  8727 | `	/* Delimit the expression */` |
|      90 |  8728 | `	pEnd = pGen->pIn;` |
|     186 |  8729 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  8730 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  8731 | `			/* Increment nesting level */` |
|       3 |  8732 | `			iNest++;` |
|     185 |  8733 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  8734 | `			/* Decrement nesting level */` |
|       3 |  8735 | `			iNest--;` |
|     183 |  8736 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  8737 | `			break;` |
|       - |  8738 | `		}` |
|      98 |  8739 | `		pEnd++;` |
|       2 |  8740 | `	}` |
|      90 |  8741 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  8742 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  8743 | `		if( rc == SXERR_ABORT ){` |
|       - |  8744 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8745 | `			return SXERR_ABORT;` |
|       - |  8746 | `		}` |
|     ! 0 |  8747 | `	}` |
|       - |  8748 | `	/* Swap token stream */` |
|      90 |  8749 | `	pTmp = pGen->pEnd;` |
|      90 |  8750 | `	pGen->pEnd = pEnd;` |
|      90 |  8751 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  8752 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  8753 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  8754 | `	/* Emit the done instruction */` |
|      90 |  8755 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  8756 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  8757 | `	/* Update token stream */` |
|      90 |  8758 | `	pGen->pIn  = pEnd;` |
|      90 |  8759 | `	pGen->pEnd = pTmp;` |
|      90 |  8760 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  8761 | `		return SXERR_ABORT;` |
|       - |  8762 | `	}` |
|      90 |  8763 | `	return SXRET_OK;` |
|      46 |  8764 |  |
|       - |  8765 | `/*` |
|       - |  8766 | ` * Compile the smart switch statement.` |
|       - |  8767 | ` * According to the PHP language reference manual` |
|       - |  8768 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  8769 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  8770 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  8771 | ` *  This is exactly what the switch statement is for.` |
|       - |  8772 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  8773 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  8774 | ` *  of the outer loop, use continue 2.` |
|       - |  8775 | ` *  Note that switch/case does loose comparision.` |
|       - |  8776 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  8777 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  8778 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  8779 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  8780 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  8781 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  8782 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  8783 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  8784 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  8785 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  8786 | ` *  list for the next case.` |
|       - |  8787 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  8788 | ` *  or floating-point numbers and strings.` |
|       - |  8789 | ` */` |
|      28 |  8790 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  8791 |  |
|       - |  8792 | `	GenBlock *pSwitchBlock;` |
|       - |  8793 | `	SyToken *pTmp,*pEnd;` |
|       - |  8794 | `	ph7_switch *pSwitch;` |
|       - |  8795 | `	sxu32 nToken;` |
|       - |  8796 | `	sxu32 nLine;` |
|       - |  8797 | `	sxi32 rc;` |
|      30 |  8798 | `	nLine = pGen->pIn->nLine;` |
|       - |  8799 | `	/* Jump the 'switch' keyword */` |
|      30 |  8800 | `	pGen->pIn++;` |
|      30 |  8801 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  8802 | `		/* Syntax error */` |
|     ! 0 |  8803 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  8804 | `		if( rc == SXERR_ABORT ){` |
|       - |  8805 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8806 | `			return SXERR_ABORT;` |
|       - |  8807 | `		}` |
|     ! 0 |  8808 | `		goto Synchronize;` |
|       - |  8809 | `	}` |
|       - |  8810 | `	/* Jump the left parenthesis '(' */` |
|      30 |  8811 | `	pGen->pIn++;` |
|      30 |  8812 | `	pEnd = 0; /* cc warning */` |
|       - |  8813 | `	/* Create the loop block */` |
|      44 |  8814 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  8815 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  8816 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8817 | `		return SXERR_ABORT;` |
|       - |  8818 | `	}` |
|       - |  8819 | `	/* Delimit the condition */` |
|      30 |  8820 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  8821 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  8822 | `		/* Empty expression */` |
|     ! 0 |  8823 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  8824 | `		if( rc == SXERR_ABORT ){` |
|       - |  8825 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8826 | `			return SXERR_ABORT;` |
|       - |  8827 | `		}` |
|     ! 0 |  8828 | `	}` |
|       - |  8829 | `	/* Swap token streams */` |
|      30 |  8830 | `	pTmp = pGen->pEnd;` |
|      30 |  8831 | `	pGen->pEnd = pEnd;` |
|       - |  8832 | `	/* Compile the expression */` |
|      30 |  8833 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  8834 | `	if( rc == SXERR_ABORT ){` |
|       - |  8835 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  8836 | `		return SXERR_ABORT;` |
|       - |  8837 | `	}` |
|       - |  8838 | `	/* Update token stream */` |
|      30 |  8839 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  8840 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8841 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  8842 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8843 | `			return SXERR_ABORT;` |
|       - |  8844 | `		}` |
|     ! 0 |  8845 | `		pGen->pIn++;` |
|     ! 0 |  8846 | `	}` |
|      30 |  8847 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  8848 | `	pGen->pEnd = pTmp;` |
|      30 |  8849 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  8850 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  8851 | `			pTmp = pGen->pIn;` |
|     ! 0 |  8852 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  8853 | `				pTmp--;` |
|     ! 0 |  8854 | `			}` |
|       - |  8855 | `			/* Unexpected token */` |
|     ! 0 |  8856 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  8857 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8858 | `				return SXERR_ABORT;` |
|       - |  8859 | `			}` |
|     ! 0 |  8860 | `			goto Synchronize;` |
|       - |  8861 | `	}` |
|       - |  8862 | `	/* Set the delimiter token */` |
|      30 |  8863 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  8864 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  8865 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  8866 | `	}else{` |
|      28 |  8867 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  8868 | `	}` |
|      30 |  8869 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  8870 | `	/* Create the switch blocks container */` |
|      30 |  8871 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  8872 | `	if( pSwitch == 0 ){` |
|       - |  8873 | `		/* Abort compilation */` |
|     ! 0 |  8874 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8875 | `		return SXERR_ABORT;` |
|       - |  8876 | `	}` |
|       - |  8877 | `	/* Zero the structure */` |
|      30 |  8878 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  8879 | `	/* Initialize fields */` |
|      30 |  8880 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  8881 | `	/* Emit the switch instruction */` |
|      30 |  8882 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  8883 | `	/* Compile case blocks */` |
|      96 |  8884 | `	for(;;){` |
|       - |  8885 | `		sxu32 nKwrd;` |
|     112 |  8886 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8887 | `			/* No more input to process */` |
|     ! 0 |  8888 | `			break;` |
|       - |  8889 | `		}` |
|     112 |  8890 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8891 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  8892 | `				/* Unexpected token */` |
|     ! 0 |  8893 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  8894 | `					&pGen->pIn->sData);` |
|     ! 0 |  8895 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8896 | `					return SXERR_ABORT;` |
|       - |  8897 | `				}` |
|       - |  8898 | `				/* FALL THROUGH */` |
|     ! 0 |  8899 | `			}` |
|       - |  8900 | `			/* Block compiled */` |
|     ! 0 |  8901 | `			break;` |
|       - |  8902 | `		}` |
|       - |  8903 | `		/* Extract the keyword */` |
|     112 |  8904 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  8905 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  8906 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  8907 | `				/* Unexpected token */` |
|     ! 0 |  8908 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  8909 | `					&pGen->pIn->sData);` |
|     ! 0 |  8910 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8911 | `					return SXERR_ABORT;` |
|       - |  8912 | `				}` |
|       - |  8913 | `				/* FALL THROUGH */` |
|     ! 0 |  8914 | `			}` |
|       - |  8915 | `			/* Block compiled */` |
|       3 |  8916 | `			break;` |
|       - |  8917 | `		}` |
|     110 |  8918 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  8919 | `			/*` |
|       - |  8920 | `			 * Accroding to the PHP language reference manual` |
|       - |  8921 | `			 *  A special case is the default case. This case matches anything` |
|       - |  8922 | `			 *  that wasn't matched by the other cases.` |
|       - |  8923 | `			 */` |
|      22 |  8924 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  8925 | `				/* Default case already compiled */` |
|     ! 0 |  8926 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  8927 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8928 | `					return SXERR_ABORT;` |
|       - |  8929 | `				}` |
|     ! 0 |  8930 | `			}` |
|      22 |  8931 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  8932 | `			/* Compile the default block */` |
|      22 |  8933 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  8934 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  8935 | `				return SXERR_ABORT;` |
|      22 |  8936 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  8937 | `				break;` |
|       1 |  8938 | `			}` |
|      91 |  8939 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  8940 | `			ph7_case_expr sCase;` |
|       - |  8941 | `			/* Standard case block */` |
|      90 |  8942 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  8943 | `			/* initialize the structure */` |
|      90 |  8944 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  8945 | `			/* Compile the case expression */` |
|      90 |  8946 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  8947 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8948 | `				return SXERR_ABORT;` |
|       - |  8949 | `			}` |
|       - |  8950 | `			/* Compile the case block */` |
|      90 |  8951 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  8952 | `			/* Insert in the switch container */` |
|      90 |  8953 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  8954 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  8955 | `				return SXERR_ABORT;` |
|      90 |  8956 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  8957 | `				break;` |
|       - |  8958 | `			}` |
|      42 |  8959 | `		}else{` |
|       - |  8960 | `			/* Unexpected token */` |
|     ! 0 |  8961 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  8962 | `				&pGen->pIn->sData);` |
|     ! 0 |  8963 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8964 | `				return SXERR_ABORT;` |
|       - |  8965 | `			}` |
|     ! 0 |  8966 | `			break;` |
|       - |  8967 | `		}` |
|       2 |  8968 | `	}` |
|       - |  8969 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  8970 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  8971 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  8972 | `	/* Release the loop block */` |
|      30 |  8973 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  8974 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  8975 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  8976 | `		pGen->pIn++;` |
|      14 |  8977 | `	}` |
|       - |  8978 | `	/* Statement successfully compiled */` |
|      30 |  8979 | `	return SXRET_OK;` |
|     ! 0 |  8980 | `Synchronize:` |
|       - |  8981 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  8982 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  8983 | `		pGen->pIn++;` |
|     ! 0 |  8984 | `	}` |
|     ! 0 |  8985 | `	return SXRET_OK;` |
|      16 |  8986 |  |
|       - |  8987 | `/*` |
|       - |  8988 | ` * Generate bytecode for a given expression tree.` |
|       - |  8989 | ` * If something goes wrong while generating bytecode` |
|       - |  8990 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  8991 | ` * this function takes care of generating the appropriate` |
|       - |  8992 | ` * error message.` |
|       - |  8993 | ` */` |
| 2520618 |  8994 | `static sxi32 GenStateEmitExprCode(` |
|       - |  8995 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  8996 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  8997 | `	sxi32 iFlags /* Control flags */` |
|       - |  8998 | `	)` |
|       2 |  8999 |  |
|       - |  9000 | `	VmInstr *pInstr;` |
|       - |  9001 | `	sxu32 nJmpIdx;` |
| 2520620 |  9002 | `	sxi32 iP1 = 0;` |
| 2520620 |  9003 | `	sxu32 iP2 = 0;` |
| 2520620 |  9004 | `	void *p3  = 0;` |
|       - |  9005 | `	sxi32 iVmOp;` |
|       - |  9006 | `	sxi32 rc;` |
| 2520620 |  9007 | `	if( pNode->xCode ){` |
|       - |  9008 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9009 | `		/* Compile node */` |
| 1562168 |  9010 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1562168 |  9011 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1562168 |  9012 | `		RE_SWAP_DELIMITER(pGen);` |
| 1562168 |  9013 | `		return rc;` |
|       - |  9014 | `	}` |
|  958454 |  9015 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9016 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9017 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9018 | `		return SXERR_ABORT;` |
|       - |  9019 | `	}` |
|  958454 |  9020 | `	iVmOp = pNode->pOp->iVmOp;` |
|  958454 |  9021 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9022 | `		sxu32 nJmp = 0;` |
|       - |  9023 | `		VmInstr *pInstrFix;` |
|       - |  9024 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9025 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9026 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9027 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9028 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9029 | `		if( pNode->pRight ){` |
|      47 |  9030 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9031 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9032 | `				return rc;` |
|       - |  9033 | `			}` |
|       - |  9034 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9035 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9036 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9037 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9038 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9039 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9040 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9041 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9042 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9043 | `				pInstrFix->iP2 = 3;` |
|       9 |  9044 | `			}` |
|      23 |  9045 | `		}` |
|       - |  9046 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9047 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9048 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9049 | `		if( pNode->pLeft ){` |
|      47 |  9050 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9051 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9052 | `				return rc;` |
|       - |  9053 | `			}` |
|      23 |  9054 | `		}` |
|       - |  9055 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9056 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9057 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9058 | `		if( nJmp > 0 ){` |
|      47 |  9059 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9060 | `			if( pInstrFix ){` |
|      47 |  9061 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9062 | `			}` |
|      23 |  9063 | `		}` |
|      47 |  9064 | `		return SXRET_OK;` |
|       - |  9065 | `	}` |
|  958408 |  9066 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9067 | `		sxu32 nJz,nJmp;` |
|       - |  9068 | `		/* Ternary operator require special handling */` |
|       - |  9069 | `		/* Phase#1: Compile the condition */` |
|    1952 |  9070 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    1952 |  9071 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9072 | `			return rc;` |
|       - |  9073 | `		}` |
|    1952 |  9074 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    1952 |  9075 | `		if( pNode->pLeft ){` |
|       - |  9076 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9077 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1884 |  9078 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9079 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1884 |  9080 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1884 |  9081 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9082 | `				return rc;` |
|       - |  9083 | `			}` |
|     943 |  9084 | `		}else{` |
|       - |  9085 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9086 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9087 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9088 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9089 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9090 | `		}` |
|       - |  9091 | `		/* Phase#4: Emit the unconditional jump */` |
|    1952 |  9092 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9093 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    1952 |  9094 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    1952 |  9095 | `		if( pInstr ){` |
|    1952 |  9096 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     975 |  9097 | `		}` |
|    1952 |  9098 | `		if( !pNode->pLeft ){` |
|       - |  9099 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9100 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9101 | `		}` |
|       - |  9102 | `		/* Phase#6: Compile the 'else' expression */` |
|    1952 |  9103 | `		if( pNode->pRight ){` |
|    1952 |  9104 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    1952 |  9105 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9106 | `				return rc;` |
|       - |  9107 | `			}` |
|     975 |  9108 | `		}` |
|    1952 |  9109 | `		if( nJmp > 0 ){` |
|       - |  9110 | `			/* Phase#7: Fix the unconditional jump */` |
|    1952 |  9111 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    1952 |  9112 | `			if( pInstr ){` |
|    1952 |  9113 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     975 |  9114 | `			}` |
|     975 |  9115 | `		}` |
|       - |  9116 | `		/* All done */` |
|    1952 |  9117 | `		return SXRET_OK;` |
|       - |  9118 | `	}` |
|       - |  9119 | `	/* Generate code for the left tree */` |
|  956458 |  9120 | `	if( pNode->pLeft ){` |
|  956422 |  9121 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - |  9122 | `			ph7_expr_node **apNode;` |
|  320966 |  9123 | `			int hasSpread = 0;` |
|       - |  9124 | `			sxi32 n;` |
|       - |  9125 | `			/* Recurse and generate bytecodes for function arguments */` |
|  320966 |  9126 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|       - |  9127 | `			/* Read-only load */` |
|  320966 |  9128 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  641040 |  9129 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|  320076 |  9130 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  320076 |  9131 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9132 | `					return rc;` |
|       - |  9133 | `				}` |
|  320076 |  9134 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - |  9135 | `					/* Emit spread opcode to unpack this array argument */` |
|      15 |  9136 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      15 |  9137 | `					hasSpread = 1;` |
|       7 |  9138 | `				}` |
|  160039 |  9139 | `			}` |
|       - |  9140 | `			/* Total number of given arguments */` |
|  320966 |  9141 | `			iP1 = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|  320966 |  9142 | `			iP2 = hasSpread;` |
|       - |  9143 | `			/* Remove stale flags now */` |
|  320966 |  9144 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  160482 |  9145 | `		}` |
|  956422 |  9146 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  956422 |  9147 | `		if( rc != SXRET_OK ){` |
|      27 |  9148 | `			return rc;` |
|       - |  9149 | `		}` |
|  956396 |  9150 | `		if( iVmOp == PH7_OP_CALL ){` |
|  320966 |  9151 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  320966 |  9152 | `			if( pInstr ){` |
|  320966 |  9153 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  320218 |  9154 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - |  9155 | `					sxu32 nQual;` |
|       - |  9156 | `					/* Prevent constant expansion */` |
|  320218 |  9157 | `					pInstr->iP1 = 0;` |
|       - |  9158 | `					/* Namespace-qualify the function name for CALL.` |
|       - |  9159 | `					 * Only check function imports — class imports must NOT` |
|       - |  9160 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - |  9161 | `					 * handler fires before NEW; we store the original literal` |
|       - |  9162 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - |  9163 | `					 * can recover the unqualified name and re-qualify with` |
|       - |  9164 | `					 * class imports. */ {` |
|  320218 |  9165 | `						int fromImport = 0;` |
|  320218 |  9166 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  320218 |  9167 | `						pInstr->iP2 = (sxi32)nQual;` |
|  320218 |  9168 | `						if( nQual != nOrig ){` |
|       - |  9169 | `							/* Store original literal index in CALL's iP2 so the` |
|       - |  9170 | `							 * NEW handler can recover the unqualified name. */` |
|      72 |  9171 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      72 |  9172 | `							if( !fromImport ){` |
|      62 |  9173 | `								p3 = (void *)1;` |
|      30 |  9174 | `							}` |
|      37 |  9175 | `						}` |
|       - |  9176 | `					}` |
|  160858 |  9177 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - |  9178 | `					/* Method call,flag that */` |
|     634 |  9179 | `					pInstr->iP2 = 1;` |
|     316 |  9180 | `				}` |
|  160484 |  9181 | `			}` |
|  795914 |  9182 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - |  9183 | `			ph7_expr_node **apNode;` |
|       - |  9184 | `			sxi32 n;` |
|       - |  9185 | `			/* Recurse and generate bytecodes for array index */` |
|   71926 |  9186 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  129774 |  9187 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   57850 |  9188 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   57850 |  9189 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9190 | `					return rc;` |
|       - |  9191 | `				}` |
|   28926 |  9192 | `			}` |
|   71926 |  9193 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   57850 |  9194 | `				iP1 = 1; /* Node have an index associated with it */` |
|   28924 |  9195 | `			}` |
|   71926 |  9196 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - |  9197 | `				/* Create an empty entry when the desired index is not found */` |
|   28416 |  9198 | `				iP2 = 1;` |
|   14209 |  9199 | `			}` |
|  599470 |  9200 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - |  9201 | `			/* POP the left node */` |
|      32 |  9202 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 |  9203 | `		}` |
|  478197 |  9204 | `	}` |
|  956432 |  9205 | `	rc = SXRET_OK;` |
|  956432 |  9206 | `	nJmpIdx = 0;` |
|       - |  9207 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - |  9208 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - |  9209 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  956432 |  9210 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     260 |  9211 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     260 |  9212 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     260 |  9213 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     260 |  9214 | `			int isSpecial = 0;` |
|     260 |  9215 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     176 |  9216 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     176 |  9217 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     187 |  9218 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     153 |  9219 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      78 |  9220 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      88 |  9221 | `					isSpecial = 1;` |
|      43 |  9222 | `				}` |
|     108 |  9223 | `			}` |
|     302 |  9224 | `			pInstr->iP1 = 0;` |
|     302 |  9225 | `			if( !isSpecial ){` |
|     132 |  9226 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      65 |  9227 | `			}` |
|       - |  9228 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - |  9229 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     218 |  9230 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     132 |  9231 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     132 |  9232 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 |  9233 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 |  9234 | `					return SXRET_OK;` |
|       - |  9235 | `				}` |
|      44 |  9236 | `			}` |
|      87 |  9237 | `		}` |
|     159 |  9238 | `	}` |
|       - |  9239 | `	/* Generate code for the right tree */` |
|  956356 |  9240 | `	if( pNode->pRight ){` |
|  499710 |  9241 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - |  9242 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    8828 |  9243 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  495297 |  9244 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - |  9245 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    2948 |  9246 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  489411 |  9247 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - |  9248 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      32 |  9249 | `			iVmOp = 0; /* No binary operator to emit */` |
|      32 |  9250 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  487923 |  9251 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  218004 |  9252 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  109001 |  9253 | `		}` |
|  499710 |  9254 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  499710 |  9255 | `		if( iVmOp == PH7_OP_STORE ){` |
|  215020 |  9256 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  214994 |  9257 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - |  9258 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - |  9259 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - |  9260 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - |  9261 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - |  9262 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - |  9263 | `				 */` |
|      54 |  9264 | `				iVmOp = 0;` |
|  214994 |  9265 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  214968 |  9266 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - |  9267 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   47858 |  9268 | `					iP2 = 1;` |
|   23930 |  9269 | `				}else{` |
|  167112 |  9270 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - |  9271 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   28354 |  9272 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   28354 |  9273 | `						iP1 = pInstr->iP1;` |
|   14178 |  9274 | `					}else{` |
|  138760 |  9275 | `						p3 = pInstr->p3;` |
|       - |  9276 | `					}` |
|       - |  9277 | `					/* POP the last dynamic load instruction */` |
|  167112 |  9278 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  9279 | `				}` |
|  107485 |  9280 | `			}` |
|  392201 |  9281 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 |  9282 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 |  9283 | `			if( pInstr ){` |
|      48 |  9284 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - |  9285 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - |  9286 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - |  9287 | `					 */` |
|      15 |  9288 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 |  9289 | `					iP1 = pInstr->iP1;` |
|      15 |  9290 | `					iP2 = pInstr->iP2;` |
|      15 |  9291 | `					p3  = pInstr->p3;` |
|       8 |  9292 | `				}else{` |
|      34 |  9293 | `					p3 = pInstr->p3;` |
|       - |  9294 | `				}` |
|      23 |  9295 | `			}` |
|      23 |  9296 | `		}` |
|  249854 |  9297 | `	}` |
|  956356 |  9298 | `	if( iVmOp > 0 ){` |
|  956244 |  9299 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11464 |  9300 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - |  9301 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8416 |  9302 | `				iP1 = 1;` |
|    4209 |  9303 | `			}` |
|  950513 |  9304 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - |  9305 | `			/* Namespace-qualify the class name for NEW */ {` |
|   14580 |  9306 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   14580 |  9307 | `				VmInstr *pCallInstr = 0;` |
|   14580 |  9308 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   14564 |  9309 | `					pCallInstr = pPeek;` |
|   14564 |  9310 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7281 |  9311 | `				}` |
|   14580 |  9312 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - |  9313 | `					sxu32 nLitForClass;` |
|       - |  9314 | `					/* If the CALL handler already qualified the name using` |
|       - |  9315 | `					 * function imports, recover the original unqualified` |
|       - |  9316 | `					 * literal so we can re-qualify with class imports. */` |
|   14578 |  9317 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 |  9318 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 |  9319 | `					}else{` |
|   14546 |  9320 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - |  9321 | `					}` |
|   14578 |  9322 | `					pPeek->iP1 = 0;` |
|   14578 |  9323 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7288 |  9324 | `				}` |
|       - |  9325 | `			}` |
|   14580 |  9326 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   14580 |  9327 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - |  9328 | `				VmInstr *pPrev;` |
|   14564 |  9329 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   14564 |  9330 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - |  9331 | `					/* Pop the call instruction */` |
|   14564 |  9332 | `					iP1 = pInstr->iP1;` |
|   14564 |  9333 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7281 |  9334 | `				}` |
|    7283 |  9335 | `			}` |
|  937493 |  9336 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - |  9337 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - |  9338 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 |  9339 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 |  9340 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 |  9341 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 |  9342 | `				int isSpecialIs = 0;` |
|      50 |  9343 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 |  9344 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 |  9345 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 |  9346 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 |  9347 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 |  9348 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 |  9349 | `						isSpecialIs = 1;` |
|       5 |  9350 | `					}` |
|      23 |  9351 | `				}` |
|      52 |  9352 | `				pInstr->iP1 = 0;` |
|      52 |  9353 | `				if( !isSpecialIs ){` |
|      38 |  9354 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 |  9355 | `				}` |
|      25 |  9356 | `			}` |
|  930183 |  9357 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - |  9358 | `			/* Prevent constant expansion for member/property names.` |
|       - |  9359 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - |  9360 | `			 * should not trigger constant lookup. */` |
|  107888 |  9361 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  107888 |  9362 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  107854 |  9363 | `				pInstr->iP1 = 0;` |
|   53926 |  9364 | `			}` |
|  107888 |  9365 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - |  9366 | `				/* Static member access,remember that */` |
|     184 |  9367 | `				iP1 = 1;` |
|     184 |  9368 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     184 |  9369 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      28 |  9370 | `					p3 = pInstr->p3;` |
|      28 |  9371 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      13 |  9372 | `				}` |
|      91 |  9373 | `			}` |
|   53943 |  9374 | `		}` |
|       - |  9375 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  956242 |  9376 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  478120 |  9377 | `	}` |
|  956354 |  9378 | `	if( nJmpIdx > 0 ){` |
|       - |  9379 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   11804 |  9380 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   11804 |  9381 | `		if( pInstr ){` |
|   11804 |  9382 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    5901 |  9383 | `		}` |
|    5901 |  9384 | `	}` |
|  956354 |  9385 | `	return rc;` |
| 1260293 |  9386 |  |
|       - |  9387 | `/*` |
|       - |  9388 | ` * Compile a PHP expression.` |
|       - |  9389 | ` * According to the PHP language reference manual:` |
|       - |  9390 | ` *  Expressions are the most important building stones of PHP.` |
|       - |  9391 | ` *  In PHP, almost anything you write is an expression.` |
|       - |  9392 | ` *  The simplest yet most accurate way to define an expression` |
|       - |  9393 | ` *  is "anything that has a value".` |
|       - |  9394 | ` * If something goes wrong while compiling the expression,this` |
|       - |  9395 | ` * function takes care of generating the appropriate error` |
|       - |  9396 | ` * message.` |
|       - |  9397 | ` */` |
|  680838 |  9398 | `static sxi32 PH7_CompileExpr(` |
|       - |  9399 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  9400 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  9401 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - |  9402 | `	)` |
|       2 |  9403 |  |
|       - |  9404 | `	ph7_expr_node *pRoot;` |
|       - |  9405 | `	SySet sExprNode;` |
|       - |  9406 | `	SyToken *pEnd;` |
|       - |  9407 | `	sxi32 nExpr;` |
|       - |  9408 | `	sxi32 iNest;` |
|       - |  9409 | `	sxi32 rc;` |
|       - |  9410 | `	/* Initialize worker variables */` |
|  680840 |  9411 | `	nExpr = 0;` |
|  680840 |  9412 | `	pRoot = 0;` |
|  680840 |  9413 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  680840 |  9414 | `	SySetAlloc(&sExprNode,0x10);` |
|  680840 |  9415 | `	rc = SXRET_OK;` |
|       - |  9416 | `	/* Delimit the expression */` |
|  680840 |  9417 | `	pEnd = pGen->pIn;` |
|  680840 |  9418 | `	iNest = 0;` |
| 4589808 |  9419 | `	while( pEnd < pGen->pEnd ){` |
| 4352512 |  9420 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - |  9421 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     236 |  9422 | `			iNest++;` |
| 4352395 |  9423 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     244 |  9424 | `			iNest--;` |
| 4352157 |  9425 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  443750 |  9426 | `			if( iNest <= 0 ){` |
|  443544 |  9427 | `				break;` |
|       - |  9428 | `			}` |
|     103 |  9429 | `		}` |
| 3908970 |  9430 | `		pEnd++;` |
|       2 |  9431 | `	}` |
|  680840 |  9432 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   11510 |  9433 | `		SyToken *pEnd2 = pGen->pIn;` |
|   11510 |  9434 | `		iNest = 0;` |
|       - |  9435 | `		/* Stop at the first comma */` |
|   23048 |  9436 | `		while( pEnd2 < pEnd ){` |
|   11544 |  9437 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      12 |  9438 | `				iNest++;` |
|   11539 |  9439 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      12 |  9440 | `				iNest--;` |
|   11529 |  9441 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|       9 |  9442 | `				if( iNest <= 0 ){` |
|       5 |  9443 | `					break;` |
|       - |  9444 | `				}` |
|       2 |  9445 | `			}` |
|   11540 |  9446 | `			pEnd2++;` |
|       2 |  9447 | `		}` |
|   11510 |  9448 | `		if( pEnd2 <pEnd ){` |
|       5 |  9449 | `			pEnd = pEnd2;` |
|       2 |  9450 | `		}` |
|    5754 |  9451 | `	}` |
|  680840 |  9452 | `	if( pEnd > pGen->pIn ){` |
|  680830 |  9453 | `		SyToken *pTmp = pGen->pEnd;` |
|       - |  9454 | `		/* Swap delimiter */` |
|  680830 |  9455 | `		pGen->pEnd = pEnd;` |
|       - |  9456 | `		/* Try to get an expression tree */` |
|  680830 |  9457 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  680830 |  9458 | `		if( rc == SXRET_OK && pRoot ){` |
|  680660 |  9459 | `			rc = SXRET_OK;` |
|  680660 |  9460 | `			if( xTreeValidator ){` |
|       - |  9461 | `				/* Call the upper layer validator callback */` |
|   14632 |  9462 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    7315 |  9463 | `			}` |
|  680660 |  9464 | `			if( rc != SXERR_ABORT ){` |
|       - |  9465 | `				/* Generate code for the given tree */` |
|  680660 |  9466 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|  340329 |  9467 | `			}` |
|  680660 |  9468 | `			nExpr = 1;` |
|  340329 |  9469 | `		}` |
|       - |  9470 | `		/* Release the whole tree */` |
|  680830 |  9471 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - |  9472 | `		/* Synchronize token stream */` |
|  680830 |  9473 | `		pGen->pEnd = pTmp;` |
|  680830 |  9474 | `		pGen->pIn  = pEnd;` |
|  680830 |  9475 | `		if( rc == SXERR_ABORT ){` |
|      11 |  9476 | `			SySetRelease(&sExprNode);` |
|      11 |  9477 | `			return SXERR_ABORT;` |
|       - |  9478 | `		}` |
|  340409 |  9479 | `	}` |
|  680830 |  9480 | `	SySetRelease(&sExprNode);` |
|  680830 |  9481 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  340421 |  9482 |  |
|       - |  9483 | `/*` |
|       - |  9484 | ` * Return a pointer to the node construct handler associated` |
|       - |  9485 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - |  9486 | ` */` |
|  169344 |  9487 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 |  9488 |  |
|  169346 |  9489 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - |  9490 | `		/* Numeric literal: Either real or integer */` |
|   92882 |  9491 | `		return PH7_CompileNumLiteral;` |
|   76466 |  9492 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - |  9493 | `		/* Double quoted string */` |
|   16310 |  9494 | `		return PH7_CompileString;` |
|   60158 |  9495 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - |  9496 | `		/* Single quoted string */` |
|   60046 |  9497 | `		return PH7_CompileSimpleString;` |
|     114 |  9498 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - |  9499 | `		/* Heredoc */` |
|      66 |  9500 | `		return PH7_CompileHereDoc;` |
|      50 |  9501 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - |  9502 | `		/* Nowdoc */` |
|      44 |  9503 | `		return PH7_CompileNowDoc;` |
|       7 |  9504 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - |  9505 | `		/* Backtick quoted string */` |
|       5 |  9506 | `		return PH7_CompileBacktic;` |
|       - |  9507 | `	}` |
|       3 |  9508 | `	return 0;` |
|   84674 |  9509 |  |
|       - |  9510 | `/*` |
|       - |  9511 | ` * Compile an unset() statement.` |
|       - |  9512 | ` * unset($var, $arr[$key], ...);` |
|       - |  9513 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - |  9514 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - |  9515 | ` * parent array before extracting the element to unset.` |
|       - |  9516 | ` */` |
|    2712 |  9517 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 |  9518 |  |
|    2714 |  9519 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2714 |  9520 | `	sxu32 nIdx = 0;` |
|       - |  9521 | `	SyString sName;` |
|       - |  9522 | `	sxi32 rc;` |
|       - |  9523 | `	/* Jump the 'unset' keyword */` |
|    2714 |  9524 | `	pGen->pIn++;` |
|       - |  9525 | `	/* Save delimiter */` |
|    2714 |  9526 | `	pTmp = pGen->pEnd;` |
|       - |  9527 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2714 |  9528 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2714 |  9529 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - |  9530 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - |  9531 | `		SyToken *pClose;` |
|    2714 |  9532 | `		pGen->pIn++;   /* Skip '(' */` |
|    2714 |  9533 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2714 |  9534 | `		pEnd = pClose; /* Stop at ')' */` |
|    1356 |  9535 | `	}` |
|    2714 |  9536 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - |  9537 | `	/* Resolve the 'unset' builtin name once */` |
|    2714 |  9538 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     332 |  9539 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     332 |  9540 | `		if( pObj == 0 ){` |
|     ! 0 |  9541 | `			return SXERR_ABORT;` |
|       - |  9542 | `		}` |
|     332 |  9543 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     332 |  9544 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     165 |  9545 | `	}` |
|       - |  9546 | `	/* Compile each comma-separated argument */` |
|    8994 |  9547 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6282 |  9548 | `		if( pGen->pIn < pNext ){` |
|    6282 |  9549 | `			pGen->pEnd = pNext;` |
|    6282 |  9550 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - |  9551 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,0);` |
|    6282 |  9552 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9553 | `				return SXERR_ABORT;` |
|       - |  9554 | `			}` |
|    6282 |  9555 | `			if( rc != SXERR_EMPTY ){` |
|       - |  9556 | `				/* Emit call for this single argument */` |
|    6280 |  9557 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6280 |  9558 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6280 |  9559 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3139 |  9560 | `			}` |
|    3140 |  9561 | `		}` |
|       - |  9562 | `		/* Jump trailing commas */` |
|    9852 |  9563 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3572 |  9564 | `			pNext++;` |
|       2 |  9565 | `		}` |
|    6282 |  9566 | `		pGen->pIn = pNext;` |
|       2 |  9567 | `	}` |
|       - |  9568 | `	/* Skip past the closing ')' if present */` |
|    2714 |  9569 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2714 |  9570 | `		pGen->pIn++;` |
|    1356 |  9571 | `	}` |
|       - |  9572 | `	/* Restore token stream */` |
|    2714 |  9573 | `	pGen->pEnd = pTmp;` |
|    2714 |  9574 | `	return SXRET_OK;` |
|    1358 |  9575 |  |
|       - |  9576 | `/*` |
|       - |  9577 | ` * PHP Language construct table.` |
|       - |  9578 | ` */` |
|       - |  9579 | `static const LangConstruct aLangConstruct[] = {` |
|       - |  9580 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - |  9581 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - |  9582 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - |  9583 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - |  9584 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - |  9585 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - |  9586 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - |  9587 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - |  9588 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - |  9589 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - |  9590 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - |  9591 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - |  9592 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - |  9593 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - |  9594 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - |  9595 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - |  9596 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - |  9597 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - |  9598 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - |  9599 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - |  9600 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - |  9601 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - |  9602 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - |  9603 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - |  9604 | `};` |
|       - |  9605 | `/*` |
|       - |  9606 | ` * Return a pointer to the statement handler routine associated` |
|       - |  9607 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - |  9608 | ` */` |
|  412966 |  9609 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - |  9610 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - |  9611 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - |  9612 | `	)` |
|       2 |  9613 |  |
|  412968 |  9614 | `	sxu32 n = 0;` |
| 1737603 |  9615 | `	for(;;){` |
| 3475208 |  9616 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   48476 |  9617 | `			break;` |
|       - |  9618 | `		}` |
| 3426734 |  9619 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  364494 |  9620 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 |  9621 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 |  9622 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - |  9623 | `					/* 'static' (class context),return null */` |
|     ! 0 |  9624 | `					return 0;` |
|       - |  9625 | `				}` |
|     ! 0 |  9626 | `			}` |
|  364492 |  9627 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 |  9628 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 |  9629 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - |  9630 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 |  9631 | `				return 0;` |
|       - |  9632 | `			}` |
|       - |  9633 | `			/* Return a pointer to the handler.` |
|       - |  9634 | `			*/` |
|  364494 |  9635 | `			return aLangConstruct[n].xConstruct;` |
|       - |  9636 | `		}` |
| 3062242 |  9637 | `		n++;` |
|       2 |  9638 | `	}` |
|   48476 |  9639 | `	if( pLookahed ){` |
|   48476 |  9640 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8446 |  9641 | `			return PH7_CompileClassInterface;` |
|   40032 |  9642 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   39826 |  9643 | `			return PH7_CompileClass;` |
|     208 |  9644 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 |  9645 | `			return PH7_CompileTrait;` |
|     152 |  9646 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      19 |  9647 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      18 |  9648 | `				return PH7_CompileAbstractClass;` |
|     136 |  9649 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 |  9650 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 |  9651 | `				return PH7_CompileFinalClass;` |
|       - |  9652 | `		}` |
|      67 |  9653 | `	}` |
|       - |  9654 | `	/* Not a language construct */` |
|     136 |  9655 | `	return 0;` |
|  206485 |  9656 |  |
|       - |  9657 | `/*` |
|       - |  9658 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - |  9659 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - |  9660 | ` */` |
|     134 |  9661 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 |  9662 |  |
|       - |  9663 | `	int rc;` |
|     136 |  9664 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     136 |  9665 | `	if( rc == FALSE ){` |
|      40 |  9666 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      38 |  9667 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - |  9668 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - |  9669 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - |  9670 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - |  9671 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - |  9672 | `			*/` |
|       - |  9673 | `			){` |
|      34 |  9674 | `				rc = TRUE;` |
|      16 |  9675 | `		}` |
|      20 |  9676 | `	}` |
|     136 |  9677 | `	return rc;` |
|       2 |  9678 |  |
|       - |  9679 | `/*` |
|       - |  9680 | ` * Compile a PHP chunk.` |
|       - |  9681 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - |  9682 | ` * takes care of generating the appropriate error message.` |
|       - |  9683 | ` */` |
|  554458 |  9684 | `static sxi32 GenStateCompileChunk(` |
|       - |  9685 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  9686 | `	sxi32 iFlags         /* Compile flags */` |
|       - |  9687 | `	)` |
|       2 |  9688 |  |
|       - |  9689 | `	ProcLangConstruct xCons;` |
|       - |  9690 | `	sxi32 rc;` |
|  554460 |  9691 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  331294 |  9692 | `	for(;;){` |
|  662590 |  9693 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9694 | `			/* No more input to process */` |
|   11978 |  9695 | `			break;` |
|       - |  9696 | `		}` |
|  650614 |  9697 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - |  9698 | `			/* Compile block */` |
|      16 |  9699 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      16 |  9700 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9701 | `				break;` |
|       - |  9702 | `			}` |
|       9 |  9703 | `		}else{` |
|  650600 |  9704 | `			xCons = 0;` |
|  650600 |  9705 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  412968 |  9706 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - |  9707 | `				/* Try to extract a language construct handler */` |
|  412968 |  9708 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  412968 |  9709 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 |  9710 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9711 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 |  9712 | `						&pGen->pIn->sData);` |
|       9 |  9713 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9714 | `						break;` |
|       - |  9715 | `					}` |
|       - |  9716 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - |  9717 | `					 * this erroneous statement.` |
|       - |  9718 | `					 */` |
|       9 |  9719 | `					xCons = PH7_ErrorRecover;` |
|       4 |  9720 | `				}` |
|  444117 |  9721 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   41592 |  9722 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - |  9723 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 |  9724 | `				xCons = PH7_CompileLabel;` |
|      56 |  9725 | `			}` |
|  650600 |  9726 | `			if( xCons == 0 ){` |
|       - |  9727 | `				/* Assume an expression an try to compile it */` |
|  237648 |  9728 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  237648 |  9729 | `				if(  rc != SXERR_EMPTY ){` |
|       - |  9730 | `					/* Pop l-value */` |
|  237510 |  9731 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  118754 |  9732 | `				}` |
|  118825 |  9733 | `			}else{` |
|       - |  9734 | `				/* Go compile the sucker */` |
|  412954 |  9735 | `				rc = xCons(&(*pGen));` |
|       - |  9736 | `			}` |
|  650600 |  9737 | `			if( rc == SXERR_ABORT ){` |
|       - |  9738 | `				/* Request to abort compilation */` |
|      11 |  9739 | `				break;` |
|       - |  9740 | `			}` |
|       - |  9741 | `		}` |
|       - |  9742 | `		/* Ignore trailing semi-colons ';' */` |
| 1077470 |  9743 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  426868 |  9744 | `			pGen->pIn++;` |
|       2 |  9745 | `		}` |
|  650604 |  9746 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - |  9747 | `			/* Compile a single statement and return */` |
|  542474 |  9748 | `			break;` |
|       - |  9749 | `		}` |
|       - |  9750 | `		/* LOOP ONE */` |
|       - |  9751 | `		/* LOOP TWO */` |
|       - |  9752 | `		/* LOOP THREE */` |
|       - |  9753 | `		/* LOOP FOUR */` |
|       2 |  9754 | `	}` |
|       - |  9755 | `	/* Return compilation status */` |
|  554460 |  9756 | `	return rc;` |
|       2 |  9757 |  |
|       - |  9758 | `/*` |
|       - |  9759 | ` * Compile a Raw PHP chunk.` |
|       - |  9760 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - |  9761 | ` * takes care of generating the appropriate error message.` |
|       - |  9762 | ` */` |
|   11988 |  9763 | `static sxi32 PH7_CompilePHP(` |
|       - |  9764 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9765 | `	SySet *pTokenSet,     /* Token set */` |
|       - |  9766 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - |  9767 | `	)` |
|       2 |  9768 |  |
|   11990 |  9769 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - |  9770 | `	sxi32 rc;` |
|       - |  9771 | `	/* Reset the token set */` |
|   11990 |  9772 | `	SySetReset(&(*pTokenSet));` |
|       - |  9773 | `	/* Mark as the default token set */` |
|   11990 |  9774 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - |  9775 | `	/* Advance the stream cursor */` |
|   11990 |  9776 | `	pGen->pRawIn++;` |
|       - |  9777 | `	/* Tokenize the PHP chunk first */` |
|   11990 |  9778 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - |  9779 | `	/* Point to the head and tail of the token stream. */` |
|   11990 |  9780 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   11990 |  9781 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   11990 |  9782 | `	if( is_expr ){` |
|     ! 0 |  9783 | `		rc = SXERR_EMPTY;` |
|     ! 0 |  9784 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  9785 | `			/* A simple expression,compile it */` |
|     ! 0 |  9786 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  9787 | `		}` |
|       - |  9788 | `		/* Emit the DONE instruction */` |
|     ! 0 |  9789 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 |  9790 | `		return SXRET_OK;` |
|       - |  9791 | `	}` |
|   11990 |  9792 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  9793 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  9794 | `		/*` |
|       - |  9795 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - |  9796 | `		 * According to the PHP reference manual:` |
|       - |  9797 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - |  9798 | `		 *  immediately follow` |
|       - |  9799 | `		 *  the opening tag with an equals sign as follows:` |
|       - |  9800 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - |  9801 | `		 * Symisc extension:` |
|       - |  9802 | `		 *   This short syntax works with all PHP opening` |
|       - |  9803 | `		 *   tags unlike the default PHP engine that handle` |
|       - |  9804 | `		 *   only short tag.` |
|       - |  9805 | `		 */` |
|       - |  9806 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 |  9807 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 |  9808 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 |  9809 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 |  9810 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 |  9811 | `		if( rc != SXERR_EMPTY ){` |
|       3 |  9812 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 |  9813 | `		}` |
|       3 |  9814 | `		return SXRET_OK;` |
|       - |  9815 | `	}` |
|       - |  9816 | `	/* Compile the PHP chunk */` |
|   11988 |  9817 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - |  9818 | `	/* Fix exceptions jumps */` |
|   11988 |  9819 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9820 | `	/* Fix gotos now, the jump destination is resolved */` |
|   11988 |  9821 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 |  9822 | `		rc = SXERR_ABORT;` |
|       1 |  9823 | `	}` |
|       - |  9824 | `	/* Reset container */` |
|   11988 |  9825 | `	SySetReset(&pGen->aGoto);` |
|   11988 |  9826 | `	SySetReset(&pGen->aLabel);` |
|       - |  9827 | `	/* Compilation result */` |
|   11988 |  9828 | `	return rc;` |
|    5996 |  9829 |  |
|       - |  9830 | `/*` |
|       - |  9831 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - |  9832 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - |  9833 | ` * This is the only compile interface exported from this file.` |
|       - |  9834 | ` */` |
|   14176 |  9835 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - |  9836 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - |  9837 | `	SyString *pScript,  /* Script to compile */` |
|       - |  9838 | `	sxi32 iFlags        /* Compile flags */` |
|       - |  9839 | `	)` |
|       2 |  9840 |  |
|       - |  9841 | `	SySet aPhpToken,aRawToken;` |
|       - |  9842 | `	ph7_gen_state *pCodeGen;` |
|       - |  9843 | `	ph7_value *pRawObj;` |
|       - |  9844 | `	sxu32 nObjIdx;` |
|       - |  9845 | `	sxi32 nRawObj;` |
|       - |  9846 | `	int is_expr;` |
|       - |  9847 | `	sxi32 rc;` |
|   14178 |  9848 | `	if( pScript->nByte < 1 ){` |
|       - |  9849 | `		/* Nothing to compile */` |
|     ! 0 |  9850 | `		return PH7_OK;` |
|       - |  9851 | `	}` |
|       - |  9852 | `	/* Initialize the tokens containers */` |
|   14178 |  9853 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14178 |  9854 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14178 |  9855 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14178 |  9856 | `	is_expr = 0;` |
|   14178 |  9857 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - |  9858 | `		SyToken sTmp;` |
|       - |  9859 | `		/* PHP only: -*/` |
|    2832 |  9860 | `		sTmp.nLine = 1;` |
|    2832 |  9861 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2832 |  9862 | `		sTmp.pUserData = 0;` |
|    2832 |  9863 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2832 |  9864 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2832 |  9865 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - |  9866 | `			/* A simple PHP expression */` |
|     ! 0 |  9867 | `			is_expr = 1;` |
|     ! 0 |  9868 | `		}` |
|    1417 |  9869 | `	}else{` |
|       - |  9870 | `		/* Tokenize raw text */` |
|   11348 |  9871 | `		SySetAlloc(&aRawToken,32);` |
|   11348 |  9872 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - |  9873 | `	}` |
|   14178 |  9874 | `	pCodeGen = &pVm->sCodeGen;` |
|       - |  9875 | `	/* Process high-level tokens */` |
|   14178 |  9876 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14178 |  9877 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14178 |  9878 | `	rc = PH7_OK;` |
|   14178 |  9879 | `	if( is_expr ){` |
|       - |  9880 | `		/* Compile the expression */` |
|     ! 0 |  9881 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 |  9882 | `		goto cleanup;` |
|       - |  9883 | `	}` |
|   14178 |  9884 | `	nObjIdx = 0;` |
|       - |  9885 | `	/* Each compilation unit starts in the global namespace.` |
|       - |  9886 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - |  9887 | `	 * preventing namespace bleeding across include()d files. */` |
|   14178 |  9888 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - |  9889 | `	/* Start the compilation process */` |
|   12765 |  9890 | `	for(;;){` |
|   37508 |  9891 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14166 |  9892 | `			break; /* No more tokens to process */` |
|       - |  9893 | `		}` |
|   23344 |  9894 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - |  9895 | `			/* Compile the PHP chunk */` |
|   11990 |  9896 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   11990 |  9897 | `			if( rc == SXERR_ABORT ){` |
|      13 |  9898 | `				break;` |
|       - |  9899 | `			}` |
|   11978 |  9900 | `			continue;` |
|       - |  9901 | `		}` |
|       - |  9902 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11356 |  9903 | `		nRawObj = 0;` |
|   22710 |  9904 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - |  9905 | `			/* Consume the raw chunk without any processing */` |
|   11356 |  9906 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11356 |  9907 | `			if( pRawObj == 0 ){` |
|     ! 0 |  9908 | `				rc = SXERR_MEM;` |
|     ! 0 |  9909 | `				break;` |
|       - |  9910 | `			}` |
|       - |  9911 | `			/* Mark as constant and emit the load constant instruction */` |
|   11356 |  9912 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11356 |  9913 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11356 |  9914 | `			++nRawObj;` |
|   11356 |  9915 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 |  9916 | `		}` |
|   11356 |  9917 | `		if( nRawObj > 0 ){` |
|       - |  9918 | `			/* Emit the consume instruction */` |
|   11356 |  9919 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5677 |  9920 | `		}` |
|    7090 |  9921 | `	}` |
|    7088 |  9922 | `cleanup:` |
|   14178 |  9923 | `	SySetRelease(&aRawToken);` |
|   14178 |  9924 | `	SySetRelease(&aPhpToken);` |
|   14178 |  9925 | `	return rc;` |
|    7090 |  9926 |  |
|       - |  9927 | `/*` |
|       - |  9928 | ` * Utility routines.Initialize the code generator.` |
|       - |  9929 | ` */` |
|    2802 |  9930 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - |  9931 | `	ph7_vm *pVm,       /* Target VM */` |
|       - |  9932 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - |  9933 | `	void *pErrData     /* Last argument to xErr() */` |
|       - |  9934 | `	)` |
|       2 |  9935 |  |
|    2804 |  9936 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - |  9937 | `	/* Zero the structure */` |
|    2804 |  9938 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - |  9939 | `	/* Initial state */` |
|    2804 |  9940 | `	pGen->pVm  = &(*pVm);` |
|    2804 |  9941 | `	pGen->xErr = xErr;` |
|    2804 |  9942 | `	pGen->pErrData = pErrData;` |
|    2804 |  9943 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2804 |  9944 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2804 |  9945 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2804 |  9946 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - |  9947 | `	/* Error log buffer */` |
|    2804 |  9948 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - |  9949 | `	/* General purpose working buffer */` |
|    2804 |  9950 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - |  9951 | `	/* Namespace state */` |
|    2804 |  9952 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2804 |  9953 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2804 |  9954 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2804 |  9955 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - |  9956 | `	/* Create the global scope */` |
|    2804 |  9957 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - |  9958 | `	/* Point to the global scope */` |
|    2804 |  9959 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2804 |  9960 | `	return SXRET_OK;` |
|       2 |  9961 |  |
|       - |  9962 | `/*` |
|       - |  9963 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - |  9964 | ` */` |
|   16698 |  9965 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - |  9966 | `	ph7_vm *pVm,       /* Target VM */` |
|       - |  9967 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - |  9968 | `	void *pErrData     /* Last argument to xErr() */` |
|       - |  9969 | `	)` |
|       2 |  9970 |  |
|   16700 |  9971 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - |  9972 | `	GenBlock *pBlock,*pParent;` |
|       - |  9973 | `	/* Reset state */` |
|   16700 |  9974 | `	SySetReset(&pGen->aLabel);` |
|   16700 |  9975 | `	SySetReset(&pGen->aGoto);` |
|   16700 |  9976 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   16700 |  9977 | `	SyBlobRelease(&pGen->sWorker);` |
|   16700 |  9978 | `	SyBlobRelease(&pGen->sNamespace);` |
|   16700 |  9979 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   16700 |  9980 | `	SyHashRelease(&pGen->hUseImports);` |
|   16700 |  9981 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   16700 |  9982 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   16700 |  9983 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   16700 |  9984 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   16700 |  9985 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - |  9986 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - |  9987 | `	 * They intern variable names and literal strings that are referenced by` |
|       - |  9988 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - |  9989 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - |  9990 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - |  9991 | `	 * number of unique names, which is acceptable. */` |
|       - |  9992 | `	/* Point to the global scope */` |
|   16700 |  9993 | `	pBlock = pGen->pCurrent;` |
|   16700 |  9994 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 |  9995 | `		pParent = pBlock->pParent;` |
|     ! 0 |  9996 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 |  9997 | `		pBlock = pParent;` |
|     ! 0 |  9998 | `	}` |
|   16700 |  9999 | `	pGen->xErr = xErr;` |
|   16700 | 10000 | `	pGen->pErrData = pErrData;` |
|   16700 | 10001 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   16700 | 10002 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   16700 | 10003 | `	pGen->pIn = pGen->pEnd = 0;` |
|   16700 | 10004 | `	pGen->nErr = 0;` |
|   16700 | 10005 | `	return SXRET_OK;` |
|       2 | 10006 |  |
|       - | 10007 | `/*` |
|       - | 10008 | ` * Generate a compile-time error message.` |
|       - | 10009 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 10010 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 10011 | ` * abort compilation immediately.` |
|       - | 10012 | ` */` |
|     520 | 10013 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 10014 |  |
|     522 | 10015 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     522 | 10016 | `	const char *zErr = "Error";` |
|       - | 10017 | `	SyString *pFile;` |
|       - | 10018 | `	va_list ap;` |
|       - | 10019 | `	sxi32 rc;` |
|       - | 10020 | `	/* Reset the working buffer */` |
|     522 | 10021 | `	SyBlobReset(pWorker);` |
|       - | 10022 | `	/* Peek the processed file path if available */` |
|     522 | 10023 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     522 | 10024 | `	if( nErrType == E_ERROR ){` |
|       - | 10025 | `		/* Increment the error counter */` |
|     436 | 10026 | `		pGen->nErr++;` |
|     436 | 10027 | `		if( pGen->nErr > 15 ){` |
|       - | 10028 | `			/* Error count limit reached */` |
|       5 | 10029 | `			if( pGen->xErr ){` |
|       5 | 10030 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 10031 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 10032 | `				if( pFile ){` |
|       5 | 10033 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 10034 | `				}` |
|       5 | 10035 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 10036 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 10037 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 10038 | `				}` |
|       2 | 10039 | `			}` |
|       - | 10040 | `			/* Abort immediately */` |
|       5 | 10041 | `			return SXERR_ABORT;` |
|       - | 10042 | `		}` |
|     215 | 10043 | `	}` |
|     518 | 10044 | `	if( pGen->xErr == 0 ){` |
|       - | 10045 | `		/* No available error consumer,return immediately */` |
|       3 | 10046 | `		return SXRET_OK;` |
|       - | 10047 | `	}` |
|     515 | 10048 | `	switch(nErrType){` |
|     429 | 10049 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 10050 | `	case E_WARNING: zErr = "Warning";     break;` |
|      53 | 10051 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 10052 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 10053 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 10054 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 10055 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 10056 | `	default:` |
|     ! 0 | 10057 | `		break;` |
|       - | 10058 | `	}` |
|     515 | 10059 | `	rc = SXRET_OK;` |
|       - | 10060 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     515 | 10061 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     515 | 10062 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     515 | 10063 | `	va_start(ap,zFormat);` |
|     515 | 10064 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     515 | 10065 | `	va_end(ap);` |
|     515 | 10066 | `	if( pFile ){` |
|     515 | 10067 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     257 | 10068 | `	}` |
|       - | 10069 | `	/* Append a new line */` |
|     515 | 10070 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     515 | 10071 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 10072 | `		/* Consume the generated error message */` |
|     515 | 10073 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     257 | 10074 | `	}` |
|     515 | 10075 | `	return rc;` |
|     262 | 10076 |  |
|       - | 10077 |  |
