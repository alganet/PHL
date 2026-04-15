# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4952/6268 lines (79.00%)

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
|    3164 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3166 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    8927 |   131 | `	for(;;){` |
|   17856 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    3058 |   133 | `			iCount--; /* Decrement nesting level */` |
|    3058 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    3032 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   14826 |   140 | `		pBlock = pBlock->pParent;` |
|   14826 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1584 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  618728 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  618730 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  618730 |   162 | `	pBlock->pUserData   = pUserData;` |
|  618730 |   163 | `	pBlock->pGen        = pGen;` |
|  618730 |   164 | `	pBlock->iFlags      = iType;` |
|  618730 |   165 | `	pBlock->pParent     = 0;` |
|  618730 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  618730 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  618730 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  615830 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  615832 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  615832 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  615832 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  615832 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  615832 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  615832 |   200 | `	pGen->pCurrent = pBlock;` |
|  615832 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  298022 |   203 | `		*ppBlock = pBlock;` |
|  149010 |   204 | `	}` |
|  615832 |   205 | `	return SXRET_OK;` |
|  307917 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  615822 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  615824 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  615824 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  615824 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  615822 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  615824 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  615824 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  615824 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  615824 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  615822 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  615824 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  615824 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  615824 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  615824 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  615824 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  615824 |   244 | `	return SXRET_OK;` |
|  307913 |   245 |  |
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
|  187468 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  187470 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  187470 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  187470 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  187470 |   265 | `	return rc;` |
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
|  437968 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  437970 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  803224 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  365256 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  142220 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  223038 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   35572 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  187468 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  187468 |   298 | `		if( pInstr ){` |
|  187468 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  187468 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  187468 |   302 | `			aFix[n].nJumpType = -1;` |
|   93733 |   303 | `		}` |
|   93735 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  437970 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  167270 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  167272 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  167418 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  167270 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  167402 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  167270 |   358 | `	return SXRET_OK;` |
|   83637 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  544434 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  544436 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  544436 |   367 | `	if( pEntry == 0 ){` |
|  268668 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  275770 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  275770 |   371 | `	return SXRET_OK;` |
|  272219 |   372 |  |
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
|  268666 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  268668 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  268668 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  134333 |   387 | `	}` |
|  268668 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|   95622 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|   95624 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|   95624 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   95624 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|   95624 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|   95624 |   408 | `	return pObj;` |
|   47813 |   409 |  |
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
|   96146 |   470 | `static int GenStateFindBadNumericSeparator(` |
|       - |   471 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   472 |  |
|   96148 |   473 | `	const char *z = pRaw->zString;` |
|   96148 |   474 | `	sxu32 n = pRaw->nByte;` |
|   96148 |   475 | `	int base = 10;` |
|       - |   476 | `	sxu32 i, start;` |
|   96148 |   477 | `	if( n < 2 ) return 0;` |
|    8598 |   478 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   479 | `		base = 16;` |
|    8563 |   480 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   481 | `		base = 2;` |
|     139 |   482 | `	}` |
|   31742 |   483 | `	for( i = 0; i < n; ++i ){` |
|   23160 |   484 | `		if( z[i] != '_' ) continue;` |
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
|    8584 |   501 | `	return 0;` |
|   48075 |   502 |  |
|       - |   503 | `/*` |
|       - |   504 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   505 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   506 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   507 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   508 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   509 | ` * so callers can bail from the current construct).` |
|       - |   510 | ` */` |
|   96146 |   511 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   512 |  |
|   96148 |   513 | `	const char *zBad = 0;` |
|   96148 |   514 | `	sxu32 nBad = 0;` |
|       - |   515 | `	SyString sBad;` |
|       - |   516 | `	sxi32 rc;` |
|   96148 |   517 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|   96134 |   518 | `		return SXRET_OK;` |
|       - |   519 | `	}` |
|      15 |   520 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   521 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   522 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   523 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   524 | `		return SXERR_ABORT;` |
|       - |   525 | `	}` |
|      15 |   526 | `	return SXERR_SYNTAX;` |
|   48075 |   527 |  |
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
|   96132 |   544 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   545 | `	SyMemBackend *pAlloc,` |
|       - |   546 | `	const SyString *pToken,` |
|       - |   547 | `	char *zScratch, sxu32 nScratch,` |
|       - |   548 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   549 |  |
|       - |   550 | `	sxu32 i, j;` |
|   96134 |   551 | `	int hasUnderscore = 0;` |
|       - |   552 | `	char *zBuf;` |
|   96134 |   553 | `	*pzAlloc = 0;` |
|  204762 |   554 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  108882 |   555 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   54316 |   556 | `	}` |
|   96134 |   557 | `	if( !hasUnderscore ){` |
|   95882 |   558 | `		SyStringDupPtr(pOut, pToken);` |
|   95882 |   559 | `		return SXRET_OK;` |
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
|   48068 |   576 |  |
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
|   96118 |   593 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   594 |  |
|   96120 |   595 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|   96120 |   596 | `	sxu32 nIdx = 0;` |
|       - |   597 | `	char zScratch[GEN_NUM_SCRATCH];` |
|   96120 |   598 | `	char *zAlloc = 0;` |
|       - |   599 | `	SyString sNum;` |
|       - |   600 | `	sxi32 rc;` |
|   48059 |   601 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|   96120 |   602 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|   96120 |   603 | `	if( rc != SXRET_OK ){` |
|      11 |   604 | `		return rc;` |
|       - |   605 | `	}` |
|  144164 |   606 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   48054 |   607 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|   96110 |   608 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   609 | `		return SXERR_ABORT;` |
|       - |   610 | `	}` |
|   96110 |   611 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   612 | `		ph7_value *pObj;` |
|       - |   613 | `		sxi64 iValue;` |
|   95624 |   614 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|   95624 |   615 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|   95624 |   616 | `		if( pObj == 0 ){` |
|     ! 0 |   617 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   618 | `			return SXERR_ABORT;` |
|       - |   619 | `		}` |
|   95624 |   620 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   47813 |   621 | `	}else{` |
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
|   96110 |   634 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   635 | `	/* Emit the load constant instruction */` |
|   96110 |   636 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   637 | `	/* Node successfully compiled */` |
|   96110 |   638 | `	return SXRET_OK;` |
|   48061 |   639 |  |
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
|   61986 |   651 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   652 |  |
|   61988 |   653 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   654 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   655 | `	ph7_value *pObj;` |
|       - |   656 | `	sxu32 nIdx;` |
|   61988 |   657 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   658 | `	/* Delimit the string */` |
|   61988 |   659 | `	zIn  = pStr->zString;` |
|   61988 |   660 | `	zEnd = &zIn[pStr->nByte];` |
|   61988 |   661 | `	if( zIn >= zEnd ){` |
|       - |   662 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   663 | `		 * rather than reserving a new object each time. */` |
|     144 |   664 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     144 |   665 | `		return SXRET_OK;` |
|       - |   666 | `	}` |
|   61846 |   667 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   668 | `		/* Already processed,emit the load constant instruction` |
|       - |   669 | `		 * and return.` |
|       - |   670 | `		 */` |
|   18126 |   671 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18126 |   672 | `		return SXRET_OK;` |
|       - |   673 | `	}` |
|       - |   674 | `	/* Reserve a new constant */` |
|   43722 |   675 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   43722 |   676 | `	if( pObj == 0 ){` |
|     ! 0 |   677 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   678 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   679 | `		return SXERR_ABORT;` |
|       - |   680 | `	}` |
|   43722 |   681 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   682 | `	/* Compile the node */` |
|   43762 |   683 | `	for(;;){` |
|   87526 |   684 | `		if( zIn >= zEnd ){` |
|       - |   685 | `			/* End of input */` |
|   43722 |   686 | `			break;` |
|       - |   687 | `		}` |
|   43806 |   688 | `		zCur = zIn;` |
|  695904 |   689 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  652100 |   690 | `			zIn++;` |
|       2 |   691 | `		}` |
|   43806 |   692 | `		if( zIn > zCur ){` |
|       - |   693 | `			/* Append raw contents*/` |
|   43786 |   694 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   21892 |   695 | `		}` |
|   43806 |   696 | `		zIn++;` |
|   43806 |   697 | `		if( zIn < zEnd ){` |
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
|   43806 |   712 | `		zIn++;` |
|       2 |   713 | `	}` |
|       - |   714 | `	/* Emit the load constant instruction */` |
|   43722 |   715 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   43722 |   716 | `	if( pStr->nByte < 1024 ){` |
|       - |   717 | `		/* Install in the literal table */` |
|   43722 |   718 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   21860 |   719 | `	}` |
|       - |   720 | `	/* Node successfully compiled */` |
|   43722 |   721 | `	return SXRET_OK;` |
|   30995 |   722 |  |
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
|    1934 |   888 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1936 |   899 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   900 | `	/* Preallocate some slots */` |
|    1936 |   901 | `	SySetAlloc(&sToken,0x08);` |
|       - |   902 | `	/* Tokenize the text */` |
|    1936 |   903 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   904 | `	/* Swap delimiter */` |
|    1936 |   905 | `	pTmpIn  = pGen->pIn;` |
|    1936 |   906 | `	pTmpEnd = pGen->pEnd;` |
|    1936 |   907 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1936 |   908 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   909 | `	/* Compile the expression */` |
|    1936 |   910 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   911 | `	/* Restore token stream */` |
|    1936 |   912 | `	pGen->pIn  = pTmpIn;` |
|    1936 |   913 | `	pGen->pEnd = pTmpEnd;` |
|       - |   914 | `	/* Release the token set */` |
|    1936 |   915 | `	SySetRelease(&sToken);` |
|       - |   916 | `	/* Compilation result */` |
|    1936 |   917 | `	return rc;` |
|       2 |   918 |  |
|       - |   919 | `/*` |
|       - |   920 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   921 | ` */` |
|   18332 |   922 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   923 |  |
|       - |   924 | `	ph7_value *pConstObj;` |
|   18334 |   925 | `	sxu32 nIdx = 0;` |
|       - |   926 | `	/* Reserve a new constant */` |
|   18334 |   927 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   18334 |   928 | `	if( pConstObj == 0 ){` |
|     ! 0 |   929 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   930 | `		return 0;` |
|       - |   931 | `	}` |
|   18334 |   932 | `	(*pCount)++;` |
|   18334 |   933 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   934 | `	/* Emit the load constant instruction */` |
|   18334 |   935 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18334 |   936 | `	return pConstObj;` |
|    9168 |   937 |  |
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
|   16946 |   976 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |   977 |  |
|   16948 |   978 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |   979 | `	const char *zIn,*zCur,*zEnd;` |
|   16948 |   980 | `	ph7_value *pObj = 0;` |
|       - |   981 | `	sxi32 iCons;` |
|       - |   982 | `	sxi32 rc;` |
|       - |   983 | `	/* Delimit the string */` |
|   16948 |   984 | `	zIn  = pStr->zString;` |
|   16948 |   985 | `	zEnd = &zIn[pStr->nByte];` |
|   16948 |   986 | `	if( zIn >= zEnd ){` |
|       - |   987 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |   988 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |   989 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |   990 | `		 */` |
|     234 |   991 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     234 |   992 | `		return SXRET_OK;` |
|       - |   993 | `	}` |
|   16716 |   994 | `	zCur = 0;` |
|       - |   995 | `	/* Compile the node */` |
|   16716 |   996 | `	iCons = 0;` |
|    9324 |   997 | `	for(;;){` |
|   28180 |   998 | `		zCur = zIn;` |
|  144284 |   999 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  118040 |  1000 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      59 |  1001 | `				break;` |
|  117926 |  1002 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1822 |  1003 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     911 |  1004 | `					break;` |
|       - |  1005 | `			}` |
|  116106 |  1006 | `			zIn++;` |
|       2 |  1007 | `		}` |
|   28180 |  1008 | `		if( zIn > zCur ){` |
|   12800 |  1009 | `			if( pObj == 0 ){` |
|   12524 |  1010 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12524 |  1011 | `				if( pObj == 0 ){` |
|     ! 0 |  1012 | `					return SXERR_ABORT;` |
|       - |  1013 | `				}` |
|    6261 |  1014 | `			}` |
|   12800 |  1015 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6399 |  1016 | `		}` |
|   28180 |  1017 | `		if( zIn >= zEnd ){` |
|   16716 |  1018 | `			break;` |
|       - |  1019 | `		}` |
|   11466 |  1020 | `		if( zIn[0] == '\\' ){` |
|    9532 |  1021 | `			const char *zPtr = 0;` |
|       - |  1022 | `			sxu32 n;` |
|    9532 |  1023 | `			zIn++;` |
|    9532 |  1024 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1025 | `				break;` |
|       - |  1026 | `			}` |
|    9532 |  1027 | `			if( pObj == 0 ){` |
|    5812 |  1028 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5812 |  1029 | `				if( pObj == 0 ){` |
|     ! 0 |  1030 | `					return SXERR_ABORT;` |
|       - |  1031 | `				}` |
|    2905 |  1032 | `			}` |
|    9532 |  1033 | `			n = sizeof(char); /* size of conversion */` |
|    9532 |  1034 | `			switch( zIn[0] ){` |
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
|    4400 |  1055 | `			case 'n':` |
|       - |  1056 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8802 |  1057 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8802 |  1058 | `				break;` |
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
|    9532 |  1126 | `			zIn += n;` |
|    9532 |  1127 | `			continue;` |
|       - |  1128 | `		}` |
|    1936 |  1129 | `		if( zIn[0] == '{' ){` |
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
|    1820 |  1163 | `			const char *zExpr = zIn;` |
|       - |  1164 | `			/* Assemble variable name */` |
|     915 |  1165 | `			for(;;){` |
|       - |  1166 | `				/* Jump leading dollars */` |
|    3650 |  1167 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1820 |  1168 | `					zIn++;` |
|       2 |  1169 | `				}` |
|     915 |  1170 | `				for(;;){` |
|   10735 |  1171 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    7990 |  1172 | `						zIn++;` |
|       2 |  1173 | `					}` |
|    1832 |  1174 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1175 | `						/* UTF-8 stream */` |
|     ! 0 |  1176 | `						zIn++;` |
|     ! 0 |  1177 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1178 | `							zIn++;` |
|     ! 0 |  1179 | `						}` |
|     ! 0 |  1180 | `						continue;` |
|       - |  1181 | `					}` |
|    1832 |  1182 | `					break;` |
|     ! 0 |  1183 | `				}` |
|    1832 |  1184 | `				if( zIn >= zEnd ){` |
|     110 |  1185 | `					break;` |
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
|    1820 |  1234 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1820 |  1235 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1236 | `				return SXERR_ABORT;` |
|       - |  1237 | `			}` |
|    1820 |  1238 | `			if( rc != SXERR_EMPTY ){` |
|    1818 |  1239 | `				++iCons;` |
|     908 |  1240 | `			}` |
|       - |  1241 | `		}` |
|       - |  1242 | `		/* Invalidate the previously used constant */` |
|    1936 |  1243 | `		pObj = 0;` |
|       2 |  1244 | `	}/*for(;;)*/` |
|   16716 |  1245 | `	if( iCons > 1 ){` |
|       - |  1246 | `		/* Concatenate all compiled constants */` |
|    1432 |  1247 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     715 |  1248 | `	}` |
|       - |  1249 | `	/* Node successfully compiled */` |
|   16716 |  1250 | `	return SXRET_OK;` |
|    8475 |  1251 |  |
|       - |  1252 | `/*` |
|       - |  1253 | ` * Compile a double quoted string.` |
|       - |  1254 | ` *  See the block-comment above for more information.` |
|       - |  1255 | ` */` |
|   16886 |  1256 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1257 |  |
|       - |  1258 | `	sxi32 rc;` |
|   16888 |  1259 | `	rc = GenStateCompileString(&(*pGen));` |
|    8443 |  1260 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1261 | `	/* Compilation result */` |
|   16888 |  1262 | `	return rc;` |
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
|   17000 |  1306 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   17002 |  1317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1318 | `	/* Compile the expression*/` |
|   17002 |  1319 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1320 | `	/* Restore token stream */` |
|   17002 |  1321 | `	RE_SWAP_DELIMITER(pGen);` |
|   17002 |  1322 | `	return rc;` |
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
|   25244 |  1361 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1362 |  |
|       - |  1363 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1364 | `	SyToken *pKey,*pCur;` |
|   25246 |  1365 | `	sxi32 iEmitRef = 0;` |
|   25246 |  1366 | `	sxi32 nPair = 0;` |
|       - |  1367 | `	sxi32 iNest;` |
|       - |  1368 | `	sxi32 rc;` |
|   25246 |  1369 | `	xValidator = 0;` |
|   20455 |  1370 | `	for(;;){` |
|       - |  1371 | `		/* Jump leading commas */` |
|   46178 |  1372 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5268 |  1373 | `			pGen->pIn++;` |
|       2 |  1374 | `		}` |
|   40912 |  1375 | `		pCur = pGen->pIn;` |
|   40912 |  1376 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1377 | `			/* No more entry to process */` |
|   25234 |  1378 | `			break;` |
|       - |  1379 | `		}` |
|   15680 |  1380 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1381 | `			continue;` |
|       - |  1382 | `		}` |
|       - |  1383 | `		/* Compile the key if available */` |
|   15680 |  1384 | `		pKey = pCur;` |
|   15680 |  1385 | `		iNest = 0;` |
|   43672 |  1386 | `		while( pCur < pGen->pIn ){` |
|   29214 |  1387 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 |  1388 | `				break;` |
|       - |  1389 | `			}` |
|       - |  1390 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1391 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1392 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1393 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1394 | `			 */` |
|   27998 |  1395 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
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
|   27992 |  1459 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      84 |  1460 | `				iNest++;` |
|   27951 |  1461 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - |  1462 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - |  1463 | `				 * parser will shortly detect any syntax error.` |
|       - |  1464 | `				 */` |
|      84 |  1465 | `				iNest--;` |
|      41 |  1466 | `			}` |
|   27992 |  1467 | `			pCur++;` |
|       2 |  1468 | `		}` |
|   15680 |  1469 | `		rc = SXERR_EMPTY;` |
|   15680 |  1470 | `		if( pCur < pGen->pIn ){` |
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
|   15067 |  1486 | `		}else if( pKey == pCur ){` |
|       - |  1487 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1488 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1489 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1490 | `		}else{` |
|       - |  1491 | `			/* Reset back the cursor and point to the entry value */` |
|   14464 |  1492 | `			pCur = pKey;` |
|       - |  1493 | `		}` |
|   15670 |  1494 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1495 | `			/* No available key,load NULL */` |
|   14466 |  1496 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7232 |  1497 | `		}` |
|   15670 |  1498 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
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
|   15668 |  1513 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15668 |  1514 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1515 | `			return SXERR_ABORT;` |
|       - |  1516 | `		}` |
|   15668 |  1517 | `		if( iEmitRef ){` |
|       - |  1518 | `			/* Emit the load reference instruction */` |
|      32 |  1519 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1520 | `		}` |
|   15668 |  1521 | `		xValidator = 0;` |
|   15668 |  1522 | `		iEmitRef = 0;` |
|   15668 |  1523 | `		nPair++;` |
|       2 |  1524 | `	}` |
|       - |  1525 | `	/* Emit the load map instruction */` |
|   25234 |  1526 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1527 | `	/* Node successfully compiled */` |
|   25234 |  1528 | `	return SXRET_OK;` |
|   12624 |  1529 |  |
|       - |  1530 | `/*` |
|       - |  1531 | ` * Compile the 'array' language construct.` |
|       - |  1532 | ` *	 According to the PHP language reference manual` |
|       - |  1533 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1534 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1535 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1536 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1537 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1538 | ` */` |
|   24948 |  1539 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1540 |  |
|       - |  1541 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   24950 |  1542 | `	pGen->pIn += 2;` |
|   24950 |  1543 | `	pGen->pEnd--;` |
|   12474 |  1544 | `	SXUNUSED(iCompileFlag);` |
|   24950 |  1545 | `	return GenStateCompileArrayBody(pGen);` |
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
|      52 |  2224 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
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
|  846922 |  2664 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2665 |  |
|  846924 |  2666 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2667 | `	sxi32 iVv;` |
|       - |  2668 | `	sxi32 iP1;` |
|       - |  2669 | `	void *p3;` |
|       - |  2670 | `	sxi32 rc;` |
|  846924 |  2671 | `	iVv = -1; /* Variable variable counter */` |
| 1693858 |  2672 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  846936 |  2673 | `		pGen->pIn++;` |
|  846936 |  2674 | `		iVv++;` |
|       2 |  2675 | `	}` |
|  846924 |  2676 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2677 | `		/* Invalid variable name */` |
|     ! 0 |  2678 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2679 | `		if( rc == SXERR_ABORT ){` |
|       - |  2680 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2681 | `			return SXERR_ABORT;` |
|       - |  2682 | `		}` |
|     ! 0 |  2683 | `		return SXRET_OK;` |
|       - |  2684 | `	}` |
|  846924 |  2685 | `	p3  = 0;` |
|  846924 |  2686 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
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
|  846908 |  2706 | `		char *zName = 0;` |
|       - |  2707 | `		/* Extract variable name */` |
|  846908 |  2708 | `		pName = &pGen->pIn->sData;` |
|       - |  2709 | `		/* Advance the stream cursor */` |
|  846908 |  2710 | `		pGen->pIn++;` |
|  846908 |  2711 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  846908 |  2712 | `		if( pEntry == 0 ){` |
|       - |  2713 | `			/* Duplicate name */` |
|  121844 |  2714 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  121844 |  2715 | `			if( zName == 0 ){` |
|     ! 0 |  2716 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2717 | `				return SXERR_ABORT;` |
|       - |  2718 | `			}` |
|       - |  2719 | `			/* Install in the hashtable */` |
|  121844 |  2720 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   60923 |  2721 | `		}else{` |
|       - |  2722 | `			/* Name already available */` |
|  725066 |  2723 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2724 | `		}` |
|  846908 |  2725 | `		p3 = (void *)zName;` |
|       - |  2726 | `	}` |
|  846920 |  2727 | `	iP1 = 0;` |
|  846920 |  2728 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  325248 |  2729 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2730 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  318790 |  2731 | `			iP1 = 1;` |
|  159394 |  2732 | `		}` |
|  162623 |  2733 | `	}` |
|       - |  2734 | `	/* Emit the load instruction */` |
|  846920 |  2735 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  846932 |  2736 | `	while( iVv > 0 ){` |
|      13 |  2737 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2738 | `		iVv--;` |
|       1 |  2739 | `	}` |
|       - |  2740 | `	/* Node successfully compiled */` |
|  846920 |  2741 | `	return SXRET_OK;` |
|  423463 |  2742 |  |
|       - |  2743 | `/*` |
|       - |  2744 | ` * Load a literal.` |
|       - |  2745 | ` */` |
|  568148 |  2746 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2747 |  |
|  568150 |  2748 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2749 | `	ph7_value *pObj;` |
|       - |  2750 | `	SyString *pStr;` |
|       - |  2751 | `	sxu32 nIdx;` |
|       - |  2752 | `	/* Extract token value */` |
|  568150 |  2753 | `	pStr = &pToken->sData;` |
|       - |  2754 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  568150 |  2755 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  103334 |  2756 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2757 | `			/* NULL constant are always indexed at 0 */` |
|   43988 |  2758 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   43988 |  2759 | `			return SXRET_OK;` |
|   59348 |  2760 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2761 | `			/* TRUE constant are always indexed at 1 */` |
|     518 |  2762 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     518 |  2763 | `			return SXRET_OK;` |
|       2 |  2764 | `		}` |
|  539036 |  2765 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|   89606 |  2766 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2767 | `			/* FALSE constant are always indexed at 2 */` |
|   38258 |  2768 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   38258 |  2769 | `			return SXRET_OK;` |
|  466152 |  2770 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   79180 |  2771 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2772 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    5800 |  2773 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    5800 |  2774 | `			if( pObj == 0 ){` |
|     ! 0 |  2775 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2776 | `				return SXERR_ABORT;` |
|       - |  2777 | `			}` |
|    5800 |  2778 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2779 | `			/* Emit the load constant instruction */` |
|    5800 |  2780 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    5800 |  2781 | `			return SXRET_OK;` |
|  435381 |  2782 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   29234 |  2783 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
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
|  434489 |  2799 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   12194 |  2800 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  428386 |  2801 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   15274 |  2802 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
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
|  479578 |  2832 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2833 | `		ph7_value *pLitObj;` |
|       - |  2834 | `		/* Unknown literal,install it in the literal table */` |
|  224510 |  2835 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  224510 |  2836 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2837 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2838 | `			return SXERR_ABORT;` |
|       - |  2839 | `		}` |
|  224510 |  2840 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  224510 |  2841 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  112254 |  2842 | `	}` |
|       - |  2843 | `	/* Emit the load constant instruction */` |
|  479578 |  2844 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  479578 |  2845 | `	return SXRET_OK;` |
|  284076 |  2846 |  |
|       - |  2847 | `/*` |
|       - |  2848 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2849 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2850 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2851 | ` * Otherwise, load the simple literal directly.` |
|       - |  2852 | ` */` |
|  568176 |  2853 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2854 |  |
|       - |  2855 | `	sxi32 rc;` |
|  568178 |  2856 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2857 | `		return SXRET_OK;` |
|       - |  2858 | `	}` |
|       - |  2859 | `	/* Check if this is a multi-token namespace path */` |
|  568178 |  2860 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2861 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      30 |  2862 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      30 |  2863 | `		int isAbsolute = 0;` |
|      30 |  2864 | `		SyBlobReset(pWorker);` |
|       - |  2865 | `		/* Check for leading backslash (absolute path) */` |
|      30 |  2866 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      28 |  2867 | `			isAbsolute = 1;` |
|      28 |  2868 | `			pGen->pIn++; /* Skip leading backslash */` |
|      13 |  2869 | `		}` |
|       - |  2870 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      30 |  2871 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2872 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2873 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2874 | `		}` |
|       - |  2875 | `		/* Collect all path components */` |
|     110 |  2876 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     110 |  2877 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      42 |  2878 | `				SyBlobAppend(pWorker,"\\",1);` |
|      22 |  2879 | `			}else{` |
|      70 |  2880 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2881 | `			}` |
|     110 |  2882 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      30 |  2883 | `				pGen->pIn++;` |
|      30 |  2884 | `				break;` |
|       - |  2885 | `			}` |
|      82 |  2886 | `			pGen->pIn++;` |
|       2 |  2887 | `		}` |
|      30 |  2888 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2889 | `			ph7_value *pObj;` |
|       - |  2890 | `			SyString sPath;` |
|       - |  2891 | `			sxu32 nIdx;` |
|      30 |  2892 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2893 | `			/* Install in the literal table */` |
|      30 |  2894 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
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
|      30 |  2905 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|      30 |  2906 | `			return SXRET_OK;` |
|       - |  2907 | `		}` |
|     ! 0 |  2908 | `	}` |
|       - |  2909 | `	/* Single-token literal: load directly */` |
|  568150 |  2910 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  568150 |  2911 | `	return rc;` |
|  284090 |  2912 |  |
|       - |  2913 | `/*` |
|       - |  2914 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2915 | ` */` |
|  568176 |  2916 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2917 |  |
|       - |  2918 | `	sxi32 rc;` |
|  568178 |  2919 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  568178 |  2920 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2921 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2922 | `		return rc;` |
|       - |  2923 | `	}` |
|       - |  2924 | `	/* Node successfully compiled */` |
|  568178 |  2925 | `	return SXRET_OK;` |
|  284090 |  2926 |  |
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
|    3026 |  3086 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3087 |  |
|    3028 |  3088 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17702 |  3089 | `	while( pBlock && pBlock != pTarget ){` |
|   14676 |  3090 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
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
|   14676 |  3102 | `		pBlock = pBlock->pParent;` |
|       2 |  3103 | `	}` |
|    3028 |  3104 |  |
|    2942 |  3105 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3106 |  |
|       - |  3107 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3108 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3109 | `	sxu32 nLineLocal;` |
|       - |  3110 | `	sxi32 rc;` |
|    2944 |  3111 | `	nLineLocal = pGen->pIn->nLine;` |
|    2944 |  3112 | `	iLevel = 0;` |
|       - |  3113 | `	/* Jump the 'continue' keyword */` |
|    2944 |  3114 | `	pGen->pIn++;` |
|    2944 |  3115 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
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
|    2944 |  3141 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2944 |  3142 | `	if( pLoop == 0 ){` |
|       - |  3143 | `		/* Illegal continue */` |
|      11 |  3144 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3145 | `		if( rc == SXERR_ABORT ){` |
|       - |  3146 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3147 | `			return SXERR_ABORT;` |
|       - |  3148 | `		}` |
|       6 |  3149 | `	}else{` |
|    2934 |  3150 | `		sxu32 nInstrIdx = 0;` |
|       - |  3151 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2934 |  3152 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2934 |  3153 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
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
|    2930 |  3165 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2930 |  3166 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3167 | `				JumpFixup sJumpFix;` |
|       - |  3168 | `				/* Post-continue */` |
|      14 |  3169 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3170 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3171 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3172 | `			}` |
|       - |  3173 | `		}` |
|       - |  3174 | `	}` |
|    2944 |  3175 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3176 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3177 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3178 | `	}` |
|       - |  3179 | `	/* Statement successfully compiled */` |
|    2944 |  3180 | `	return SXRET_OK;` |
|    1473 |  3181 |  |
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
|  319218 |  3458 | `static sxi32 PH7_CompileBlock(` |
|       - |  3459 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3460 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3461 | `	)` |
|       2 |  3462 |  |
|       - |  3463 | `	sxi32 rc;` |
|       - |  3464 | `	sxu32 nLine;` |
|  319220 |  3465 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  317812 |  3466 | `		nLine = pGen->pIn->nLine;` |
|  317812 |  3467 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  317812 |  3468 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3469 | `			return SXERR_ABORT;` |
|       - |  3470 | `		}` |
|  317812 |  3471 | `		pGen->pIn++;` |
|       - |  3472 | `		/* Compile until we hit the closing braces '}' */` |
|  438689 |  3473 | `		for(;;){` |
|  877380 |  3474 | `			if( pGen->pIn >= pGen->pEnd ){` |
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
|  877360 |  3485 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3486 | `				/* Closing braces found,break immediately*/` |
|  317792 |  3487 | `				pGen->pIn++;` |
|  317792 |  3488 | `				break;` |
|       - |  3489 | `			}` |
|       - |  3490 | `			/* Compile a single statement */` |
|  559570 |  3491 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  559570 |  3492 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3493 | `				return SXERR_ABORT;` |
|       - |  3494 | `			}` |
|       2 |  3495 | `		}` |
|  317812 |  3496 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  160315 |  3497 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
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
|  319220 |  3547 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3548 | `		pGen->pIn++;` |
|     ! 0 |  3549 | `	}` |
|  319220 |  3550 | `	return SXRET_OK;` |
|  159611 |  3551 |  |
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
|   11694 |  3571 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3572 |  |
|   11696 |  3573 | `	GenBlock *pWhileBlock = 0;` |
|   11696 |  3574 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3575 | `	sxu32 nFalseJump;` |
|       - |  3576 | `	sxu32 nLine;` |
|       - |  3577 | `	sxi32 rc;` |
|   11696 |  3578 | `	nLine = pGen->pIn->nLine;` |
|       - |  3579 | `	/* Jump the 'while' keyword */` |
|   11696 |  3580 | `	pGen->pIn++;` |
|   11696 |  3581 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3582 | `		/* Syntax error */` |
|     ! 0 |  3583 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3584 | `		if( rc == SXERR_ABORT ){` |
|       - |  3585 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3586 | `			return SXERR_ABORT;` |
|       - |  3587 | `		}` |
|     ! 0 |  3588 | `		goto Synchronize;` |
|       - |  3589 | `	}` |
|       - |  3590 | `	/* Jump the left parenthesis '(' */` |
|   11696 |  3591 | `	pGen->pIn++;` |
|       - |  3592 | `	/* Create the loop block */` |
|   11696 |  3593 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11696 |  3594 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3595 | `		return SXERR_ABORT;` |
|       - |  3596 | `	}` |
|       - |  3597 | `	/* Delimit the condition */` |
|   11696 |  3598 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11696 |  3599 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3600 | `		/* Empty expression */` |
|       3 |  3601 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3602 | `		if( rc == SXERR_ABORT ){` |
|       - |  3603 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3604 | `			return SXERR_ABORT;` |
|       - |  3605 | `		}` |
|       1 |  3606 | `	}` |
|       - |  3607 | `	/* Swap token streams */` |
|   11696 |  3608 | `	pTmp = pGen->pEnd;` |
|   11696 |  3609 | `	pGen->pEnd = pEnd;` |
|       - |  3610 | `	/* Compile the expression */` |
|   11696 |  3611 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11696 |  3612 | `	if( rc == SXERR_ABORT ){` |
|       - |  3613 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3614 | `		return SXERR_ABORT;` |
|       - |  3615 | `	}` |
|       - |  3616 | `	/* Update token stream */` |
|   11696 |  3617 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3618 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3619 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3620 | `			return SXERR_ABORT;` |
|       - |  3621 | `		}` |
|     ! 0 |  3622 | `		pGen->pIn++;` |
|     ! 0 |  3623 | `	}` |
|       - |  3624 | `	/* Synchronize pointers */` |
|   11696 |  3625 | `	pGen->pIn  = &pEnd[1];` |
|   11696 |  3626 | `	pGen->pEnd = pTmp;` |
|       - |  3627 | `	/* Emit the false jump */` |
|   11696 |  3628 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3629 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11696 |  3630 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3631 | `	/* Compile the loop body */` |
|   11696 |  3632 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11696 |  3633 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3634 | `		return SXERR_ABORT;` |
|       - |  3635 | `	}` |
|       - |  3636 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11696 |  3637 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3638 | `	/* Fix all jumps now the destination is resolved */` |
|   11696 |  3639 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3640 | `	/* Release the loop block */` |
|   11696 |  3641 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3642 | `	/* Statement successfully compiled */` |
|   11696 |  3643 | `	return SXRET_OK;` |
|     ! 0 |  3644 | `Synchronize:` |
|       - |  3645 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3646 | `	 * compiling this erroneous block.` |
|       - |  3647 | `	 */` |
|     ! 0 |  3648 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3649 | `		pGen->pIn++;` |
|     ! 0 |  3650 | `	}` |
|     ! 0 |  3651 | `	return SXRET_OK;` |
|    5849 |  3652 |  |
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
|   11698 |  3800 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3801 |  |
|   11700 |  3802 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11700 |  3803 | `	GenBlock *pForBlock = 0;` |
|       - |  3804 | `	sxu32 nFalseJump;` |
|       - |  3805 | `	sxu32 nLine;` |
|       - |  3806 | `	sxi32 rc;` |
|   11700 |  3807 | `	nLine = pGen->pIn->nLine;` |
|       - |  3808 | `	/* Jump the 'for' keyword */` |
|   11700 |  3809 | `	pGen->pIn++;` |
|   11700 |  3810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3811 | `		/* Syntax error */` |
|     ! 0 |  3812 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3813 | `		if( rc == SXERR_ABORT ){` |
|       - |  3814 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3815 | `			return SXERR_ABORT;` |
|       - |  3816 | `		}` |
|     ! 0 |  3817 | `		return SXRET_OK;` |
|       - |  3818 | `	}` |
|       - |  3819 | `	/* Jump the left parenthesis '(' */` |
|   11700 |  3820 | `	pGen->pIn++;` |
|       - |  3821 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11700 |  3822 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11700 |  3823 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|   11700 |  3838 | `	pTmp = pGen->pEnd;` |
|   11700 |  3839 | `	pGen->pEnd = pEnd;` |
|       - |  3840 | `	/* Compile initialization expressions if available */` |
|   11700 |  3841 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3842 | `	/* Pop operand lvalues */` |
|   11700 |  3843 | `	if( rc == SXERR_ABORT ){` |
|       - |  3844 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3845 | `		return SXERR_ABORT;` |
|   11700 |  3846 | `	}else if( rc != SXERR_EMPTY ){` |
|   11698 |  3847 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5848 |  3848 | `	}` |
|   11700 |  3849 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11700 |  3860 | `	pGen->pIn++;` |
|       - |  3861 | `	/* Create the loop block */` |
|   11700 |  3862 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11700 |  3863 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3864 | `		return SXERR_ABORT;` |
|       - |  3865 | `	}` |
|       - |  3866 | `	/* Deffer continue jumps */` |
|   11700 |  3867 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3868 | `	/* Compile the condition */` |
|   11700 |  3869 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11700 |  3870 | `	if( rc == SXERR_ABORT ){` |
|       - |  3871 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3872 | `		return SXERR_ABORT;` |
|   11700 |  3873 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3874 | `		/* Emit the false jump */` |
|   11698 |  3875 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3876 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11698 |  3877 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5848 |  3878 | `	}` |
|   11700 |  3879 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
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
|   11696 |  3890 | `	pGen->pIn++;` |
|       - |  3891 | `	/* Save the post condition stream */` |
|   11696 |  3892 | `	pPostStart = pGen->pIn;` |
|       - |  3893 | `	/* Compile the loop body */` |
|   11696 |  3894 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11696 |  3895 | `	pGen->pEnd = pTmp;` |
|   11696 |  3896 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11696 |  3897 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3898 | `		return SXERR_ABORT;` |
|       - |  3899 | `	}` |
|       - |  3900 | `	/* Fix post-continue jumps */` |
|   11696 |  3901 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
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
|   11696 |  3917 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3918 | `		pPostStart++;` |
|     ! 0 |  3919 | `	}` |
|   11696 |  3920 | `	if( pPostStart < pEnd ){` |
|       - |  3921 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11696 |  3922 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11696 |  3923 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11696 |  3924 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3925 | `			/* Syntax error */` |
|     ! 0 |  3926 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  3927 | `			if( rc == SXERR_ABORT ){` |
|       - |  3928 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3929 | `				return SXERR_ABORT;` |
|       - |  3930 | `			}` |
|     ! 0 |  3931 | `			return SXRET_OK;` |
|       - |  3932 | `		}` |
|   11696 |  3933 | `		RE_SWAP_DELIMITER(pGen);` |
|   11696 |  3934 | `		if( rc == SXERR_ABORT ){` |
|       - |  3935 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3936 | `			return SXERR_ABORT;` |
|   11696 |  3937 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  3938 | `			/* Pop operand lvalue */` |
|   11696 |  3939 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5847 |  3940 | `		}` |
|    5847 |  3941 | `	}` |
|       - |  3942 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11696 |  3943 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  3944 | `	/* Fix all jumps now the destination is resolved */` |
|   11696 |  3945 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3946 | `	/* Release the loop block */` |
|   11696 |  3947 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3948 | `	/* Statement successfully compiled */` |
|   11696 |  3949 | `	return SXRET_OK;` |
|    5851 |  3950 |  |
|       - |  3951 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  3952 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  3953 | ` * are allowed.` |
|       - |  3954 | ` */` |
|    6214 |  3955 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  3956 |  |
|    6216 |  3957 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6216 |  3958 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  3959 | `		/* Unexpected expression */` |
|     ! 0 |  3960 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  3961 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  3962 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  3963 | `			rc = SXERR_INVALID;` |
|     ! 0 |  3964 | `		}` |
|     ! 0 |  3965 | `	}` |
|    6216 |  3966 | `	return rc;` |
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
|    3164 |  3994 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  3995 |  |
|    3166 |  3996 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3166 |  3997 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3166 |  3998 | `	GenBlock *pForeachBlock = 0;` |
|       - |  3999 | `	ph7_foreach_info *pInfo;` |
|       - |  4000 | `	sxu32 nFalseJump;` |
|       - |  4001 | `	VmInstr *pInstr;` |
|       - |  4002 | `	sxu32 nLine;` |
|       - |  4003 | `	sxi32 rc;` |
|    3166 |  4004 | `	nLine = pGen->pIn->nLine;` |
|       - |  4005 | `	/* Jump the 'foreach' keyword */` |
|    3166 |  4006 | `	pGen->pIn++;` |
|    3166 |  4007 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4008 | `		/* Syntax error */` |
|     ! 0 |  4009 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4010 | `		if( rc == SXERR_ABORT ){` |
|       - |  4011 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4012 | `			return SXERR_ABORT;` |
|       - |  4013 | `		}` |
|     ! 0 |  4014 | `		goto Synchronize;` |
|       - |  4015 | `	}` |
|       - |  4016 | `	/* Jump the left parenthesis '(' */` |
|    3166 |  4017 | `	pGen->pIn++;` |
|       - |  4018 | `	/* Create the loop block */` |
|    3166 |  4019 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3166 |  4020 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4021 | `		return SXERR_ABORT;` |
|       - |  4022 | `	}` |
|       - |  4023 | `	/* Delimit the expression */` |
|    3166 |  4024 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3166 |  4025 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
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
|    3166 |  4040 | `	pCur = pGen->pIn;` |
|   21254 |  4041 | `	while( pCur < pEnd ){` |
|   21254 |  4042 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3176 |  4043 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3176 |  4044 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4045 | `				/* Break with the first 'as' found */` |
|    3166 |  4046 | `				break;` |
|       - |  4047 | `			}` |
|       5 |  4048 | `		}` |
|       - |  4049 | `		/* Advance the stream cursor */` |
|   18090 |  4050 | `		pCur++;` |
|       2 |  4051 | `	}` |
|    3166 |  4052 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4053 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4054 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4055 | `		if( rc == SXERR_ABORT ){` |
|       - |  4056 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4057 | `			return SXERR_ABORT;` |
|       - |  4058 | `		}` |
|     ! 0 |  4059 | `		goto Synchronize;` |
|       - |  4060 | `	}` |
|       - |  4061 | `	/* Swap token streams */` |
|    3166 |  4062 | `	pTmp = pGen->pEnd;` |
|    3166 |  4063 | `	pGen->pEnd = pCur;` |
|    3166 |  4064 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3166 |  4065 | `	if( rc == SXERR_ABORT ){` |
|       - |  4066 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4067 | `		return SXERR_ABORT;` |
|       - |  4068 | `	}` |
|       - |  4069 | `	/* Update token stream */` |
|    3166 |  4070 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4071 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4072 | `		if( rc == SXERR_ABORT ){` |
|       - |  4073 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4074 | `			return SXERR_ABORT;` |
|       - |  4075 | `		}` |
|     ! 0 |  4076 | `		pGen->pIn++;` |
|     ! 0 |  4077 | `	}` |
|    3166 |  4078 | `	pCur++; /* Jump the 'as' keyword */` |
|    3166 |  4079 | `	pGen->pIn = pCur;` |
|    3166 |  4080 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4081 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4082 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4083 | `			return SXERR_ABORT;` |
|       - |  4084 | `		}` |
|     ! 0 |  4085 | `	}` |
|       - |  4086 | `	/* Create the foreach context */` |
|    3166 |  4087 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3166 |  4088 | `	if( pInfo == 0 ){` |
|     ! 0 |  4089 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4090 | `		return SXERR_ABORT;` |
|       - |  4091 | `	}` |
|       - |  4092 | `	/* Zero the structure */` |
|    3166 |  4093 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4094 | `	/* Initialize structure fields */` |
|    3166 |  4095 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4096 | `	/* Check if we have a key field */` |
|    9544 |  4097 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6380 |  4098 | `		pCur++;` |
|       2 |  4099 | `	}` |
|    3166 |  4100 | `	if( pCur < pEnd ){` |
|       - |  4101 | `		/* Compile the expression holding the key name */` |
|    3062 |  4102 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4103 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4104 | `			if( rc == SXERR_ABORT ){` |
|       - |  4105 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4106 | `				return SXERR_ABORT;` |
|       - |  4107 | `			}` |
|     ! 0 |  4108 | `		}else{` |
|    3062 |  4109 | `			pGen->pEnd = pCur;` |
|    3062 |  4110 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3062 |  4111 | `			if( rc == SXERR_ABORT ){` |
|       - |  4112 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4113 | `				return SXERR_ABORT;` |
|       - |  4114 | `			}` |
|    3062 |  4115 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3062 |  4116 | `			if( pInstr->p3 ){` |
|       - |  4117 | `				/* Record key name */` |
|    3062 |  4118 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1530 |  4119 | `			}` |
|    3062 |  4120 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4121 | `		}` |
|    3062 |  4122 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1530 |  4123 | `	}` |
|    3166 |  4124 | `	pGen->pEnd = pEnd;` |
|    3166 |  4125 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4126 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4127 | `		if( rc == SXERR_ABORT ){` |
|       - |  4128 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4129 | `			return SXERR_ABORT;` |
|       - |  4130 | `		}` |
|     ! 0 |  4131 | `		goto Synchronize;` |
|       - |  4132 | `	}` |
|    3166 |  4133 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4134 | `		pGen->pIn++;` |
|       - |  4135 | `		/* Pass by reference  */` |
|      11 |  4136 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4137 | `	}` |
|       - |  4138 | `	/* Check if the value target is list() */` |
|    3166 |  4139 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
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
|    3161 |  4180 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
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
|    3156 |  4213 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3156 |  4214 | `		if( rc == SXERR_ABORT ){` |
|       - |  4215 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4216 | `			return SXERR_ABORT;` |
|       - |  4217 | `		}` |
|    3156 |  4218 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3156 |  4219 | `		if( pInstr->p3 ){` |
|       - |  4220 | `			/* Record value name */` |
|    3156 |  4221 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1577 |  4222 | `		}` |
|       - |  4223 | `	}` |
|       - |  4224 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3164 |  4225 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4226 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3164 |  4227 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4228 | `	/* Record the first instruction to execute */` |
|    3164 |  4229 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4230 | `	/* Emit the FOREACH_STEP instruction */` |
|    3164 |  4231 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4232 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3164 |  4233 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4234 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3164 |  4235 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
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
|    3164 |  4263 | `	pGen->pIn = &pEnd[1];` |
|    3164 |  4264 | `	pGen->pEnd = pTmp;` |
|    3164 |  4265 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3164 |  4266 | `	if( rc == SXERR_ABORT ){` |
|       - |  4267 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4268 | `		return SXERR_ABORT;` |
|       - |  4269 | `	}` |
|       - |  4270 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3164 |  4271 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4272 | `	/* Fix all jumps now the destination is resolved */` |
|    3164 |  4273 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4274 | `	/* Release the loop block */` |
|    3164 |  4275 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4276 | `	/* Statement successfully compiled */` |
|    3164 |  4277 | `	return SXRET_OK;` |
|       1 |  4278 | `Synchronize:` |
|       - |  4279 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4280 | `	 * compiling this erroneous block.` |
|       - |  4281 | `	 */` |
|       3 |  4282 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4283 | `		pGen->pIn++;` |
|     ! 0 |  4284 | `	}` |
|       3 |  4285 | `	return SXRET_OK;` |
|    1584 |  4286 |  |
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
|  116072 |  4319 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4320 |  |
|  116074 |  4321 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  116074 |  4322 | `	GenBlock *pCondBlock = 0;` |
|       - |  4323 | `	sxu32 nJumpIdx;` |
|       - |  4324 | `	sxu32 nKeyID;` |
|       - |  4325 | `	sxi32 rc;` |
|       - |  4326 | `	/* Jump the 'if' keyword */` |
|  116074 |  4327 | `	pGen->pIn++;` |
|  116074 |  4328 | `	pToken = pGen->pIn;` |
|       - |  4329 | `	/* Create the conditional block */` |
|  116074 |  4330 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  116074 |  4331 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4332 | `		return SXERR_ABORT;` |
|       - |  4333 | `	}` |
|       - |  4334 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   63845 |  4335 | `	for(;;){` |
|  127692 |  4336 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
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
|  127692 |  4349 | `		pToken++;` |
|       - |  4350 | `		/* Delimit the condition */` |
|  127692 |  4351 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  127692 |  4352 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
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
|  127692 |  4365 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4366 | `		/* Compile the condition */` |
|  127692 |  4367 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4368 | `		/* Update token stream */` |
|  127692 |  4369 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4370 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4371 | `			pGen->pIn++;` |
|     ! 0 |  4372 | `		}` |
|  127692 |  4373 | `		pGen->pIn  = &pEnd[1];` |
|  127692 |  4374 | `		pGen->pEnd = pTmp;` |
|  127692 |  4375 | `		if( rc == SXERR_ABORT ){` |
|       - |  4376 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4377 | `			return SXERR_ABORT;` |
|       - |  4378 | `		}` |
|       - |  4379 | `		/* Emit the false jump */` |
|  127692 |  4380 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4381 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  127692 |  4382 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4383 | `		/* Compile the body */` |
|  127692 |  4384 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  127692 |  4385 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4386 | `			return SXERR_ABORT;` |
|       - |  4387 | `		}` |
|  127692 |  4388 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   34348 |  4389 | `			break;` |
|       - |  4390 | `		}` |
|       - |  4391 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   59000 |  4392 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   59000 |  4393 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   37960 |  4394 | `			break;` |
|       - |  4395 | `		}` |
|       - |  4396 | `		/* Emit the unconditional jump */` |
|   21042 |  4397 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4398 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21042 |  4399 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   21042 |  4400 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   15220 |  4401 | `			pToken = &pGen->pIn[1];` |
|   15220 |  4402 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5826 |  4403 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4713 |  4404 | `					break;` |
|       - |  4405 | `			}` |
|    5798 |  4406 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2898 |  4407 | `		}` |
|   11620 |  4408 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4409 | `		/* Synchronize cursors */` |
|   11620 |  4410 | `		pToken = pGen->pIn;` |
|       - |  4411 | `		/* Fix the false jump */` |
|   11620 |  4412 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4413 | `	} /* For(;;) */` |
|       - |  4414 | `	/* Fix the false jump */` |
|  116074 |  4415 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  116074 |  4416 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   47380 |  4417 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4418 | `			/* Compile the else block */` |
|    9424 |  4419 | `			pGen->pIn++;` |
|    9424 |  4420 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9424 |  4421 | `			if( rc == SXERR_ABORT ){` |
|       - |  4422 |  |
|     ! 0 |  4423 | `				return SXERR_ABORT;` |
|       - |  4424 | `			}` |
|    4711 |  4425 | `	}` |
|  116074 |  4426 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4427 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  116074 |  4428 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4429 | `	/* Release the conditional block */` |
|  116074 |  4430 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4431 | `	/* Statement successfully compiled */` |
|  116074 |  4432 | `	return SXRET_OK;` |
|     ! 0 |  4433 | `Synchronize:` |
|       - |  4434 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4435 | `	 */` |
|     ! 0 |  4436 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4437 | `		pGen->pIn++;` |
|     ! 0 |  4438 | `	}` |
|     ! 0 |  4439 | `	return SXRET_OK;` |
|   58038 |  4440 |  |
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
|      32 |  4462 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4463 |  |
|      34 |  4464 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4465 | `	sxi32 nExpr;` |
|       - |  4466 | `	sxi32 rc;` |
|       - |  4467 | `	/* Jump the 'global' keyword */` |
|      34 |  4468 | `	pGen->pIn++;` |
|      34 |  4469 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4470 | `		/* Nothing to process */` |
|     ! 0 |  4471 | `		return SXRET_OK;` |
|       - |  4472 | `	}` |
|      34 |  4473 | `	pTmp = pGen->pEnd;` |
|      34 |  4474 | `	nExpr = 0;` |
|      68 |  4475 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      36 |  4476 | `		if( pGen->pIn < pNext ){` |
|      36 |  4477 | `			pGen->pEnd = pNext;` |
|      36 |  4478 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4479 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4480 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4481 | `					return SXERR_ABORT;` |
|       - |  4482 | `				}` |
|     ! 0 |  4483 | `			}else{` |
|      36 |  4484 | `				pGen->pIn++;` |
|      36 |  4485 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4486 | `					/* Emit a warning */` |
|     ! 0 |  4487 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4488 | `				}else{` |
|      36 |  4489 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      36 |  4490 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4491 | `						return SXERR_ABORT;` |
|      36 |  4492 | `					}else if(rc != SXERR_EMPTY ){` |
|      36 |  4493 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      36 |  4494 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4495 | `							/* Variable name, not a constant */` |
|      36 |  4496 | `							pLast->iP1 = 0;` |
|      17 |  4497 | `						}` |
|      36 |  4498 | `						nExpr++;` |
|      17 |  4499 | `					}` |
|       - |  4500 | `				}` |
|       - |  4501 | `			}` |
|      17 |  4502 | `		}` |
|       - |  4503 | `		/* Next expression in the stream */` |
|      36 |  4504 | `		pGen->pIn = pNext;` |
|       - |  4505 | `		/* Jump trailing commas */` |
|      38 |  4506 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4507 | `			pGen->pIn++;` |
|       1 |  4508 | `		}` |
|       2 |  4509 | `	}` |
|       - |  4510 | `	/* Restore token stream */` |
|      34 |  4511 | `	pGen->pEnd = pTmp;` |
|      34 |  4512 | `	if( nExpr > 0 ){` |
|       - |  4513 | `		/* Emit the uplink instruction */` |
|      34 |  4514 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      16 |  4515 | `	}` |
|      34 |  4516 | `	return SXRET_OK;` |
|      18 |  4517 |  |
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
|  168832 |  4534 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4535 |  |
|  168834 |  4536 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4537 | `	sxi32 rc;` |
|       - |  4538 | `	/* Jump the 'return' keyword */` |
|  168834 |  4539 | `	pGen->pIn++;` |
|  168834 |  4540 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4541 | `		/* Compile the expression */` |
|  168812 |  4542 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  168812 |  4543 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4544 | `			return SXERR_ABORT;` |
|  168812 |  4545 | `		}else if(rc != SXERR_EMPTY ){` |
|  168812 |  4546 | `			nRet = 1;` |
|   84405 |  4547 | `		}` |
|   84405 |  4548 | `	}` |
|       - |  4549 | `	/* Emit the done instruction */` |
|  168834 |  4550 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  168834 |  4551 | `	return SXRET_OK;` |
|   84418 |  4552 |  |
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
|   11730 |  4643 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4644 |  |
|   11732 |  4645 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4646 | `	sxi32 rc;` |
|       - |  4647 | `	/* Jump the 'echo' keyword */` |
|   11732 |  4648 | `	pGen->pIn++;` |
|       - |  4649 | `	/* Compile arguments one after one */` |
|   11732 |  4650 | `	pTmp = pGen->pEnd;` |
|   24472 |  4651 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   12742 |  4652 | `		if( pGen->pIn < pNext ){` |
|   12742 |  4653 | `			pGen->pEnd = pNext;` |
|   12742 |  4654 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   12742 |  4655 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4656 | `				return SXERR_ABORT;` |
|   12742 |  4657 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4658 | `				/* Emit the consume instruction */` |
|   12718 |  4659 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6358 |  4660 | `			}` |
|    6370 |  4661 | `		}` |
|       - |  4662 | `		/* Jump trailing commas */` |
|   13752 |  4663 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    1012 |  4664 | `			pNext++;` |
|       2 |  4665 | `		}` |
|   12742 |  4666 | `		pGen->pIn = pNext;` |
|       2 |  4667 | `	}` |
|       - |  4668 | `	/* Restore token stream */` |
|   11732 |  4669 | `	pGen->pEnd = pTmp;` |
|   11732 |  4670 | `	return SXRET_OK;` |
|    5867 |  4671 |  |
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
|  346378 |  4838 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
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
|  346380 |  4849 | `	if( pFromImport ){` |
|  331040 |  4850 | `		*pFromImport = 0;` |
|  165519 |  4851 | `	}` |
|  346380 |  4852 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  346380 |  4853 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4854 | `		return nOrigIdx;` |
|       - |  4855 | `	}` |
|  346380 |  4856 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  346380 |  4857 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4858 | `	/* Skip if already qualified (contains backslash) */` |
|  346380 |  4859 | `	hasNsSep = 0;` |
| 3724238 |  4860 | `	for( k = 0; k < nLit; k++ ){` |
| 3377896 |  4861 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1688931 |  4862 | `	}` |
|  346380 |  4863 | `	if( hasNsSep ){` |
|      38 |  4864 | `		return nOrigIdx;` |
|       - |  4865 | `	}` |
|       - |  4866 | `	/* Check use imports first (works even outside namespaces) */` |
|  346344 |  4867 | `	SyBlobReset(&pGen->sWorker);` |
|  346344 |  4868 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  346344 |  4869 | `	if( pImport ){` |
|      38 |  4870 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4871 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4872 | `		if( pFromImport ){` |
|      18 |  4873 | `			*pFromImport = 1;` |
|       8 |  4874 | `		}` |
|      20 |  4875 | `	}else{` |
|  346308 |  4876 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  346220 |  4877 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
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
|  173191 |  4896 |  |
|       - |  4897 | `/*` |
|       - |  4898 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4899 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4900 | ` */` |
|   29368 |  4901 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4902 |  |
|       - |  4903 | `	SyHashEntry *pImport;` |
|       - |  4904 | `	/* Check use imports first */` |
|   29370 |  4905 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   29370 |  4906 | `	if( pImport ){` |
|      12 |  4907 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      12 |  4908 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      12 |  4909 | `		return;` |
|       - |  4910 | `	}` |
|       - |  4911 | `	/* Prepend current namespace if active */` |
|   29360 |  4912 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4913 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4914 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4915 | `	}` |
|   29360 |  4916 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   14686 |  4917 |  |
|       - |  4918 | `/*` |
|       - |  4919 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4920 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4921 | ` * The caller must release pOut when done.` |
|       - |  4922 | ` */` |
|   50098 |  4923 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4924 |  |
|   50100 |  4925 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      54 |  4926 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      54 |  4927 | `		SyBlobAppend(pOut,"\\",1);` |
|      26 |  4928 | `	}` |
|   50100 |  4929 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   50100 |  4930 |  |
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
|   46398 |  5311 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5312 |  |
|       - |  5313 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5314 | `	SySet *pInstrContainer;` |
|       - |  5315 | `	sxi32 rc;` |
|       - |  5316 | `	/* Swap token stream */` |
|   46400 |  5317 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   46400 |  5318 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   46400 |  5319 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5320 | `	/* Compile the expression holding the argument value */` |
|   46400 |  5321 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5322 | `	/* Emit the done instruction */` |
|   46400 |  5323 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   46400 |  5324 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   46400 |  5325 | `	RE_SWAP_DELIMITER(pGen);` |
|   46400 |  5326 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5327 | `		return SXERR_ABORT;` |
|       - |  5328 | `	}` |
|   46400 |  5329 | `	return SXRET_OK;` |
|   23201 |  5330 |  |
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
|   55890 |  5368 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       2 |  5369 |  |
|       - |  5370 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5371 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5372 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5373 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5374 | `	sxi32 rc;` |
|       - |  5375 |  |
|   55892 |  5376 | `	pIn = pGen->pIn;` |
|   55892 |  5377 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5378 | `	/* Process arguments one after one */` |
|   70675 |  5379 | `	for(;;){` |
|  141352 |  5380 | `		if( pIn >= pEnd ){` |
|       - |  5381 | `			/* No more arguments to process */` |
|   55880 |  5382 | `			break;` |
|       - |  5383 | `		}` |
|   85474 |  5384 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   85474 |  5385 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   85474 |  5386 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   85474 |  5387 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5388 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|   85474 |  5389 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   52294 |  5390 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   52294 |  5391 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      42 |  5392 | `				if( !bCtorCtx ){` |
|       5 |  5393 | `					if( bAbstractCtx ){` |
|       3 |  5394 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5395 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5396 | `					}else{` |
|       3 |  5397 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5398 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5399 | `					}` |
|       5 |  5400 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5401 | `						return SXERR_ABORT;` |
|       - |  5402 | `					}` |
|       5 |  5403 | `					return SXERR_SYNTAX;` |
|       - |  5404 | `				}` |
|      38 |  5405 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      38 |  5406 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5407 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      37 |  5408 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5409 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5410 | `				}else{` |
|      34 |  5411 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5412 | `				}` |
|      38 |  5413 | `				pIn++;` |
|      18 |  5414 | `			}` |
|   26144 |  5415 | `		}` |
|       - |  5416 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  114538 |  5417 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   73265 |  5418 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   59603 |  5419 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   58124 |  5420 | `			sxu32 nLineLocal = pIn->nLine;` |
|   58124 |  5421 | `			sxi32 iTFlags = 0;` |
|   58124 |  5422 | `			pGen->pIn = pIn;` |
|   58124 |  5423 | `			rc = GenStateParseUnionTypeDecl(` |
|   29061 |  5424 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   29061 |  5425 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5426 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5427 | `				/* bAllowVoid */ 0,` |
|   29061 |  5428 | `						nLineLocal);` |
|   58124 |  5429 | `			pIn = pGen->pIn;` |
|   58124 |  5430 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5431 | `				return SXERR_ABORT;` |
|   58124 |  5432 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5433 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5434 | `				return SXERR_SYNTAX;` |
|   58122 |  5435 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5436 | `				if( pIn < pEnd ){` |
|       7 |  5437 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5438 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5439 | `						&pIn->sData);` |
|       3 |  5440 | `				}else{` |
|     ! 0 |  5441 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5442 | `						"syntax error, unexpected end of file");` |
|       - |  5443 | `				}` |
|       5 |  5444 | `				return SXERR_SYNTAX;` |
|       - |  5445 | `			}` |
|   58118 |  5446 | `			sArg.iFlags \|= iTFlags;` |
|   29058 |  5447 | `		}` |
|   85464 |  5448 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5449 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5450 | `			return rc;` |
|       - |  5451 | `		}` |
|   85464 |  5452 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5453 | `			/* Pass by reference,record that */` |
|    2926 |  5454 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2926 |  5455 | `			pIn++;` |
|    1462 |  5456 | `		}` |
|   85464 |  5457 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5458 | `			/* Variadic parameter: ...$args */` |
|      40 |  5459 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      40 |  5460 | `			pIn++;` |
|      19 |  5461 | `		}` |
|   85464 |  5462 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5463 | `			/* Invalid argument */` |
|     ! 0 |  5464 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5465 | `			return rc;` |
|       - |  5466 | `		}` |
|   85464 |  5467 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5468 | `		/* Copy argument name */` |
|   85464 |  5469 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   85464 |  5470 | `		if( zDup == 0 ){` |
|     ! 0 |  5471 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5472 | `			return SXERR_ABORT;` |
|       - |  5473 | `		}` |
|   85464 |  5474 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   85464 |  5475 | `		pIn++;` |
|   85464 |  5476 | `		if( pIn < pEnd ){` |
|   52796 |  5477 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5478 | `				SyToken *pDefend;` |
|   46402 |  5479 | `				sxi32 iNest = 0;` |
|   46402 |  5480 | `				pIn++; /* Jump the equal sign */` |
|   46402 |  5481 | `				pDefend = pIn;` |
|       - |  5482 | `				/* Process the default value associated with this argument */` |
|   98596 |  5483 | `				while( pDefend < pEnd ){` |
|   75388 |  5484 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   23194 |  5485 | `						break;` |
|       - |  5486 | `					}` |
|   52196 |  5487 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5488 | `						/* Increment nesting level */` |
|    2900 |  5489 | `						iNest++;` |
|   50747 |  5490 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5491 | `						/* Decrement nesting level */` |
|    2900 |  5492 | `						iNest--;` |
|    1449 |  5493 | `					}` |
|   52196 |  5494 | `					pDefend++;` |
|       2 |  5495 | `				}` |
|   46402 |  5496 | `				if( pIn >= pDefend ){` |
|       3 |  5497 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5498 | `					return rc;` |
|       - |  5499 | `				}` |
|       - |  5500 | `				/* Process default value */` |
|   46400 |  5501 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   46400 |  5502 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5503 | `					return rc;` |
|       - |  5504 | `				}` |
|       - |  5505 | `				/* Point beyond the default value */` |
|   46400 |  5506 | `				pIn = pDefend;` |
|   23199 |  5507 | `			}` |
|   52794 |  5508 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5509 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5510 | `				return rc;` |
|       - |  5511 | `			}` |
|   52794 |  5512 | `			pIn++; /* Jump the trailing comma */` |
|   26396 |  5513 | `		}` |
|       - |  5514 | `		/* Append argument signature */` |
|   85462 |  5515 | `		if( sArg.nType > 0 ){` |
|   58078 |  5516 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5517 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    5814 |  5518 | `				int marker = 'o';` |
|    5814 |  5519 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    5814 |  5520 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    2908 |  5521 | `			}else{` |
|       - |  5522 | `				int c;` |
|   52266 |  5523 | `				c = 'n'; /* cc warning */` |
|       - |  5524 | `				/* Type leading character */` |
|   52266 |  5525 | `				switch(sArg.nType){` |
|     ! 0 |  5526 | `				case MEMOBJ_HASHMAP:` |
|       - |  5527 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5528 | `					c = 'h';` |
|     ! 0 |  5529 | `					break;` |
|    7273 |  5530 | `				case MEMOBJ_INT:` |
|       - |  5531 | `					/* Integer */` |
|   14548 |  5532 | `					c = 'i';` |
|   14548 |  5533 | `					break;` |
|     ! 0 |  5534 | `				case MEMOBJ_BOOL:` |
|       - |  5535 | `					/* Bool */` |
|     ! 0 |  5536 | `					c = 'b';` |
|     ! 0 |  5537 | `					break;` |
|     ! 0 |  5538 | `				case MEMOBJ_REAL:` |
|       - |  5539 | `					/* Float */` |
|     ! 0 |  5540 | `					c = 'f';` |
|     ! 0 |  5541 | `					break;` |
|   18852 |  5542 | `				case MEMOBJ_STRING:` |
|       - |  5543 | `					/* String */` |
|   37706 |  5544 | `					c = 's';` |
|   37706 |  5545 | `					break;` |
|       7 |  5546 | `				case MEMOBJ_OBJ:` |
|       - |  5547 | `					/* Object */` |
|      16 |  5548 | `					c = 'o';` |
|      14 |  5549 | `					break;` |
|     ! 0 |  5550 | `				default:` |
|     ! 0 |  5551 | `					break;` |
|       - |  5552 | `				}` |
|   52266 |  5553 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5554 | `			}` |
|   29040 |  5555 | `		}else{` |
|       - |  5556 | `			/* No type is associated with this parameter which mean` |
|       - |  5557 | `			 * that this function is not condidate for overloading.` |
|       - |  5558 | `			 */` |
|   27386 |  5559 | `			SyBlobRelease(&sSig);` |
|       - |  5560 | `		}` |
|       - |  5561 | `		/* Save in the argument set */` |
|   85462 |  5562 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5563 | `	}` |
|   55880 |  5564 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5565 | `		/* Save function signature */` |
|   34874 |  5566 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   17436 |  5567 | `	}` |
|   55880 |  5568 | `	return SXRET_OK;` |
|   27947 |  5569 |  |
|       - |  5570 | `/*` |
|       - |  5571 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5572 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5573 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5574 | ` */` |
|  154926 |  5575 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5576 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5577 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5578 | `	)` |
|       2 |  5579 |  |
|       - |  5580 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5581 | `	GenBlock *pBlock;` |
|       - |  5582 | `	sxu32 nGotoOfft;` |
|       - |  5583 | `	sxi32 rc;` |
|       - |  5584 | `	/* Attach the new function */` |
|  154928 |  5585 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  154928 |  5586 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5587 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5588 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5589 | `		return SXERR_ABORT;` |
|       - |  5590 | `	}` |
|  154928 |  5591 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5592 | `	/* Swap bytecode containers */` |
|  154928 |  5593 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  154928 |  5594 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5595 | `	/* Emit constructor property promotion prologue:` |
|       - |  5596 | `	 *   $this->NAME = $NAME;` |
|       - |  5597 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5598 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5599 | `	{` |
|  154928 |  5600 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5601 | `		sxu32 i;` |
|  237414 |  5602 | `		for( i = 0; i < nArg; i++ ){` |
|   82488 |  5603 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5604 | `			char *zSrc;` |
|       - |  5605 | `			sxu32 nSrc,nName;` |
|       - |  5606 | `			SySet sToken;` |
|       - |  5607 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5608 | `			sxi32 rcPromote;` |
|   82488 |  5609 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   82460 |  5610 | `				continue;` |
|       - |  5611 | `			}` |
|       - |  5612 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5613 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5614 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5615 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5616 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      30 |  5617 | `			nName = SyStringLength(&pArg->sName);` |
|      30 |  5618 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      30 |  5619 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      30 |  5620 | `			if( zSrc == 0 ){` |
|     ! 0 |  5621 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5622 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5623 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5624 | `				return SXERR_ABORT;` |
|       - |  5625 | `			}` |
|       - |  5626 | `			{` |
|      30 |  5627 | `				char *z = zSrc;` |
|      30 |  5628 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      30 |  5629 | `				z += sizeof("$this->")-1;` |
|      30 |  5630 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5631 | `				z += nName;` |
|      30 |  5632 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      30 |  5633 | `				z += sizeof(" = $")-1;` |
|      30 |  5634 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5635 | `				z += nName;` |
|      30 |  5636 | `				*z = 0;` |
|       - |  5637 | `			}` |
|      30 |  5638 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      30 |  5639 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      30 |  5640 | `			pTmpIn = pGen->pIn;` |
|      30 |  5641 | `			pTmpEnd = pGen->pEnd;` |
|      30 |  5642 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      30 |  5643 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      30 |  5644 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  5645 | `			pGen->pIn = pTmpIn;` |
|      30 |  5646 | `			pGen->pEnd = pTmpEnd;` |
|      30 |  5647 | `			SySetRelease(&sToken);` |
|      30 |  5648 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5649 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5650 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5651 | `				return SXERR_ABORT;` |
|       - |  5652 | `			}` |
|       - |  5653 | `			/* Discard the assignment result — this is a statement expression. */` |
|      30 |  5654 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      16 |  5655 | `		}` |
|       - |  5656 | `	}` |
|       - |  5657 | `	/* Compile the body */` |
|  154928 |  5658 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5659 | `	/* Fix exception jumps now the destination is resolved */` |
|  154928 |  5660 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5661 | `	/* Emit the final return if not yet done */` |
|  154928 |  5662 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5663 | `	/* Fix gotos jumps now the destination is resolved */` |
|  154928 |  5664 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5665 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5666 | `	}` |
|  154928 |  5667 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5668 | `	/* Restore the default container */` |
|  154928 |  5669 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5670 | `	/* Leave function block */` |
|  154928 |  5671 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  154928 |  5672 | `	if( rc == SXERR_ABORT ){` |
|       - |  5673 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5674 | `		return SXERR_ABORT;` |
|       - |  5675 | `	}` |
|       - |  5676 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5677 | `	{` |
|  154928 |  5678 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5679 | `		sxu32 i;` |
| 3215286 |  5680 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3060378 |  5681 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5682 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5683 | `				break;` |
|       - |  5684 | `			}` |
| 1530181 |  5685 | `		}` |
|       - |  5686 | `	}` |
|       - |  5687 | `	/* All done, function body compiled */` |
|  154928 |  5688 | `	return SXRET_OK;` |
|   77465 |  5689 |  |
|       - |  5690 | `/*` |
|       - |  5691 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5692 | ` * According to the PHP language reference manual.` |
|       - |  5693 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5694 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5695 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5696 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5697 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5698 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5699 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5700 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5701 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5702 | ` *` |
|       - |  5703 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5704 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5705 | ` * on these extension.` |
|       - |  5706 | ` */` |
|       - |  5707 | `/*` |
|       - |  5708 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5709 | ` */` |
|      76 |  5710 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5711 |  |
|       - |  5712 | `	sxu32 i;` |
|     238 |  5713 | `	for( i = 0; i < n; i++ ){` |
|     212 |  5714 | `		int a = zA[i], b = zB[i];` |
|     212 |  5715 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     212 |  5716 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     212 |  5717 | `		if( a != b ) return a - b;` |
|      82 |  5718 | `	}` |
|      28 |  5719 | `	return 0;` |
|      40 |  5720 |  |
|       - |  5721 | `/*` |
|       - |  5722 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5723 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5724 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5725 | ` */` |
|       - |  5726 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5727 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5728 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5729 |  |
|       - |  5730 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5731 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5732 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5733 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5734 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5735 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5736 |  |
|       - |  5737 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5738 | `struct PhlTypeAtom {` |
|       - |  5739 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5740 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5741 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5742 | `	sxu32 nCanon;` |
|       - |  5743 | `};` |
|       - |  5744 |  |
|       - |  5745 | `/*` |
|       - |  5746 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5747 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5748 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5749 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5750 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5751 | ` * already be consumed by the caller.` |
|       - |  5752 | ` */` |
|   58410 |  5753 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5754 |  |
|   58412 |  5755 | `	SyToken *pIn = pGen->pIn;` |
|   58412 |  5756 | `	SyZero(pOut, sizeof(*pOut));` |
|   58412 |  5757 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   58412 |  5758 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5759 | `		return SXERR_SYNTAX;` |
|       - |  5760 | `	}` |
|       - |  5761 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   58412 |  5762 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5763 | `		pIn++;` |
|       8 |  5764 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5765 | `			return SXERR_SYNTAX;` |
|       - |  5766 | `		}` |
|       3 |  5767 | `	}` |
|   58412 |  5768 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5769 | `		return SXERR_SYNTAX;` |
|       - |  5770 | `	}` |
|   58412 |  5771 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   52542 |  5772 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   52542 |  5773 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5774 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   52535 |  5775 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|       8 |  5776 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   52525 |  5777 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   14682 |  5778 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   45182 |  5779 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   37792 |  5780 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   18947 |  5781 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      22 |  5782 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      42 |  5783 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5784 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5785 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5786 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5787 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5788 | `			pOut->sClass = pIn->sData;` |
|       4 |  5789 | `		}else{` |
|       3 |  5790 | `			return SXERR_SYNTAX;` |
|       - |  5791 | `		}` |
|   52540 |  5792 | `		pIn++;` |
|   26271 |  5793 | `	}else{` |
|       - |  5794 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5795 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    5872 |  5796 | `		SyString *pT = &pIn->sData;` |
|    5872 |  5797 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5798 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5799 | `			pIn++;` |
|    5867 |  5800 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      12 |  5801 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      12 |  5802 | `			pIn++;` |
|    5857 |  5803 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5804 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5805 | `			pIn++;` |
|       2 |  5806 | `		}else{` |
|       - |  5807 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    5850 |  5808 | `			SyToken *pFirst = pIn;` |
|    5850 |  5809 | `			SyToken *pLast = pIn;` |
|    5850 |  5810 | `			pOut->nType = SXU32_HIGH;` |
|    5850 |  5811 | `			pOut->sClass = pIn->sData;` |
|    5850 |  5812 | `			pIn++;` |
|    8775 |  5813 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    5853 |  5814 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  5815 | `				pLast = &pIn[1];` |
|       3 |  5816 | `				pIn += 2;` |
|       1 |  5817 | `			}` |
|    5850 |  5818 | `			if( pLast != pFirst ){` |
|       3 |  5819 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  5820 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  5821 | `				pOut->sClass.zString = zFirst;` |
|       3 |  5822 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  5823 | `			}` |
|       - |  5824 | `		}` |
|       - |  5825 | `	}` |
|   58410 |  5826 | `	pGen->pIn = pIn;` |
|   58410 |  5827 | `	return SXRET_OK;` |
|   29207 |  5828 |  |
|       - |  5829 |  |
|       - |  5830 | `/*` |
|       - |  5831 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  5832 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  5833 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  5834 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  5835 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  5836 | ` */` |
|   58314 |  5837 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  5838 |  |
|       - |  5839 | `	int i;` |
|   58316 |  5840 | `	int nNonNull = 0;` |
|  116710 |  5841 | `	for( i = 0; i < nAtoms; i++ ){` |
|   58396 |  5842 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   58386 |  5843 | `			nNonNull++;` |
|   29192 |  5844 | `		}` |
|   29199 |  5845 | `	}` |
|   58316 |  5846 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  5847 | `		/* Shorthand: ?T */` |
|      54 |  5848 | `		for( i = 0; i < nAtoms; i++ ){` |
|      54 |  5849 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      54 |  5850 | `			SyBlobAppend(pBlob, "?", 1);` |
|      54 |  5851 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  5852 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  5853 | `			}else{` |
|      44 |  5854 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  5855 | `			}` |
|      54 |  5856 | `			return;` |
|     ! 0 |  5857 | `		}` |
|     ! 0 |  5858 | `	}` |
|       - |  5859 | `	{` |
|   58264 |  5860 | `		int bFirst = 1;` |
|       - |  5861 | `		/* 1) Classes in declaration order */` |
|  116600 |  5862 | `		for( i = 0; i < nAtoms; i++ ){` |
|   58338 |  5863 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    5844 |  5864 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    5844 |  5865 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    5844 |  5866 | `				bFirst = 0;` |
|    2921 |  5867 | `			}` |
|   29170 |  5868 | `		}` |
|       - |  5869 | `		/* 2) Built-ins in canonical order */` |
|       - |  5870 | `		{` |
|       - |  5871 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  5872 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  5873 | `			int k;` |
|  407836 |  5874 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  647018 |  5875 | `				for( i = 0; i < nAtoms; i++ ){` |
|  349928 |  5876 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   52484 |  5877 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   52484 |  5878 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   52484 |  5879 | `						bFirst = 0;` |
|   52484 |  5880 | `						break;` |
|       - |  5881 | `					}` |
|  148724 |  5882 | `				}` |
|  174788 |  5883 | `			}` |
|       - |  5884 | `		}` |
|       - |  5885 | `		/* 3) null suffix */` |
|   58264 |  5886 | `		if( bNullable ){` |
|       6 |  5887 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  5888 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  5889 | `		}` |
|       - |  5890 | `	}` |
|   29159 |  5891 |  |
|       - |  5892 |  |
|       - |  5893 | `/*` |
|       - |  5894 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  5895 | ` *` |
|       - |  5896 | ` * Outputs:` |
|       - |  5897 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  5898 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  5899 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  5900 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  5901 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  5902 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  5903 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  5904 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  5905 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  5906 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  5907 | ` *` |
|       - |  5908 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  5909 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  5910 | ` */` |
|   58324 |  5911 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  5912 | `	ph7_gen_state *pGen,` |
|       - |  5913 | `	sxu32 *pnType,` |
|       - |  5914 | `	SyString *pClass,` |
|       - |  5915 | `	SySet *pAlts,` |
|       - |  5916 | `	sxi32 *piTypeFlags,` |
|       - |  5917 | `	SyString *pTypeText,` |
|       - |  5918 | `	int iNullableFlag,` |
|       - |  5919 | `	int iUnionFlag,` |
|       - |  5920 | `	int bAllowVoid,` |
|       - |  5921 | `	sxu32 nLine` |
|       2 |  5922 | `){` |
|       - |  5923 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   58326 |  5924 | `	int nAtoms = 0;` |
|   58326 |  5925 | `	int bShortNullable = 0;` |
|   58326 |  5926 | `	int bExplicitNull = 0;` |
|       - |  5927 | `	sxi32 rc;` |
|   58326 |  5928 | `	*pnType = 0;` |
|   58326 |  5929 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   58326 |  5930 | `	*piTypeFlags = 0;` |
|   58326 |  5931 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  5932 |  |
|   58326 |  5933 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5934 | `		return SXRET_OK;` |
|       - |  5935 | `	}` |
|       - |  5936 | ``	/* Optional `?` shorthand prefix */`` |
|   58324 |  5937 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      50 |  5938 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      50 |  5939 | `		bShortNullable = 1;` |
|      50 |  5940 | `		pGen->pIn++;` |
|      50 |  5941 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5942 | `			return SXERR_SYNTAX;` |
|       - |  5943 | `		}` |
|      24 |  5944 | `	}` |
|       - |  5945 | `	/* First atom is mandatory */` |
|   58326 |  5946 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   58326 |  5947 | `	if( rc != SXRET_OK ){` |
|       3 |  5948 | `		return rc;` |
|       - |  5949 | `	}` |
|   58324 |  5950 | `	nAtoms = 1;` |
|       - |  5951 | ``	/* Subsequent atoms separated by `\|` */`` |
|   87614 |  5952 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   58455 |  5953 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      90 |  5954 | `		if( bShortNullable ){` |
|       - |  5955 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  5956 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  5957 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  5958 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  5959 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  5960 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  5961 | `		}` |
|      88 |  5962 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  5963 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5964 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  5965 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  5966 | `		}` |
|      88 |  5967 | ``		pGen->pIn++; /* skip `\|` */`` |
|      88 |  5968 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      88 |  5969 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  5970 | `			return rc;` |
|       - |  5971 | `		}` |
|      88 |  5972 | `		nAtoms++;` |
|       2 |  5973 | `	}` |
|       - |  5974 | `	/* Validation pass.` |
|       - |  5975 | `	 *` |
|       - |  5976 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  5977 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  5978 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  5979 | `	 */` |
|       - |  5980 | `	{` |
|       - |  5981 | `		int i, j;` |
|   58322 |  5982 | `		int bHasNonNull = 0;` |
|  116722 |  5983 | `		for( i = 0; i < nAtoms; i++ ){` |
|   58408 |  5984 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      12 |  5985 | `				if( nAtoms > 1 ){` |
|       3 |  5986 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5987 | `						"Void can only be used as a standalone type");` |
|       3 |  5988 | `					return SXERR_SYNTAX;` |
|       - |  5989 | `				}` |
|      10 |  5990 | `				if( !bAllowVoid ){` |
|     ! 0 |  5991 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5992 | `						"void cannot be used here");` |
|     ! 0 |  5993 | `					return SXERR_SYNTAX;` |
|       - |  5994 | `				}` |
|      10 |  5995 | `				if( bShortNullable ){` |
|     ! 0 |  5996 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  5997 | `						"Void type cannot be nullable");` |
|     ! 0 |  5998 | `					return SXERR_SYNTAX;` |
|       - |  5999 | `				}` |
|       4 |  6000 | `			}` |
|   58406 |  6001 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6002 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6003 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6004 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6005 | `				 * does not return), and folding them would mislead any` |
|       - |  6006 | `				 * future return-enforcement work. */` |
|       3 |  6007 | `				if( nAtoms > 1 ){` |
|       3 |  6008 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6009 | `						"never can only be used as a standalone type");` |
|       3 |  6010 | `					return SXERR_SYNTAX;` |
|       - |  6011 | `				}` |
|     ! 0 |  6012 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6013 | `					"never type is not yet implemented");` |
|     ! 0 |  6014 | `				return SXERR_SYNTAX;` |
|       - |  6015 | `			}` |
|   58404 |  6016 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  6017 | `				bExplicitNull = 1;` |
|       7 |  6018 | `			}else{` |
|   58394 |  6019 | `				bHasNonNull = 1;` |
|       - |  6020 | `			}` |
|       - |  6021 | `			/* Duplicate detection */` |
|   58520 |  6022 | `			for( j = 0; j < i; j++ ){` |
|     120 |  6023 | `				int bDup = 0;` |
|     120 |  6024 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  6025 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6026 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6027 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6028 | `								aAtoms[j].sClass.zString,` |
|      12 |  6029 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6030 | `							bDup = 1;` |
|     ! 0 |  6031 | `						}` |
|       8 |  6032 | `					}else{` |
|       3 |  6033 | `						bDup = 1;` |
|       - |  6034 | `					}` |
|       7 |  6035 | `				}` |
|     120 |  6036 | `				if( bDup ){` |
|       - |  6037 | `					const char *zName;` |
|       - |  6038 | `					sxu32 nName;` |
|       3 |  6039 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6040 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6041 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6042 | `					}else{` |
|       3 |  6043 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6044 | `						nName = aAtoms[i].nCanon;` |
|       - |  6045 | `					}` |
|       4 |  6046 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6047 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6048 | `					return SXERR_SYNTAX;` |
|       - |  6049 | `				}` |
|      60 |  6050 | `			}` |
|   29202 |  6051 | `		}` |
|   58316 |  6052 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  6053 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6054 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  6055 | `			return SXERR_SYNTAX;` |
|       - |  6056 | `		}` |
|       - |  6057 | `	}` |
|       - |  6058 | `	/* Compute nullability flag */` |
|   58316 |  6059 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      58 |  6060 | `		*piTypeFlags \|= iNullableFlag;` |
|      28 |  6061 | `	}` |
|       - |  6062 | `	/* Build canonical type text */` |
|   58316 |  6063 | `	if( pTypeText ){` |
|       - |  6064 | `		SyBlob sBlob;` |
|   58316 |  6065 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   87450 |  6066 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   29157 |  6067 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   58316 |  6068 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   87461 |  6069 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   58306 |  6070 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   58308 |  6071 | `			if( zDup ){` |
|   58308 |  6072 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   29153 |  6073 | `			}` |
|   29153 |  6074 | `		}` |
|   58316 |  6075 | `		SyBlobRelease(&sBlob);` |
|   29157 |  6076 | `	}` |
|       - |  6077 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6078 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6079 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6080 | `	{` |
|   58316 |  6081 | `		int nNonNull = 0;` |
|   58316 |  6082 | `		int iNonNullIdx = -1;` |
|       - |  6083 | `		int i;` |
|  116710 |  6084 | `		for( i = 0; i < nAtoms; i++ ){` |
|   58396 |  6085 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   58386 |  6086 | `				nNonNull++;` |
|   58386 |  6087 | `				iNonNullIdx = i;` |
|   29192 |  6088 | `			}` |
|   29199 |  6089 | `		}` |
|   58316 |  6090 | `		if( nNonNull <= 1 ){` |
|       - |  6091 | `			/* Fast path: store as single type. */` |
|   58262 |  6092 | `			if( iNonNullIdx >= 0 ){` |
|   58262 |  6093 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   58262 |  6094 | `				if( pA->nType == SXU32_HIGH ){` |
|    8741 |  6095 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    2913 |  6096 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    5828 |  6097 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    5828 |  6098 | `					*pnType = SXU32_HIGH;` |
|    5828 |  6099 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   55349 |  6100 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      10 |  6101 | `					*pnType = MEMOBJ_VOID;` |
|       6 |  6102 | `				}else{` |
|       - |  6103 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6104 | `					 * pass above rejects it as not-yet-implemented. */` |
|   52428 |  6105 | `					*pnType = pA->nType;` |
|       - |  6106 | `				}` |
|   29130 |  6107 | `			}` |
|   29132 |  6108 | `		}else{` |
|       - |  6109 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      56 |  6110 | `			*piTypeFlags \|= iUnionFlag;` |
|     184 |  6111 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6112 | `				ph7_type_alt sAlt;` |
|     130 |  6113 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     126 |  6114 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     126 |  6115 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      41 |  6116 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      13 |  6117 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      28 |  6118 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      28 |  6119 | `					sAlt.nType = SXU32_HIGH;` |
|      28 |  6120 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      15 |  6121 | `				}else{` |
|     100 |  6122 | `					sAlt.nType = aAtoms[i].nType;` |
|     100 |  6123 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6124 | `				}` |
|     126 |  6125 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      64 |  6126 | `			}` |
|       - |  6127 | `		}` |
|       - |  6128 | `	}` |
|   58316 |  6129 | `	return SXRET_OK;` |
|   29164 |  6130 |  |
|       - |  6131 |  |
|       - |  6132 | `/*` |
|       - |  6133 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6134 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6135 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6136 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6137 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6138 | `` *          and union types `: T\|U`.`` |
|       - |  6139 | ` */` |
|  178252 |  6140 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6141 |  |
|  178254 |  6142 | `	sxi32 iFlags = 0;` |
|       - |  6143 | `	sxi32 rc;` |
|       - |  6144 | `	sxu32 nLine;` |
|  178254 |  6145 | `	pFunc->nReturnType = 0;` |
|  178254 |  6146 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  178254 |  6147 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  178254 |  6148 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  178164 |  6149 | `		return SXRET_OK;` |
|       - |  6150 | `	}` |
|      92 |  6151 | `	pGen->pIn++; /* Skip ':' */` |
|      92 |  6152 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6153 | `		return SXRET_OK;` |
|       - |  6154 | `	}` |
|      92 |  6155 | `	nLine = pGen->pIn->nLine;` |
|      92 |  6156 | `	rc = GenStateParseUnionTypeDecl(` |
|      45 |  6157 | `		pGen,` |
|      45 |  6158 | `		&pFunc->nReturnType,` |
|      45 |  6159 | `		&pFunc->sReturnClass,` |
|      45 |  6160 | `		&pFunc->aReturnUnion,` |
|       - |  6161 | `		&iFlags,` |
|      45 |  6162 | `		&pFunc->sReturnTypeName,` |
|       - |  6163 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6164 | `		/* iUnionFlag */ 0,` |
|       - |  6165 | `		/* bAllowVoid */ 1,` |
|      45 |  6166 | `		nLine);` |
|      45 |  6167 | `	(void)iFlags;` |
|      92 |  6168 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6169 | `		return SXERR_ABORT;` |
|       - |  6170 | `	}` |
|      92 |  6171 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6172 | `		/* Error already reported */` |
|     ! 0 |  6173 | `		return SXERR_SYNTAX;` |
|       - |  6174 | `	}` |
|      92 |  6175 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6176 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6177 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6178 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6179 | `				&pGen->pIn->sData);` |
|       3 |  6180 | `		}else{` |
|     ! 0 |  6181 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6182 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6183 | `		}` |
|       5 |  6184 | `		return SXERR_SYNTAX;` |
|       - |  6185 | `	}` |
|      88 |  6186 | `	return SXRET_OK;` |
|   89128 |  6187 |  |
|       - |  6188 |  |
|   38508 |  6189 | `static sxi32 GenStateCompileFunc(` |
|       - |  6190 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6191 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6192 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6193 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6194 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6195 | `	)` |
|       2 |  6196 |  |
|       - |  6197 | `	ph7_vm_func *pFunc;` |
|       - |  6198 | `	SyToken *pEnd;` |
|       - |  6199 | `	sxu32 nLine;` |
|       - |  6200 | `	char *zName;` |
|       - |  6201 | `	sxi32 rc;` |
|       - |  6202 | `	/* Extract line number */` |
|   38510 |  6203 | `	nLine = pGen->pIn->nLine;` |
|       - |  6204 | `	/* Jump the left parenthesis '(' */` |
|   38510 |  6205 | `	pGen->pIn++;` |
|       - |  6206 | `	/* Delimit the function signature */` |
|   38510 |  6207 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   38510 |  6208 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6209 | `		/* Syntax error */` |
|       7 |  6210 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6211 | `		if( rc == SXERR_ABORT ){` |
|       - |  6212 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6213 | `			return SXERR_ABORT;` |
|       - |  6214 | `		}` |
|       7 |  6215 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6216 | `		return SXRET_OK;` |
|       - |  6217 | `	}` |
|       - |  6218 | `	/* Create the function state */` |
|   38504 |  6219 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   38504 |  6220 | `	if( pFunc == 0 ){` |
|     ! 0 |  6221 | `		goto OutOfMem;` |
|       - |  6222 | `	}` |
|       - |  6223 | `	/* Build the function name, prepending namespace if active */` |
|   38511 |  6224 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6225 | `		SyBlob sFQN;` |
|       - |  6226 | `		sxu32 nLen;` |
|      16 |  6227 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6228 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6229 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6230 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6231 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6232 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6233 | `		SyBlobRelease(&sFQN);` |
|      16 |  6234 | `		if( zName == 0 ){` |
|     ! 0 |  6235 | `			goto OutOfMem;` |
|       - |  6236 | `		}` |
|      16 |  6237 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6238 | `	}else{` |
|   38490 |  6239 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   38490 |  6240 | `		if( zName == 0 ){` |
|     ! 0 |  6241 | `			goto OutOfMem;` |
|       - |  6242 | `		}` |
|   38490 |  6243 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6244 | `	}` |
|   38504 |  6245 | `	if( pGen->pIn < pEnd ){` |
|       - |  6246 | `		/* Collect function arguments */` |
|   26718 |  6247 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   26718 |  6248 | `		if( rc == SXERR_ABORT ){` |
|       - |  6249 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6250 | `			return SXERR_ABORT;` |
|       - |  6251 | `		}` |
|   13358 |  6252 | `	}` |
|       - |  6253 | `	/* Point past ')' and parse optional return type ': type' */` |
|   38504 |  6254 | `	pGen->pIn = &pEnd[1];` |
|       - |  6255 | `	{` |
|   38504 |  6256 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   38504 |  6257 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6258 | `			return SXERR_ABORT;` |
|   38504 |  6259 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6260 | `			return SXERR_SYNTAX;` |
|       - |  6261 | `		}` |
|       - |  6262 | `	}` |
|   38500 |  6263 | `	if( bHandleClosure ){` |
|       - |  6264 | `		ph7_vm_func_closure_env sEnv;` |
|     178 |  6265 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     176 |  6266 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      97 |  6267 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 |  6268 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6269 | `				/* Closure,record environment variable */` |
|      16 |  6270 | `				pGen->pIn++;` |
|      16 |  6271 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6272 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6273 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6274 | `						return SXERR_ABORT;` |
|       - |  6275 | `					}` |
|     ! 0 |  6276 | `				}` |
|      16 |  6277 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6278 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 |  6279 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 |  6280 | `					int iFlagsLocal = 0;` |
|      34 |  6281 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 |  6282 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 |  6283 | `						break;` |
|       - |  6284 | `					}` |
|      20 |  6285 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 |  6286 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6287 | `						/* Pass by reference,record that */` |
|     ! 0 |  6288 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6289 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6290 | `							);` |
|     ! 0 |  6291 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6292 | `						pGen->pIn++;` |
|     ! 0 |  6293 | `					}` |
|      18 |  6294 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 |  6295 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6296 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6297 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6298 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6299 | `								return SXERR_ABORT;` |
|       - |  6300 | `							}` |
|       - |  6301 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6302 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6303 | `								pGen->pIn++;` |
|     ! 0 |  6304 | `							}` |
|     ! 0 |  6305 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6306 | `								pGen->pIn++;` |
|     ! 0 |  6307 | `							}` |
|     ! 0 |  6308 | `							break;` |
|       - |  6309 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6310 | `					}else{` |
|       - |  6311 | `						SyString *pNameLocal;` |
|       - |  6312 | `						char *zDup;` |
|       - |  6313 | `						/* Duplicate variable name */` |
|      20 |  6314 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 |  6315 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 |  6316 | `						if( zDup ){` |
|       - |  6317 | `							/* Zero the structure */` |
|      20 |  6318 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 |  6319 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 |  6320 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 |  6321 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 |  6322 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6323 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6324 | `									got_this = 1;` |
|     ! 0 |  6325 | `							}` |
|       - |  6326 | `							/* Save imported variable */` |
|      20 |  6327 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 |  6328 | `						}else{` |
|     ! 0 |  6329 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6330 | `							 return SXERR_ABORT;` |
|       - |  6331 | `						}` |
|       - |  6332 | `					}` |
|      20 |  6333 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 |  6334 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6335 | `						/* Ignore trailing commas */` |
|       7 |  6336 | `						pGen->pIn++;` |
|       1 |  6337 | `					}` |
|       2 |  6338 | `				}` |
|      16 |  6339 | `				if( !got_this ){` |
|       - |  6340 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6341 | `					 * available to the closure environment.` |
|       - |  6342 | `					 */` |
|      16 |  6343 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 |  6344 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 |  6345 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 |  6346 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 |  6347 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 |  6348 | `				}` |
|      16 |  6349 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6350 | `					/* Mark as closure */` |
|      16 |  6351 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 |  6352 | `				}` |
|       7 |  6353 | `		}` |
|      88 |  6354 | `	}` |
|       - |  6355 | `	/* Compile the body */` |
|   38500 |  6356 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   38500 |  6357 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6358 | `		return SXERR_ABORT;` |
|       - |  6359 | `	}` |
|   38500 |  6360 | `	if( ppFunc ){` |
|     178 |  6361 | `		*ppFunc = pFunc;` |
|      88 |  6362 | `	}` |
|   38500 |  6363 | `	rc = SXRET_OK;` |
|   38500 |  6364 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6365 | `		/* Finally register the function */` |
|   38486 |  6366 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   19242 |  6367 | `	}` |
|   38500 |  6368 | `	if( rc == SXRET_OK ){` |
|   38500 |  6369 | `		return SXRET_OK;` |
|       - |  6370 | `	}` |
|       - |  6371 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6372 | `OutOfMem:` |
|       - |  6373 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6374 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6375 | `	 */` |
|     ! 0 |  6376 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6377 | `	return SXERR_ABORT;` |
|   19256 |  6378 |  |
|       - |  6379 | `/*` |
|       - |  6380 | ` * Compile a standard PHP function.` |
|       - |  6381 | ` *  Refer to the block-comment above for more information.` |
|       - |  6382 | ` */` |
|   38338 |  6383 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6384 |  |
|       - |  6385 | `	SyString *pName;` |
|       - |  6386 | `	sxi32 iFlags;` |
|       - |  6387 | `	sxu32 nLine;` |
|       - |  6388 | `	sxi32 rc;` |
|       - |  6389 |  |
|   38340 |  6390 | `	nLine = pGen->pIn->nLine;` |
|   38340 |  6391 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   38340 |  6392 | `	iFlags = 0;` |
|   38340 |  6393 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6394 | `		/* Return by reference,remember that */` |
|       7 |  6395 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6396 | `		/* Jump the '&' token */` |
|       7 |  6397 | `		pGen->pIn++;` |
|       3 |  6398 | `	}` |
|   38340 |  6399 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6400 | `		/* Invalid function name */` |
|       5 |  6401 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6402 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6403 | `			return SXERR_ABORT;` |
|       - |  6404 | `		}` |
|       - |  6405 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6406 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6407 | `			pGen->pIn++;` |
|       1 |  6408 | `		}` |
|       5 |  6409 | `		return SXRET_OK;` |
|       - |  6410 | `	}` |
|   38336 |  6411 | `	pName = &pGen->pIn->sData;` |
|   38336 |  6412 | `	nLine = pGen->pIn->nLine;` |
|       - |  6413 | `	/* Jump the function name */` |
|   38336 |  6414 | `	pGen->pIn++;` |
|   38336 |  6415 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6416 | `		/* Syntax error */` |
|       3 |  6417 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6418 | `		if( rc == SXERR_ABORT ){` |
|       - |  6419 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6420 | `			return SXERR_ABORT;` |
|       - |  6421 | `		}` |
|       - |  6422 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6423 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6424 | `			pGen->pIn++;` |
|     ! 0 |  6425 | `		}` |
|       3 |  6426 | `		return SXRET_OK;` |
|       - |  6427 | `	}` |
|       - |  6428 | `	/* Compile function body */` |
|   38334 |  6429 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   38334 |  6430 | `	return rc;` |
|   19171 |  6431 |  |
|       - |  6432 | `/*` |
|       - |  6433 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6434 | ` * According to the PHP language reference manual` |
|       - |  6435 | ` *  Visibility:` |
|       - |  6436 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6437 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6438 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6439 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6440 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6441 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6442 | ` */` |
|  177800 |  6443 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6444 |  |
|  177802 |  6445 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8764 |  6446 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  169040 |  6447 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   20330 |  6448 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6449 | `	}` |
|       - |  6450 | `	/* Assume public by default */` |
|  148712 |  6451 | `	return PH7_CLASS_PROT_PUBLIC;` |
|   88902 |  6452 |  |
|       - |  6453 | `/*` |
|       - |  6454 | ` * Compile a class constant.` |
|       - |  6455 | ` * According to the PHP language reference manual` |
|       - |  6456 | ` *  Class Constants` |
|       - |  6457 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6458 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6459 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6460 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6461 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6462 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6463 | ` * Symisc eXtension.` |
|       - |  6464 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6465 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6466 | ` *  Example:` |
|       - |  6467 | ` *   class Test{` |
|       - |  6468 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6469 | ` *   };` |
|       - |  6470 | ` *   var_dump(TEST::MyConst);` |
|       - |  6471 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6472 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6473 | ` */` |
|      30 |  6474 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6475 |  |
|      32 |  6476 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6477 | `	SySet *pInstrContainer;` |
|       - |  6478 | `	ph7_class_attr *pCons;` |
|       - |  6479 | `	SyString *pName;` |
|       - |  6480 | `	sxi32 rc;` |
|       - |  6481 | `	/* Extract visibility level */` |
|      32 |  6482 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6483 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6484 | `loop:` |
|       - |  6485 | `	/* Mark as constant */` |
|      32 |  6486 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6487 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6488 | `		/* Invalid constant name */` |
|     ! 0 |  6489 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6490 | `		if( rc == SXERR_ABORT ){` |
|       - |  6491 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6492 | `			return SXERR_ABORT;` |
|       - |  6493 | `		}` |
|     ! 0 |  6494 | `		goto Synchronize;` |
|       - |  6495 | `	}` |
|       - |  6496 | `	/* Peek constant name */` |
|      32 |  6497 | `	pName = &pGen->pIn->sData;` |
|       - |  6498 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6499 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6500 | `		/* Reserved constant name */` |
|     ! 0 |  6501 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6502 | `		if( rc == SXERR_ABORT ){` |
|       - |  6503 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6504 | `			return SXERR_ABORT;` |
|       - |  6505 | `		}` |
|     ! 0 |  6506 | `		goto Synchronize;` |
|       - |  6507 | `	}` |
|       - |  6508 | `	/* Advance the stream cursor */` |
|      32 |  6509 | `	pGen->pIn++;` |
|      32 |  6510 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6511 | `		/* Invalid declaration */` |
|     ! 0 |  6512 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6513 | `		if( rc == SXERR_ABORT ){` |
|       - |  6514 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6515 | `			return SXERR_ABORT;` |
|       - |  6516 | `		}` |
|     ! 0 |  6517 | `		goto Synchronize;` |
|       - |  6518 | `	}` |
|      32 |  6519 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6520 | `	/* Allocate a new class attribute */` |
|      32 |  6521 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6522 | `	if( pCons == 0 ){` |
|     ! 0 |  6523 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6524 | `		return SXERR_ABORT;` |
|       - |  6525 | `	}` |
|       - |  6526 | `	/* Swap bytecode container */` |
|      32 |  6527 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6528 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6529 | `	/* Compile constant value.` |
|       - |  6530 | `	 */` |
|      32 |  6531 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6532 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6533 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6534 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6535 | `			return SXERR_ABORT;` |
|       - |  6536 | `		}` |
|       1 |  6537 | `	}` |
|       - |  6538 | `	/* Emit the done instruction */` |
|      32 |  6539 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6540 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6541 | `	if( rc == SXERR_ABORT ){` |
|       - |  6542 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6543 | `		return SXERR_ABORT;` |
|       - |  6544 | `	}` |
|       - |  6545 | `	/* All done,install the constant */` |
|      32 |  6546 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6547 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6548 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6549 | `		return SXERR_ABORT;` |
|       - |  6550 | `	}` |
|      32 |  6551 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6552 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6553 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6554 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6555 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6556 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6557 | `				pTok--;` |
|     ! 0 |  6558 | `			}` |
|     ! 0 |  6559 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6560 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6561 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6562 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6563 | `				return SXERR_ABORT;` |
|       - |  6564 | `			}` |
|     ! 0 |  6565 | `		}else{` |
|     ! 0 |  6566 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6567 | `				goto loop;` |
|       - |  6568 | `			}` |
|       - |  6569 | `		}` |
|     ! 0 |  6570 | `	}` |
|      32 |  6571 | `	return SXRET_OK;` |
|     ! 0 |  6572 | `Synchronize:` |
|       - |  6573 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6574 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6575 | `		pGen->pIn++;` |
|     ! 0 |  6576 | `	}` |
|     ! 0 |  6577 | `	return SXERR_CORRUPT;` |
|      17 |  6578 |  |
|       - |  6579 | `/*` |
|       - |  6580 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6581 | ` * According to the PHP language reference manual` |
|       - |  6582 | ` *  Properties` |
|       - |  6583 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6584 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6585 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6586 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6587 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6588 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6589 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6590 | ` * Symisc eXtension.` |
|       - |  6591 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6592 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6593 | ` *  Example:` |
|       - |  6594 | ` *   class Test{` |
|       - |  6595 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6596 | ` *   };` |
|       - |  6597 | ` *   var_dump(TEST::myVar);` |
|       - |  6598 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6599 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6600 | ` */` |
|       - |  6601 | `/*` |
|       - |  6602 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6603 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6604 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6605 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6606 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6607 | ` */` |
|  116514 |  6608 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6609 |  |
|  116516 |  6610 | `	SyToken *p = pStart;` |
|  116516 |  6611 | `	if( p >= pEnd ) return 0;` |
|  116516 |  6612 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6613 | `		p++;` |
|      16 |  6614 | `		if( p >= pEnd ) return 0;` |
|       7 |  6615 | `	}` |
|  116516 |  6616 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6617 | `		p++;` |
|       3 |  6618 | `		if( p >= pEnd ) return 0;` |
|       1 |  6619 | `	}` |
|  116516 |  6620 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6621 | `		return 0;` |
|       - |  6622 | `	}` |
|       - |  6623 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6624 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6625 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6626 | `	 * dispatch site. */` |
|  116516 |  6627 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  116502 |  6628 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  116549 |  6629 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3047 |  6630 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  116404 |  6631 | `			return 0;` |
|       - |  6632 | `		}` |
|      49 |  6633 | `	}` |
|     114 |  6634 | `	p++;` |
|       - |  6635 | `	/* Consume optional namespace path */` |
|     116 |  6636 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6637 | `		p += 2;` |
|       1 |  6638 | `	}` |
|       - |  6639 | ``	/* Consume any `\| Type` union alternatives */`` |
|     186 |  6640 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      76 |  6641 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6642 | `		p++;` |
|      14 |  6643 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6644 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6645 | `		p++;` |
|      14 |  6646 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6647 | `			p += 2;` |
|     ! 0 |  6648 | `		}` |
|       2 |  6649 | `	}` |
|     114 |  6650 | `	if( p >= pEnd ) return 0;` |
|     114 |  6651 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   58259 |  6652 |  |
|       - |  6653 |  |
|       - |  6654 | `/*` |
|       - |  6655 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6656 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6657 | ` * if not). Recognized forms:` |
|       - |  6658 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6659 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6660 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6661 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6662 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6663 | ` * on unrecoverable error.` |
|       - |  6664 | ` *` |
|       - |  6665 | ` * When a type is parsed:` |
|       - |  6666 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6667 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6668 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6669 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6670 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6671 | ` */` |
|     112 |  6672 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6673 | `	ph7_gen_state *pGen,` |
|       - |  6674 | `	sxu32 *pnType,` |
|       - |  6675 | `	SyString *pClass,` |
|       - |  6676 | `	sxi32 *piTypeFlags,` |
|       - |  6677 | `	SyString *pTypeText,` |
|       - |  6678 | `	SySet *pAlts` |
|       2 |  6679 | `){` |
|     114 |  6680 | `	sxi32 iFlags = 0;` |
|       - |  6681 | `	sxi32 rc;` |
|     114 |  6682 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6683 | `		return SXRET_OK;` |
|       - |  6684 | `	}` |
|       - |  6685 | `	/* If the first token is '$', there's no type */` |
|     114 |  6686 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6687 | `		return SXRET_OK;` |
|       - |  6688 | `	}` |
|     114 |  6689 | `	rc = GenStateParseUnionTypeDecl(` |
|      56 |  6690 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6691 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6692 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6693 | `		/* bAllowVoid */ 0,` |
|     112 |  6694 | `		pGen->pIn->nLine);` |
|     114 |  6695 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6696 | `		return rc;` |
|       - |  6697 | `	}` |
|       - |  6698 | `	/* Verify next token is '$' (start of property name) */` |
|     114 |  6699 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6700 | `		return SXERR_SYNTAX;` |
|       - |  6701 | `	}` |
|     114 |  6702 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     114 |  6703 | `	return SXRET_OK;` |
|      58 |  6704 |  |
|       - |  6705 |  |
|       - |  6706 | `/*` |
|       - |  6707 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6708 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6709 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6710 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6711 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6712 | ` * by the type parser itself before reaching here.` |
|       - |  6713 | ` *` |
|       - |  6714 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6715 | ` * use in the error message.` |
|       - |  6716 | ` */` |
|     178 |  6717 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6718 | `	sxu32 nType,` |
|       - |  6719 | `	const SyString *pClass,` |
|       - |  6720 | `	const char **pzName,` |
|       - |  6721 | `	sxu32 *pnName)` |
|       2 |  6722 |  |
|       - |  6723 | `	const char *z;` |
|       - |  6724 | `	sxu32 n;` |
|     180 |  6725 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     154 |  6726 | `		return 0;` |
|       - |  6727 | `	}` |
|      28 |  6728 | `	z = pClass->zString;` |
|      28 |  6729 | `	n = pClass->nByte;` |
|      28 |  6730 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       5 |  6731 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6732 | `	}` |
|      24 |  6733 | `	if( n == 5 && SyMemcmpNoCase(z,"mixed",5) == 0 ){` |
|     ! 0 |  6734 | `		*pzName = "mixed"; *pnName = 5; return 1;` |
|       - |  6735 | `	}` |
|      24 |  6736 | `	if( n == 8 && SyMemcmpNoCase(z,"iterable",8) == 0 ){` |
|     ! 0 |  6737 | `		*pzName = "iterable"; *pnName = 8; return 1;` |
|       - |  6738 | `	}` |
|      24 |  6739 | `	return 0;` |
|      91 |  6740 |  |
|       - |  6741 |  |
|       - |  6742 | `/*` |
|       - |  6743 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6744 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6745 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6746 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6747 | `` * unions like `callable\|int`).`` |
|       - |  6748 | ` *` |
|       - |  6749 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6750 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6751 | ` */` |
|     150 |  6752 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6753 | `	ph7_gen_state *pGen,` |
|       - |  6754 | `	ph7_class *pClass,` |
|       - |  6755 | `	const SyString *pPropName,` |
|       - |  6756 | `	sxu32 nType,` |
|       - |  6757 | `	const SyString *pTypeClass,` |
|       - |  6758 | `	const SyString *pTypeText,` |
|       - |  6759 | `	SySet *pUnionAlts,` |
|       - |  6760 | `	sxu32 nLine)` |
|       2 |  6761 |  |
|     152 |  6762 | `	const char *zBad = 0;` |
|     152 |  6763 | `	sxu32 nBad = 0;` |
|       - |  6764 | `	SyString sFallback;` |
|       - |  6765 | `	const SyString *pBad;` |
|       - |  6766 | `	sxi32 rc;` |
|     152 |  6767 | `	int bDisallowed = 0;` |
|     152 |  6768 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6769 | `		bDisallowed = 1;` |
|     151 |  6770 | `	}else if( pUnionAlts ){` |
|       - |  6771 | `		sxu32 i;` |
|      42 |  6772 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      30 |  6773 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      30 |  6774 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6775 | `				bDisallowed = 1;` |
|       3 |  6776 | `				break;` |
|       - |  6777 | `			}` |
|      15 |  6778 | `		}` |
|       7 |  6779 | `	}` |
|     152 |  6780 | `	if( !bDisallowed ){` |
|     148 |  6781 | `		return SXRET_OK;` |
|       - |  6782 | `	}` |
|       - |  6783 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6784 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6785 | `	 * canonical spelling if the type text is unavailable. */` |
|       5 |  6786 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       5 |  6787 | `		pBad = pTypeText;` |
|       3 |  6788 | `	}else{` |
|     ! 0 |  6789 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6790 | `		pBad = &sFallback;` |
|       - |  6791 | `	}` |
|       7 |  6792 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6793 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6794 | `		&pClass->sName,pPropName,pBad);` |
|       5 |  6795 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6796 | `		return SXERR_ABORT;` |
|       - |  6797 | `	}` |
|       5 |  6798 | `	return SXERR_SYNTAX;` |
|      77 |  6799 |  |
|       - |  6800 |  |
|   38098 |  6801 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6802 |  |
|   38100 |  6803 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6804 | `	ph7_class_attr *pAttr;` |
|       - |  6805 | `	SyString *pName;` |
|       - |  6806 | `	sxi32 rc;` |
|   38100 |  6807 | `	sxu32 nType = 0;` |
|       - |  6808 | `	SyString sTypeClass;` |
|       - |  6809 | `	SyString sTypeText;` |
|       - |  6810 | `	SySet aUnionAlts;` |
|   38100 |  6811 | `	sxi32 iTypeFlags = 0;` |
|   38100 |  6812 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   38100 |  6813 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   38100 |  6814 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6815 | `	/* Extract visibility level */` |
|   38100 |  6816 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6817 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   38156 |  6818 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     114 |  6819 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     114 |  6820 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6821 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6822 | `			goto Synchronize;` |
|     114 |  6823 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  6824 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6825 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  6826 | `				&pGen->pIn->sData);` |
|     ! 0 |  6827 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6828 | `				return SXERR_ABORT;` |
|       - |  6829 | `			}` |
|     ! 0 |  6830 | `			goto Synchronize;` |
|     114 |  6831 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6832 | `			return SXERR_ABORT;` |
|       - |  6833 | `		}` |
|      56 |  6834 | `	}` |
|     ! 0 |  6835 | `loop:` |
|   38104 |  6836 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6837 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  6838 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6839 | `			return SXERR_ABORT;` |
|       - |  6840 | `		}` |
|     ! 0 |  6841 | `		goto Synchronize;` |
|       - |  6842 | `	}` |
|   38104 |  6843 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   38104 |  6844 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  6845 | `		/* Invalid attribute name */` |
|     ! 0 |  6846 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  6847 | `		if( rc == SXERR_ABORT ){` |
|       - |  6848 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6849 | `			return SXERR_ABORT;` |
|       - |  6850 | `		}` |
|     ! 0 |  6851 | `		goto Synchronize;` |
|       - |  6852 | `	}` |
|       - |  6853 | `	/* Peek attribute name */` |
|   38104 |  6854 | `	pName = &pGen->pIn->sData;` |
|       - |  6855 | `	/* Advance the stream cursor */` |
|   38104 |  6856 | `	pGen->pIn++;` |
|   38104 |  6857 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  6858 | `		/* Invalid declaration */` |
|       3 |  6859 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  6860 | `		if( rc == SXERR_ABORT ){` |
|       - |  6861 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6862 | `			return SXERR_ABORT;` |
|       - |  6863 | `		}` |
|       3 |  6864 | `		goto Synchronize;` |
|       - |  6865 | `	}` |
|       - |  6866 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  6867 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  6868 | `	 * by the type parser. */` |
|   38102 |  6869 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     176 |  6870 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  6871 | `			&sTypeText,` |
|     116 |  6872 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     118 |  6873 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6874 | `			return SXERR_ABORT;` |
|     118 |  6875 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  6876 | `			goto Synchronize;` |
|       - |  6877 | `		}` |
|      58 |  6878 | `	}` |
|       - |  6879 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   38102 |  6880 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  6881 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  6882 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  6883 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6884 | `			return SXERR_ABORT;` |
|       - |  6885 | `		}` |
|       3 |  6886 | `		goto Synchronize;` |
|       - |  6887 | `	}` |
|       - |  6888 | `	/* Allocate a new class attribute */` |
|   38100 |  6889 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   38100 |  6890 | `	if( pAttr == 0 ){` |
|     ! 0 |  6891 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  6892 | `		return SXERR_ABORT;` |
|       - |  6893 | `	}` |
|   38100 |  6894 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     116 |  6895 | `		pAttr->nType = nType;` |
|     116 |  6896 | `		pAttr->sClass = sTypeClass;` |
|     116 |  6897 | `		pAttr->sTypeName = sTypeText;` |
|     116 |  6898 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  6899 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  6900 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  6901 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  6902 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  6903 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  6904 | `			sxu32 i;` |
|      32 |  6905 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  6906 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  6907 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  6908 | `			}` |
|       5 |  6909 | `		}` |
|      57 |  6910 | `	}` |
|   38100 |  6911 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  6912 | `		SySet *pInstrContainer;` |
|   11920 |  6913 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  6914 | `		/* Swap bytecode container */` |
|   11920 |  6915 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   11920 |  6916 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  6917 | `		/* Compile attribute value.` |
|       - |  6918 | `		 */` |
|   11920 |  6919 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   11920 |  6920 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  6921 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  6922 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6923 | `				return SXERR_ABORT;` |
|       - |  6924 | `			}` |
|     ! 0 |  6925 | `		}` |
|       - |  6926 | `		/* Emit the done instruction */` |
|   11920 |  6927 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   11920 |  6928 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5959 |  6929 | `	}` |
|       - |  6930 | `	/* All done,install the attribute */` |
|   38100 |  6931 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   38100 |  6932 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6933 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6934 | `		return SXERR_ABORT;` |
|       - |  6935 | `	}` |
|   38100 |  6936 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6937 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  6938 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  6939 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  6940 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6941 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6942 | `				pTok--;` |
|     ! 0 |  6943 | `			}` |
|     ! 0 |  6944 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6945 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  6946 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6947 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6948 | `				return SXERR_ABORT;` |
|       - |  6949 | `			}` |
|     ! 0 |  6950 | `		}else{` |
|       5 |  6951 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  6952 | `				goto loop;` |
|       - |  6953 | `			}` |
|       - |  6954 | `		}` |
|     ! 0 |  6955 | `	}` |
|   38096 |  6956 | `	SySetRelease(&aUnionAlts);` |
|   38096 |  6957 | `	return SXRET_OK;` |
|       2 |  6958 | `Synchronize:` |
|       - |  6959 | `	/* Synchronize with the first semi-colon */` |
|      11 |  6960 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  6961 | `		pGen->pIn++;` |
|       1 |  6962 | `	}` |
|       5 |  6963 | `	SySetRelease(&aUnionAlts);` |
|       5 |  6964 | `	return SXERR_CORRUPT;` |
|   19051 |  6965 |  |
|       - |  6966 | `/*` |
|       - |  6967 | ` * Compile a class method.` |
|       - |  6968 | ` *` |
|       - |  6969 | ` * Refer to the official documentation for more information` |
|       - |  6970 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  6971 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  6972 | ` * overloading and many more.` |
|       - |  6973 | ` */` |
|  139672 |  6974 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  6975 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6976 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  6977 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  6978 | `	int doBody,          /* TRUE to process method body */` |
|       - |  6979 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  6980 | `	)` |
|       2 |  6981 |  |
|  139674 |  6982 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6983 | `	ph7_class_method *pMeth;` |
|       - |  6984 | `	sxi32 iFuncFlags;` |
|       - |  6985 | `	SyString *pName;` |
|       - |  6986 | `	SyToken *pEnd;` |
|       - |  6987 | `	sxi32 rc;` |
|       - |  6988 | `	/* Extract visibility level */` |
|  139674 |  6989 | `	iProtection = GetProtectionLevel(iProtection);` |
|  139674 |  6990 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  139674 |  6991 | `	iFuncFlags = 0;` |
|  139674 |  6992 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  6993 | `		/* Invalid method name */` |
|     ! 0 |  6994 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  6995 | `		if( rc == SXERR_ABORT ){` |
|       - |  6996 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6997 | `			return SXERR_ABORT;` |
|       - |  6998 | `		}` |
|     ! 0 |  6999 | `		goto Synchronize;` |
|       - |  7000 | `	}` |
|  139674 |  7001 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7002 | `		/* Return by reference,remember that */` |
|     ! 0 |  7003 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7004 | `		/* Jump the '&' token */` |
|     ! 0 |  7005 | `		pGen->pIn++;` |
|     ! 0 |  7006 | `	}` |
|  139674 |  7007 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7008 | `		/* Invalid method name */` |
|     ! 0 |  7009 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7010 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7011 | `			return SXERR_ABORT;` |
|       - |  7012 | `		}` |
|     ! 0 |  7013 | `		goto Synchronize;` |
|       - |  7014 | `	}` |
|       - |  7015 | `	/* Peek method name */` |
|  139674 |  7016 | `	pName = &pGen->pIn->sData;` |
|  139674 |  7017 | `	nLine = pGen->pIn->nLine;` |
|       - |  7018 | `	/* Jump the method name */` |
|  139674 |  7019 | `	pGen->pIn++;` |
|  139674 |  7020 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7021 | `		/* Abstract method */` |
|   23236 |  7022 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7023 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7024 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7025 | `				&pClass->sName,pName);` |
|     ! 0 |  7026 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7027 | `				return SXERR_ABORT;` |
|       - |  7028 | `			}` |
|     ! 0 |  7029 | `		}` |
|       - |  7030 | `		/* Assemble method signature only */` |
|   23236 |  7031 | `		doBody = FALSE;` |
|   11617 |  7032 | `	}` |
|  139674 |  7033 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7034 | `		/* Syntax error */` |
|     ! 0 |  7035 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7036 | `		if( rc == SXERR_ABORT ){` |
|       - |  7037 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7038 | `			return SXERR_ABORT;` |
|       - |  7039 | `		}` |
|     ! 0 |  7040 | `		goto Synchronize;` |
|       - |  7041 | `	}` |
|       - |  7042 | `	/* Allocate a new class_method instance */` |
|  139674 |  7043 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  139674 |  7044 | `	if( pMeth == 0 ){` |
|     ! 0 |  7045 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7046 | `		return SXERR_ABORT;` |
|       - |  7047 | `	}` |
|       - |  7048 | `	/* Jump the left parenthesis '(' */` |
|  139674 |  7049 | `	pGen->pIn++;` |
|  139674 |  7050 | `	pEnd = 0; /* cc warning */` |
|       - |  7051 | `	/* Delimit the method signature */` |
|  139674 |  7052 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  139674 |  7053 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7054 | `		/* Syntax error */` |
|       3 |  7055 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7056 | `		if( rc == SXERR_ABORT ){` |
|       - |  7057 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7058 | `			return SXERR_ABORT;` |
|       - |  7059 | `		}` |
|       3 |  7060 | `		goto Synchronize;` |
|       - |  7061 | `	}` |
|       - |  7062 | `	{` |
|  139672 |  7063 | `		int bIsCtor = 0;` |
|  139672 |  7064 | `		int bAbstractCtor = 0;` |
|  202215 |  7065 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|   84383 |  7066 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  132382 |  7067 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   14582 |  7068 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7069 | `				bAbstractCtor = 1;` |
|       2 |  7070 | `			}else{` |
|   14580 |  7071 | `				bIsCtor = 1;` |
|       - |  7072 | `			}` |
|    7290 |  7073 | `		}` |
|  139672 |  7074 | `		if( pGen->pIn < pEnd ){` |
|       - |  7075 | `			/* Collect method arguments */` |
|   29126 |  7076 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   29126 |  7077 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7078 | `				return SXERR_ABORT;` |
|       - |  7079 | `			}` |
|   14562 |  7080 | `		}` |
|       - |  7081 | `	}` |
|       - |  7082 | `	/* Point past ')' and parse optional return type ': type' */` |
|  139672 |  7083 | `	pGen->pIn = &pEnd[1];` |
|       - |  7084 | `	{` |
|  139672 |  7085 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  139672 |  7086 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7087 | `			return SXERR_ABORT;` |
|  139672 |  7088 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7089 | `			goto Synchronize;` |
|       - |  7090 | `		}` |
|       - |  7091 | `	}` |
|       - |  7092 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7093 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7094 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7095 | `	{` |
|  139672 |  7096 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7097 | `		sxu32 i;` |
|  189116 |  7098 | `		for( i = 0; i < nArg; i++ ){` |
|   49454 |  7099 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7100 | `			ph7_class_attr *pAttr;` |
|   49454 |  7101 | `			sxi32 iAttrFlags = 0;` |
|   49454 |  7102 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   49418 |  7103 | `				continue;` |
|       - |  7104 | `			}` |
|      38 |  7105 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7106 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7107 | `					"Cannot declare variadic promoted property");` |
|       3 |  7108 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7109 | `					return SXERR_ABORT;` |
|       - |  7110 | `				}` |
|       3 |  7111 | `				goto Synchronize;` |
|       - |  7112 | `			}` |
|       - |  7113 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7114 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7115 | `			 * appear as an alternative of a union type. */` |
|      34 |  7116 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       6 |  7117 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      53 |  7118 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7119 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7120 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7121 | `					nLine);` |
|      36 |  7122 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7123 | `					return SXERR_ABORT;` |
|      36 |  7124 | `				}else if( rc != SXRET_OK ){` |
|       5 |  7125 | `					goto Synchronize;` |
|       - |  7126 | `				}` |
|      15 |  7127 | `			}` |
|       - |  7128 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      32 |  7129 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7130 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7131 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7132 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7133 | `					return SXERR_ABORT;` |
|       - |  7134 | `				}` |
|       3 |  7135 | `				goto Synchronize;` |
|       - |  7136 | `			}` |
|      30 |  7137 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      28 |  7138 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7139 | `			}` |
|      30 |  7140 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7141 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7142 | `			}` |
|      30 |  7143 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7144 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7145 | `			}` |
|      30 |  7146 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      30 |  7147 | `			if( pAttr == 0 ){` |
|     ! 0 |  7148 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7149 | `				return SXERR_ABORT;` |
|       - |  7150 | `			}` |
|      30 |  7151 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      28 |  7152 | `				pAttr->nType = pArg->nType;` |
|      28 |  7153 | `				pAttr->sClass = pArg->sClass;` |
|      28 |  7154 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      28 |  7155 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7156 | `					sxu32 k;` |
|     ! 0 |  7157 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7158 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7159 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7160 | `					}` |
|     ! 0 |  7161 | `				}` |
|      13 |  7162 | `			}` |
|      30 |  7163 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      30 |  7164 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7165 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7166 | `				return SXERR_ABORT;` |
|       - |  7167 | `			}` |
|      16 |  7168 | `		}` |
|       - |  7169 | `	}` |
|  139664 |  7170 | `	if( doBody ){` |
|       - |  7171 | `		/* Compile method body */` |
|  116430 |  7172 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  116430 |  7173 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7174 | `			return SXERR_ABORT;` |
|       - |  7175 | `		}` |
|   58216 |  7176 | `	}else{` |
|       - |  7177 | `		/* Only method signature is allowed */` |
|   23236 |  7178 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7179 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7180 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7181 | `				if( rc == SXERR_ABORT ){` |
|       - |  7182 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7183 | `					return SXERR_ABORT;` |
|       - |  7184 | `				}` |
|     ! 0 |  7185 | `				return SXERR_CORRUPT;` |
|       - |  7186 | `			}` |
|       - |  7187 | `	}` |
|       - |  7188 | `	/* All done,install the method */` |
|  139664 |  7189 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  139664 |  7190 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7191 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7192 | `		return SXERR_ABORT;` |
|       - |  7193 | `	}` |
|  139664 |  7194 | `	return SXRET_OK;` |
|       5 |  7195 | `Synchronize:` |
|       - |  7196 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7197 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      21 |  7198 | `		pGen->pIn++;` |
|       1 |  7199 | `	}` |
|      11 |  7200 | `	return SXERR_CORRUPT;` |
|   69838 |  7201 |  |
|       - |  7202 | `/*` |
|       - |  7203 | ` * Compile an object interface.` |
|       - |  7204 | ` *  According to the PHP language reference manual` |
|       - |  7205 | ` *   Object Interfaces:` |
|       - |  7206 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7207 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7208 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7209 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7210 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7211 | ` */` |
|    8732 |  7212 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  7213 |  |
|    8734 |  7214 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7215 | `	ph7_class *pClass,*pBase;` |
|       - |  7216 | `	SyToken *pEnd,*pTmp;` |
|       - |  7217 | `	SyString *pName;` |
|       - |  7218 | `	sxi32 nKwrd;` |
|       - |  7219 | `	sxi32 rc;` |
|       - |  7220 | `	/* Jump the 'interface' keyword */` |
|    8734 |  7221 | `	pGen->pIn++;` |
|       - |  7222 | `	/* Extract interface name */` |
|    8734 |  7223 | `	pName = &pGen->pIn->sData;` |
|       - |  7224 | `	/* Advance the stream cursor */` |
|    8734 |  7225 | `	pGen->pIn++;` |
|       - |  7226 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7227 | `		SyBlob sFQN;` |
|       - |  7228 | `		SyString sFQNStr;` |
|    8734 |  7229 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|    8734 |  7230 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|    8734 |  7231 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|    8734 |  7232 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|    8734 |  7233 | `		SyBlobRelease(&sFQN);` |
|       - |  7234 | `	}` |
|    8734 |  7235 | `	if( pClass == 0 ){` |
|     ! 0 |  7236 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7237 | `		return SXERR_ABORT;` |
|       - |  7238 | `	}` |
|       - |  7239 | `	/* Mark as an interface */` |
|    8734 |  7240 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7241 | `	/* Assume no base class is given */` |
|    8734 |  7242 | `	pBase = 0;` |
|    8734 |  7243 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  7244 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  7245 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7246 | `			SyString *pBaseName;` |
|       - |  7247 | `			/* Extract base interface */` |
|       3 |  7248 | `			pGen->pIn++;` |
|       3 |  7249 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7250 | `				/* Syntax error */` |
|     ! 0 |  7251 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7252 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7253 | `					pName);` |
|     ! 0 |  7254 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7255 | `				if( rc == SXERR_ABORT ){` |
|       - |  7256 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7257 | `					return SXERR_ABORT;` |
|       - |  7258 | `				}` |
|     ! 0 |  7259 | `				return SXRET_OK;` |
|       - |  7260 | `			}` |
|       3 |  7261 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  7262 | `			{` |
|       - |  7263 | `				SyBlob sResolved;` |
|       3 |  7264 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       3 |  7265 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|       4 |  7266 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|       2 |  7267 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       3 |  7268 | `				SyBlobRelease(&sResolved);` |
|       - |  7269 | `			}` |
|       - |  7270 | `			/* Only interfaces is allowed */` |
|       3 |  7271 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7272 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7273 | `			}` |
|       3 |  7274 | `			if( pBase == 0 ){` |
|       - |  7275 | `				/* Inexistant interface */` |
|     ! 0 |  7276 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pBaseName);` |
|     ! 0 |  7277 | `				if( rc == SXERR_ABORT ){` |
|       - |  7278 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7279 | `					return SXERR_ABORT;` |
|       - |  7280 | `				}` |
|     ! 0 |  7281 | `			}` |
|       - |  7282 | `			/* Advance the stream cursor */` |
|       3 |  7283 | `			pGen->pIn++;` |
|       1 |  7284 | `		}` |
|       1 |  7285 | `	}` |
|    8734 |  7286 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7287 | `		/* Syntax error */` |
|     ! 0 |  7288 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7289 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7290 | `		if( rc == SXERR_ABORT ){` |
|       - |  7291 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7292 | `			return SXERR_ABORT;` |
|       - |  7293 | `		}` |
|     ! 0 |  7294 | `		return SXRET_OK;` |
|       - |  7295 | `	}` |
|    8734 |  7296 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|    8734 |  7297 | `	pEnd = 0; /* cc warning */` |
|       - |  7298 | `	/* Delimit the interface body */` |
|    8734 |  7299 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|    8734 |  7300 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7301 | `		/* Syntax error */` |
|     ! 0 |  7302 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7303 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7304 | `		if( rc == SXERR_ABORT ){` |
|       - |  7305 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7306 | `			return SXERR_ABORT;` |
|       - |  7307 | `		}` |
|     ! 0 |  7308 | `		return SXRET_OK;` |
|       - |  7309 | `	}` |
|       - |  7310 | `	/* Swap token stream */` |
|    8734 |  7311 | `	pTmp = pGen->pEnd;` |
|    8734 |  7312 | `	pGen->pEnd = pEnd;` |
|       - |  7313 | `	/* Start the parse process` |
|       - |  7314 | `	 * Note (According to the PHP reference manual):` |
|       - |  7315 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7316 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7317 | `	 */` |
|   15977 |  7318 | `	for(;;){` |
|       - |  7319 | `		/* Jump leading/trailing semi-colons */` |
|   55178 |  7320 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   23224 |  7321 | `			pGen->pIn++;` |
|       2 |  7322 | `		}` |
|   31956 |  7323 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7324 | `			/* End of interface body */` |
|    8732 |  7325 | `			break;` |
|       - |  7326 | `		}` |
|   23226 |  7327 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7328 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7329 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7330 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7331 | `			if( rc == SXERR_ABORT ){` |
|       - |  7332 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7333 | `				return SXERR_ABORT;` |
|       - |  7334 | `			}` |
|     ! 0 |  7335 | `			goto done;` |
|       - |  7336 | `		}` |
|       - |  7337 | `		/* Extract the current keyword */` |
|   23226 |  7338 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   23226 |  7339 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7340 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7341 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7342 | `			const char *zKind = "member";` |
|       3 |  7343 | `			SyString *pMemberName = 0;` |
|       3 |  7344 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7345 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7346 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7347 | `					zKind = "constant";` |
|       3 |  7348 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7349 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7350 | `					}` |
|       1 |  7351 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7352 | `					zKind = "method";` |
|     ! 0 |  7353 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7354 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7355 | `					}` |
|     ! 0 |  7356 | `				}` |
|       1 |  7357 | `			}` |
|       3 |  7358 | `			if( pMemberName ){` |
|       4 |  7359 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7360 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7361 | `			}else{` |
|     ! 0 |  7362 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7363 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7364 | `			}` |
|       3 |  7365 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7366 | `				return SXERR_ABORT;` |
|       - |  7367 | `			}` |
|       3 |  7368 | `			goto done;` |
|       - |  7369 | `		}` |
|   23224 |  7370 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7371 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7372 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7373 | `			if( rc == SXERR_ABORT ){` |
|       - |  7374 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7375 | `				return SXERR_ABORT;` |
|       - |  7376 | `			}` |
|     ! 0 |  7377 | `			goto done;` |
|       - |  7378 | `		}` |
|   23224 |  7379 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7380 | `			/* Advance the stream cursor */` |
|   23220 |  7381 | `			pGen->pIn++;` |
|   23220 |  7382 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7383 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7384 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7385 | `				if( rc == SXERR_ABORT ){` |
|       - |  7386 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7387 | `					return SXERR_ABORT;` |
|       - |  7388 | `				}` |
|     ! 0 |  7389 | `				goto done;` |
|       - |  7390 | `			}` |
|   23220 |  7391 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   23220 |  7392 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7393 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7394 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7395 | `				if( rc == SXERR_ABORT ){` |
|       - |  7396 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7397 | `					return SXERR_ABORT;` |
|       - |  7398 | `				}` |
|     ! 0 |  7399 | `				goto done;` |
|       - |  7400 | `			}` |
|   11609 |  7401 | `		}` |
|   23224 |  7402 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7403 | `			/* Parse constant */` |
|       3 |  7404 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7405 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7406 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7407 | `					return SXERR_ABORT;` |
|       - |  7408 | `				}` |
|     ! 0 |  7409 | `				goto done;` |
|       - |  7410 | `			}` |
|       2 |  7411 | `		}else{` |
|   23222 |  7412 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   23222 |  7413 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7414 | `				/* Static method,record that */` |
|     ! 0 |  7415 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7416 | `				/* Advance the stream cursor */` |
|     ! 0 |  7417 | `				pGen->pIn++;` |
|     ! 0 |  7418 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  7419 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7420 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7421 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7422 | `						if( rc == SXERR_ABORT ){` |
|       - |  7423 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7424 | `							return SXERR_ABORT;` |
|       - |  7425 | `						}` |
|     ! 0 |  7426 | `						goto done;` |
|       - |  7427 | `				}` |
|     ! 0 |  7428 | `			}` |
|       - |  7429 | `			/* Process method signature (no body for interface methods) */` |
|   23222 |  7430 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   23222 |  7431 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7432 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7433 | `					return SXERR_ABORT;` |
|       - |  7434 | `				}` |
|     ! 0 |  7435 | `				goto done;` |
|       - |  7436 | `			}` |
|       - |  7437 | `		}` |
|       2 |  7438 | `	}` |
|       - |  7439 | `	/* Install the interface */` |
|    8732 |  7440 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|    8732 |  7441 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7442 | `		/* Inherit from the base interface */` |
|       3 |  7443 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       1 |  7444 | `	}` |
|    8732 |  7445 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7446 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7447 | `		return SXERR_ABORT;` |
|       - |  7448 | `	}` |
|    4365 |  7449 | `done:` |
|       - |  7450 | `	/* Point beyond the interface body */` |
|    8734 |  7451 | `	pGen->pIn  = &pEnd[1];` |
|    8734 |  7452 | `	pGen->pEnd = pTmp;` |
|    8734 |  7453 | `	return PH7_OK;` |
|    4368 |  7454 |  |
|       - |  7455 | `/*` |
|       - |  7456 | ` * Compile a user-defined class.` |
|       - |  7457 | ` * According to the PHP language reference manual` |
|       - |  7458 | ` *  class` |
|       - |  7459 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7460 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7461 | ` *  of the properties and methods belonging to the class.` |
|       - |  7462 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7463 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7464 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7465 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7466 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7467 | ` *  (called "methods").` |
|       - |  7468 | ` */` |
|       - |  7469 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7470 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7471 | `struct TraitUseEntry {` |
|       - |  7472 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7473 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7474 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7475 | `};` |
|       - |  7476 | `/*` |
|       - |  7477 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7478 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7479 | ` */` |
|   41284 |  7480 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7481 |  |
|       - |  7482 | `	ph7_class **apIface;` |
|       - |  7483 | `	sxu32 nIface,i;` |
|       - |  7484 | `	sxi32 rc;` |
|   41286 |  7485 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7486 | `		return SXRET_OK;` |
|       - |  7487 | `	}` |
|   41286 |  7488 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   41286 |  7489 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   44222 |  7490 | `	for(i = 0; i < nIface; i++){` |
|    2938 |  7491 | `		ph7_class *pIface = apIface[i];` |
|       - |  7492 | `		SyHashEntry *pEntry;` |
|    2938 |  7493 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   17506 |  7494 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   14570 |  7495 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7496 | `			ph7_class_method *pImplMeth;` |
|   14570 |  7497 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7498 | `			/* Find the implementing method in the class */` |
|   14570 |  7499 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   14570 |  7500 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7501 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7502 | `			}` |
|       - |  7503 | `			/* Check visibility: interface methods must be implemented as public */` |
|   14556 |  7504 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7505 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7506 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7507 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7508 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7509 | `					return SXERR_ABORT;` |
|       - |  7510 | `				}` |
|       1 |  7511 | `			}` |
|       - |  7512 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7513 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7514 | `			 */` |
|       - |  7515 | `			{` |
|   14556 |  7516 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   14556 |  7517 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   14556 |  7518 | `				int sigError = 0;` |
|   14556 |  7519 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7520 | `					sigError = 1;` |
|   14555 |  7521 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7522 | `					/* Extra parameters must all have default values */` |
|       5 |  7523 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7524 | `					sxu32 k;` |
|       7 |  7525 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7526 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7527 | `							sigError = 1;` |
|       3 |  7528 | `							break;` |
|       - |  7529 | `						}` |
|       2 |  7530 | `					}` |
|       2 |  7531 | `				}` |
|   14556 |  7532 | `				if( sigError ){` |
|       - |  7533 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7534 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7535 | `					sxu32 j;` |
|       5 |  7536 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7537 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7538 | `					/* Build implementing method signature */` |
|       5 |  7539 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7540 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7541 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7542 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7543 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7544 | `					}` |
|       - |  7545 | `					/* Build interface method signature */` |
|       5 |  7546 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7547 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7548 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7549 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7550 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7551 | `					}` |
|       7 |  7552 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7553 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7554 | `						&pClass->sName,pMName,` |
|       4 |  7555 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7556 | `						&pIface->sName,pMName,` |
|       4 |  7557 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7558 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7559 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7560 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7561 | `						return SXERR_ABORT;` |
|       - |  7562 | `					}` |
|       2 |  7563 | `				}` |
|       - |  7564 | `			}` |
|       2 |  7565 | `		}` |
|    1470 |  7566 | `	}` |
|   41286 |  7567 | `	return SXRET_OK;` |
|   20644 |  7568 |  |
|       - |  7569 | `/*` |
|       - |  7570 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7571 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7572 | ` */` |
|   41284 |  7573 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7574 |  |
|       - |  7575 | `	ph7_class_method *pMeth;` |
|       - |  7576 | `	SyHashEntry *pEntry;` |
|       - |  7577 | `	sxu32 nAbstract;` |
|       - |  7578 | `	SyBlob sMsg;` |
|       - |  7579 | `	sxi32 rc;` |
|       - |  7580 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   41286 |  7581 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      22 |  7582 | `		return SXRET_OK;` |
|       - |  7583 | `	}` |
|       - |  7584 | `	/* Count abstract methods */` |
|   41266 |  7585 | `	nAbstract = 0;` |
|   41266 |  7586 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  389962 |  7587 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  348698 |  7588 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  348698 |  7589 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7590 | `			nAbstract++;` |
|       8 |  7591 | `		}` |
|       2 |  7592 | `	}` |
|   41266 |  7593 | `	if( nAbstract == 0 ){` |
|   41252 |  7594 | `		return SXRET_OK;` |
|       - |  7595 | `	}` |
|       - |  7596 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7597 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7598 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7599 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7600 | `		&pClass->sName,nAbstract,` |
|       7 |  7601 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7602 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7603 | `	/* Second pass: list methods with origins */` |
|       - |  7604 | `	{` |
|      15 |  7605 | `		sxu32 nListed = 0;` |
|      15 |  7606 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7607 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7608 | `			ph7_class *pOrigin = 0;` |
|       - |  7609 | `			SyString *pMName;` |
|      19 |  7610 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7611 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7612 | `				continue;` |
|       - |  7613 | `			}` |
|      17 |  7614 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7615 | `			if( nListed > 0 ){` |
|       3 |  7616 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7617 | `			}` |
|       - |  7618 | `			/* Find the origin of this abstract method.` |
|       - |  7619 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7620 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7621 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7622 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7623 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7624 | `			 * class's namespace.` |
|       - |  7625 | `			 */` |
|       - |  7626 | `			{` |
|       - |  7627 | `				ph7_class **apIface;` |
|       - |  7628 | `				ph7_class **apTrait;` |
|       - |  7629 | `				ph7_class *pWalk;` |
|       - |  7630 | `				sxu32 i;` |
|       - |  7631 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7632 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7633 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7634 | `				 */` |
|      17 |  7635 | `				if( pClass->pBase ){` |
|       9 |  7636 | `					pWalk = pClass->pBase;` |
|      17 |  7637 | `					while( pWalk ){` |
|       - |  7638 | `						ph7_class_method *pParentMeth;` |
|      11 |  7639 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7640 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7641 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7642 | `							 * in this class's ancestor chain.` |
|       - |  7643 | `							 */` |
|      11 |  7644 | `							int fromIface = 0;` |
|      11 |  7645 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7646 | `							while( pAnc ){` |
|       - |  7647 | `								ph7_class **apPI;` |
|       - |  7648 | `								sxu32 j;` |
|      13 |  7649 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7650 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7651 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7652 | `										fromIface = 1;` |
|       9 |  7653 | `										break;` |
|       - |  7654 | `									}` |
|     ! 0 |  7655 | `								}` |
|      13 |  7656 | `								if( fromIface ) break;` |
|       5 |  7657 | `								pAnc = pAnc->pBase;` |
|       1 |  7658 | `							}` |
|      11 |  7659 | `							if( !fromIface ){` |
|       3 |  7660 | `								pOrigin = pWalk;` |
|       3 |  7661 | `								break;` |
|       - |  7662 | `							}` |
|       4 |  7663 | `						}` |
|       9 |  7664 | `						pWalk = pWalk->pBase;` |
|       1 |  7665 | `					}` |
|       4 |  7666 | `				}` |
|       - |  7667 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7668 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7669 | `				 */` |
|      17 |  7670 | `				if( !pOrigin ){` |
|      15 |  7671 | `					pWalk = pClass;` |
|      37 |  7672 | `					while( pWalk && !pOrigin ){` |
|      23 |  7673 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7674 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7675 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7676 | `							ph7_class *pDeepest = 0;` |
|      25 |  7677 | `							while( pIface ){` |
|      13 |  7678 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7679 | `									pDeepest = pIface;` |
|       6 |  7680 | `								}` |
|      13 |  7681 | `								pIface = pIface->pBase;` |
|       1 |  7682 | `							}` |
|      13 |  7683 | `							if( pDeepest ){` |
|      13 |  7684 | `								pOrigin = pDeepest;` |
|      13 |  7685 | `								break;` |
|       - |  7686 | `							}` |
|     ! 0 |  7687 | `						}` |
|      23 |  7688 | `						pWalk = pWalk->pBase;` |
|       1 |  7689 | `					}` |
|       7 |  7690 | `				}` |
|       - |  7691 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7692 | `				if( !pOrigin ){` |
|       3 |  7693 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7694 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7695 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7696 | `							pOrigin = pClass;` |
|       3 |  7697 | `							break;` |
|       - |  7698 | `						}` |
|     ! 0 |  7699 | `					}` |
|       1 |  7700 | `				}` |
|       - |  7701 | `			}` |
|      17 |  7702 | `			if( pOrigin ){` |
|      17 |  7703 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7704 | `			}else{` |
|       - |  7705 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7706 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7707 | `			}` |
|      17 |  7708 | `			nListed++;` |
|       1 |  7709 | `		}` |
|       - |  7710 | `	}` |
|      15 |  7711 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7712 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7713 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7714 | `	SyBlobRelease(&sMsg);` |
|      15 |  7715 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7716 | `		return SXERR_ABORT;` |
|       - |  7717 | `	}` |
|      15 |  7718 | `	return SXRET_OK;` |
|   20644 |  7719 |  |
|   41298 |  7720 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  7721 |  |
|   41300 |  7722 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7723 | `	ph7_class *pClass,*pBase;` |
|       - |  7724 | `	SyToken *pEnd,*pTmp;` |
|       - |  7725 | `	sxi32 iProtection;` |
|       - |  7726 | `	SySet aInterfaces;` |
|       - |  7727 | `	SySet aUseEntries;` |
|       - |  7728 | `	sxi32 iAttrflags;` |
|       - |  7729 | `	SyString *pName;` |
|       - |  7730 | `	sxi32 nKwrd;` |
|       - |  7731 | `	sxi32 rc;` |
|       - |  7732 | `	/* Jump the 'class' keyword */` |
|   41300 |  7733 | `	pGen->pIn++;` |
|   41300 |  7734 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7735 | `		/* Syntax error */` |
|     ! 0 |  7736 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  7737 | `		if( rc == SXERR_ABORT ){` |
|       - |  7738 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7739 | `			return SXERR_ABORT;` |
|       - |  7740 | `		}` |
|       - |  7741 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  7742 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  7743 | `			pGen->pIn++;` |
|     ! 0 |  7744 | `		}` |
|     ! 0 |  7745 | `		return SXRET_OK;` |
|       - |  7746 | `	}` |
|       - |  7747 | `	/* Extract class name */` |
|   41300 |  7748 | `	pName = &pGen->pIn->sData;` |
|       - |  7749 | `	/* Advance the stream cursor */` |
|   41300 |  7750 | `	pGen->pIn++;` |
|       - |  7751 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7752 | `		SyBlob sFQN;` |
|       - |  7753 | `		SyString sFQNStr;` |
|   41300 |  7754 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   41300 |  7755 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   41300 |  7756 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   41300 |  7757 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   41300 |  7758 | `		SyBlobRelease(&sFQN);` |
|       - |  7759 | `	}` |
|   41300 |  7760 | `	if( pClass == 0 ){` |
|     ! 0 |  7761 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7762 | `		return SXERR_ABORT;` |
|       - |  7763 | `	}` |
|       - |  7764 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   41300 |  7765 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   41300 |  7766 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  7767 | `	/* Assume a standalone class */` |
|   41300 |  7768 | `	pBase = 0;` |
|   41300 |  7769 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  7770 | `		SyString *pBaseName;` |
|   29138 |  7771 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   29138 |  7772 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|   26204 |  7773 | `			pGen->pIn++; /* Advance the stream cursor */` |
|   26204 |  7774 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7775 | `				/* Syntax error */` |
|     ! 0 |  7776 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7777 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|     ! 0 |  7778 | `					pName);` |
|     ! 0 |  7779 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7780 | `				if( rc == SXERR_ABORT ){` |
|       - |  7781 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7782 | `					return SXERR_ABORT;` |
|       - |  7783 | `				}` |
|     ! 0 |  7784 | `				return SXRET_OK;` |
|       - |  7785 | `			}` |
|       - |  7786 | `			/* Extract base class name and resolve through namespace/imports */` |
|   26204 |  7787 | `			pBaseName = &pGen->pIn->sData;` |
|       - |  7788 | `			{` |
|       - |  7789 | `				SyBlob sResolved;` |
|   26204 |  7790 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   26204 |  7791 | `				GenStateResolveName(pGen,pBaseName,&sResolved);` |
|   39305 |  7792 | `				pBase = PH7_VmExtractClass(pGen->pVm,` |
|   26202 |  7793 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   26204 |  7794 | `				SyBlobRelease(&sResolved);` |
|       - |  7795 | `			}` |
|       - |  7796 | `			/* Interfaces are not allowed */` |
|   26204 |  7797 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  7798 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7799 | `			}` |
|   26204 |  7800 | `			if( pBase == 0 ){` |
|       - |  7801 | `				/* Inexistant base class */` |
|     ! 0 |  7802 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base class '%z'",pBaseName);` |
|     ! 0 |  7803 | `				if( rc == SXERR_ABORT ){` |
|       - |  7804 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7805 | `					return SXERR_ABORT;` |
|       - |  7806 | `				}` |
|     ! 0 |  7807 | `			}else{` |
|   26204 |  7808 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  7809 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  7810 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  7811 | `					if( rc == SXERR_ABORT ){` |
|       - |  7812 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7813 | `						return SXERR_ABORT;` |
|       - |  7814 | `					}` |
|     ! 0 |  7815 | `				}` |
|       - |  7816 | `			}` |
|       - |  7817 | `			/* Advance the stream cursor */` |
|   26204 |  7818 | `			pGen->pIn++;` |
|   13101 |  7819 | `		}` |
|   29138 |  7820 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  7821 | `			ph7_class *pInterface;` |
|       - |  7822 | `			SyString *pIntName;` |
|       - |  7823 | `			/* Interface implementation */` |
|    2938 |  7824 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    1468 |  7825 | `			for(;;){` |
|    2938 |  7826 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7827 | `					/* Syntax error */` |
|     ! 0 |  7828 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7829 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  7830 | `						pName);` |
|     ! 0 |  7831 | `					if( rc == SXERR_ABORT ){` |
|       - |  7832 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7833 | `						return SXERR_ABORT;` |
|       - |  7834 | `					}` |
|     ! 0 |  7835 | `					break;` |
|       - |  7836 | `				}` |
|       - |  7837 | `				/* Extract interface name and resolve through namespace/imports */` |
|    2938 |  7838 | `				pIntName = &pGen->pIn->sData;` |
|       - |  7839 | `				{` |
|       - |  7840 | `					SyBlob sResolved;` |
|    2938 |  7841 | `					SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    2938 |  7842 | `					GenStateResolveName(pGen,pIntName,&sResolved);` |
|    5874 |  7843 | `					pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    2936 |  7844 | `						(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    2938 |  7845 | `					SyBlobRelease(&sResolved);` |
|       - |  7846 | `				}` |
|       - |  7847 | `				/* Only interfaces are allowed */` |
|    2938 |  7848 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7849 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  7850 | `				}` |
|    2938 |  7851 | `				if( pInterface == 0 ){` |
|       - |  7852 | `					/* Inexistant interface */` |
|     ! 0 |  7853 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Inexistant base interface '%z'",pIntName);` |
|     ! 0 |  7854 | `					if( rc == SXERR_ABORT ){` |
|       - |  7855 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7856 | `						return SXERR_ABORT;` |
|       - |  7857 | `					}` |
|     ! 0 |  7858 | `				}else{` |
|       - |  7859 | `					/* Register interface */` |
|    2938 |  7860 | `					SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  7861 | `				}` |
|       - |  7862 | `				/* Advance the stream cursor */` |
|    2938 |  7863 | `				pGen->pIn++;` |
|    2938 |  7864 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    1470 |  7865 | `					break;` |
|       - |  7866 | `				}` |
|     ! 0 |  7867 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  7868 | `			}` |
|    1468 |  7869 | `		}` |
|   14568 |  7870 | `	}` |
|   41300 |  7871 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7872 | `		/* Syntax error */` |
|     ! 0 |  7873 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  7874 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7875 | `		if( rc == SXERR_ABORT ){` |
|       - |  7876 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7877 | `			return SXERR_ABORT;` |
|       - |  7878 | `		}` |
|     ! 0 |  7879 | `		return SXRET_OK;` |
|       - |  7880 | `	}` |
|   41300 |  7881 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   41300 |  7882 | `	pEnd = 0; /* cc warning */` |
|       - |  7883 | `	/* Delimit the class body */` |
|   41300 |  7884 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   41300 |  7885 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7886 | `		/* Syntax error */` |
|     ! 0 |  7887 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  7888 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7889 | `		if( rc == SXERR_ABORT ){` |
|       - |  7890 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7891 | `			return SXERR_ABORT;` |
|       - |  7892 | `		}` |
|     ! 0 |  7893 | `		return SXRET_OK;` |
|       - |  7894 | `	}` |
|       - |  7895 | `	/* Swap token stream */` |
|   41300 |  7896 | `	pTmp = pGen->pEnd;` |
|   41300 |  7897 | `	pGen->pEnd = pEnd;` |
|       - |  7898 | `	/* Set the inherited flags */` |
|   41300 |  7899 | `	pClass->iFlags = iFlags;` |
|       - |  7900 | `	/* Start the parse process */` |
|   78860 |  7901 | `	for(;;){` |
|       - |  7902 | `		/* Jump leading/trailing semi-colons */` |
|  233986 |  7903 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   38152 |  7904 | `			pGen->pIn++;` |
|       2 |  7905 | `		}` |
|  195836 |  7906 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7907 | `			/* End of class body */` |
|   41286 |  7908 | `			break;` |
|       - |  7909 | `		}` |
|  154552 |  7910 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  7911 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7912 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7913 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7914 | `			if( rc == SXERR_ABORT ){` |
|       - |  7915 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7916 | `				return SXERR_ABORT;` |
|       - |  7917 | `			}` |
|     ! 0 |  7918 | `			goto done;` |
|       - |  7919 | `		}` |
|       - |  7920 | `		/* Assume public visibility */` |
|  154552 |  7921 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  154552 |  7922 | `		iAttrflags = 0;` |
|  154552 |  7923 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  7924 | `			/* Extract the current keyword */` |
|  154552 |  7925 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  154552 |  7926 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  7927 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  7928 | `				TraitUseEntry sUse;` |
|      44 |  7929 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  7930 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  7931 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  7932 | `				for(;;){` |
|       - |  7933 | `					ph7_class *pTrait;` |
|       - |  7934 | `					SyString *pTraitName;` |
|      52 |  7935 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  7936 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7937 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  7938 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7939 | `							return SXERR_ABORT;` |
|       - |  7940 | `						}` |
|     ! 0 |  7941 | `						break;` |
|       - |  7942 | `					}` |
|      52 |  7943 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  7944 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  7945 | `						SyBlob sResolved;` |
|      52 |  7946 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  7947 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  7948 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  7949 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  7950 | `						SyBlobRelease(&sResolved);` |
|       - |  7951 | `					}` |
|       - |  7952 | `					/* Only traits are allowed */` |
|      52 |  7953 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  7954 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  7955 | `					}` |
|      52 |  7956 | `					if( pTrait == 0 ){` |
|     ! 0 |  7957 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7958 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  7959 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  7960 | `							return SXERR_ABORT;` |
|       - |  7961 | `						}` |
|     ! 0 |  7962 | `					}else{` |
|      52 |  7963 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  7964 | `					}` |
|      52 |  7965 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  7966 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  7967 | `						break;` |
|       - |  7968 | `					}` |
|       9 |  7969 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  7970 | `				}` |
|       - |  7971 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  7972 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  7973 | `					SyToken *pBlock;` |
|       9 |  7974 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  7975 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  7976 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  7977 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  7978 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  7979 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  7980 | `					}else{` |
|     ! 0 |  7981 | `						pGen->pIn = pGen->pEnd;` |
|       - |  7982 | `					}` |
|       4 |  7983 | `				}` |
|      44 |  7984 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  7985 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  7986 | `				continue;` |
|       - |  7987 | `			}` |
|  154510 |  7988 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  151492 |  7989 | `				iProtection = nKwrd;` |
|  151492 |  7990 | `				pGen->pIn++; /* Jump the visibility token */` |
|  151490 |  7991 | `				if( pGen->pIn >= pGen->pEnd` |
|  151492 |  7992 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  7993 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7994 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7995 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  7996 | `					if( rc == SXERR_ABORT ){` |
|       - |  7997 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  7998 | `						return SXERR_ABORT;` |
|       - |  7999 | `					}` |
|     ! 0 |  8000 | `					goto done;` |
|       - |  8001 | `				}` |
|  151492 |  8002 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8003 | `					/* Attribute declaration (untyped) */` |
|   37966 |  8004 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   37966 |  8005 | `					if( rc != SXRET_OK ){` |
|       3 |  8006 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8007 | `							return SXERR_ABORT;` |
|       - |  8008 | `						}` |
|       3 |  8009 | `						goto done;` |
|       - |  8010 | `					}` |
|   37964 |  8011 | `					continue;` |
|       - |  8012 | `				}` |
|  113528 |  8013 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8014 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     104 |  8015 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     104 |  8016 | `					if( rc != SXRET_OK ){` |
|       3 |  8017 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8018 | `							return SXERR_ABORT;` |
|       - |  8019 | `						}` |
|       3 |  8020 | `						goto done;` |
|       - |  8021 | `					}` |
|     102 |  8022 | `					continue;` |
|       - |  8023 | `				}` |
|       - |  8024 | `				/* Extract the keyword */` |
|  113426 |  8025 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   56712 |  8026 | `			}` |
|  116444 |  8027 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8028 | `				/* Process constant declaration */` |
|      30 |  8029 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  8030 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8031 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8032 | `						return SXERR_ABORT;` |
|       - |  8033 | `					}` |
|     ! 0 |  8034 | `					goto done;` |
|       - |  8035 | `				}` |
|      16 |  8036 | `			}else{` |
|  116416 |  8037 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8038 | `					/* Static method or attribute,record that */` |
|    2936 |  8039 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2936 |  8040 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2936 |  8041 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8042 | `						/* Extract the keyword */` |
|    2932 |  8043 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2932 |  8044 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8045 | `							iProtection = nKwrd;` |
|     ! 0 |  8046 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8047 | `						}` |
|    1465 |  8048 | `					}` |
|    2934 |  8049 | `					if( pGen->pIn >= pGen->pEnd` |
|    2936 |  8050 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8051 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8052 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8053 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8054 | `						if( rc == SXERR_ABORT ){` |
|       - |  8055 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8056 | `							return SXERR_ABORT;` |
|       - |  8057 | `						}` |
|     ! 0 |  8058 | `						goto done;` |
|       - |  8059 | `					}` |
|    2936 |  8060 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8061 | `						/* Attribute declaration */` |
|       5 |  8062 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8063 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8064 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8065 | `								return SXERR_ABORT;` |
|       - |  8066 | `							}` |
|     ! 0 |  8067 | `							goto done;` |
|       - |  8068 | `						}` |
|       5 |  8069 | `						continue;` |
|       - |  8070 | `					}` |
|    2932 |  8071 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8072 | `						/* Typed static attribute declaration */` |
|       8 |  8073 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       8 |  8074 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8075 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8076 | `								return SXERR_ABORT;` |
|       - |  8077 | `							}` |
|     ! 0 |  8078 | `							goto done;` |
|       - |  8079 | `						}` |
|       8 |  8080 | `						continue;` |
|       - |  8081 | `					}` |
|       - |  8082 | `					/* Extract the keyword */` |
|    2926 |  8083 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  114944 |  8084 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8085 | `					/* Abstract method,record that */` |
|      12 |  8086 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8087 | `					/* Mark the whole class as abstract */` |
|      12 |  8088 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8089 | `					/* Advance the stream cursor */` |
|      12 |  8090 | `					pGen->pIn++;` |
|      12 |  8091 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8092 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8093 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8094 | `							iProtection = nKwrd;` |
|      10 |  8095 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8096 | `						}` |
|       5 |  8097 | `					}` |
|      12 |  8098 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8099 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8100 | `							/* Static method */` |
|     ! 0 |  8101 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8102 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8103 | `					}` |
|      12 |  8104 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8105 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8106 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8107 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8108 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8109 | `							if( rc == SXERR_ABORT ){` |
|       - |  8110 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8111 | `								return SXERR_ABORT;` |
|       - |  8112 | `							}` |
|     ! 0 |  8113 | `							goto done;` |
|       - |  8114 | `					}` |
|      12 |  8115 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  113477 |  8116 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8117 | `					/* final method ,record that */` |
|       5 |  8118 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  8119 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  8120 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8121 | `						/* Extract the keyword */` |
|       5 |  8122 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8123 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8124 | `							iProtection = nKwrd;` |
|       5 |  8125 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8126 | `						}` |
|       2 |  8127 | `					}` |
|       5 |  8128 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8129 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8130 | `							/* Static method */` |
|     ! 0 |  8131 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8132 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8133 | `					}` |
|       5 |  8134 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8135 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8136 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8137 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8138 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8139 | `							if( rc == SXERR_ABORT ){` |
|       - |  8140 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8141 | `								return SXERR_ABORT;` |
|       - |  8142 | `							}` |
|     ! 0 |  8143 | `							goto done;` |
|       - |  8144 | `					}` |
|       5 |  8145 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8146 | `				}` |
|  116406 |  8147 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8148 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8149 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8150 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8151 | `						if( rc == SXERR_ABORT ){` |
|       - |  8152 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8153 | `							return SXERR_ABORT;` |
|       - |  8154 | `						}` |
|     ! 0 |  8155 | `						goto done;` |
|       - |  8156 | `				}` |
|  116406 |  8157 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8158 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8159 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8160 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8161 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8162 | `						if( rc == SXERR_ABORT ){` |
|       - |  8163 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8164 | `							return SXERR_ABORT;` |
|       - |  8165 | `						}` |
|     ! 0 |  8166 | `						goto done;` |
|       - |  8167 | `					}` |
|       - |  8168 | `					/* Attribute declaration */` |
|       7 |  8169 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8170 | `				}else{` |
|       - |  8171 | `					/* Process method declaration */` |
|  116400 |  8172 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8173 | `				}` |
|  116406 |  8174 | `				if( rc != SXRET_OK ){` |
|      11 |  8175 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8176 | `						return SXERR_ABORT;` |
|       - |  8177 | `					}` |
|      11 |  8178 | `					goto done;` |
|       - |  8179 | `				}` |
|       - |  8180 | `			}` |
|   58213 |  8181 | `		}else{` |
|       - |  8182 | `			/* Attribute declaration */` |
|     ! 0 |  8183 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8184 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8185 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8186 | `					return SXERR_ABORT;` |
|       - |  8187 | `				}` |
|     ! 0 |  8188 | `				goto done;` |
|       - |  8189 | `			}` |
|       - |  8190 | `		}` |
|       2 |  8191 | `	}` |
|       - |  8192 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8193 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8194 | `	 */` |
|       - |  8195 | `	{` |
|       - |  8196 | `		TraitUseEntry *apUse;` |
|       - |  8197 | `		sxu32 nU;` |
|   41286 |  8198 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   41328 |  8199 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  8200 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  8201 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  8202 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  8203 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8204 | `			sxu32 nT;` |
|      44 |  8205 | `			if( !hasResolution ){` |
|       - |  8206 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  8207 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  8208 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  8209 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8210 | `						break;` |
|       - |  8211 | `					}` |
|      22 |  8212 | `				}` |
|      19 |  8213 | `			}else{` |
|       - |  8214 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8215 | `				 * then use the block to resolve method conflicts.` |
|       - |  8216 | `				 */` |
|       - |  8217 | `				SyToken *pR;` |
|      19 |  8218 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  8219 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8220 | `					ph7_class_attr *pAR;` |
|       - |  8221 | `					SyHashEntry *pER;` |
|       - |  8222 | `					SyString *pNR;` |
|      11 |  8223 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  8224 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8225 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8226 | `						pNR = &pAR->sName;` |
|     ! 0 |  8227 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8228 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8229 | `						}` |
|     ! 0 |  8230 | `					}` |
|      11 |  8231 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  8232 | `				}` |
|       - |  8233 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  8234 | `				pR = pUse->pResolvStart;` |
|      21 |  8235 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8236 | `					SyString sTrait,sMethod;` |
|       - |  8237 | `					ph7_class *pSrcTrait;` |
|       - |  8238 | `					ph7_class_method *pMeth;` |
|       - |  8239 | `					sxi32 nRKwrd;` |
|      33 |  8240 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8241 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8242 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8243 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8244 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8245 | `					sMethod = pR->sData;` |
|      13 |  8246 | `					pR++;` |
|      13 |  8247 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8248 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8249 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8250 | `							sTrait = sMethod;` |
|       7 |  8251 | `							pR++;` |
|       7 |  8252 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8253 | `							sMethod = pR->sData;` |
|       7 |  8254 | `							pR++;` |
|       3 |  8255 | `						}` |
|       3 |  8256 | `					}` |
|      13 |  8257 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8258 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8259 | `						continue;` |
|       - |  8260 | `					}` |
|      13 |  8261 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8262 | `					pR++;` |
|      13 |  8263 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8264 | `						pSrcTrait = 0;` |
|       7 |  8265 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8266 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8267 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8268 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8269 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8270 | `								break;` |
|       - |  8271 | `							}` |
|       2 |  8272 | `						}` |
|       5 |  8273 | `						if( pSrcTrait ){` |
|       5 |  8274 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8275 | `							if( pMeth ){` |
|       5 |  8276 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8277 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8278 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8279 | `								}` |
|       2 |  8280 | `							}` |
|       2 |  8281 | `						}` |
|       2 |  8282 | `					}` |
|      29 |  8283 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8284 | `				}` |
|       - |  8285 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8286 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8287 | `					ph7_class_method *pMR;` |
|       - |  8288 | `					SyHashEntry *pER;` |
|       - |  8289 | `					SyString *pNR;` |
|      11 |  8290 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8291 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8292 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8293 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8294 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8295 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8296 | `						}` |
|       1 |  8297 | `					}` |
|       6 |  8298 | `				}` |
|       - |  8299 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8300 | `				pR = pUse->pResolvStart;` |
|      21 |  8301 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8302 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8303 | `					ph7_class *pSrcTrait;` |
|       - |  8304 | `					ph7_class_method *pMeth;` |
|      21 |  8305 | `					int hasQual = 0;` |
|       - |  8306 | `					sxi32 nRKwrd;` |
|      33 |  8307 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8308 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8309 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8310 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8311 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8312 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8313 | `					sMethod = pR->sData;` |
|      13 |  8314 | `					pR++;` |
|      13 |  8315 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8316 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8317 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8318 | `							sTrait = sMethod;` |
|       7 |  8319 | `							hasQual = 1;` |
|       7 |  8320 | `							pR++;` |
|       7 |  8321 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8322 | `							sMethod = pR->sData;` |
|       7 |  8323 | `							pR++;` |
|       3 |  8324 | `						}` |
|       3 |  8325 | `					}` |
|      13 |  8326 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8327 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8328 | `						continue;` |
|       - |  8329 | `					}` |
|      13 |  8330 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8331 | `					pR++;` |
|      13 |  8332 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8333 | `						sxi32 iNewVis = -1;` |
|       9 |  8334 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8335 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8336 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8337 | `								iNewVis = nAK;` |
|       7 |  8338 | `								pR++;` |
|       3 |  8339 | `							}` |
|       3 |  8340 | `						}` |
|       9 |  8341 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8342 | `							sAlias = pR->sData;` |
|       7 |  8343 | `							pR++;` |
|       3 |  8344 | `						}` |
|       9 |  8345 | `						pMeth = 0;` |
|       9 |  8346 | `						if( hasQual ){` |
|       3 |  8347 | `							pSrcTrait = 0;` |
|       5 |  8348 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8349 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8350 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8351 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8352 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8353 | `									break;` |
|       - |  8354 | `								}` |
|       2 |  8355 | `							}` |
|       3 |  8356 | `							if( pSrcTrait ){` |
|       3 |  8357 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8358 | `							}` |
|       2 |  8359 | `						}else{` |
|       7 |  8360 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8361 | `						}` |
|       9 |  8362 | `						if( pMeth ){` |
|       9 |  8363 | `							if( sAlias.nByte > 0 ){` |
|       - |  8364 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8365 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8366 | `								 */` |
|       - |  8367 | `								ph7_class_method *pAlias;` |
|       - |  8368 | `								char *zAliasDup;` |
|       7 |  8369 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8370 | `								if( pAlias ){` |
|       7 |  8371 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8372 | `									if( iNewVis >= 0 ){` |
|       5 |  8373 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8374 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8375 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8376 | `									}` |
|       7 |  8377 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8378 | `									if( zAliasDup ){` |
|       7 |  8379 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8380 | `									}` |
|       4 |  8381 | `								}` |
|       6 |  8382 | `							}else if( iNewVis >= 0 ){` |
|       - |  8383 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8384 | `								ph7_class_method *pCopy;` |
|       3 |  8385 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8386 | `								if( pCopy ){` |
|       3 |  8387 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8388 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8389 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8390 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8391 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8392 | `									/* Replace the method in the class hash */` |
|       3 |  8393 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8394 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8395 | `								}` |
|       1 |  8396 | `							}` |
|       4 |  8397 | `						}` |
|       4 |  8398 | `						SXUNUSED(hasQual);` |
|       4 |  8399 | `					}` |
|      17 |  8400 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8401 | `				}` |
|       - |  8402 | `			}` |
|      44 |  8403 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  8404 | `		}` |
|       - |  8405 | `	}` |
|       - |  8406 | `	/* Install the class */` |
|   41286 |  8407 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   41286 |  8408 | `	if( rc == SXRET_OK ){` |
|       - |  8409 | `		ph7_class **apInterface;` |
|       - |  8410 | `		sxu32 n;` |
|   41286 |  8411 | `		if( pBase ){` |
|       - |  8412 | `			/* Inherit from base class and mark as a subclass */` |
|   26204 |  8413 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   13101 |  8414 | `		}` |
|   41286 |  8415 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   44222 |  8416 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8417 | `			/* Implements one or more interface */` |
|    2938 |  8418 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    2938 |  8419 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8420 | `				break;` |
|       - |  8421 | `			}` |
|    1470 |  8422 | `		}` |
|       - |  8423 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   41286 |  8424 | `		if( rc == SXRET_OK ){` |
|   41286 |  8425 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   41286 |  8426 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8427 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8428 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8429 | `				return SXERR_ABORT;` |
|       - |  8430 | `			}` |
|   20642 |  8431 | `		}` |
|       - |  8432 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   41286 |  8433 | `		if( rc == SXRET_OK ){` |
|   41286 |  8434 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   41286 |  8435 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8436 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8437 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8438 | `				return SXERR_ABORT;` |
|       - |  8439 | `			}` |
|   20642 |  8440 | `		}` |
|   20642 |  8441 | `	}` |
|   41286 |  8442 | `	SySetRelease(&aUseEntries);` |
|   41286 |  8443 | `	SySetRelease(&aInterfaces);` |
|   41286 |  8444 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8445 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8446 | `		return SXERR_ABORT;` |
|       - |  8447 | `	}` |
|   20642 |  8448 | `done:` |
|       - |  8449 | `	/* Point beyond the class body */` |
|   41300 |  8450 | `	pGen->pIn = &pEnd[1];` |
|   41300 |  8451 | `	pGen->pEnd = pTmp;` |
|   41300 |  8452 | `	return PH7_OK;` |
|   20651 |  8453 |  |
|       - |  8454 | `/*` |
|       - |  8455 | ` * Compile a user-defined abstract class.` |
|       - |  8456 | ` *  According to the PHP language reference manual` |
|       - |  8457 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8458 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8459 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8460 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8461 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8462 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8463 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8464 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8465 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8466 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8467 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8468 | ` *   could differ.` |
|       - |  8469 | ` */` |
|      18 |  8470 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8471 |  |
|       - |  8472 | `	sxi32 rc;` |
|      20 |  8473 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      20 |  8474 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      20 |  8475 | `	return rc;` |
|       2 |  8476 |  |
|       - |  8477 | `/*` |
|       - |  8478 | ` * Compile a user-defined final class.` |
|       - |  8479 | ` *  According to the PHP language reference manual` |
|       - |  8480 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8481 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8482 | ` *    final then it cannot be extended.` |
|       - |  8483 | ` */` |
|       2 |  8484 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8485 |  |
|       - |  8486 | `	sxi32 rc;` |
|       3 |  8487 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8488 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8489 | `	return rc;` |
|       1 |  8490 |  |
|       - |  8491 | `/*` |
|       - |  8492 | ` * Compile a user-defined trait.` |
|       - |  8493 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8494 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8495 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8496 | ` */` |
|      54 |  8497 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8498 |  |
|      56 |  8499 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8500 | `	ph7_class *pClass;` |
|       - |  8501 | `	SyToken *pEnd,*pTmp;` |
|       - |  8502 | `	sxi32 iProtection;` |
|       - |  8503 | `	sxi32 iAttrflags;` |
|       - |  8504 | `	SyString *pName;` |
|       - |  8505 | `	sxi32 nKwrd;` |
|       - |  8506 | `	sxi32 rc;` |
|       - |  8507 | `	/* Jump the 'trait' keyword */` |
|      56 |  8508 | `	pGen->pIn++;` |
|      56 |  8509 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8510 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8511 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8512 | `			return SXERR_ABORT;` |
|       - |  8513 | `		}` |
|     ! 0 |  8514 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8515 | `			pGen->pIn++;` |
|     ! 0 |  8516 | `		}` |
|     ! 0 |  8517 | `		return SXRET_OK;` |
|       - |  8518 | `	}` |
|       - |  8519 | `	/* Extract trait name */` |
|      56 |  8520 | `	pName = &pGen->pIn->sData;` |
|      56 |  8521 | `	pGen->pIn++;` |
|       - |  8522 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8523 | `		SyBlob sFQN;` |
|       - |  8524 | `		SyString sFQNStr;` |
|      56 |  8525 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8526 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8527 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8528 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8529 | `		SyBlobRelease(&sFQN);` |
|       - |  8530 | `	}` |
|      56 |  8531 | `	if( pClass == 0 ){` |
|     ! 0 |  8532 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8533 | `		return SXERR_ABORT;` |
|       - |  8534 | `	}` |
|       - |  8535 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8536 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8537 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8538 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8539 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8540 | `			return SXERR_ABORT;` |
|       - |  8541 | `		}` |
|     ! 0 |  8542 | `		return SXRET_OK;` |
|       - |  8543 | `	}` |
|      56 |  8544 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8545 | `	pEnd = 0;` |
|      56 |  8546 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8547 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8548 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8549 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8550 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8551 | `			return SXERR_ABORT;` |
|       - |  8552 | `		}` |
|     ! 0 |  8553 | `		return SXRET_OK;` |
|       - |  8554 | `	}` |
|       - |  8555 | `	/* Swap token stream */` |
|      56 |  8556 | `	pTmp = pGen->pEnd;` |
|      56 |  8557 | `	pGen->pEnd = pEnd;` |
|       - |  8558 | `	/* Mark as trait */` |
|      56 |  8559 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8560 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8561 | `	for(;;){` |
|     154 |  8562 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8563 | `			pGen->pIn++;` |
|       2 |  8564 | `		}` |
|     130 |  8565 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8566 | `			break;` |
|       - |  8567 | `		}` |
|      76 |  8568 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8569 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8570 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8571 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8572 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8573 | `				return SXERR_ABORT;` |
|       - |  8574 | `			}` |
|     ! 0 |  8575 | `			goto done;` |
|       - |  8576 | `		}` |
|      76 |  8577 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8578 | `		iAttrflags = 0;` |
|      76 |  8579 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8580 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8581 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8582 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8583 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8584 | `				for(;;){` |
|       - |  8585 | `					ph7_class *pUsedTrait;` |
|       - |  8586 | `					SyString *pUsedName;` |
|       5 |  8587 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8588 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8589 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8590 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8591 | `							return SXERR_ABORT;` |
|       - |  8592 | `						}` |
|     ! 0 |  8593 | `						break;` |
|       - |  8594 | `					}` |
|       5 |  8595 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8596 | `					{` |
|       - |  8597 | `						SyBlob sResolved;` |
|       5 |  8598 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8599 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8600 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8601 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8602 | `						SyBlobRelease(&sResolved);` |
|       - |  8603 | `					}` |
|       5 |  8604 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8605 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8606 | `					}` |
|       5 |  8607 | `					if( pUsedTrait == 0 ){` |
|       4 |  8608 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8609 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8610 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8611 | `							return SXERR_ABORT;` |
|       - |  8612 | `						}` |
|       2 |  8613 | `					}else{` |
|       3 |  8614 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8615 | `					}` |
|       5 |  8616 | `					pGen->pIn++;` |
|       5 |  8617 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8618 | `						break;` |
|       - |  8619 | `					}` |
|     ! 0 |  8620 | `					pGen->pIn++;` |
|     ! 0 |  8621 | `				}` |
|       5 |  8622 | `				continue;` |
|       - |  8623 | `			}` |
|      72 |  8624 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8625 | `				iProtection = nKwrd;` |
|      68 |  8626 | `				pGen->pIn++;` |
|      66 |  8627 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8628 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8629 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8630 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8631 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8632 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8633 | `						return SXERR_ABORT;` |
|       - |  8634 | `					}` |
|     ! 0 |  8635 | `					goto done;` |
|       - |  8636 | `				}` |
|      68 |  8637 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8638 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8639 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8640 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8641 | `							return SXERR_ABORT;` |
|       - |  8642 | `						}` |
|     ! 0 |  8643 | `						goto done;` |
|       - |  8644 | `					}` |
|      11 |  8645 | `					continue;` |
|       - |  8646 | `				}` |
|      58 |  8647 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8648 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8649 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8650 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8651 | `							return SXERR_ABORT;` |
|       - |  8652 | `						}` |
|     ! 0 |  8653 | `						goto done;` |
|       - |  8654 | `					}` |
|       5 |  8655 | `					continue;` |
|       - |  8656 | `				}` |
|      53 |  8657 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8658 | `			}` |
|      57 |  8659 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8660 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8661 | `					"Traits cannot have constants");` |
|     ! 0 |  8662 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8663 | `					return SXERR_ABORT;` |
|       - |  8664 | `				}` |
|     ! 0 |  8665 | `				goto done;` |
|     ! 0 |  8666 | `			}else{` |
|      57 |  8667 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8668 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8669 | `					pGen->pIn++;` |
|       5 |  8670 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8671 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8672 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8673 | `							iProtection = nKwrd;` |
|     ! 0 |  8674 | `							pGen->pIn++;` |
|     ! 0 |  8675 | `						}` |
|       1 |  8676 | `					}` |
|       4 |  8677 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8678 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8679 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8680 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8681 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8682 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8683 | `							return SXERR_ABORT;` |
|       - |  8684 | `						}` |
|     ! 0 |  8685 | `						goto done;` |
|       - |  8686 | `					}` |
|       5 |  8687 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8688 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8689 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8690 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8691 | `								return SXERR_ABORT;` |
|       - |  8692 | `							}` |
|     ! 0 |  8693 | `							goto done;` |
|       - |  8694 | `						}` |
|       3 |  8695 | `						continue;` |
|       - |  8696 | `					}` |
|       3 |  8697 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8698 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8699 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8700 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8701 | `								return SXERR_ABORT;` |
|       - |  8702 | `							}` |
|     ! 0 |  8703 | `							goto done;` |
|       - |  8704 | `						}` |
|     ! 0 |  8705 | `						continue;` |
|       - |  8706 | `					}` |
|       3 |  8707 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  8708 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  8709 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  8710 | `					pGen->pIn++;` |
|       5 |  8711 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  8712 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8713 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8714 | `							iProtection = nKwrd;` |
|       5 |  8715 | `							pGen->pIn++;` |
|       2 |  8716 | `						}` |
|       2 |  8717 | `					}` |
|       5 |  8718 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8719 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8720 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8721 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  8722 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8723 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8724 | `							return SXERR_ABORT;` |
|       - |  8725 | `						}` |
|     ! 0 |  8726 | `						goto done;` |
|       - |  8727 | `					}` |
|       5 |  8728 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8729 | `				}` |
|      55 |  8730 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8731 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8732 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  8733 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8734 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8735 | `						return SXERR_ABORT;` |
|       - |  8736 | `					}` |
|     ! 0 |  8737 | `					goto done;` |
|       - |  8738 | `				}` |
|      55 |  8739 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  8740 | `					pGen->pIn++;` |
|     ! 0 |  8741 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  8742 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8743 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8744 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8745 | `							return SXERR_ABORT;` |
|       - |  8746 | `						}` |
|     ! 0 |  8747 | `						goto done;` |
|       - |  8748 | `					}` |
|     ! 0 |  8749 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8750 | `				}else{` |
|      55 |  8751 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8752 | `				}` |
|      55 |  8753 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8754 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8755 | `						return SXERR_ABORT;` |
|       - |  8756 | `					}` |
|     ! 0 |  8757 | `					goto done;` |
|       - |  8758 | `				}` |
|       - |  8759 | `			}` |
|      28 |  8760 | `		}else{` |
|     ! 0 |  8761 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8762 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8763 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8764 | `					return SXERR_ABORT;` |
|       - |  8765 | `				}` |
|     ! 0 |  8766 | `				goto done;` |
|       - |  8767 | `			}` |
|       - |  8768 | `		}` |
|       1 |  8769 | `	}` |
|       - |  8770 | `	/* Install the trait */` |
|      56 |  8771 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  8772 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8773 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8774 | `		return SXERR_ABORT;` |
|       - |  8775 | `	}` |
|      27 |  8776 | `done:` |
|       - |  8777 | `	/* Point beyond the trait body */` |
|      56 |  8778 | `	pGen->pIn = &pEnd[1];` |
|      56 |  8779 | `	pGen->pEnd = pTmp;` |
|      56 |  8780 | `	return PH7_OK;` |
|      29 |  8781 |  |
|       - |  8782 | `/*` |
|       - |  8783 | ` * Compile a user-defined class.` |
|       - |  8784 | ` *  According to the PHP language reference manual` |
|       - |  8785 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  8786 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  8787 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  8788 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  8789 | ` *   and functions (called "methods").` |
|       - |  8790 | ` */` |
|   41278 |  8791 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  8792 |  |
|       - |  8793 | `	sxi32 rc;` |
|   41280 |  8794 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   41280 |  8795 | `	return rc;` |
|       2 |  8796 |  |
|       - |  8797 | `/*` |
|       - |  8798 | ` * Exception handling.` |
|       - |  8799 | ` *  According to the PHP language reference manual` |
|       - |  8800 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  8801 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  8802 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  8803 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  8804 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  8805 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  8806 | ` *    (or re-thrown) within a catch block.` |
|       - |  8807 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  8808 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  8809 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  8810 | ` *    been defined with set_exception_handler().` |
|       - |  8811 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  8812 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  8813 | ` */` |
|       - |  8814 | `/*` |
|       - |  8815 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  8816 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  8817 | ` * indicates failure.` |
|       - |  8818 | ` */` |
|    8756 |  8819 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  8820 |  |
|    8758 |  8821 | `	sxi32 rc = SXRET_OK;` |
|    8758 |  8822 | `	if( pRoot->pOp ){` |
|    8752 |  8823 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_NEW /* new Exception() */` |
|    4378 |  8824 | `			&& pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */ && pRoot->pOp->iOp != EXPR_OP_DC /* :: */){` |
|       - |  8825 | `			/* Unexpected expression */` |
|     ! 0 |  8826 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8827 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  8828 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  8829 | `				rc = SXERR_INVALID;` |
|     ! 0 |  8830 | `			}` |
|       2 |  8831 | `		}` |
|    4381 |  8832 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  8833 | `		/* Unexpected expression */` |
|     ! 0 |  8834 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  8835 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  8836 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  8837 | `			rc = SXERR_INVALID;` |
|     ! 0 |  8838 | `		}` |
|     ! 0 |  8839 | `	}` |
|    8758 |  8840 | `	return rc;` |
|       2 |  8841 |  |
|       - |  8842 | `/*` |
|       - |  8843 | ` * Compile a 'throw' statement.` |
|       - |  8844 | ` * throw: This is how you trigger an exception.` |
|       - |  8845 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  8846 | ` */` |
|    8756 |  8847 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  8848 |  |
|    8758 |  8849 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8850 | `	GenBlock *pBlock;` |
|       - |  8851 | `	sxu32 nIdx;` |
|       - |  8852 | `	sxi32 rc;` |
|    8758 |  8853 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  8854 | `	/* Compile the expression */` |
|    8758 |  8855 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8758 |  8856 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  8857 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  8858 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8859 | `			return SXERR_ABORT;` |
|       - |  8860 | `		}` |
|     ! 0 |  8861 | `		return SXRET_OK;` |
|       - |  8862 | `	}` |
|    8758 |  8863 | `	pBlock = pGen->pCurrent;` |
|       - |  8864 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   40698 |  8865 | `	while(pBlock->pParent){` |
|   40694 |  8866 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8754 |  8867 | `			break;` |
|       - |  8868 | `		}` |
|       - |  8869 | `		/* Point to the parent block */` |
|   31942 |  8870 | `		pBlock = pBlock->pParent;` |
|       2 |  8871 | `	}` |
|       - |  8872 | `	/* Emit the throw instruction */` |
|    8758 |  8873 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  8874 | `	/* Emit the jump */` |
|    8758 |  8875 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8758 |  8876 | `	return SXRET_OK;` |
|    4380 |  8877 |  |
|       - |  8878 | `/*` |
|       - |  8879 | ` * Compile a 'catch' block.` |
|       - |  8880 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  8881 | ` * an object containing the exception information.` |
|       - |  8882 | ` */` |
|     162 |  8883 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  8884 |  |
|     164 |  8885 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8886 | `	ph7_exception_block sCatch;` |
|       - |  8887 | `	SySet *pInstrContainer;` |
|       - |  8888 | `	SyString sClassName;` |
|       - |  8889 | `	GenBlock *pCatch;` |
|       - |  8890 | `	SyToken *pToken;` |
|       - |  8891 | `	SyString *pName;` |
|       - |  8892 | `	char *zDup;` |
|       - |  8893 | `	sxi32 rc;` |
|     164 |  8894 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  8895 | `	/* Zero the structure */` |
|     164 |  8896 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  8897 | `	/* Initialize fields */` |
|     164 |  8898 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     164 |  8899 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     164 |  8900 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  8901 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  8902 | `			pToken = pGen->pIn;` |
|     ! 0 |  8903 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8904 | `				pToken--;` |
|     ! 0 |  8905 | `			}` |
|     ! 0 |  8906 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8907 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8908 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8909 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8910 | `				return SXERR_ABORT;` |
|       - |  8911 | `			}` |
|     ! 0 |  8912 | `			return SXERR_INVALID;` |
|       - |  8913 | `	}` |
|       - |  8914 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     164 |  8915 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|      93 |  8916 | `	for(;;){` |
|     188 |  8917 | `		int isAbsolute = 0;` |
|       - |  8918 | `		SyBlob sName;` |
|     188 |  8919 | `		SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|       - |  8920 | `		/* Accept optional leading '\' for fully-qualified names */` |
|     188 |  8921 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|       9 |  8922 | `			isAbsolute = 1;` |
|       9 |  8923 | `			pGen->pIn++;` |
|       4 |  8924 | `		}` |
|     188 |  8925 | `		if( pGen->pIn >= pGen->pEnd \|\|` |
|     186 |  8926 | `			(pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       5 |  8927 | `			SyBlobRelease(&sName);` |
|       5 |  8928 | `			pToken = pGen->pIn;` |
|       5 |  8929 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8930 | `				pToken--;` |
|     ! 0 |  8931 | `			}` |
|       7 |  8932 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8933 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  8934 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  8935 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8936 | `				return SXERR_ABORT;` |
|       - |  8937 | `			}` |
|       5 |  8938 | `			return SXERR_INVALID;` |
|       - |  8939 | `		}` |
|       - |  8940 | `		/* Collect namespace-qualified name: ID [\ ID]* */` |
|     184 |  8941 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|     184 |  8942 | `		pGen->pIn++;` |
|     279 |  8943 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|      99 |  8944 | `			&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       5 |  8945 | `			SyBlobAppend(&sName,"\\",1);` |
|       5 |  8946 | `			pGen->pIn++; /* Skip '\' separator */` |
|       5 |  8947 | `			SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       5 |  8948 | `			pGen->pIn++;` |
|       1 |  8949 | `		}` |
|       - |  8950 | `		/* Resolve through namespace/imports for non-absolute names */` |
|     184 |  8951 | `		if( !isAbsolute ){` |
|       - |  8952 | `			SyString sRaw;` |
|       - |  8953 | `			SyBlob sResolved;` |
|     176 |  8954 | `			SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|     176 |  8955 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     176 |  8956 | `			GenStateResolveName(pGen,&sRaw,&sResolved);` |
|     263 |  8957 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     174 |  8958 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     176 |  8959 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     176 |  8960 | `			SyBlobRelease(&sResolved);` |
|      89 |  8961 | `		}else{` |
|       - |  8962 | `			/* Absolute name: use as-is without namespace prefix */` |
|      13 |  8963 | `			zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       8 |  8964 | `				(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|       9 |  8965 | `			SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sName));` |
|       - |  8966 | `		}` |
|     184 |  8967 | `		SyBlobRelease(&sName);` |
|     184 |  8968 | `		if( zDup == 0 ){` |
|     ! 0 |  8969 | `			goto Mem;` |
|       - |  8970 | `		}` |
|     184 |  8971 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     184 |  8972 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  8973 | `			goto Mem;` |
|       - |  8974 | `		}` |
|       - |  8975 | `		/* Check for '\|' (multi-catch separator) */` |
|     194 |  8976 | `		if( pGen->pIn < pGen->pEnd &&` |
|     182 |  8977 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      26 |  8978 | `			pGen->pIn->sData.nByte == 1 &&` |
|      24 |  8979 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      26 |  8980 | `			pGen->pIn++; /* Consume the '\|' */` |
|      26 |  8981 | `			continue;` |
|       - |  8982 | `		}` |
|     160 |  8983 | `		break;` |
|     ! 0 |  8984 | `	}` |
|     237 |  8985 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     160 |  8986 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  8987 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  8988 | `			pToken = pGen->pIn;` |
|     ! 0 |  8989 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  8990 | `				pToken--;` |
|     ! 0 |  8991 | `			}` |
|     ! 0 |  8992 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  8993 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  8994 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  8995 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8996 | `				return SXERR_ABORT;` |
|       - |  8997 | `			}` |
|     ! 0 |  8998 | `			return SXERR_INVALID;` |
|       - |  8999 | `	}` |
|     160 |  9000 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9001 | `	/* Duplicate instance name */` |
|     160 |  9002 | `	pName = &pGen->pIn->sData;` |
|     160 |  9003 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     160 |  9004 | `	if( zDup == 0 ){` |
|     ! 0 |  9005 | `		goto Mem;` |
|       - |  9006 | `	}` |
|     160 |  9007 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     160 |  9008 | `	pGen->pIn++;` |
|     160 |  9009 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9010 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9011 | `		pToken = pGen->pIn;` |
|     ! 0 |  9012 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9013 | `			pToken--;` |
|     ! 0 |  9014 | `		}` |
|     ! 0 |  9015 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9016 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9017 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9018 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9019 | `			return SXERR_ABORT;` |
|       - |  9020 | `		}` |
|     ! 0 |  9021 | `		return SXERR_INVALID;` |
|       - |  9022 | `	}` |
|       - |  9023 | `	/* Compile the block */` |
|     160 |  9024 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9025 | `	/* Create the catch block */` |
|     160 |  9026 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     160 |  9027 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9028 | `		return SXERR_ABORT;` |
|       - |  9029 | `	}` |
|       - |  9030 | `	/* Swap bytecode container */` |
|     160 |  9031 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     160 |  9032 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9033 | `	/* Compile the block */` |
|     160 |  9034 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9035 | `	/* Fix forward jumps now the destination is resolved  */` |
|     160 |  9036 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9037 | `	/* Emit the DONE instruction */` |
|     160 |  9038 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9039 | `	/* Leave the block */` |
|     160 |  9040 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9041 | `	/* Restore the default container */` |
|     160 |  9042 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9043 | `	/* Install the catch block */` |
|     160 |  9044 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     160 |  9045 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9046 | `		goto Mem;` |
|       - |  9047 | `	}` |
|     160 |  9048 | `	return SXRET_OK;` |
|     ! 0 |  9049 | `Mem:` |
|     ! 0 |  9050 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9051 | `	return SXERR_ABORT;` |
|      83 |  9052 |  |
|       - |  9053 | `/*` |
|       - |  9054 | ` * Compile a 'try' block.` |
|       - |  9055 | ` * A function using an exception should be in a "try" block.` |
|       - |  9056 | ` * If the exception does not trigger, the code will continue` |
|       - |  9057 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9058 | ` * is "thrown".` |
|       - |  9059 | ` */` |
|     170 |  9060 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  9061 |  |
|       - |  9062 | `	ph7_exception *pException;` |
|     172 |  9063 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9064 | `	GenBlock *pTry;` |
|       - |  9065 | `	sxu32 nJmpIdx;` |
|       - |  9066 | `	sxi32 rc;` |
|       - |  9067 | `	/* Create the exception container */` |
|     172 |  9068 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     172 |  9069 | `	if( pException == 0 ){` |
|     ! 0 |  9070 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9071 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9072 | `		return SXERR_ABORT;` |
|       - |  9073 | `	}` |
|       - |  9074 | `	/* Zero the structure */` |
|     172 |  9075 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9076 | `	/* Initialize fields */` |
|     172 |  9077 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     172 |  9078 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     172 |  9079 | `	pException->iHasFinally = 0;` |
|     172 |  9080 | `	pException->iFinallyDone = 0;` |
|     172 |  9081 | `	pException->pVm = pGen->pVm;` |
|       - |  9082 | `	/* Create the try block */` |
|     172 |  9083 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     172 |  9084 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9085 | `		return SXERR_ABORT;` |
|       - |  9086 | `	}` |
|       - |  9087 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     172 |  9088 | `	pTry->pUserData = pException;` |
|       - |  9089 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     172 |  9090 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9091 | `	/* Fix the jump later when the destination is resolved */` |
|     172 |  9092 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     172 |  9093 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9094 | `	/* Compile the block */` |
|     172 |  9095 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     172 |  9096 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9097 | `		return SXERR_ABORT;` |
|       - |  9098 | `	}` |
|       - |  9099 | `	/* Fix forward jumps now the destination is resolved */` |
|     172 |  9100 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9101 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     172 |  9102 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9103 | `	/* Leave the block */` |
|     172 |  9104 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9105 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     172 |  9106 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     168 |  9107 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9108 | `		/* Compile one or more catch blocks */` |
|     160 |  9109 | `		for(;;){` |
|     320 |  9110 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     252 |  9111 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|      81 |  9112 | `					break;` |
|       - |  9113 | `			}` |
|     164 |  9114 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     164 |  9115 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9116 | `				return SXERR_ABORT;` |
|       - |  9117 | `			}` |
|       2 |  9118 | `		}` |
|      79 |  9119 | `	}` |
|       - |  9120 | `	/* Compile optional finally block */` |
|     172 |  9121 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      86 |  9122 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9123 | `		SySet *pInstrContainer;` |
|       - |  9124 | `		GenBlock *pFinBlock;` |
|      32 |  9125 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9126 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  9127 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  9128 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9129 | `			return SXERR_ABORT;` |
|       - |  9130 | `		}` |
|       - |  9131 | `		/* Swap bytecode container */` |
|      32 |  9132 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  9133 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9134 | `		/* Compile the finally body */` |
|      32 |  9135 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  9136 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9137 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9138 | `			return SXERR_ABORT;` |
|       - |  9139 | `		}` |
|       - |  9140 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  9141 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9142 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  9143 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9144 | `		/* Leave the block */` |
|      32 |  9145 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9146 | `		/* Restore the default container */` |
|      32 |  9147 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  9148 | `		pException->iHasFinally = 1;` |
|      15 |  9149 | `	}` |
|       - |  9150 | `	/* Must have at least one catch or finally */` |
|     172 |  9151 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  9152 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9153 | `			"Cannot use try without catch or finally");` |
|       7 |  9154 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9155 | `			return SXERR_ABORT;` |
|       - |  9156 | `		}` |
|       3 |  9157 | `	}` |
|     172 |  9158 | `	return SXRET_OK;` |
|      87 |  9159 |  |
|       - |  9160 | `/*` |
|       - |  9161 | ` * Compile a switch block.` |
|       - |  9162 | ` *  (See block-comment below for more information)` |
|       - |  9163 | ` */` |
|     108 |  9164 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  9165 |  |
|     110 |  9166 | `	sxi32 rc = SXRET_OK;` |
|     110 |  9167 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9168 | `		/* Unexpected token */` |
|     ! 0 |  9169 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9170 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9171 | `			return SXERR_ABORT;` |
|       - |  9172 | `		}` |
|     ! 0 |  9173 | `		pGen->pIn++;` |
|     ! 0 |  9174 | `	}` |
|     110 |  9175 | `	pGen->pIn++;` |
|       - |  9176 | `	/* First instruction to execute in this block. */` |
|     110 |  9177 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9178 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9179 | `	 * or the '}' token */` |
|     182 |  9180 | `	for(;;){` |
|     366 |  9181 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9182 | `			/* No more input to process */` |
|     ! 0 |  9183 | `			break;` |
|       - |  9184 | `		}` |
|     366 |  9185 | `		rc = SXRET_OK;` |
|     366 |  9186 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  9187 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  9188 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9189 | `					/* Unexpected token */` |
|     ! 0 |  9190 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9191 | `						&pGen->pIn->sData);` |
|     ! 0 |  9192 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9193 | `						return SXERR_ABORT;` |
|       - |  9194 | `					}` |
|       - |  9195 | `					/* FALL THROUGH */` |
|     ! 0 |  9196 | `				}` |
|      28 |  9197 | `				rc = SXERR_EOF;` |
|      28 |  9198 | `				break;` |
|       - |  9199 | `			}` |
|      23 |  9200 | `		}else{` |
|       - |  9201 | `			sxi32 nKwrd;` |
|       - |  9202 | `			/* Extract the keyword */` |
|     298 |  9203 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  9204 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  9205 | `				break;` |
|       - |  9206 | `			}` |
|     218 |  9207 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9208 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9209 | `					/* Unexpected token */` |
|     ! 0 |  9210 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9211 | `						&pGen->pIn->sData);` |
|     ! 0 |  9212 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9213 | `						return SXERR_ABORT;` |
|       - |  9214 | `					}` |
|       - |  9215 | `					/* FALL THROUGH */` |
|     ! 0 |  9216 | `				}` |
|       - |  9217 | `				/* Block compiled */` |
|       3 |  9218 | `				break;` |
|       - |  9219 | `			}` |
|       - |  9220 | `		}` |
|       - |  9221 | `		/* Compile block */` |
|     258 |  9222 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  9223 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9224 | `			return SXERR_ABORT;` |
|       - |  9225 | `		}` |
|       2 |  9226 | `	}` |
|     110 |  9227 | `	return rc;` |
|      56 |  9228 |  |
|       - |  9229 | `/*` |
|       - |  9230 | ` * Compile a case eXpression.` |
|       - |  9231 | ` *  (See block-comment below for more information)` |
|       - |  9232 | ` */` |
|      88 |  9233 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  9234 |  |
|       - |  9235 | `	SySet *pInstrContainer;` |
|       - |  9236 | `	SyToken *pEnd,*pTmp;` |
|      90 |  9237 | `	sxi32 iNest = 0;` |
|       - |  9238 | `	sxi32 rc;` |
|       - |  9239 | `	/* Delimit the expression */` |
|      90 |  9240 | `	pEnd = pGen->pIn;` |
|     186 |  9241 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  9242 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9243 | `			/* Increment nesting level */` |
|       3 |  9244 | `			iNest++;` |
|     185 |  9245 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9246 | `			/* Decrement nesting level */` |
|       3 |  9247 | `			iNest--;` |
|     183 |  9248 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  9249 | `			break;` |
|       - |  9250 | `		}` |
|      98 |  9251 | `		pEnd++;` |
|       2 |  9252 | `	}` |
|      90 |  9253 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9254 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9255 | `		if( rc == SXERR_ABORT ){` |
|       - |  9256 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9257 | `			return SXERR_ABORT;` |
|       - |  9258 | `		}` |
|     ! 0 |  9259 | `	}` |
|       - |  9260 | `	/* Swap token stream */` |
|      90 |  9261 | `	pTmp = pGen->pEnd;` |
|      90 |  9262 | `	pGen->pEnd = pEnd;` |
|      90 |  9263 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  9264 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9265 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9266 | `	/* Emit the done instruction */` |
|      90 |  9267 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9268 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9269 | `	/* Update token stream */` |
|      90 |  9270 | `	pGen->pIn  = pEnd;` |
|      90 |  9271 | `	pGen->pEnd = pTmp;` |
|      90 |  9272 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9273 | `		return SXERR_ABORT;` |
|       - |  9274 | `	}` |
|      90 |  9275 | `	return SXRET_OK;` |
|      46 |  9276 |  |
|       - |  9277 | `/*` |
|       - |  9278 | ` * Compile the smart switch statement.` |
|       - |  9279 | ` * According to the PHP language reference manual` |
|       - |  9280 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9281 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9282 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9283 | ` *  This is exactly what the switch statement is for.` |
|       - |  9284 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9285 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9286 | ` *  of the outer loop, use continue 2.` |
|       - |  9287 | ` *  Note that switch/case does loose comparision.` |
|       - |  9288 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9289 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9290 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9291 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9292 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9293 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9294 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9295 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9296 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9297 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9298 | ` *  list for the next case.` |
|       - |  9299 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9300 | ` *  or floating-point numbers and strings.` |
|       - |  9301 | ` */` |
|      28 |  9302 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9303 |  |
|       - |  9304 | `	GenBlock *pSwitchBlock;` |
|       - |  9305 | `	SyToken *pTmp,*pEnd;` |
|       - |  9306 | `	ph7_switch *pSwitch;` |
|       - |  9307 | `	sxu32 nToken;` |
|       - |  9308 | `	sxu32 nLine;` |
|       - |  9309 | `	sxi32 rc;` |
|      30 |  9310 | `	nLine = pGen->pIn->nLine;` |
|       - |  9311 | `	/* Jump the 'switch' keyword */` |
|      30 |  9312 | `	pGen->pIn++;` |
|      30 |  9313 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9314 | `		/* Syntax error */` |
|     ! 0 |  9315 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9316 | `		if( rc == SXERR_ABORT ){` |
|       - |  9317 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9318 | `			return SXERR_ABORT;` |
|       - |  9319 | `		}` |
|     ! 0 |  9320 | `		goto Synchronize;` |
|       - |  9321 | `	}` |
|       - |  9322 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9323 | `	pGen->pIn++;` |
|      30 |  9324 | `	pEnd = 0; /* cc warning */` |
|       - |  9325 | `	/* Create the loop block */` |
|      44 |  9326 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9327 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9328 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9329 | `		return SXERR_ABORT;` |
|       - |  9330 | `	}` |
|       - |  9331 | `	/* Delimit the condition */` |
|      30 |  9332 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9333 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9334 | `		/* Empty expression */` |
|     ! 0 |  9335 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9336 | `		if( rc == SXERR_ABORT ){` |
|       - |  9337 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9338 | `			return SXERR_ABORT;` |
|       - |  9339 | `		}` |
|     ! 0 |  9340 | `	}` |
|       - |  9341 | `	/* Swap token streams */` |
|      30 |  9342 | `	pTmp = pGen->pEnd;` |
|      30 |  9343 | `	pGen->pEnd = pEnd;` |
|       - |  9344 | `	/* Compile the expression */` |
|      30 |  9345 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9346 | `	if( rc == SXERR_ABORT ){` |
|       - |  9347 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9348 | `		return SXERR_ABORT;` |
|       - |  9349 | `	}` |
|       - |  9350 | `	/* Update token stream */` |
|      30 |  9351 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9352 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9353 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9354 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9355 | `			return SXERR_ABORT;` |
|       - |  9356 | `		}` |
|     ! 0 |  9357 | `		pGen->pIn++;` |
|     ! 0 |  9358 | `	}` |
|      30 |  9359 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9360 | `	pGen->pEnd = pTmp;` |
|      30 |  9361 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9362 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9363 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9364 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9365 | `				pTmp--;` |
|     ! 0 |  9366 | `			}` |
|       - |  9367 | `			/* Unexpected token */` |
|     ! 0 |  9368 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9369 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9370 | `				return SXERR_ABORT;` |
|       - |  9371 | `			}` |
|     ! 0 |  9372 | `			goto Synchronize;` |
|       - |  9373 | `	}` |
|       - |  9374 | `	/* Set the delimiter token */` |
|      30 |  9375 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9376 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9377 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9378 | `	}else{` |
|      28 |  9379 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9380 | `	}` |
|      30 |  9381 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9382 | `	/* Create the switch blocks container */` |
|      30 |  9383 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9384 | `	if( pSwitch == 0 ){` |
|       - |  9385 | `		/* Abort compilation */` |
|     ! 0 |  9386 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9387 | `		return SXERR_ABORT;` |
|       - |  9388 | `	}` |
|       - |  9389 | `	/* Zero the structure */` |
|      30 |  9390 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9391 | `	/* Initialize fields */` |
|      30 |  9392 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9393 | `	/* Emit the switch instruction */` |
|      30 |  9394 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9395 | `	/* Compile case blocks */` |
|      96 |  9396 | `	for(;;){` |
|       - |  9397 | `		sxu32 nKwrd;` |
|     112 |  9398 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9399 | `			/* No more input to process */` |
|     ! 0 |  9400 | `			break;` |
|       - |  9401 | `		}` |
|     112 |  9402 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9403 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9404 | `				/* Unexpected token */` |
|     ! 0 |  9405 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9406 | `					&pGen->pIn->sData);` |
|     ! 0 |  9407 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9408 | `					return SXERR_ABORT;` |
|       - |  9409 | `				}` |
|       - |  9410 | `				/* FALL THROUGH */` |
|     ! 0 |  9411 | `			}` |
|       - |  9412 | `			/* Block compiled */` |
|     ! 0 |  9413 | `			break;` |
|       - |  9414 | `		}` |
|       - |  9415 | `		/* Extract the keyword */` |
|     112 |  9416 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9417 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9418 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9419 | `				/* Unexpected token */` |
|     ! 0 |  9420 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9421 | `					&pGen->pIn->sData);` |
|     ! 0 |  9422 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9423 | `					return SXERR_ABORT;` |
|       - |  9424 | `				}` |
|       - |  9425 | `				/* FALL THROUGH */` |
|     ! 0 |  9426 | `			}` |
|       - |  9427 | `			/* Block compiled */` |
|       3 |  9428 | `			break;` |
|       - |  9429 | `		}` |
|     110 |  9430 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9431 | `			/*` |
|       - |  9432 | `			 * Accroding to the PHP language reference manual` |
|       - |  9433 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9434 | `			 *  that wasn't matched by the other cases.` |
|       - |  9435 | `			 */` |
|      22 |  9436 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9437 | `				/* Default case already compiled */` |
|     ! 0 |  9438 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9439 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9440 | `					return SXERR_ABORT;` |
|       - |  9441 | `				}` |
|     ! 0 |  9442 | `			}` |
|      22 |  9443 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9444 | `			/* Compile the default block */` |
|      22 |  9445 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9446 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9447 | `				return SXERR_ABORT;` |
|      22 |  9448 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9449 | `				break;` |
|       1 |  9450 | `			}` |
|      91 |  9451 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9452 | `			ph7_case_expr sCase;` |
|       - |  9453 | `			/* Standard case block */` |
|      90 |  9454 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9455 | `			/* initialize the structure */` |
|      90 |  9456 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9457 | `			/* Compile the case expression */` |
|      90 |  9458 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9459 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9460 | `				return SXERR_ABORT;` |
|       - |  9461 | `			}` |
|       - |  9462 | `			/* Compile the case block */` |
|      90 |  9463 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9464 | `			/* Insert in the switch container */` |
|      90 |  9465 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9466 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9467 | `				return SXERR_ABORT;` |
|      90 |  9468 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9469 | `				break;` |
|       - |  9470 | `			}` |
|      42 |  9471 | `		}else{` |
|       - |  9472 | `			/* Unexpected token */` |
|     ! 0 |  9473 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9474 | `				&pGen->pIn->sData);` |
|     ! 0 |  9475 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9476 | `				return SXERR_ABORT;` |
|       - |  9477 | `			}` |
|     ! 0 |  9478 | `			break;` |
|       - |  9479 | `		}` |
|       2 |  9480 | `	}` |
|       - |  9481 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9482 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9483 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9484 | `	/* Release the loop block */` |
|      30 |  9485 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9486 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9487 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9488 | `		pGen->pIn++;` |
|      14 |  9489 | `	}` |
|       - |  9490 | `	/* Statement successfully compiled */` |
|      30 |  9491 | `	return SXRET_OK;` |
|     ! 0 |  9492 | `Synchronize:` |
|       - |  9493 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9494 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9495 | `		pGen->pIn++;` |
|     ! 0 |  9496 | `	}` |
|     ! 0 |  9497 | `	return SXRET_OK;` |
|      16 |  9498 |  |
|       - |  9499 | `/*` |
|       - |  9500 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9501 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9502 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9503 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9504 | ` */` |
|       - |  9505 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9506 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9507 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9508 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9509 |  |
|       - |  9510 | `/*` |
|       - |  9511 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9512 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9513 | ` * patched entries from the pending set.` |
|       - |  9514 | ` */` |
| 1977188 |  9515 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       2 |  9516 |  |
| 1977190 |  9517 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9518 | `	sxu32 nTarget;` |
|       - |  9519 | `	sxu32 *aIdx;` |
|       - |  9520 | `	sxu32 i;` |
| 1977190 |  9521 | `	if( nCur <= nBaseline ){` |
| 1977100 |  9522 | `		return;` |
|       - |  9523 | `	}` |
|      92 |  9524 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      92 |  9525 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     190 |  9526 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     100 |  9527 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     100 |  9528 | `		if( pInstr ){` |
|     100 |  9529 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9530 | `		}` |
|      51 |  9531 | `	}` |
|      92 |  9532 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  988596 |  9533 |  |
|       - |  9534 |  |
|       - |  9535 | `/*` |
|       - |  9536 | ` * Generate bytecode for a given expression tree.` |
|       - |  9537 | ` * If something goes wrong while generating bytecode` |
|       - |  9538 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9539 | ` * this function takes care of generating the appropriate` |
|       - |  9540 | ` * error message.` |
|       - |  9541 | ` */` |
| 2607122 |  9542 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9543 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9544 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9545 | `	sxi32 iFlags /* Control flags */` |
|       - |  9546 | `	)` |
|       2 |  9547 |  |
|       - |  9548 | `	VmInstr *pInstr;` |
|       - |  9549 | `	sxu32 nJmpIdx;` |
| 2607124 |  9550 | `	sxi32 iP1 = 0;` |
| 2607124 |  9551 | `	sxu32 iP2 = 0;` |
| 2607124 |  9552 | `	void *p3  = 0;` |
|       - |  9553 | `	sxi32 iVmOp;` |
|       - |  9554 | `	sxi32 rc;` |
| 2607124 |  9555 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 2607124 |  9556 | `	sxu32 nRhsNsBase = 0;` |
| 2607124 |  9557 | `	if( pNode->xCode ){` |
|       - |  9558 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9559 | `		/* Compile node */` |
| 1615930 |  9560 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1615930 |  9561 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1615930 |  9562 | `		RE_SWAP_DELIMITER(pGen);` |
| 1615930 |  9563 | `		return rc;` |
|       - |  9564 | `	}` |
|  991196 |  9565 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9566 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9567 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9568 | `		return SXERR_ABORT;` |
|       - |  9569 | `	}` |
|  991196 |  9570 | `	iVmOp = pNode->pOp->iVmOp;` |
|  991196 |  9571 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9572 | `		sxu32 nJmp = 0;` |
|       - |  9573 | `		sxu32 nNcNsBase;` |
|       - |  9574 | `		VmInstr *pInstrFix;` |
|       - |  9575 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9576 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9577 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9578 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9579 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9580 | `		if( pNode->pRight ){` |
|      47 |  9581 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9582 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9583 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9584 | `				return rc;` |
|       - |  9585 | `			}` |
|      47 |  9586 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - |  9587 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9588 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9589 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9590 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9591 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9592 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9593 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9594 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9595 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9596 | `				pInstrFix->iP2 = 3;` |
|       9 |  9597 | `			}` |
|      23 |  9598 | `		}` |
|       - |  9599 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9600 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9601 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9602 | `		if( pNode->pLeft ){` |
|      47 |  9603 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9604 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9605 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9606 | `				return rc;` |
|       - |  9607 | `			}` |
|      47 |  9608 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      23 |  9609 | `		}` |
|       - |  9610 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9611 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9612 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9613 | `		if( nJmp > 0 ){` |
|      47 |  9614 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9615 | `			if( pInstrFix ){` |
|      47 |  9616 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9617 | `			}` |
|      23 |  9618 | `		}` |
|      47 |  9619 | `		return SXRET_OK;` |
|       - |  9620 | `	}` |
|  991150 |  9621 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9622 | `		sxu32 nJz,nJmp;` |
|       - |  9623 | `		sxu32 nTernaryNsBase;` |
|       - |  9624 | `		/* Ternary operator require special handling */` |
|       - |  9625 | `		/* Phase#1: Compile the condition */` |
|    2022 |  9626 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2022 |  9627 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2022 |  9628 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9629 | `			return rc;` |
|       - |  9630 | `		}` |
|       - |  9631 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - |  9632 | `		 * compiling the condition must short-circuit to the end of the` |
|       - |  9633 | `		 * condition expression, not leak past the ternary. */` |
|    2022 |  9634 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2022 |  9635 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2022 |  9636 | `		if( pNode->pLeft ){` |
|       - |  9637 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9638 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1954 |  9639 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9640 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1954 |  9641 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    1954 |  9642 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1954 |  9643 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9644 | `				return rc;` |
|       - |  9645 | `			}` |
|    1954 |  9646 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     978 |  9647 | `		}else{` |
|       - |  9648 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9649 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9650 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9651 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9652 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9653 | `		}` |
|       - |  9654 | `		/* Phase#4: Emit the unconditional jump */` |
|    2022 |  9655 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9656 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2022 |  9657 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2022 |  9658 | `		if( pInstr ){` |
|    2022 |  9659 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1010 |  9660 | `		}` |
|    2022 |  9661 | `		if( !pNode->pLeft ){` |
|       - |  9662 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9663 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9664 | `		}` |
|       - |  9665 | `		/* Phase#6: Compile the 'else' expression */` |
|    2022 |  9666 | `		if( pNode->pRight ){` |
|    2022 |  9667 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2022 |  9668 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2022 |  9669 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9670 | `				return rc;` |
|       - |  9671 | `			}` |
|    2022 |  9672 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1010 |  9673 | `		}` |
|    2022 |  9674 | `		if( nJmp > 0 ){` |
|       - |  9675 | `			/* Phase#7: Fix the unconditional jump */` |
|    2022 |  9676 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2022 |  9677 | `			if( pInstr ){` |
|    2022 |  9678 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1010 |  9679 | `			}` |
|    1010 |  9680 | `		}` |
|       - |  9681 | `		/* All done */` |
|    2022 |  9682 | `		return SXRET_OK;` |
|       - |  9683 | `	}` |
|  989130 |  9684 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - |  9685 | `	/* Generate code for the left tree */` |
|  989130 |  9686 | `	if( pNode->pLeft ){` |
|  989092 |  9687 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  989092 |  9688 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - |  9689 | `			ph7_expr_node **apNode;` |
|  331854 |  9690 | `			int hasSpread = 0;` |
|  331854 |  9691 | `			int hasNamed = 0;` |
|       - |  9692 | `			sxi32 nArgs;` |
|       - |  9693 | `			sxi32 n;` |
|       - |  9694 | `			/* Recurse and generate bytecodes for function arguments */` |
|  331854 |  9695 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  331854 |  9696 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - |  9697 | `			/* Validate: no positional arguments after named arguments */` |
|       - |  9698 | `			{` |
|  331854 |  9699 | `				int seenNamed = 0;` |
|  662742 |  9700 | `				for( n = 0; n < nArgs; ++n ){` |
|  330892 |  9701 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     176 |  9702 | `						seenNamed = 1;` |
|     176 |  9703 | `						hasNamed = 1;` |
|  330805 |  9704 | `					}else if( seenNamed && !(apNode[n]->iFlags & EXPR_NODE_SPREAD) ){` |
|       3 |  9705 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - |  9706 | `							"Cannot use positional argument after named argument");` |
|       3 |  9707 | `						return SXERR_SYNTAX;` |
|       - |  9708 | `					}` |
|  165446 |  9709 | `				}` |
|       - |  9710 | `			}` |
|       - |  9711 | `			/* Read-only load */` |
|  331852 |  9712 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  662738 |  9713 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  330888 |  9714 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  330888 |  9715 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  330888 |  9716 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9717 | `					return rc;` |
|       - |  9718 | `				}` |
|       - |  9719 | `				/* Each argument is an independent nullsafe scope. */` |
|  330888 |  9720 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  330888 |  9721 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - |  9722 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 |  9723 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 |  9724 | `					hasSpread = 1;` |
|       9 |  9725 | `				}` |
|  165445 |  9726 | `			}` |
|       - |  9727 | `			/* Total number of given arguments */` |
|  331852 |  9728 | `			iP1 = nArgs;` |
|  331852 |  9729 | `			iP2 = hasSpread;` |
|       - |  9730 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - |  9731 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  331852 |  9732 | `			if( hasNamed ){` |
|      94 |  9733 | `				sxu32 nStrBytes = 0;` |
|       - |  9734 | `				char *zBuf;` |
|     278 |  9735 | `				for( n = 0; n < nArgs; ++n ){` |
|     186 |  9736 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9737 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      86 |  9738 | `					}` |
|      94 |  9739 | `				}` |
|       - |  9740 | `				{` |
|      94 |  9741 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      94 |  9742 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      92 |  9743 | `					&pGen->pVm->sAllocator, mapSize);` |
|      94 |  9744 | `				if( pMap ){` |
|      94 |  9745 | `					SyZero(pMap, mapSize);` |
|      94 |  9746 | `					pMap->bHasNamed = 1;` |
|      94 |  9747 | `					pMap->nTotal = (sxu32)nArgs;` |
|      94 |  9748 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      94 |  9749 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     278 |  9750 | `					for( n = 0; n < nArgs; ++n ){` |
|     186 |  9751 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 |  9752 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     174 |  9753 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     174 |  9754 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     174 |  9755 | `							zBuf += nb;` |
|      86 |  9756 | `						}` |
|       - |  9757 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      94 |  9758 | `					}` |
|      94 |  9759 | `					p3 = (void *)pMap;` |
|      46 |  9760 | `				}` |
|       - |  9761 | `				}` |
|      46 |  9762 | `			}` |
|       - |  9763 | `			/* Remove stale flags now */` |
|  331852 |  9764 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  165925 |  9765 | `		}` |
|  989090 |  9766 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|  989090 |  9767 | `		if( rc != SXRET_OK ){` |
|      31 |  9768 | `			return rc;` |
|       - |  9769 | `		}` |
|  989060 |  9770 | `		if( !bIsChainOp ){` |
|       - |  9771 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - |  9772 | `			 * target the end of that LHS chain, which is right here. */` |
|  471010 |  9773 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  235504 |  9774 | `		}` |
|  989060 |  9775 | `		if( iVmOp == PH7_OP_CALL ){` |
|  331852 |  9776 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  331852 |  9777 | `			if( pInstr ){` |
|  331852 |  9778 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  331040 |  9779 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - |  9780 | `					sxu32 nQual;` |
|       - |  9781 | `					/* Prevent constant expansion */` |
|  331040 |  9782 | `					pInstr->iP1 = 0;` |
|       - |  9783 | `					/* Namespace-qualify the function name for CALL.` |
|       - |  9784 | `					 * Only check function imports — class imports must NOT` |
|       - |  9785 | ``					 * affect function resolution.  For `new Foo()`, the CALL`` |
|       - |  9786 | `					 * handler fires before NEW; we store the original literal` |
|       - |  9787 | `					 * index in the CALL instruction's iP2 so the NEW handler` |
|       - |  9788 | `					 * can recover the unqualified name and re-qualify with` |
|       - |  9789 | `					 * class imports. */ {` |
|  331040 |  9790 | `						int fromImport = 0;` |
|  331040 |  9791 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  331040 |  9792 | `						pInstr->iP2 = (sxi32)nQual;` |
|  331040 |  9793 | `						if( nQual != nOrig ){` |
|       - |  9794 | `							/* Store original literal index in CALL's iP2 so the` |
|       - |  9795 | `							 * NEW handler can recover the unqualified name. */` |
|      72 |  9796 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      72 |  9797 | `							if( !fromImport ){` |
|       - |  9798 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      62 |  9799 | `								if( p3 == 0 ){` |
|      62 |  9800 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      60 |  9801 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      62 |  9802 | `									if( pMap ){` |
|      62 |  9803 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      62 |  9804 | `										p3 = (void *)pMap;` |
|      30 |  9805 | `									}` |
|      30 |  9806 | `								}` |
|      62 |  9807 | `								if( p3 ){` |
|      62 |  9808 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      30 |  9809 | `								}` |
|      30 |  9810 | `							}` |
|      37 |  9811 | `						}` |
|       - |  9812 | `					}` |
|  166333 |  9813 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - |  9814 | `					/* Method call,flag that */` |
|     662 |  9815 | `					pInstr->iP2 = 1;` |
|     330 |  9816 | `				}` |
|  165927 |  9817 | `			}` |
|  823135 |  9818 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - |  9819 | `			ph7_expr_node **apNode;` |
|       - |  9820 | `			sxi32 n;` |
|       - |  9821 | `			/* Recurse and generate bytecodes for array index */` |
|   74344 |  9822 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  134126 |  9823 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   59784 |  9824 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   59784 |  9825 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   59784 |  9826 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9827 | `					return rc;` |
|       - |  9828 | `				}` |
|       - |  9829 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   59784 |  9830 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   29893 |  9831 | `			}` |
|   74344 |  9832 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   59784 |  9833 | `				iP1 = 1; /* Node have an index associated with it */` |
|   29891 |  9834 | `			}` |
|   74344 |  9835 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - |  9836 | `				/* Create an empty entry when the desired index is not found */` |
|   29380 |  9837 | `				iP2 = 1;` |
|   14691 |  9838 | `			}` |
|  620039 |  9839 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - |  9840 | `			/* POP the left node */` |
|      32 |  9841 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 |  9842 | `		}` |
|  494529 |  9843 | `	}` |
|  989098 |  9844 | `	rc = SXRET_OK;` |
|  989098 |  9845 | `	nJmpIdx = 0;` |
|       - |  9846 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - |  9847 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - |  9848 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  989098 |  9849 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     266 |  9850 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     266 |  9851 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     266 |  9852 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     266 |  9853 | `			int isSpecial = 0;` |
|     266 |  9854 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     178 |  9855 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     178 |  9856 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     191 |  9857 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     156 |  9858 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      81 |  9859 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      90 |  9860 | `					isSpecial = 1;` |
|      44 |  9861 | `				}` |
|     110 |  9862 | `			}` |
|     310 |  9863 | `			pInstr->iP1 = 0;` |
|     310 |  9864 | `			if( !isSpecial ){` |
|     134 |  9865 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      66 |  9866 | `			}` |
|       - |  9867 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - |  9868 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     222 |  9869 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     134 |  9870 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     134 |  9871 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 |  9872 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 |  9873 | `					return SXRET_OK;` |
|       - |  9874 | `				}` |
|      45 |  9875 | `			}` |
|      89 |  9876 | `		}` |
|     165 |  9877 | `	}` |
|       - |  9878 | `	/* Generate code for the right tree */` |
|  989020 |  9879 | `	if( pNode->pRight ){` |
|  516828 |  9880 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - |  9881 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9116 |  9882 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  512271 |  9883 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - |  9884 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3044 |  9885 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  506193 |  9886 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - |  9887 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      54 |  9888 | `			iVmOp = 0; /* No binary operator to emit */` |
|      54 |  9889 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  504695 |  9890 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - |  9891 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - |  9892 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - |  9893 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - |  9894 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - |  9895 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - |  9896 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     100 |  9897 | `			sxu32 nNsJmp = 0;` |
|     100 |  9898 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     100 |  9899 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  504571 |  9900 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  225518 |  9901 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  112758 |  9902 | `		}` |
|  516828 |  9903 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  516828 |  9904 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  516828 |  9905 | `		if( !bIsChainOp ){` |
|       - |  9906 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - |  9907 | `			 * operator instruction is emitted. */` |
|  405012 |  9908 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  202505 |  9909 | `		}` |
|  516828 |  9910 | `		if( iVmOp == PH7_OP_STORE ){` |
|  222436 |  9911 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  222410 |  9912 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - |  9913 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - |  9914 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - |  9915 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - |  9916 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - |  9917 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - |  9918 | `				 */` |
|      54 |  9919 | `				iVmOp = 0;` |
|  222410 |  9920 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  222384 |  9921 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - |  9922 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   49560 |  9923 | `					iP2 = 1;` |
|   24781 |  9924 | `				}else{` |
|  172826 |  9925 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - |  9926 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   29318 |  9927 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   29318 |  9928 | `						iP1 = pInstr->iP1;` |
|   14660 |  9929 | `					}else{` |
|  143510 |  9930 | `						p3 = pInstr->p3;` |
|       - |  9931 | `					}` |
|       - |  9932 | `					/* POP the last dynamic load instruction */` |
|  172826 |  9933 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - |  9934 | `				}` |
|  111193 |  9935 | `			}` |
|  405611 |  9936 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 |  9937 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 |  9938 | `			if( pInstr ){` |
|      48 |  9939 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - |  9940 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - |  9941 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - |  9942 | `					 */` |
|      15 |  9943 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 |  9944 | `					iP1 = pInstr->iP1;` |
|      15 |  9945 | `					iP2 = pInstr->iP2;` |
|      15 |  9946 | `					p3  = pInstr->p3;` |
|       8 |  9947 | `				}else{` |
|      34 |  9948 | `					p3 = pInstr->p3;` |
|       - |  9949 | `				}` |
|      23 |  9950 | `			}` |
|      23 |  9951 | `		}` |
|  258413 |  9952 | `	}` |
|  989020 |  9953 | `	if( iVmOp > 0 ){` |
|  988886 |  9954 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   11854 |  9955 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - |  9956 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8704 |  9957 | `				iP1 = 1;` |
|    4353 |  9958 | `			}` |
|  982960 |  9959 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - |  9960 | `			/* Namespace-qualify the class name for NEW */ {` |
|   15176 |  9961 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   15176 |  9962 | `				VmInstr *pCallInstr = 0;` |
|   15176 |  9963 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   15160 |  9964 | `					pCallInstr = pPeek;` |
|   15160 |  9965 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7579 |  9966 | `				}` |
|   15176 |  9967 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|       - |  9968 | `					sxu32 nLitForClass;` |
|       - |  9969 | `					/* If the CALL handler already qualified the name using` |
|       - |  9970 | `					 * function imports, recover the original unqualified` |
|       - |  9971 | `					 * literal so we can re-qualify with class imports. */` |
|   15174 |  9972 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 |  9973 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 |  9974 | `					}else{` |
|   15142 |  9975 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - |  9976 | `					}` |
|   15174 |  9977 | `					pPeek->iP1 = 0;` |
|   15174 |  9978 | `					pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7586 |  9979 | `				}` |
|       - |  9980 | `			}` |
|   15176 |  9981 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   15176 |  9982 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - |  9983 | `				VmInstr *pPrev;` |
|   15160 |  9984 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   15160 |  9985 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - |  9986 | `					/* Pop the call instruction, preserve named-arg map */` |
|   15160 |  9987 | `					iP1 = pInstr->iP1;` |
|   15160 |  9988 | `					if( pInstr->p3 ){` |
|      38 |  9989 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      18 |  9990 | `					}` |
|   15160 |  9991 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7579 |  9992 | `				}` |
|    7581 |  9993 | `			}` |
|  969447 |  9994 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - |  9995 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - |  9996 | `			 * Namespace-qualify it, but skip self/static/parent. */` |
|      50 |  9997 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      50 |  9998 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      50 |  9999 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      50 | 10000 | `				int isSpecialIs = 0;` |
|      50 | 10001 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      46 | 10002 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      46 | 10003 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      49 | 10004 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      41 | 10005 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      23 | 10006 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 10007 | `						isSpecialIs = 1;` |
|       5 | 10008 | `					}` |
|      23 | 10009 | `				}` |
|      52 | 10010 | `				pInstr->iP1 = 0;` |
|      52 | 10011 | `				if( !isSpecialIs ){` |
|      38 | 10012 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      18 | 10013 | `				}` |
|      25 | 10014 | `			}` |
|  961839 | 10015 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10016 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10017 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10018 | `			 * should not trigger constant lookup. */` |
|  111818 | 10019 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  111818 | 10020 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  111784 | 10021 | `				pInstr->iP1 = 0;` |
|   55891 | 10022 | `			}` |
|  111818 | 10023 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10024 | `				/* Static member access,remember that */` |
|     188 | 10025 | `				iP1 = 1;` |
|     188 | 10026 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     188 | 10027 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      28 | 10028 | `					p3 = pInstr->p3;` |
|      28 | 10029 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      13 | 10030 | `				}` |
|      93 | 10031 | `			}` |
|   55908 | 10032 | `		}` |
|       - | 10033 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  988884 | 10034 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  494441 | 10035 | `	}` |
|  989018 | 10036 | `	if( nJmpIdx > 0 ){` |
|       - | 10037 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   12210 | 10038 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   12210 | 10039 | `		if( pInstr ){` |
|   12210 | 10040 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6104 | 10041 | `		}` |
|    6104 | 10042 | `	}` |
|  989018 | 10043 | `	return rc;` |
| 1303544 | 10044 |  |
|       - | 10045 | `/*` |
|       - | 10046 | ` * Compile a PHP expression.` |
|       - | 10047 | ` * According to the PHP language reference manual:` |
|       - | 10048 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10049 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10050 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10051 | ` *  is "anything that has a value".` |
|       - | 10052 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10053 | ` * function takes care of generating the appropriate error` |
|       - | 10054 | ` * message.` |
|       - | 10055 | ` */` |
|  704610 | 10056 | `static sxi32 PH7_CompileExpr(` |
|       - | 10057 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10058 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10059 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10060 | `	)` |
|       2 | 10061 |  |
|       - | 10062 | `	ph7_expr_node *pRoot;` |
|       - | 10063 | `	SySet sExprNode;` |
|       - | 10064 | `	SyToken *pEnd;` |
|       - | 10065 | `	sxi32 nExpr;` |
|       - | 10066 | `	sxi32 iNest;` |
|       - | 10067 | `	sxi32 rc;` |
|       - | 10068 | `	sxu32 nNullsafeBase;` |
|       - | 10069 | `	/* Initialize worker variables */` |
|  704612 | 10070 | `	nExpr = 0;` |
|  704612 | 10071 | `	pRoot = 0;` |
|       - | 10072 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10073 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  704612 | 10074 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  704612 | 10075 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  704612 | 10076 | `	SySetAlloc(&sExprNode,0x10);` |
|  704612 | 10077 | `	rc = SXRET_OK;` |
|       - | 10078 | `	/* Delimit the expression */` |
|  704612 | 10079 | `	pEnd = pGen->pIn;` |
|  704612 | 10080 | `	iNest = 0;` |
| 4749060 | 10081 | `	while( pEnd < pGen->pEnd ){` |
| 4503270 | 10082 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10083 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     326 | 10084 | `			iNest++;` |
| 4503108 | 10085 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     334 | 10086 | `			iNest--;` |
| 4502780 | 10087 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  459036 | 10088 | `			if( iNest <= 0 ){` |
|  458822 | 10089 | `				break;` |
|       - | 10090 | `			}` |
|     107 | 10091 | `		}` |
| 4044450 | 10092 | `		pEnd++;` |
|       2 | 10093 | `	}` |
|  704612 | 10094 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   11950 | 10095 | `		SyToken *pEnd2 = pGen->pIn;` |
|   11950 | 10096 | `		iNest = 0;` |
|       - | 10097 | `		/* Stop at the first comma */` |
|   23948 | 10098 | `		while( pEnd2 < pEnd ){` |
|   12004 | 10099 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      16 | 10100 | `				iNest++;` |
|   11997 | 10101 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      16 | 10102 | `				iNest--;` |
|   11983 | 10103 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      13 | 10104 | `				if( iNest <= 0 ){` |
|       5 | 10105 | `					break;` |
|       - | 10106 | `				}` |
|       4 | 10107 | `			}` |
|   12000 | 10108 | `			pEnd2++;` |
|       2 | 10109 | `		}` |
|   11950 | 10110 | `		if( pEnd2 <pEnd ){` |
|       5 | 10111 | `			pEnd = pEnd2;` |
|       2 | 10112 | `		}` |
|    5974 | 10113 | `	}` |
|  704612 | 10114 | `	if( pEnd > pGen->pIn ){` |
|  704602 | 10115 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10116 | `		/* Swap delimiter */` |
|  704602 | 10117 | `		pGen->pEnd = pEnd;` |
|       - | 10118 | `		/* Try to get an expression tree */` |
|  704602 | 10119 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  704602 | 10120 | `		if( rc == SXRET_OK && pRoot ){` |
|  704420 | 10121 | `			rc = SXRET_OK;` |
|  704420 | 10122 | `			if( xTreeValidator ){` |
|       - | 10123 | `				/* Call the upper layer validator callback */` |
|   21584 | 10124 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   10791 | 10125 | `			}` |
|  704420 | 10126 | `			if( rc != SXERR_ABORT ){` |
|       - | 10127 | `				/* Generate code for the given tree */` |
|  704420 | 10128 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10129 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10130 | `				 * expression so they short-circuit to its end. */` |
|  704420 | 10131 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  352209 | 10132 | `			}` |
|  704420 | 10133 | `			nExpr = 1;` |
|  352209 | 10134 | `		}` |
|       - | 10135 | `		/* Release the whole tree */` |
|  704602 | 10136 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10137 | `		/* Synchronize token stream */` |
|  704602 | 10138 | `		pGen->pEnd = pTmp;` |
|  704602 | 10139 | `		pGen->pIn  = pEnd;` |
|  704602 | 10140 | `		if( rc == SXERR_ABORT ){` |
|      11 | 10141 | `			SySetRelease(&sExprNode);` |
|      11 | 10142 | `			return SXERR_ABORT;` |
|       - | 10143 | `		}` |
|  352295 | 10144 | `	}` |
|  704602 | 10145 | `	SySetRelease(&sExprNode);` |
|  704602 | 10146 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  352307 | 10147 |  |
|       - | 10148 | `/*` |
|       - | 10149 | ` * Return a pointer to the node construct handler associated` |
|       - | 10150 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10151 | ` */` |
|  175198 | 10152 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 10153 |  |
|  175200 | 10154 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10155 | `		/* Numeric literal: Either real or integer */` |
|   96210 | 10156 | `		return PH7_CompileNumLiteral;` |
|   78992 | 10157 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10158 | `		/* Double quoted string */` |
|   16894 | 10159 | `		return PH7_CompileString;` |
|   62100 | 10160 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10161 | `		/* Single quoted string */` |
|   61988 | 10162 | `		return PH7_CompileSimpleString;` |
|     114 | 10163 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10164 | `		/* Heredoc */` |
|      66 | 10165 | `		return PH7_CompileHereDoc;` |
|      50 | 10166 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10167 | `		/* Nowdoc */` |
|      44 | 10168 | `		return PH7_CompileNowDoc;` |
|       7 | 10169 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10170 | `		/* Backtick quoted string */` |
|       5 | 10171 | `		return PH7_CompileBacktic;` |
|       - | 10172 | `	}` |
|       3 | 10173 | `	return 0;` |
|   87601 | 10174 |  |
|       - | 10175 | `/*` |
|       - | 10176 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10177 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10178 | ` * in write context" parse error.` |
|       - | 10179 | ` */` |
|    6454 | 10180 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       2 | 10181 |  |
|       - | 10182 | `	sxi32 rc;` |
|    6456 | 10183 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6454 | 10184 | `		return SXRET_OK;` |
|       - | 10185 | `	}` |
|       5 | 10186 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10187 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10188 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10189 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3229 | 10190 |  |
|       - | 10191 | `/*` |
|       - | 10192 | ` * Compile an unset() statement.` |
|       - | 10193 | ` * unset($var, $arr[$key], ...);` |
|       - | 10194 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10195 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10196 | ` * parent array before extracting the element to unset.` |
|       - | 10197 | ` */` |
|    2798 | 10198 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 10199 |  |
|    2800 | 10200 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2800 | 10201 | `	sxu32 nIdx = 0;` |
|       - | 10202 | `	SyString sName;` |
|       - | 10203 | `	sxi32 rc;` |
|       - | 10204 | `	/* Jump the 'unset' keyword */` |
|    2800 | 10205 | `	pGen->pIn++;` |
|       - | 10206 | `	/* Save delimiter */` |
|    2800 | 10207 | `	pTmp = pGen->pEnd;` |
|       - | 10208 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2800 | 10209 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2800 | 10210 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10211 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10212 | `		SyToken *pClose;` |
|    2800 | 10213 | `		pGen->pIn++;   /* Skip '(' */` |
|    2800 | 10214 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2800 | 10215 | `		pEnd = pClose; /* Stop at ')' */` |
|    1399 | 10216 | `	}` |
|    2800 | 10217 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10218 | `	/* Resolve the 'unset' builtin name once */` |
|    2800 | 10219 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     336 | 10220 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     336 | 10221 | `		if( pObj == 0 ){` |
|     ! 0 | 10222 | `			return SXERR_ABORT;` |
|       - | 10223 | `		}` |
|     336 | 10224 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     336 | 10225 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     167 | 10226 | `	}` |
|       - | 10227 | `	/* Compile each comma-separated argument */` |
|    9256 | 10228 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6458 | 10229 | `		if( pGen->pIn < pNext ){` |
|    6458 | 10230 | `			pGen->pEnd = pNext;` |
|    6458 | 10231 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10232 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,` |
|       - | 10233 | `				GenStateUnsetValidator);` |
|    6458 | 10234 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10235 | `				return SXERR_ABORT;` |
|       - | 10236 | `			}` |
|    6458 | 10237 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10238 | `				/* Emit call for this single argument */` |
|    6456 | 10239 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6456 | 10240 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,0,0);` |
|    6456 | 10241 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3227 | 10242 | `			}` |
|    3228 | 10243 | `		}` |
|       - | 10244 | `		/* Jump trailing commas */` |
|   10118 | 10245 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3662 | 10246 | `			pNext++;` |
|       2 | 10247 | `		}` |
|    6458 | 10248 | `		pGen->pIn = pNext;` |
|       2 | 10249 | `	}` |
|       - | 10250 | `	/* Skip past the closing ')' if present */` |
|    2800 | 10251 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2800 | 10252 | `		pGen->pIn++;` |
|    1399 | 10253 | `	}` |
|       - | 10254 | `	/* Restore token stream */` |
|    2800 | 10255 | `	pGen->pEnd = pTmp;` |
|    2800 | 10256 | `	return SXRET_OK;` |
|    1401 | 10257 |  |
|       - | 10258 | `/*` |
|       - | 10259 | ` * PHP Language construct table.` |
|       - | 10260 | ` */` |
|       - | 10261 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10262 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10263 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10264 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10265 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10266 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10267 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10268 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10269 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10270 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10271 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10272 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10273 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10274 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10275 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10276 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10277 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10278 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10279 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10280 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10281 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10282 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10283 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10284 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10285 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10286 | `};` |
|       - | 10287 | `/*` |
|       - | 10288 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10289 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10290 | ` */` |
|  427040 | 10291 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10292 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10293 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10294 | `	)` |
|       2 | 10295 |  |
|  427042 | 10296 | `	sxu32 n = 0;` |
| 1797999 | 10297 | `	for(;;){` |
| 3596000 | 10298 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   50224 | 10299 | `			break;` |
|       - | 10300 | `		}` |
| 3545778 | 10301 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  376820 | 10302 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10303 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10304 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10305 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10306 | `					return 0;` |
|       - | 10307 | `				}` |
|     ! 0 | 10308 | `			}` |
|  376818 | 10309 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 10310 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 10311 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10312 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10313 | `				return 0;` |
|       - | 10314 | `			}` |
|       - | 10315 | `			/* Return a pointer to the handler.` |
|       - | 10316 | `			*/` |
|  376820 | 10317 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10318 | `		}` |
| 3168960 | 10319 | `		n++;` |
|       2 | 10320 | `	}` |
|   50224 | 10321 | `	if( pLookahed ){` |
|   50224 | 10322 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|    8734 | 10323 | `			return PH7_CompileClassInterface;` |
|   41492 | 10324 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   41280 | 10325 | `			return PH7_CompileClass;` |
|     214 | 10326 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 | 10327 | `			return PH7_CompileTrait;` |
|     158 | 10328 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      21 | 10329 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      20 | 10330 | `				return PH7_CompileAbstractClass;` |
|     140 | 10331 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 10332 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10333 | `				return PH7_CompileFinalClass;` |
|       - | 10334 | `		}` |
|      69 | 10335 | `	}` |
|       - | 10336 | `	/* Not a language construct */` |
|     140 | 10337 | `	return 0;` |
|  213522 | 10338 |  |
|       - | 10339 | `/*` |
|       - | 10340 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10341 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10342 | ` */` |
|     138 | 10343 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 10344 |  |
|       - | 10345 | `	int rc;` |
|     140 | 10346 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     140 | 10347 | `	if( rc == FALSE ){` |
|      44 | 10348 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 | 10349 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10350 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10351 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10352 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10353 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10354 | `			*/` |
|       - | 10355 | `			){` |
|      38 | 10356 | `				rc = TRUE;` |
|      18 | 10357 | `		}` |
|      22 | 10358 | `	}` |
|     140 | 10359 | `	return rc;` |
|       2 | 10360 |  |
|       - | 10361 | `/*` |
|       - | 10362 | ` * Compile a PHP chunk.` |
|       - | 10363 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10364 | ` * takes care of generating the appropriate error message.` |
|       - | 10365 | ` */` |
|  573320 | 10366 | `static sxi32 GenStateCompileChunk(` |
|       - | 10367 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10368 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10369 | `	)` |
|       2 | 10370 |  |
|       - | 10371 | `	ProcLangConstruct xCons;` |
|       - | 10372 | `	sxi32 rc;` |
|  573322 | 10373 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  342594 | 10374 | `	for(;;){` |
|  685190 | 10375 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10376 | `			/* No more input to process */` |
|   12336 | 10377 | `			break;` |
|       - | 10378 | `		}` |
|  672856 | 10379 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10380 | `			/* Compile block */` |
|      16 | 10381 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      16 | 10382 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10383 | `				break;` |
|       - | 10384 | `			}` |
|       9 | 10385 | `		}else{` |
|  672842 | 10386 | `			xCons = 0;` |
|  672842 | 10387 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  427042 | 10388 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10389 | `				/* Try to extract a language construct handler */` |
|  427042 | 10390 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  427042 | 10391 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10392 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10393 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10394 | `						&pGen->pIn->sData);` |
|       9 | 10395 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10396 | `						break;` |
|       - | 10397 | `					}` |
|       - | 10398 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10399 | `					 * this erroneous statement.` |
|       - | 10400 | `					 */` |
|       9 | 10401 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10402 | `				}` |
|  459322 | 10403 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   43016 | 10404 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10405 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10406 | `				xCons = PH7_CompileLabel;` |
|      56 | 10407 | `			}` |
|  672842 | 10408 | `			if( xCons == 0 ){` |
|       - | 10409 | `				/* Assume an expression an try to compile it */` |
|  245820 | 10410 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  245820 | 10411 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10412 | `					/* Pop l-value */` |
|  245670 | 10413 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  122834 | 10414 | `				}` |
|  122911 | 10415 | `			}else{` |
|       - | 10416 | `				/* Go compile the sucker */` |
|  427024 | 10417 | `				rc = xCons(&(*pGen));` |
|       - | 10418 | `			}` |
|  672842 | 10419 | `			if( rc == SXERR_ABORT ){` |
|       - | 10420 | `				/* Request to abort compilation */` |
|      11 | 10421 | `				break;` |
|       - | 10422 | `			}` |
|       - | 10423 | `		}` |
|       - | 10424 | `		/* Ignore trailing semi-colons ';' */` |
| 1114278 | 10425 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  441434 | 10426 | `			pGen->pIn++;` |
|       2 | 10427 | `		}` |
|  672846 | 10428 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10429 | `			/* Compile a single statement and return */` |
|  560978 | 10430 | `			break;` |
|       - | 10431 | `		}` |
|       - | 10432 | `		/* LOOP ONE */` |
|       - | 10433 | `		/* LOOP TWO */` |
|       - | 10434 | `		/* LOOP THREE */` |
|       - | 10435 | `		/* LOOP FOUR */` |
|       2 | 10436 | `	}` |
|       - | 10437 | `	/* Return compilation status */` |
|  573322 | 10438 | `	return rc;` |
|       2 | 10439 |  |
|       - | 10440 | `/*` |
|       - | 10441 | ` * Compile a Raw PHP chunk.` |
|       - | 10442 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10443 | ` * takes care of generating the appropriate error message.` |
|       - | 10444 | ` */` |
|   12346 | 10445 | `static sxi32 PH7_CompilePHP(` |
|       - | 10446 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10447 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10448 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10449 | `	)` |
|       2 | 10450 |  |
|   12348 | 10451 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10452 | `	sxi32 rc;` |
|       - | 10453 | `	/* Reset the token set */` |
|   12348 | 10454 | `	SySetReset(&(*pTokenSet));` |
|       - | 10455 | `	/* Mark as the default token set */` |
|   12348 | 10456 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10457 | `	/* Advance the stream cursor */` |
|   12348 | 10458 | `	pGen->pRawIn++;` |
|       - | 10459 | `	/* Tokenize the PHP chunk first */` |
|   12348 | 10460 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10461 | `	/* Point to the head and tail of the token stream. */` |
|   12348 | 10462 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   12348 | 10463 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   12348 | 10464 | `	if( is_expr ){` |
|     ! 0 | 10465 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10466 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10467 | `			/* A simple expression,compile it */` |
|     ! 0 | 10468 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10469 | `		}` |
|       - | 10470 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10471 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10472 | `		return SXRET_OK;` |
|       - | 10473 | `	}` |
|   12348 | 10474 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10475 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10476 | `		/*` |
|       - | 10477 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10478 | `		 * According to the PHP reference manual:` |
|       - | 10479 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10480 | `		 *  immediately follow` |
|       - | 10481 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10482 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10483 | `		 * Symisc extension:` |
|       - | 10484 | `		 *   This short syntax works with all PHP opening` |
|       - | 10485 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10486 | `		 *   only short tag.` |
|       - | 10487 | `		 */` |
|       - | 10488 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10489 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10490 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 10491 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 10492 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 10493 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 10494 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 10495 | `		}` |
|       3 | 10496 | `		return SXRET_OK;` |
|       - | 10497 | `	}` |
|       - | 10498 | `	/* Compile the PHP chunk */` |
|   12346 | 10499 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 10500 | `	/* Fix exceptions jumps */` |
|   12346 | 10501 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10502 | `	/* Fix gotos now, the jump destination is resolved */` |
|   12346 | 10503 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 10504 | `		rc = SXERR_ABORT;` |
|       1 | 10505 | `	}` |
|       - | 10506 | `	/* Reset container */` |
|   12346 | 10507 | `	SySetReset(&pGen->aGoto);` |
|   12346 | 10508 | `	SySetReset(&pGen->aLabel);` |
|   12346 | 10509 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 10510 | `	/* Compilation result */` |
|   12346 | 10511 | `	return rc;` |
|    6175 | 10512 |  |
|       - | 10513 | `/*` |
|       - | 10514 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 10515 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 10516 | ` * This is the only compile interface exported from this file.` |
|       - | 10517 | ` */` |
|   14628 | 10518 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 10519 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 10520 | `	SyString *pScript,  /* Script to compile */` |
|       - | 10521 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 10522 | `	)` |
|       2 | 10523 |  |
|       - | 10524 | `	SySet aPhpToken,aRawToken;` |
|       - | 10525 | `	ph7_gen_state *pCodeGen;` |
|       - | 10526 | `	ph7_value *pRawObj;` |
|       - | 10527 | `	sxu32 nObjIdx;` |
|       - | 10528 | `	sxi32 nRawObj;` |
|       - | 10529 | `	int is_expr;` |
|       - | 10530 | `	sxi32 rc;` |
|   14630 | 10531 | `	if( pScript->nByte < 1 ){` |
|       - | 10532 | `		/* Nothing to compile */` |
|     ! 0 | 10533 | `		return PH7_OK;` |
|       - | 10534 | `	}` |
|       - | 10535 | `	/* Initialize the tokens containers */` |
|   14630 | 10536 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14630 | 10537 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14630 | 10538 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14630 | 10539 | `	is_expr = 0;` |
|   14630 | 10540 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 10541 | `		SyToken sTmp;` |
|       - | 10542 | `		/* PHP only: -*/` |
|    2928 | 10543 | `		sTmp.nLine = 1;` |
|    2928 | 10544 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2928 | 10545 | `		sTmp.pUserData = 0;` |
|    2928 | 10546 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2928 | 10547 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2928 | 10548 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 10549 | `			/* A simple PHP expression */` |
|     ! 0 | 10550 | `			is_expr = 1;` |
|     ! 0 | 10551 | `		}` |
|    1465 | 10552 | `	}else{` |
|       - | 10553 | `		/* Tokenize raw text */` |
|   11704 | 10554 | `		SySetAlloc(&aRawToken,32);` |
|   11704 | 10555 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 10556 | `	}` |
|   14630 | 10557 | `	pCodeGen = &pVm->sCodeGen;` |
|       - | 10558 | `	/* Process high-level tokens */` |
|   14630 | 10559 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14630 | 10560 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14630 | 10561 | `	rc = PH7_OK;` |
|   14630 | 10562 | `	if( is_expr ){` |
|       - | 10563 | `		/* Compile the expression */` |
|     ! 0 | 10564 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 10565 | `		goto cleanup;` |
|       - | 10566 | `	}` |
|   14630 | 10567 | `	nObjIdx = 0;` |
|       - | 10568 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 10569 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 10570 | `	 * preventing namespace bleeding across include()d files. */` |
|   14630 | 10571 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 10572 | `	/* Start the compilation process */` |
|   13169 | 10573 | `	for(;;){` |
|   38674 | 10574 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14618 | 10575 | `			break; /* No more tokens to process */` |
|       - | 10576 | `		}` |
|   24058 | 10577 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 10578 | `			/* Compile the PHP chunk */` |
|   12348 | 10579 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   12348 | 10580 | `			if( rc == SXERR_ABORT ){` |
|      13 | 10581 | `				break;` |
|       - | 10582 | `			}` |
|   12336 | 10583 | `			continue;` |
|       - | 10584 | `		}` |
|       - | 10585 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11712 | 10586 | `		nRawObj = 0;` |
|   23422 | 10587 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 10588 | `			/* Consume the raw chunk without any processing */` |
|   11712 | 10589 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11712 | 10590 | `			if( pRawObj == 0 ){` |
|     ! 0 | 10591 | `				rc = SXERR_MEM;` |
|     ! 0 | 10592 | `				break;` |
|       - | 10593 | `			}` |
|       - | 10594 | `			/* Mark as constant and emit the load constant instruction */` |
|   11712 | 10595 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11712 | 10596 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11712 | 10597 | `			++nRawObj;` |
|   11712 | 10598 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 10599 | `		}` |
|   11712 | 10600 | `		if( nRawObj > 0 ){` |
|       - | 10601 | `			/* Emit the consume instruction */` |
|   11712 | 10602 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5855 | 10603 | `		}` |
|    7316 | 10604 | `	}` |
|    7314 | 10605 | `cleanup:` |
|   14630 | 10606 | `	SySetRelease(&aRawToken);` |
|   14630 | 10607 | `	SySetRelease(&aPhpToken);` |
|   14630 | 10608 | `	return rc;` |
|    7316 | 10609 |  |
|       - | 10610 | `/*` |
|       - | 10611 | ` * Utility routines.Initialize the code generator.` |
|       - | 10612 | ` */` |
|    2898 | 10613 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 10614 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10615 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10616 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10617 | `	)` |
|       2 | 10618 |  |
|    2900 | 10619 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10620 | `	/* Zero the structure */` |
|    2900 | 10621 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 10622 | `	/* Initial state */` |
|    2900 | 10623 | `	pGen->pVm  = &(*pVm);` |
|    2900 | 10624 | `	pGen->xErr = xErr;` |
|    2900 | 10625 | `	pGen->pErrData = pErrData;` |
|    2900 | 10626 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2900 | 10627 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2900 | 10628 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    2900 | 10629 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2900 | 10630 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 10631 | `	/* Error log buffer */` |
|    2900 | 10632 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 10633 | `	/* General purpose working buffer */` |
|    2900 | 10634 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 10635 | `	/* Namespace state */` |
|    2900 | 10636 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2900 | 10637 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2900 | 10638 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2900 | 10639 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10640 | `	/* Create the global scope */` |
|    2900 | 10641 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 10642 | `	/* Point to the global scope */` |
|    2900 | 10643 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2900 | 10644 | `	return SXRET_OK;` |
|       2 | 10645 |  |
|       - | 10646 | `/*` |
|       - | 10647 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 10648 | ` */` |
|   17228 | 10649 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 10650 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10651 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10652 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10653 | `	)` |
|       2 | 10654 |  |
|   17230 | 10655 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10656 | `	GenBlock *pBlock,*pParent;` |
|       - | 10657 | `	/* Reset state */` |
|   17230 | 10658 | `	SySetReset(&pGen->aLabel);` |
|   17230 | 10659 | `	SySetReset(&pGen->aGoto);` |
|   17230 | 10660 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   17230 | 10661 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   17230 | 10662 | `	SyBlobRelease(&pGen->sWorker);` |
|   17230 | 10663 | `	SyBlobRelease(&pGen->sNamespace);` |
|   17230 | 10664 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   17230 | 10665 | `	SyHashRelease(&pGen->hUseImports);` |
|   17230 | 10666 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   17230 | 10667 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   17230 | 10668 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   17230 | 10669 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   17230 | 10670 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10671 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 10672 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 10673 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 10674 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 10675 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 10676 | `	 * number of unique names, which is acceptable. */` |
|       - | 10677 | `	/* Point to the global scope */` |
|   17230 | 10678 | `	pBlock = pGen->pCurrent;` |
|   17230 | 10679 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 10680 | `		pParent = pBlock->pParent;` |
|     ! 0 | 10681 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 10682 | `		pBlock = pParent;` |
|     ! 0 | 10683 | `	}` |
|   17230 | 10684 | `	pGen->xErr = xErr;` |
|   17230 | 10685 | `	pGen->pErrData = pErrData;` |
|   17230 | 10686 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   17230 | 10687 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   17230 | 10688 | `	pGen->pIn = pGen->pEnd = 0;` |
|   17230 | 10689 | `	pGen->nErr = 0;` |
|   17230 | 10690 | `	return SXRET_OK;` |
|       2 | 10691 |  |
|       - | 10692 | `/*` |
|       - | 10693 | ` * Generate a compile-time error message.` |
|       - | 10694 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 10695 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 10696 | ` * abort compilation immediately.` |
|       - | 10697 | ` */` |
|     554 | 10698 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 10699 |  |
|     556 | 10700 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     556 | 10701 | `	const char *zErr = "Error";` |
|       - | 10702 | `	SyString *pFile;` |
|       - | 10703 | `	va_list ap;` |
|       - | 10704 | `	sxi32 rc;` |
|       - | 10705 | `	/* Reset the working buffer */` |
|     556 | 10706 | `	SyBlobReset(pWorker);` |
|       - | 10707 | `	/* Peek the processed file path if available */` |
|     556 | 10708 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     556 | 10709 | `	if( nErrType == E_ERROR ){` |
|       - | 10710 | `		/* Increment the error counter */` |
|     454 | 10711 | `		pGen->nErr++;` |
|     454 | 10712 | `		if( pGen->nErr > 15 ){` |
|       - | 10713 | `			/* Error count limit reached */` |
|       5 | 10714 | `			if( pGen->xErr ){` |
|       5 | 10715 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 10716 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 10717 | `				if( pFile ){` |
|       5 | 10718 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 10719 | `				}` |
|       5 | 10720 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 10721 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 10722 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 10723 | `				}` |
|       2 | 10724 | `			}` |
|       - | 10725 | `			/* Abort immediately */` |
|       5 | 10726 | `			return SXERR_ABORT;` |
|       - | 10727 | `		}` |
|     224 | 10728 | `	}` |
|     552 | 10729 | `	if( pGen->xErr == 0 ){` |
|       - | 10730 | `		/* No available error consumer,return immediately */` |
|       3 | 10731 | `		return SXRET_OK;` |
|       - | 10732 | `	}` |
|     549 | 10733 | `	switch(nErrType){` |
|     447 | 10734 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 10735 | `	case E_WARNING: zErr = "Warning";     break;` |
|      69 | 10736 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 10737 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 10738 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 10739 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 10740 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 10741 | `	default:` |
|     ! 0 | 10742 | `		break;` |
|       - | 10743 | `	}` |
|     549 | 10744 | `	rc = SXRET_OK;` |
|       - | 10745 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     549 | 10746 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     549 | 10747 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     549 | 10748 | `	va_start(ap,zFormat);` |
|     549 | 10749 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     549 | 10750 | `	va_end(ap);` |
|     549 | 10751 | `	if( pFile ){` |
|     549 | 10752 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     274 | 10753 | `	}` |
|       - | 10754 | `	/* Append a new line */` |
|     549 | 10755 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     549 | 10756 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 10757 | `		/* Consume the generated error message */` |
|     549 | 10758 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     274 | 10759 | `	}` |
|     549 | 10760 | `	return rc;` |
|     279 | 10761 |  |
|       - | 10762 |  |
