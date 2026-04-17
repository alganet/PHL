# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5138/6484 lines (79.24%)

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
|    3202 |   128 | `static GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|       2 |   129 |  |
|    3204 |   130 | `	GenBlock *pBlock = pCurrent;` |
|    9041 |   131 | `	for(;;){` |
|   18084 |   132 | `		if( pBlock->iFlags & iBlockType ){` |
|    3096 |   133 | `			iCount--; /* Decrement nesting level */` |
|    3096 |   134 | `			if( iCount < 1 ){` |
|       - |   135 | `				/* Block meet with the desired criteria */` |
|    3070 |   136 | `				return pBlock;` |
|       - |   137 | `			}` |
|      13 |   138 | `		}` |
|       - |   139 | `		/* Point to the upper block */` |
|   15016 |   140 | `		pBlock = pBlock->pParent;` |
|   15016 |   141 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|       - |   142 | `			/* Forbidden */` |
|      69 |   143 | `			break;` |
|       - |   144 | `		}` |
|       2 |   145 | `	}` |
|       - |   146 | `	/* No such block */` |
|     136 |   147 | `	return 0;` |
|    1603 |   148 |  |
|       - |   149 | `/*` |
|       - |   150 | ` * Initialize a freshly allocated block instance.` |
|       - |   151 | ` */` |
|  691942 |   152 | `static void GenStateInitBlock(` |
|       - |   153 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |   154 | `	GenBlock *pBlock,    /* Target block */` |
|       - |   155 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   156 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|       - |   157 | `	void *pUserData      /* Upper layer private data */` |
|       - |   158 | `	)` |
|       2 |   159 |  |
|       - |   160 | `	/* Initialize block fields */` |
|  691944 |   161 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  691944 |   162 | `	pBlock->pUserData   = pUserData;` |
|  691944 |   163 | `	pBlock->pGen        = pGen;` |
|  691944 |   164 | `	pBlock->iFlags      = iType;` |
|  691944 |   165 | `	pBlock->pParent     = 0;` |
|  691944 |   166 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  691944 |   167 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  691944 |   168 |  |
|       - |   169 | `/*` |
|       - |   170 | ` * Allocate a new block instance.` |
|       - |   171 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|       - |   172 | ` * on success.Otherwise generate a compile-time error and abort` |
|       - |   173 | ` * processing on failure.` |
|       - |   174 | ` */` |
|  689006 |   175 | `static sxi32 GenStateEnterBlock(` |
|       - |   176 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |   177 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|       - |   178 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|       - |   179 | `	void *pUserData,      /* Upper layer private data */` |
|       - |   180 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|       - |   181 | `	)` |
|       2 |   182 |  |
|       - |   183 | `	GenBlock *pBlock;` |
|       - |   184 | `	/* Allocate a new block instance */` |
|  689008 |   185 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  689008 |   186 | `	if( pBlock == 0 ){` |
|       - |   187 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |   188 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|       - |   189 | `		 */` |
|     ! 0 |   190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|       - |   191 | `		/* Abort processing immediately */` |
|     ! 0 |   192 | `		return SXERR_ABORT;` |
|       - |   193 | `	}` |
|       - |   194 | `	/* Zero the structure */` |
|  689008 |   195 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  689008 |   196 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|       - |   197 | `	/* Link to the parent block */` |
|  689008 |   198 | `	pBlock->pParent = pGen->pCurrent;` |
|       - |   199 | `	/* Mark as the current block */` |
|  689008 |   200 | `	pGen->pCurrent = pBlock;` |
|  689008 |   201 | `	if( ppBlock ){` |
|       - |   202 | `		/* Write a pointer to the new instance */` |
|  334650 |   203 | `		*ppBlock = pBlock;` |
|  167324 |   204 | `	}` |
|  689008 |   205 | `	return SXRET_OK;` |
|  344505 |   206 |  |
|       - |   207 | `/*` |
|       - |   208 | ` * Release block fields without freeing the whole instance.` |
|       - |   209 | ` */` |
|  688998 |   210 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|       2 |   211 |  |
|  689000 |   212 | `	SySetRelease(&pBlock->aPostContFix);` |
|  689000 |   213 | `	SySetRelease(&pBlock->aJumpFix);` |
|  689000 |   214 |  |
|       - |   215 | `/*` |
|       - |   216 | ` * Release a block.` |
|       - |   217 | ` */` |
|  688998 |   218 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|       2 |   219 |  |
|  689000 |   220 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  689000 |   221 | `	GenStateReleaseBlock(&(*pBlock));` |
|       - |   222 | `	/* Free the instance */` |
|  689000 |   223 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  689000 |   224 |  |
|       - |   225 | `/*` |
|       - |   226 | ` * POP and release a block from the stack of compiled blocks.` |
|       - |   227 | ` */` |
|  688998 |   228 | `static sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|       2 |   229 |  |
|  689000 |   230 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  689000 |   231 | `	if( pBlock == 0 ){` |
|       - |   232 | `		/* No more block to pop */` |
|     ! 0 |   233 | `		return SXERR_EMPTY;` |
|       - |   234 | `	}` |
|       - |   235 | `	/* Point to the upper block */` |
|  689000 |   236 | `	pGen->pCurrent = pBlock->pParent;` |
|  689000 |   237 | `	if( ppBlock ){` |
|       - |   238 | `		/* Write a pointer to the popped block */` |
|     ! 0 |   239 | `		*ppBlock = pBlock;` |
|     ! 0 |   240 | `	}else{` |
|       - |   241 | `		/* Safely release the block */` |
|  689000 |   242 | `		GenStateFreeBlock(&(*pBlock));` |
|       - |   243 | `	}` |
|  689000 |   244 | `	return SXRET_OK;` |
|  344501 |   245 |  |
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
|  195838 |   256 | `static sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|       2 |   257 |  |
|       - |   258 | `	JumpFixup sJumpFix;` |
|       - |   259 | `	sxi32 rc;` |
|       - |   260 | `	/* Init the JumpFixup structure */` |
|  195840 |   261 | `	sJumpFix.nJumpType = nJumpType;` |
|  195840 |   262 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|       - |   263 | `	/* Insert in the jump fixup table */` |
|  195840 |   264 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|  195840 |   265 | `	return rc;` |
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
|  482316 |   278 | `static sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|       2 |   279 |  |
|       - |   280 | `	JumpFixup *aFix;` |
|       - |   281 | `	VmInstr *pInstr;` |
|       - |   282 | `	sxu32 nFixed;` |
|       - |   283 | `	sxu32 n;` |
|       - |   284 | `	/* Point to the jump fixup table */` |
|  482318 |   285 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|       - |   286 | `	/* Fix the desired jumps */` |
|  868464 |   287 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  386148 |   288 | `		if( aFix[n].nJumpType < 0 ){` |
|       - |   289 | `			/* Already fixed */` |
|  154286 |   290 | `			continue;` |
|       - |   291 | `		}` |
|  231864 |   292 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|       - |   293 | `			/* Not of our interest */` |
|   36028 |   294 | `			continue;` |
|       - |   295 | `		}` |
|       - |   296 | `		/* Point to the instruction to fix */` |
|  195838 |   297 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|  195838 |   298 | `		if( pInstr ){` |
|  195838 |   299 | `			pInstr->iP2 = nJumpDest;` |
|  195838 |   300 | `			nFixed++;` |
|       - |   301 | `			/* Mark as fixed */` |
|  195838 |   302 | `			aFix[n].nJumpType = -1;` |
|   97918 |   303 | `		}` |
|   97920 |   304 | `	}` |
|       - |   305 | `	/* Total number of fixed jumps */` |
|  482318 |   306 | `	return nFixed;` |
|       2 |   307 |  |
|       - |   308 | `/*` |
|       - |   309 | ` * Fix a 'goto' now the jump destination is resolved.` |
|       - |   310 | ` * The goto statement can be used to jump to another section` |
|       - |   311 | ` * in the program.` |
|       - |   312 | ` * Refer to the routine responsible of compiling the goto` |
|       - |   313 | ` * statement for more information.` |
|       - |   314 | ` */` |
|  195890 |   315 | `static sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|       2 |   316 |  |
|       - |   317 | `	JumpFixup *pJump,*aJumps;` |
|       - |   318 | `	Label *pLabel,*aLabel;` |
|       - |   319 | `	VmInstr *pInstr;` |
|       - |   320 | `	sxi32 rc;` |
|       - |   321 | `	sxu32 n;` |
|       - |   322 | `	/* Point to the goto table */` |
|  195892 |   323 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|       - |   324 | `	/* Fix */` |
|  196038 |   325 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|  195890 |   350 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|  196022 |   351 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     134 |   352 | `		if( aLabel[n].bRef == FALSE ){` |
|       - |   353 | `			/* Emit a warning */` |
|      37 |   354 | `			PH7_GenCompileError(&(*pGen),E_WARNING,aLabel[n].nLine,` |
|      24 |   355 | `				"Label '%z' is defined but not referenced",&aLabel[n].sName);` |
|      12 |   356 | `		}` |
|      68 |   357 | `	}` |
|  195890 |   358 | `	return SXRET_OK;` |
|   97947 |   359 |  |
|       - |   360 | `/*` |
|       - |   361 | ` * Check if a given token value is installed in the literal table.` |
|       - |   362 | ` */` |
|  619152 |   363 | `static sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|       2 |   364 |  |
|       - |   365 | `	SyHashEntry *pEntry;` |
|  619154 |   366 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  619154 |   367 | `	if( pEntry == 0 ){` |
|  269272 |   368 | `		return SXERR_NOTFOUND;` |
|       - |   369 | `	}` |
|  349884 |   370 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  349884 |   371 | `	return SXRET_OK;` |
|  309578 |   372 |  |
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
|  269270 |   383 | `static sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|       2 |   384 |  |
|  269272 |   385 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|  269272 |   386 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|  134635 |   387 | `	}` |
|  269272 |   388 | `	return SXRET_OK;` |
|       2 |   389 |  |
|       - |   390 | `/*` |
|       - |   391 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|       - |   392 | ` * in the constant table.` |
|       - |   393 | ` */` |
|  102672 |   394 | `static ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|       2 |   395 |  |
|       - |   396 | `	ph7_value *pObj;` |
|  102674 |   397 | `	sxu32 nIdx = 0; /* cc warning */` |
|       - |   398 | `	/* Reserve a new constant */` |
|  102674 |   399 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  102674 |   400 | `	if( pObj == 0 ){` |
|     ! 0 |   401 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   402 | `		return 0;` |
|       - |   403 | `	}` |
|  102674 |   404 | `	*pIdx = nIdx;` |
|       - |   405 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|       - |   406 | `	 * the constant string iterals table [optimization purposes].` |
|       - |   407 | `	 */` |
|  102674 |   408 | `	return pObj;` |
|   51338 |   409 |  |
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
|  370098 |   424 | `static void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|       2 |   425 |  |
|       - |   426 | `	VmCallArgMap *pMap;` |
|  370100 |   427 | `	if( !pGen->bStrictTypes ) return p3;` |
|      28 |   428 | `	if( p3 == 0 ){` |
|      28 |   429 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      28 |   430 | `		if( pMap == 0 ) return 0;` |
|      28 |   431 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|      28 |   432 | `		p3 = (void *)pMap;` |
|      13 |   433 | `	}` |
|      28 |   434 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      28 |   435 | `	return p3;` |
|  185051 |   436 |  |
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
|  103200 |   495 | `static int GenStateFindBadNumericSeparator(` |
|       - |   496 | `	const SyString *pRaw, const char **pBadStart, sxu32 *pBadLen)` |
|       2 |   497 |  |
|  103202 |   498 | `	const char *z = pRaw->zString;` |
|  103202 |   499 | `	sxu32 n = pRaw->nByte;` |
|  103202 |   500 | `	int base = 10;` |
|       - |   501 | `	sxu32 i, start;` |
|  103202 |   502 | `	if( n < 2 ) return 0;` |
|    8690 |   503 | `	if( z[0] == '0' && (z[1] == 'x' \|\| z[1] == 'X') ){` |
|      72 |   504 | `		base = 16;` |
|    8655 |   505 | `	}else if( z[0] == '0' && (z[1] == 'b' \|\| z[1] == 'B') ){` |
|     280 |   506 | `		base = 2;` |
|     139 |   507 | `	}` |
|   32028 |   508 | `	for( i = 0; i < n; ++i ){` |
|   23354 |   509 | `		if( z[i] != '_' ) continue;` |
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
|    8676 |   526 | `	return 0;` |
|   51602 |   527 |  |
|       - |   528 | `/*` |
|       - |   529 | ` * Emit the shared "syntax error, unexpected identifier" parse error when a` |
|       - |   530 | ` * numeric-literal token contains a misplaced PHP 7.4 separator. Returns` |
|       - |   531 | ` * SXRET_OK when the token is well-formed; on error propagates whatever` |
|       - |   532 | ` * PH7_GenCompileError returned (SXERR_ABORT when the error count is` |
|       - |   533 | ` * exhausted, otherwise the error is reported and SXERR_SYNTAX is returned` |
|       - |   534 | ` * so callers can bail from the current construct).` |
|       - |   535 | ` */` |
|  103200 |   536 | `static sxi32 GenStateValidateNumericSeparator(ph7_gen_state *pGen, SyToken *pToken)` |
|       2 |   537 |  |
|  103202 |   538 | `	const char *zBad = 0;` |
|  103202 |   539 | `	sxu32 nBad = 0;` |
|       - |   540 | `	SyString sBad;` |
|       - |   541 | `	sxi32 rc;` |
|  103202 |   542 | `	if( !GenStateFindBadNumericSeparator(&pToken->sData, &zBad, &nBad) ){` |
|  103188 |   543 | `		return SXRET_OK;` |
|       - |   544 | `	}` |
|      15 |   545 | `	SyStringInitFromBuf(&sBad, zBad, nBad);` |
|      15 |   546 | `	rc = PH7_GenCompileError(pGen, E_PARSE, pToken->nLine,` |
|       - |   547 | `		"syntax error, unexpected identifier \"%z\"", &sBad);` |
|      15 |   548 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |   549 | `		return SXERR_ABORT;` |
|       - |   550 | `	}` |
|      15 |   551 | `	return SXERR_SYNTAX;` |
|   51602 |   552 |  |
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
|  103186 |   569 | `static sxi32 GenStateStripNumericSeparators(` |
|       - |   570 | `	SyMemBackend *pAlloc,` |
|       - |   571 | `	const SyString *pToken,` |
|       - |   572 | `	char *zScratch, sxu32 nScratch,` |
|       - |   573 | `	SyString *pOut, char **pzAlloc)` |
|       2 |   574 |  |
|       - |   575 | `	sxu32 i, j;` |
|  103188 |   576 | `	int hasUnderscore = 0;` |
|       - |   577 | `	char *zBuf;` |
|  103188 |   578 | `	*pzAlloc = 0;` |
|  218972 |   579 | `	for( i = 0; i < pToken->nByte; ++i ){` |
|  116038 |   580 | `		if( pToken->zString[i] == '_' ){ hasUnderscore = 1; break; }` |
|   57894 |   581 | `	}` |
|  103188 |   582 | `	if( !hasUnderscore ){` |
|  102936 |   583 | `		SyStringDupPtr(pOut, pToken);` |
|  102936 |   584 | `		return SXRET_OK;` |
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
|   51595 |   601 |  |
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
|  103172 |   618 | `static sxi32 PH7_CompileNumLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   619 |  |
|  103174 |   620 | `	SyToken *pToken = pGen->pIn; /* Raw token */` |
|  103174 |   621 | `	sxu32 nIdx = 0;` |
|       - |   622 | `	char zScratch[GEN_NUM_SCRATCH];` |
|  103174 |   623 | `	char *zAlloc = 0;` |
|       - |   624 | `	SyString sNum;` |
|       - |   625 | `	sxi32 rc;` |
|   51586 |   626 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|  103174 |   627 | `	rc = GenStateValidateNumericSeparator(pGen, pToken);` |
|  103174 |   628 | `	if( rc != SXRET_OK ){` |
|      11 |   629 | `		return rc;` |
|       - |   630 | `	}` |
|  154745 |   631 | `	rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator, &pToken->sData,` |
|   51581 |   632 | `		zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|  103164 |   633 | `	if( rc != SXRET_OK ){` |
|     ! 0 |   634 | `		return SXERR_ABORT;` |
|       - |   635 | `	}` |
|  103164 |   636 | `	if( pToken->nType & PH7_TK_INTEGER ){` |
|       - |   637 | `		ph7_value *pObj;` |
|       - |   638 | `		sxi64 iValue;` |
|  102674 |   639 | `		iValue = PH7_TokenValueToInt64(&sNum);` |
|  102674 |   640 | `		pObj = GenStateInstallNumLiteral(&(*pGen),&nIdx);` |
|  102674 |   641 | `		if( pObj == 0 ){` |
|     ! 0 |   642 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   643 | `			return SXERR_ABORT;` |
|       - |   644 | `		}` |
|  102674 |   645 | `		PH7_MemObjInitFromInt(pGen->pVm,pObj,iValue);` |
|   51338 |   646 | `	}else{` |
|       - |   647 | `		/* Real number */` |
|       - |   648 | `		ph7_value *pObj;` |
|       - |   649 | `		/* Reserve a new constant */` |
|     492 |   650 | `		pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     492 |   651 | `		if( pObj == 0 ){` |
|     ! 0 |   652 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   653 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|     ! 0 |   654 | `			return SXERR_ABORT;` |
|       - |   655 | `		}` |
|     492 |   656 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sNum);` |
|     492 |   657 | `		PH7_MemObjToReal(pObj);` |
|       - |   658 | `	}` |
|  103164 |   659 | `	if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       - |   660 | `	/* Emit the load constant instruction */` |
|  103164 |   661 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |   662 | `	/* Node successfully compiled */` |
|  103164 |   663 | `	return SXRET_OK;` |
|   51588 |   664 |  |
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
|   74464 |   676 | `PH7_PRIVATE sxi32 PH7_CompileSimpleString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |   677 |  |
|   74466 |   678 | `	SyString *pStr = &pGen->pIn->sData; /* Constant string literal */` |
|       - |   679 | `	const char *zIn,*zCur,*zEnd;` |
|       - |   680 | `	ph7_value *pObj;` |
|       - |   681 | `	sxu32 nIdx;` |
|   74466 |   682 | `	nIdx = 0; /* Prevent compiler warning */` |
|       - |   683 | `	/* Delimit the string */` |
|   74466 |   684 | `	zIn  = pStr->zString;` |
|   74466 |   685 | `	zEnd = &zIn[pStr->nByte];` |
|   74466 |   686 | `	if( zIn >= zEnd ){` |
|       - |   687 | `		/* Empty string constant: just use the pre‑allocated index from the VM` |
|       - |   688 | `		 * rather than reserving a new object each time. */` |
|    6016 |   689 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|    6016 |   690 | `		return SXRET_OK;` |
|       - |   691 | `	}` |
|   68452 |   692 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - |   693 | `		/* Already processed,emit the load constant instruction` |
|       - |   694 | `		 * and return.` |
|       - |   695 | `		 */` |
|   27108 |   696 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   27108 |   697 | `		return SXRET_OK;` |
|       - |   698 | `	}` |
|       - |   699 | `	/* Reserve a new constant */` |
|   41346 |   700 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   41346 |   701 | `	if( pObj == 0 ){` |
|     ! 0 |   702 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |   703 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |   704 | `		return SXERR_ABORT;` |
|       - |   705 | `	}` |
|   41346 |   706 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |   707 | `	/* Compile the node */` |
|   41386 |   708 | `	for(;;){` |
|   82774 |   709 | `		if( zIn >= zEnd ){` |
|       - |   710 | `			/* End of input */` |
|   41346 |   711 | `			break;` |
|       - |   712 | `		}` |
|   41430 |   713 | `		zCur = zIn;` |
|  652016 |   714 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|  610588 |   715 | `			zIn++;` |
|       2 |   716 | `		}` |
|   41430 |   717 | `		if( zIn > zCur ){` |
|       - |   718 | `			/* Append raw contents*/` |
|   41410 |   719 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|   20704 |   720 | `		}` |
|   41430 |   721 | `		zIn++;` |
|   41430 |   722 | `		if( zIn < zEnd ){` |
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
|   41430 |   737 | `		zIn++;` |
|       2 |   738 | `	}` |
|       - |   739 | `	/* Emit the load constant instruction */` |
|   41346 |   740 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   41346 |   741 | `	if( pStr->nByte < 1024 ){` |
|       - |   742 | `		/* Install in the literal table */` |
|   41346 |   743 | `		GenStateInstallLiteral(pGen,pObj,nIdx);` |
|   20672 |   744 | `	}` |
|       - |   745 | `	/* Node successfully compiled */` |
|   41346 |   746 | `	return SXRET_OK;` |
|   37234 |   747 |  |
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
|    1946 |   913 | `static sxi32 GenStateProcessStringExpression(` |
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
|    1948 |   924 | `	SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       - |   925 | `	/* Preallocate some slots */` |
|    1948 |   926 | `	SySetAlloc(&sToken,0x08);` |
|       - |   927 | `	/* Tokenize the text */` |
|    1948 |   928 | `	PH7_TokenizePHP(zIn,(sxu32)(zEnd-zIn),nLine,&sToken);` |
|       - |   929 | `	/* Swap delimiter */` |
|    1948 |   930 | `	pTmpIn  = pGen->pIn;` |
|    1948 |   931 | `	pTmpEnd = pGen->pEnd;` |
|    1948 |   932 | `	pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|    1948 |   933 | `	pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|       - |   934 | `	/* Compile the expression */` |
|    1948 |   935 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |   936 | `	/* Restore token stream */` |
|    1948 |   937 | `	pGen->pIn  = pTmpIn;` |
|    1948 |   938 | `	pGen->pEnd = pTmpEnd;` |
|       - |   939 | `	/* Release the token set */` |
|    1948 |   940 | `	SySetRelease(&sToken);` |
|       - |   941 | `	/* Compilation result */` |
|    1948 |   942 | `	return rc;` |
|       2 |   943 |  |
|       - |   944 | `/*` |
|       - |   945 | ` * Reserve a new constant for a double quoted/heredoc string.` |
|       - |   946 | ` */` |
|   18628 |   947 | `static ph7_value * GenStateNewStrObj(ph7_gen_state *pGen,sxi32 *pCount)` |
|       2 |   948 |  |
|       - |   949 | `	ph7_value *pConstObj;` |
|   18630 |   950 | `	sxu32 nIdx = 0;` |
|       - |   951 | `	/* Reserve a new constant */` |
|   18630 |   952 | `	pConstObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   18630 |   953 | `	if( pConstObj == 0 ){` |
|     ! 0 |   954 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |   955 | `		return 0;` |
|       - |   956 | `	}` |
|   18630 |   957 | `	(*pCount)++;` |
|   18630 |   958 | `	PH7_MemObjInitFromString(pGen->pVm,pConstObj,0);` |
|       - |   959 | `	/* Emit the load constant instruction */` |
|   18630 |   960 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|   18630 |   961 | `	return pConstObj;` |
|    9316 |   962 |  |
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
|   17238 |  1001 | `static sxi32 GenStateCompileString(ph7_gen_state *pGen)` |
|       2 |  1002 |  |
|   17240 |  1003 | `	SyString *pStr = &pGen->pIn->sData; /* Raw token value */` |
|       - |  1004 | `	const char *zIn,*zCur,*zEnd;` |
|   17240 |  1005 | `	ph7_value *pObj = 0;` |
|       - |  1006 | `	sxi32 iCons;` |
|       - |  1007 | `	sxi32 rc;` |
|       - |  1008 | `	/* Delimit the string */` |
|   17240 |  1009 | `	zIn  = pStr->zString;` |
|   17240 |  1010 | `	zEnd = &zIn[pStr->nByte];` |
|   17240 |  1011 | `	if( zIn >= zEnd ){` |
|       - |  1012 | `		/* Empty string: use the shared constant reserved at VM initialization.` |
|       - |  1013 | `		 * This avoids creating a new literal for every occurrence and keeps the` |
|       - |  1014 | `		 * literal table from growing when many "" literals appear in the source.` |
|       - |  1015 | `		 */` |
|     234 |  1016 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,pGen->pVm->nEmptyStringIdx,0,0);` |
|     234 |  1017 | `		return SXRET_OK;` |
|       - |  1018 | `	}` |
|   17008 |  1019 | `	zCur = 0;` |
|       - |  1020 | `	/* Compile the node */` |
|   17008 |  1021 | `	iCons = 0;` |
|    9476 |  1022 | `	for(;;){` |
|   28680 |  1023 | `		zCur = zIn;` |
|  145580 |  1024 | `		while( zIn < zEnd && zIn[0] != '\\'  ){` |
|  118848 |  1025 | `			if( zIn[0] == '{' && &zIn[1] < zEnd && zIn[1] == '$' ){` |
|      59 |  1026 | `				break;` |
|  118734 |  1027 | `			}else if(zIn[0] == '$' && &zIn[1] < zEnd &&` |
|    1834 |  1028 | `				(((unsigned char)zIn[1] >= 0xc0 \|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '{' \|\| zIn[1] == '_')) ){` |
|     917 |  1029 | `					break;` |
|       - |  1030 | `			}` |
|  116902 |  1031 | `			zIn++;` |
|       2 |  1032 | `		}` |
|   28680 |  1033 | `		if( zIn > zCur ){` |
|   12984 |  1034 | `			if( pObj == 0 ){` |
|   12708 |  1035 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|   12708 |  1036 | `				if( pObj == 0 ){` |
|     ! 0 |  1037 | `					return SXERR_ABORT;` |
|       - |  1038 | `				}` |
|    6353 |  1039 | `			}` |
|   12984 |  1040 | `			PH7_MemObjStringAppend(pObj,zCur,(sxu32)(zIn-zCur));` |
|    6491 |  1041 | `		}` |
|   28680 |  1042 | `		if( zIn >= zEnd ){` |
|   17008 |  1043 | `			break;` |
|       - |  1044 | `		}` |
|   11674 |  1045 | `		if( zIn[0] == '\\' ){` |
|    9728 |  1046 | `			const char *zPtr = 0;` |
|       - |  1047 | `			sxu32 n;` |
|    9728 |  1048 | `			zIn++;` |
|    9728 |  1049 | `			if( zIn >= zEnd ){` |
|     ! 0 |  1050 | `				break;` |
|       - |  1051 | `			}` |
|    9728 |  1052 | `			if( pObj == 0 ){` |
|    5924 |  1053 | `				pObj = GenStateNewStrObj(&(*pGen),&iCons);` |
|    5924 |  1054 | `				if( pObj == 0 ){` |
|     ! 0 |  1055 | `					return SXERR_ABORT;` |
|       - |  1056 | `				}` |
|    2961 |  1057 | `			}` |
|    9728 |  1058 | `			n = sizeof(char); /* size of conversion */` |
|    9728 |  1059 | `			switch( zIn[0] ){` |
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
|    4498 |  1080 | `			case 'n':` |
|       - |  1081 | `				/* Line feed(new line) (LF)[ctrl+j] ASCII code 10 */` |
|    8998 |  1082 | `				PH7_MemObjStringAppend(pObj,"\n",sizeof(char));` |
|    8998 |  1083 | `				break;` |
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
|      50 |  1100 | `			case '"':` |
|       - |  1101 | `				/* Double quote */` |
|     102 |  1102 | `				PH7_MemObjStringAppend(pObj,"\"",sizeof(char));` |
|     102 |  1103 | `				break;` |
|       5 |  1104 | `			case '0':` |
|       - |  1105 | `				/* NUL byte */` |
|      11 |  1106 | `				PH7_MemObjStringAppend(pObj,"\0",sizeof(char));` |
|      11 |  1107 | `				break;` |
|     188 |  1108 | `			case 'x':` |
|     377 |  1109 | `				if((unsigned char)zIn[1] < 0xc0 && SyisHex(zIn[1]) ){` |
|       - |  1110 | `					int c;` |
|       - |  1111 | `					/* Hex digit */` |
|     363 |  1112 | `					c = SyHexToint(zIn[1]) << 4;` |
|     363 |  1113 | `					if( &zIn[2] < zEnd ){` |
|     363 |  1114 | `						c +=  SyHexToint(zIn[2]);` |
|     181 |  1115 | `					}` |
|       - |  1116 | `					/* Output char */` |
|     363 |  1117 | `					PH7_MemObjStringAppend(pObj,(const char *)&c,sizeof(char));` |
|     363 |  1118 | `					n += sizeof(char) * 2;` |
|     182 |  1119 | `				}else{` |
|       - |  1120 | `					/* Output literal character  */` |
|      15 |  1121 | `					PH7_MemObjStringAppend(pObj,"x",sizeof(char));` |
|       - |  1122 | `				}` |
|     377 |  1123 | `				break;` |
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
|    9728 |  1151 | `			zIn += n;` |
|    9728 |  1152 | `			continue;` |
|       - |  1153 | `		}` |
|    1948 |  1154 | `		if( zIn[0] == '{' ){` |
|       - |  1155 | `			/* Curly syntax */` |
|       - |  1156 | `			const char *zExpr;` |
|     117 |  1157 | `			sxi32 iNest = 1;` |
|     117 |  1158 | `			zIn++;` |
|     117 |  1159 | `			zExpr = zIn;` |
|       - |  1160 | `			/* Synchronize with the next closing curly braces */` |
|    1243 |  1161 | `			while( zIn < zEnd ){` |
|    1243 |  1162 | `				if( zIn[0] == '{' ){` |
|       - |  1163 | `					/* Increment nesting level */` |
|       9 |  1164 | `					iNest++;` |
|    1239 |  1165 | `				}else if(zIn[0] == '}' ){` |
|       - |  1166 | `					/* Decrement nesting level */` |
|     125 |  1167 | `					iNest--;` |
|     125 |  1168 | `					if( iNest <= 0 ){` |
|     117 |  1169 | `						break;` |
|       - |  1170 | `					}` |
|       4 |  1171 | `				}` |
|    1127 |  1172 | `				zIn++;` |
|       1 |  1173 | `			}` |
|       - |  1174 | `			/* Process the expression */` |
|     117 |  1175 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|     117 |  1176 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1177 | `				return SXERR_ABORT;` |
|       - |  1178 | `			}` |
|     117 |  1179 | `			if( rc != SXERR_EMPTY ){` |
|     117 |  1180 | `				++iCons;` |
|      58 |  1181 | `			}` |
|     117 |  1182 | `			if( zIn < zEnd ){` |
|       - |  1183 | `				/* Jump the trailing curly */` |
|     117 |  1184 | `				zIn++;` |
|      58 |  1185 | `			}` |
|      59 |  1186 | `		}else{` |
|       - |  1187 | `			/* Simple syntax */` |
|    1832 |  1188 | `			const char *zExpr = zIn;` |
|       - |  1189 | `			/* Assemble variable name */` |
|     921 |  1190 | `			for(;;){` |
|       - |  1191 | `				/* Jump leading dollars */` |
|    3674 |  1192 | `				while( zIn < zEnd && zIn[0] == '$' ){` |
|    1832 |  1193 | `					zIn++;` |
|       2 |  1194 | `				}` |
|     921 |  1195 | `				for(;;){` |
|   10785 |  1196 | `					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_' ) ){` |
|    8022 |  1197 | `						zIn++;` |
|       2 |  1198 | `					}` |
|    1844 |  1199 | `					if((unsigned char)zIn[0] >= 0xc0 ){` |
|       - |  1200 | `						/* UTF-8 stream */` |
|     ! 0 |  1201 | `						zIn++;` |
|     ! 0 |  1202 | `						while( zIn < zEnd && (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1203 | `							zIn++;` |
|     ! 0 |  1204 | `						}` |
|     ! 0 |  1205 | `						continue;` |
|       - |  1206 | `					}` |
|    1844 |  1207 | `					break;` |
|     ! 0 |  1208 | `				}` |
|    1844 |  1209 | `				if( zIn >= zEnd ){` |
|     118 |  1210 | `					break;` |
|       - |  1211 | `				}` |
|    1728 |  1212 | `				if( zIn[0] == '[' ){` |
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
|    1720 |  1230 | `				}else if(zIn[0] == '{' ){` |
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
|    1716 |  1248 | `				}else if( zIn[0] == '-' && &zIn[1] < zEnd && zIn[1] == '>' ){` |
|       - |  1249 | `					/* Member access operator '->' */` |
|      13 |  1250 | `					zIn += 2;` |
|    1710 |  1251 | `				}else if(zIn[0] == ':' && &zIn[1] < zEnd && zIn[1] == ':'){` |
|       - |  1252 | `					/* Static member access operator '::' */` |
|     ! 0 |  1253 | `					zIn += 2;` |
|     ! 0 |  1254 | `				}else{` |
|     853 |  1255 | `					break;` |
|       - |  1256 | `				}` |
|       1 |  1257 | `			}` |
|       - |  1258 | `			/* Process the expression */` |
|    1832 |  1259 | `			rc = GenStateProcessStringExpression(&(*pGen),pGen->pIn->nLine,zExpr,zIn);` |
|    1832 |  1260 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1261 | `				return SXERR_ABORT;` |
|       - |  1262 | `			}` |
|    1832 |  1263 | `			if( rc != SXERR_EMPTY ){` |
|    1830 |  1264 | `				++iCons;` |
|     914 |  1265 | `			}` |
|       - |  1266 | `		}` |
|       - |  1267 | `		/* Invalidate the previously used constant */` |
|    1948 |  1268 | `		pObj = 0;` |
|       2 |  1269 | `	}/*for(;;)*/` |
|   17008 |  1270 | `	if( iCons > 1 ){` |
|       - |  1271 | `		/* Concatenate all compiled constants */` |
|    1444 |  1272 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CAT,iCons,0,0,0);` |
|     721 |  1273 | `	}` |
|       - |  1274 | `	/* Node successfully compiled */` |
|   17008 |  1275 | `	return SXRET_OK;` |
|    8621 |  1276 |  |
|       - |  1277 | `/*` |
|       - |  1278 | ` * Compile a double quoted string.` |
|       - |  1279 | ` *  See the block-comment above for more information.` |
|       - |  1280 | ` */` |
|   17178 |  1281 | `PH7_PRIVATE sxi32 PH7_CompileString(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1282 |  |
|       - |  1283 | `	sxi32 rc;` |
|   17180 |  1284 | `	rc = GenStateCompileString(&(*pGen));` |
|    8589 |  1285 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  1286 | `	/* Compilation result */` |
|   17180 |  1287 | `	return rc;` |
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
|   17154 |  1331 | `static sxi32 GenStateCompileArrayEntry(` |
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
|   17156 |  1342 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|       - |  1343 | `	/* Compile the expression*/` |
|   17156 |  1344 | `	rc = PH7_CompileExpr(&(*pGen),iFlags,xValidator);` |
|       - |  1345 | `	/* Restore token stream */` |
|   17156 |  1346 | `	RE_SWAP_DELIMITER(pGen);` |
|   17156 |  1347 | `	return rc;` |
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
|   25550 |  1386 | `static sxi32 GenStateCompileArrayBody(ph7_gen_state *pGen)` |
|       2 |  1387 |  |
|       - |  1388 | `	sxi32 (*xValidator)(ph7_gen_state *,ph7_expr_node *); /* Expression tree validator callback */` |
|       - |  1389 | `	SyToken *pKey,*pCur;` |
|   25552 |  1390 | `	sxi32 iEmitRef = 0;` |
|   25552 |  1391 | `	sxi32 nPair = 0;` |
|       - |  1392 | `	sxi32 iNest;` |
|       - |  1393 | `	sxi32 rc;` |
|   25552 |  1394 | `	xValidator = 0;` |
|   20685 |  1395 | `	for(;;){` |
|       - |  1396 | `		/* Jump leading commas */` |
|   46676 |  1397 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|    5306 |  1398 | `			pGen->pIn++;` |
|       2 |  1399 | `		}` |
|   41372 |  1400 | `		pCur = pGen->pIn;` |
|   41372 |  1401 | `		if( SXRET_OK != PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pGen->pIn) ){` |
|       - |  1402 | `			/* No more entry to process */` |
|   25540 |  1403 | `			break;` |
|       - |  1404 | `		}` |
|   15834 |  1405 | `		if( pCur >= pGen->pIn ){` |
|     ! 0 |  1406 | `			continue;` |
|       - |  1407 | `		}` |
|       - |  1408 | `		/* Compile the key if available */` |
|   15834 |  1409 | `		pKey = pCur;` |
|   15834 |  1410 | `		iNest = 0;` |
|   44140 |  1411 | `		while( pCur < pGen->pIn ){` |
|   29528 |  1412 | `			if( (pCur->nType & PH7_TK_ARRAY_OP) && iNest <= 0 ){` |
|    1218 |  1413 | `				break;` |
|       - |  1414 | `			}` |
|       - |  1415 | `			/* Arrow function (PHP 7.4): 'fn(...) =>' or 'static fn(...) =>'.` |
|       - |  1416 | `			 * The '=>' inside an arrow function is not an array key/value` |
|       - |  1417 | `			 * separator — it introduces the expression body. Skip past the` |
|       - |  1418 | `			 * signature so the body scan sees no false '=>'.` |
|       - |  1419 | `			 */` |
|   28312 |  1420 | `			if( iNest == 0 && (pCur->nType & PH7_TK_KEYWORD) ){` |
|      76 |  1421 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pCur->pUserData);` |
|      76 |  1422 | `				SyToken *pFn = pCur;` |
|      74 |  1423 | `				if( nKw == PH7_TKWRD_STATIC && &pCur[1] < pGen->pIn` |
|     ! 0 |  1424 | `					&& (pCur[1].nType & PH7_TK_KEYWORD)` |
|       2 |  1425 | `					&& SX_PTR_TO_INT(pCur[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1426 | `					pFn = &pCur[1];` |
|     ! 0 |  1427 | `					nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1428 | `				}` |
|      76 |  1429 | `				if( nKw == PH7_TKWRD_FN ){` |
|       5 |  1430 | `					pCur = pFn + 1; /* past 'fn' */` |
|       5 |  1431 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1432 | `						pCur++;` |
|     ! 0 |  1433 | `					}` |
|       5 |  1434 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       5 |  1435 | `						pCur++;` |
|       5 |  1436 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1437 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       5 |  1438 | `						if( pCur < pGen->pIn ){` |
|       5 |  1439 | `							pCur++;` |
|       2 |  1440 | `						}` |
|       2 |  1441 | `					}` |
|       5 |  1442 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_COLON) ){` |
|     ! 0 |  1443 | `						pCur++;` |
|     ! 0 |  1444 | `						if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OP)` |
|     ! 0 |  1445 | `							&& pCur->sData.nByte == 1` |
|     ! 0 |  1446 | `							&& pCur->sData.zString[0] == '?' ){` |
|     ! 0 |  1447 | `							pCur++;` |
|     ! 0 |  1448 | `						}` |
|     ! 0 |  1449 | `						if( pCur < pGen->pIn` |
|     ! 0 |  1450 | `							&& (pCur->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  1451 | `							pCur++;` |
|     ! 0 |  1452 | `						}` |
|     ! 0 |  1453 | `					}` |
|       - |  1454 | `					/* The rest of the entry is the arrow function body — no` |
|       - |  1455 | `					 * outer key to extract. Stop the scan here. */` |
|       5 |  1456 | `					pCur = pGen->pIn;` |
|       5 |  1457 | `					break;` |
|       - |  1458 | `				}` |
|       - |  1459 | `				/* Match expression (PHP 8.0): 'match (subject) { ... }'.` |
|       - |  1460 | `				 * The '=>' inside match arms is not an array key/value separator —` |
|       - |  1461 | `				 * it introduces each arm's result expression. Skip past the full` |
|       - |  1462 | `				 * match span so the outer scan sees no false '=>'. */` |
|      72 |  1463 | `				if( nKw == PH7_TKWRD_MATCH ){` |
|       3 |  1464 | `					pCur++; /* past 'match' */` |
|       3 |  1465 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_LPAREN) ){` |
|       3 |  1466 | `						pCur++;` |
|       3 |  1467 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1468 | `							PH7_TK_LPAREN,PH7_TK_RPAREN,&pCur);` |
|       3 |  1469 | `						if( pCur < pGen->pIn ){` |
|       3 |  1470 | `							pCur++;` |
|       1 |  1471 | `						}` |
|       1 |  1472 | `					}` |
|       3 |  1473 | `					if( pCur < pGen->pIn && (pCur->nType & PH7_TK_OCB) ){` |
|       3 |  1474 | `						pCur++;` |
|       3 |  1475 | `						PH7_DelimitNestedTokens(pCur,pGen->pIn,` |
|       - |  1476 | `							PH7_TK_OCB,PH7_TK_CCB,&pCur);` |
|       3 |  1477 | `						if( pCur < pGen->pIn ){` |
|       3 |  1478 | `							pCur++;` |
|       1 |  1479 | `						}` |
|       1 |  1480 | `					}` |
|       3 |  1481 | `					continue;` |
|       - |  1482 | `				}` |
|      34 |  1483 | `			}` |
|   28306 |  1484 | `			if( pCur->nType & PH7_TK_LPAREN /*'('*/ ){` |
|      86 |  1485 | `				iNest++;` |
|   28264 |  1486 | `			}else if( pCur->nType & PH7_TK_RPAREN /*')'*/ ){` |
|       - |  1487 | `				/* Don't worry about mismatched parenthesis here,the expression` |
|       - |  1488 | `				 * parser will shortly detect any syntax error.` |
|       - |  1489 | `				 */` |
|      86 |  1490 | `				iNest--;` |
|      42 |  1491 | `			}` |
|   28306 |  1492 | `			pCur++;` |
|       2 |  1493 | `		}` |
|   15834 |  1494 | `		rc = SXERR_EMPTY;` |
|   15834 |  1495 | `		if( pCur < pGen->pIn ){` |
|    1218 |  1496 | `			if( &pCur[1] >= pGen->pIn ){` |
|       - |  1497 | `				/* Missing value */` |
|      11 |  1498 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing entry value");` |
|      11 |  1499 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1500 | `					return SXERR_ABORT;` |
|       - |  1501 | `				}` |
|      11 |  1502 | `				return SXRET_OK;` |
|       - |  1503 | `			}` |
|       - |  1504 | `			/* Compile the expression holding the key */` |
|    1208 |  1505 | `			rc = GenStateCompileArrayEntry(&(*pGen),pKey,pCur,` |
|       - |  1506 | `				EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,0);` |
|    1208 |  1507 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1508 | `				return SXERR_ABORT;` |
|       - |  1509 | `			}` |
|    1208 |  1510 | `			pCur++; /* Jump the '=>' operator */` |
|   15221 |  1511 | `		}else if( pKey == pCur ){` |
|       - |  1512 | `			/* Key is omitted,emit a warning */` |
|     ! 0 |  1513 | `			PH7_GenCompileError(&(*pGen),E_WARNING,pCur->nLine,"array(): Missing entry key");` |
|     ! 0 |  1514 | `			pCur++; /* Jump the '=>' operator */` |
|     ! 0 |  1515 | `		}else{` |
|       - |  1516 | `			/* Reset back the cursor and point to the entry value */` |
|   14618 |  1517 | `			pCur = pKey;` |
|       - |  1518 | `		}` |
|   15824 |  1519 | `		if( rc == SXERR_EMPTY ){` |
|       - |  1520 | `			/* No available key,load NULL */` |
|   14620 |  1521 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0 /* nil index */,0,0);` |
|    7309 |  1522 | `		}` |
|   15824 |  1523 | `		if( pCur->nType & PH7_TK_AMPER /*'&'*/){` |
|       - |  1524 | `			/* Insertion by reference, [i.e: $a = array(&$x);] */` |
|      34 |  1525 | `			xValidator = GenStateArrayNodeValidator; /* Only variable are allowed */` |
|      34 |  1526 | `			iEmitRef = 1;` |
|      34 |  1527 | `			pCur++; /* Jump the '&' token */` |
|      34 |  1528 | `			if( pCur >= pGen->pIn ){` |
|       - |  1529 | `				/* Missing value */` |
|       3 |  1530 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pCur->nLine,"array(): Missing referenced variable");` |
|       3 |  1531 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1532 | `					return SXERR_ABORT;` |
|       - |  1533 | `				}` |
|       3 |  1534 | `				return SXRET_OK;` |
|       - |  1535 | `			}` |
|      15 |  1536 | `		}` |
|       - |  1537 | `		/* Compile indice value */` |
|   15822 |  1538 | `		rc = GenStateCompileArrayEntry(&(*pGen),pCur,pGen->pIn,EXPR_FLAG_RDONLY_LOAD/*Do not create the variable if inexistant*/,xValidator);` |
|   15822 |  1539 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  1540 | `			return SXERR_ABORT;` |
|       - |  1541 | `		}` |
|   15822 |  1542 | `		if( iEmitRef ){` |
|       - |  1543 | `			/* Emit the load reference instruction */` |
|      32 |  1544 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_REF,0,0,0,0);` |
|      15 |  1545 | `		}` |
|   15822 |  1546 | `		xValidator = 0;` |
|   15822 |  1547 | `		iEmitRef = 0;` |
|   15822 |  1548 | `		nPair++;` |
|       2 |  1549 | `	}` |
|       - |  1550 | `	/* Emit the load map instruction */` |
|   25540 |  1551 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_MAP,nPair * 2,0,0,0);` |
|       - |  1552 | `	/* Node successfully compiled */` |
|   25540 |  1553 | `	return SXRET_OK;` |
|   12777 |  1554 |  |
|       - |  1555 | `/*` |
|       - |  1556 | ` * Compile the 'array' language construct.` |
|       - |  1557 | ` *	 According to the PHP language reference manual` |
|       - |  1558 | ` *   An array in PHP is actually an ordered map. A map is a type that associates` |
|       - |  1559 | ` *   values to keys. This type is optimized for several different uses; it can` |
|       - |  1560 | ` *   be treated as an array, list (vector), hash table (an implementation of a map)` |
|       - |  1561 | ` *   dictionary, collection, stack, queue, and probably more. As array values can be` |
|       - |  1562 | ` *   other arrays, trees and multidimensional arrays are also possible.` |
|       - |  1563 | ` */` |
|   25252 |  1564 | `PH7_PRIVATE sxi32 PH7_CompileArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1565 |  |
|       - |  1566 | `	/* Jump the 'array' keyword and the leading '(', exclude trailing ')'. */` |
|   25254 |  1567 | `	pGen->pIn += 2;` |
|   25254 |  1568 | `	pGen->pEnd--;` |
|   12626 |  1569 | `	SXUNUSED(iCompileFlag);` |
|   25254 |  1570 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1571 |  |
|       - |  1572 | `/*` |
|       - |  1573 | ` * Compile a short array literal using the PHP 5.4 bracket syntax.` |
|       - |  1574 | ` * [1, 2, 3] is equivalent to array(1, 2, 3).` |
|       - |  1575 | ` * ['key' => 'value'] is equivalent to array('key' => 'value').` |
|       - |  1576 | ` */` |
|     298 |  1577 | `PH7_PRIVATE sxi32 PH7_CompileShortArray(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1578 |  |
|       - |  1579 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|     300 |  1580 | `	pGen->pIn++;` |
|     300 |  1581 | `	pGen->pEnd--;` |
|     149 |  1582 | `	SXUNUSED(iCompileFlag);` |
|     300 |  1583 | `	return GenStateCompileArrayBody(pGen);` |
|       2 |  1584 |  |
|       - |  1585 | `/*` |
|       - |  1586 | ` * Expression tree validator callback for the 'list' language construct.` |
|       - |  1587 | ` * Return SXRET_OK if the tree is valid. Any other return value indicates` |
|       - |  1588 | ` * an invalid expression tree and this function will generate the appropriate` |
|       - |  1589 | ` * error message.` |
|       - |  1590 | ` * See the routine responible of compiling the list language construct` |
|       - |  1591 | ` * for more inforation.` |
|       - |  1592 | ` */` |
|     128 |  1593 | `static sxi32 GenStateListNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  1594 |  |
|     130 |  1595 | `	sxi32 rc = SXRET_OK;` |
|     130 |  1596 | `	if( pRoot->pOp ){` |
|     ! 0 |  1597 | `		if( pRoot->pOp->iOp != EXPR_OP_SUBSCRIPT /* $a[] */ && pRoot->pOp->iOp != EXPR_OP_ARROW /* -> */` |
|     ! 0 |  1598 | `			&& pRoot->pOp->iOp != EXPR_OP_DC /* :: */ ){` |
|       - |  1599 | `				/* Unexpected expression */` |
|     ! 0 |  1600 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1601 | `					"list(): Expecting a variable not an expression");` |
|     ! 0 |  1602 | `				if( rc != SXERR_ABORT ){` |
|     ! 0 |  1603 | `					rc = SXERR_INVALID;` |
|     ! 0 |  1604 | `				}` |
|     ! 0 |  1605 | `		}` |
|     130 |  1606 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  1607 | `		/* Unexpected expression */` |
|       5 |  1608 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  1609 | `			"list(): Expecting a variable not an expression");` |
|       5 |  1610 | `		if( rc != SXERR_ABORT ){` |
|       5 |  1611 | `			rc = SXERR_INVALID;` |
|       2 |  1612 | `		}` |
|       2 |  1613 | `	}` |
|     130 |  1614 | `	return rc;` |
|       2 |  1615 |  |
|       - |  1616 | `/*` |
|       - |  1617 | ` * Compile the 'list' language construct.` |
|       - |  1618 | ` *  According to the PHP language reference` |
|       - |  1619 | ` *  list(): Assign variables as if they were an array.` |
|       - |  1620 | ` *  list() is used to assign a list of variables in one operation.` |
|       - |  1621 | ` *  Description` |
|       - |  1622 | ` *   array list (mixed $varname [, mixed $... ] )` |
|       - |  1623 | ` *   Like array(), this is not really a function, but a language construct.` |
|       - |  1624 | ` *   list() is used to assign a list of variables in one operation.` |
|       - |  1625 | ` *  Parameters` |
|       - |  1626 | ` *   $varname: A variable.` |
|       - |  1627 | ` *  Return Values` |
|       - |  1628 | ` *   The assigned array.` |
|       - |  1629 | ` */` |
|       - |  1630 | `/* Nested list entry recorded during first pass of list body compilation */` |
|       - |  1631 | `struct NestedListEntry {` |
|       - |  1632 | `	sxi32 nIndex;        /* Position in the outer list (0-based) */` |
|       - |  1633 | `	SyToken *pStart;     /* Token range: start of nested construct */` |
|       - |  1634 | `	SyToken *pEnd;       /* Token range: past closing delimiter */` |
|       - |  1635 | `	sxi32 isShort;       /* 1 if [...] form, 0 if list(...) form */` |
|       - |  1636 | `};` |
|       - |  1637 | `/*` |
|       - |  1638 | ` * Shared body for list() and short list [...] compilation.` |
|       - |  1639 | ` * Assumes pGen->pIn and pGen->pEnd are already positioned past` |
|       - |  1640 | ` * the opening delimiter and before the closing delimiter.` |
|       - |  1641 | ` */` |
|      74 |  1642 | `static sxi32 GenStateCompileListBody(ph7_gen_state *pGen)` |
|       2 |  1643 |  |
|       - |  1644 | `	SySet sNested; /* Dynamically-sized container of NestedListEntry */` |
|       - |  1645 | `	SyToken *pNext;` |
|       - |  1646 | `	sxi32 nExpr;` |
|       - |  1647 | `	sxi32 rc;` |
|      76 |  1648 | `	nExpr = 0;` |
|      76 |  1649 | `	SySetInit(&sNested,&pGen->pVm->sAllocator,sizeof(struct NestedListEntry));` |
|     230 |  1650 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pGen->pEnd,&pNext) ){` |
|     156 |  1651 | `		if( pGen->pIn < pNext ){` |
|       - |  1652 | `			/* Check for nested list() */` |
|     144 |  1653 | `			if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       3 |  1654 | `				SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  1655 | `				/* Record this nested list for post-processing */` |
|       3 |  1656 | `				SyToken *pListEnd = 0;` |
|       3 |  1657 | `				if( &pGen->pIn[1] < pNext && (pGen->pIn[1].nType & PH7_TK_LPAREN) ){` |
|       3 |  1658 | `					PH7_DelimitNestedTokens(pGen->pIn+2,pNext,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       1 |  1659 | `				}` |
|       3 |  1660 | `				if( pListEnd ){` |
|       - |  1661 | `					struct NestedListEntry sEntry;` |
|       3 |  1662 | `					sEntry.nIndex = nExpr;` |
|       3 |  1663 | `					sEntry.pStart = pGen->pIn;` |
|       3 |  1664 | `					sEntry.pEnd = pListEnd + 1;` |
|       3 |  1665 | `					sEntry.isShort = 0;` |
|       3 |  1666 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       1 |  1667 | `				}` |
|       - |  1668 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|       3 |  1669 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     143 |  1670 | `			}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  1671 | `				/* Nested short destructuring [...] */` |
|      13 |  1672 | `				SyToken *pBracketEnd = 0;` |
|      13 |  1673 | `				PH7_DelimitNestedTokens(pGen->pIn+1,pNext,PH7_TK_OSB,PH7_TK_CSB,&pBracketEnd);` |
|      13 |  1674 | `				if( pBracketEnd ){` |
|       - |  1675 | `					struct NestedListEntry sEntry;` |
|      13 |  1676 | `					sEntry.nIndex = nExpr;` |
|      13 |  1677 | `					sEntry.pStart = pGen->pIn;` |
|      13 |  1678 | `					sEntry.pEnd = pBracketEnd + 1;` |
|      13 |  1679 | `					sEntry.isShort = 1;` |
|      13 |  1680 | `					SySetPut(&sNested,(const void *)&sEntry);` |
|       6 |  1681 | `				}` |
|       - |  1682 | `				/* Emit NULL placeholder — outer LOAD_LIST will skip this index */` |
|      13 |  1683 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       7 |  1684 | `			}else{` |
|       - |  1685 | `				/* Compile the expression holding the variable */` |
|     130 |  1686 | `				rc = GenStateCompileArrayEntry(&(*pGen),pGen->pIn,pNext,EXPR_FLAG_LOAD_IDX_STORE,GenStateListNodeValidator);` |
|     130 |  1687 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  1688 | `					SySetRelease(&sNested);` |
|     ! 0 |  1689 | `					return SXRET_OK;` |
|       - |  1690 | `				}` |
|       - |  1691 | `			}` |
|      73 |  1692 | `		}else{` |
|       - |  1693 | `			/* Empty entry,load NULL */` |
|      13 |  1694 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0/* NULL index */,0,0);` |
|       - |  1695 | `		}` |
|     156 |  1696 | `		nExpr++;` |
|       - |  1697 | `		/* Advance the stream cursor */` |
|     156 |  1698 | `		pGen->pIn = &pNext[1];` |
|       2 |  1699 | `	}` |
|       - |  1700 | `	/* Emit the LOAD_LIST instruction */` |
|      76 |  1701 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_LIST,nExpr,0,0,0);` |
|       - |  1702 | `	/* After LOAD_LIST, the source array is still on the stack top.` |
|       - |  1703 | `	 * For each nested entry, emit code to extract the sub-array` |
|       - |  1704 | `	 * at the corresponding index and recursively destructure it.` |
|       - |  1705 | `	 */` |
|      76 |  1706 | `	if( SySetUsed(&sNested) > 0 ){` |
|      13 |  1707 | `		struct NestedListEntry *apNested = (struct NestedListEntry *)SySetBasePtr(&sNested);` |
|       - |  1708 | `		sxu32 i;` |
|      27 |  1709 | `		for(i = 0; i < SySetUsed(&sNested); i++){` |
|      15 |  1710 | `			SyToken *pSavedIn = pGen->pIn;` |
|      15 |  1711 | `			SyToken *pSavedEnd = pGen->pEnd;` |
|       - |  1712 | `			ph7_value *pIdx;` |
|       - |  1713 | `			sxu32 nConstIdx;` |
|       - |  1714 | `			/* DUP the source array (it's on stack top) */` |
|      15 |  1715 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       - |  1716 | `			/* Push the integer index for this nested entry */` |
|      15 |  1717 | `			pIdx = PH7_ReserveConstObj(pGen->pVm,&nConstIdx);` |
|      15 |  1718 | `			if( pIdx == 0 ){` |
|     ! 0 |  1719 | `				PH7_GenCompileError(&(*pGen),E_ERROR,0,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1720 | `				SySetRelease(&sNested);` |
|     ! 0 |  1721 | `				return SXERR_ABORT;` |
|       - |  1722 | `			}` |
|      15 |  1723 | `			PH7_MemObjInitFromInt(pGen->pVm,pIdx,(sxi64)apNested[i].nIndex);` |
|      15 |  1724 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nConstIdx,0,0);` |
|       - |  1725 | `			/* LOAD_IDX: pop index, replace DUP'd source with source[index].` |
|       - |  1726 | `			 * iP2=2 signals the VM to emit an "Undefined array key" warning` |
|       - |  1727 | `			 * when the key is missing (PHP-compatible list destructuring).` |
|       - |  1728 | `			 */` |
|      15 |  1729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_IDX,1,2,0,0);` |
|       - |  1730 | `			/* Recursively compile the inner list */` |
|      15 |  1731 | `			pGen->pIn = apNested[i].pStart;` |
|      15 |  1732 | `			pGen->pEnd = apNested[i].pEnd;` |
|      15 |  1733 | `			if( apNested[i].isShort ){` |
|      13 |  1734 | `				rc = PH7_CompileShortList(&(*pGen),0);` |
|       7 |  1735 | `			}else{` |
|       3 |  1736 | `				rc = PH7_CompileList(&(*pGen),0);` |
|       - |  1737 | `			}` |
|      15 |  1738 | `			pGen->pIn = pSavedIn;` |
|      15 |  1739 | `			pGen->pEnd = pSavedEnd;` |
|      15 |  1740 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1741 | `				SySetRelease(&sNested);` |
|     ! 0 |  1742 | `				return SXERR_ABORT;` |
|       - |  1743 | `			}` |
|       - |  1744 | `			/* Pop the leftover source[index] from the inner LOAD_LIST */` |
|      15 |  1745 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       8 |  1746 | `		}` |
|       6 |  1747 | `	}` |
|      76 |  1748 | `	SySetRelease(&sNested);` |
|       - |  1749 | `	/* Node successfully compiled */` |
|      76 |  1750 | `	return SXRET_OK;` |
|      39 |  1751 |  |
|      32 |  1752 | `PH7_PRIVATE sxi32 PH7_CompileList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1753 |  |
|       - |  1754 | `	/* Jump the 'list' keyword, the leading '(' and exclude trailing ')' */` |
|      34 |  1755 | `	pGen->pIn += 2;` |
|      34 |  1756 | `	pGen->pEnd--;` |
|      16 |  1757 | `	SXUNUSED(iCompileFlag);` |
|      34 |  1758 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1759 |  |
|      42 |  1760 | `PH7_PRIVATE sxi32 PH7_CompileShortList(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1761 |  |
|       - |  1762 | `	/* Jump the leading '[', exclude trailing ']'. */` |
|      44 |  1763 | `	pGen->pIn++;` |
|      44 |  1764 | `	pGen->pEnd--;` |
|      21 |  1765 | `	SXUNUSED(iCompileFlag);` |
|      44 |  1766 | `	return GenStateCompileListBody(pGen);` |
|       2 |  1767 |  |
|       - |  1768 | `/* Forward declarations */` |
|       - |  1769 | `static sxi32 GenStateCompileFunc(ph7_gen_state *pGen,SyString *pName,sxi32 iFlags,int bHandleClosure,ph7_vm_func **ppFunc);` |
|       - |  1770 | `static int GenStateIsReservedConstant(SyString *pName);` |
|       - |  1771 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut);` |
|       - |  1772 | `/*` |
|       - |  1773 | ` * Compile an annoynmous function or a closure.` |
|       - |  1774 | ` * According to the PHP language reference` |
|       - |  1775 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |  1776 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |  1777 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |  1778 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |  1779 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |  1780 | ` *  Example Anonymous function variable assignment example` |
|       - |  1781 | ` * <?php` |
|       - |  1782 | ` * $greet = function($name)` |
|       - |  1783 | ` * {` |
|       - |  1784 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  1785 | ` * };` |
|       - |  1786 | ` * $greet('World');` |
|       - |  1787 | ` * $greet('PHP');` |
|       - |  1788 | ` * ?>` |
|       - |  1789 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  1790 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  1791 | ` */` |
|     176 |  1792 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  1793 |  |
|       - |  1794 | `	ph7_vm_func *pAnnonFunc; /* Annonymous function body */` |
|       - |  1795 | `	char zName[512];         /* Unique lambda name */` |
|       - |  1796 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  1797 | `							  * one thread is allowed to compile the script.` |
|       - |  1798 | `						      */` |
|       - |  1799 | `	ph7_value *pObj;` |
|       - |  1800 | `	SyString sName;` |
|       - |  1801 | `	sxu32 nIdx;` |
|       - |  1802 | `	sxu32 nLen;` |
|       - |  1803 | `	sxi32 rc;` |
|       - |  1804 |  |
|     178 |  1805 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     178 |  1806 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  1807 | `		pGen->pIn++;` |
|     ! 0 |  1808 | `	}` |
|       - |  1809 | `	/* Reserve a constant for the lambda */` |
|     178 |  1810 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     178 |  1811 | `	if( pObj == 0 ){` |
|     ! 0 |  1812 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  1813 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  1814 | `		return SXERR_ABORT;` |
|       - |  1815 | `	}` |
|       - |  1816 | `	/* Generate a unique name */` |
|     178 |  1817 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  1818 | `	/* Make sure the generated name is unique */` |
|     178 |  1819 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  1820 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  1821 | `	}` |
|     178 |  1822 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|     178 |  1823 | `	PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       - |  1824 | `	/* Compile the lambda body */` |
|     178 |  1825 | `	rc = GenStateCompileFunc(&(*pGen),&sName,0,TRUE,&pAnnonFunc);` |
|     178 |  1826 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  1827 | `		return SXERR_ABORT;` |
|       - |  1828 | `	}` |
|     178 |  1829 | `	if( pAnnonFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - |  1830 | `		/* Emit the load closure instruction */` |
|      16 |  1831 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       9 |  1832 | `	}else{` |
|       - |  1833 | `		/* Emit the load constant instruction */` |
|     164 |  1834 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  1835 | `	}` |
|       - |  1836 | `	/* Node successfully compiled */` |
|     178 |  1837 | `	return SXRET_OK;` |
|      90 |  1838 |  |
|       - |  1839 | `/*` |
|       - |  1840 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  1841 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  1842 | ` * enclosing arrow level, or has already been captured.` |
|       - |  1843 | ` */` |
|     122 |  1844 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  1845 | `	ph7_gen_state *pGen,` |
|       - |  1846 | `	ph7_vm_func *pFunc,` |
|       - |  1847 | `	const char *zName,` |
|       - |  1848 | `	sxu32 nByte,` |
|       - |  1849 | `	SyString *aShadow,` |
|       - |  1850 | `	sxu32 nShadow)` |
|       1 |  1851 |  |
|       - |  1852 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  1853 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  1854 | `	sxu32 n, nEnv;` |
|       - |  1855 | `	char *zDup;` |
|     123 |  1856 | `	if( nByte == 0 ){` |
|     ! 0 |  1857 | `		return SXRET_OK;` |
|       - |  1858 | `	}` |
|     122 |  1859 | `	if( nByte == sizeof("this")-1` |
|      66 |  1860 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|       3 |  1861 | `		return SXRET_OK;` |
|       - |  1862 | `	}` |
|     147 |  1863 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      94 |  1864 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      90 |  1865 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      69 |  1866 | `			return SXRET_OK;` |
|       - |  1867 | `		}` |
|      14 |  1868 | `	}` |
|      53 |  1869 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      53 |  1870 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      81 |  1871 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|      28 |  1872 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|      27 |  1873 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     ! 0 |  1874 | `			return SXRET_OK;` |
|       - |  1875 | `		}` |
|      15 |  1876 | `	}` |
|      53 |  1877 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      53 |  1878 | `	if( zDup == 0 ){` |
|     ! 0 |  1879 | `		return SXERR_ABORT;` |
|       - |  1880 | `	}` |
|      53 |  1881 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      53 |  1882 | `	sEnv.iFlags = 0;` |
|      53 |  1883 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      53 |  1884 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      53 |  1885 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      53 |  1886 | `	return SXRET_OK;` |
|      62 |  1887 |  |
|       - |  1888 | `/*` |
|       - |  1889 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  1890 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  1891 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  1892 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  1893 | ` */` |
|      14 |  1894 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  1895 | `	ph7_gen_state *pGen,` |
|       - |  1896 | `	ph7_vm_func *pFunc,` |
|       - |  1897 | `	const char *zIn,` |
|       - |  1898 | `	const char *zEnd,` |
|       - |  1899 | `	SyString *aShadow,` |
|       - |  1900 | `	sxu32 nShadow)` |
|       1 |  1901 |  |
|       - |  1902 | `	sxi32 rc;` |
|     159 |  1903 | `	while( zIn < zEnd ){` |
|     145 |  1904 | `		if( zIn[0] == '\\' ){` |
|     ! 0 |  1905 | `			zIn++;` |
|     ! 0 |  1906 | `			if( zIn < zEnd ){` |
|     ! 0 |  1907 | `				zIn++;` |
|     ! 0 |  1908 | `			}` |
|     ! 0 |  1909 | `			continue;` |
|       - |  1910 | `		}` |
|     144 |  1911 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      13 |  1912 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|      12 |  1913 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  1914 | `			const char *zName;` |
|      13 |  1915 | `			zIn++; /* skip '$' */` |
|      13 |  1916 | `			zName = zIn;` |
|      39 |  1917 | `			while( zIn < zEnd ){` |
|      35 |  1918 | `				unsigned char c = (unsigned char)zIn[0];` |
|      35 |  1919 | `				if( c >= 0xc0 ){` |
|     ! 0 |  1920 | `					zIn++;` |
|     ! 0 |  1921 | `					while( zIn < zEnd` |
|     ! 0 |  1922 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|     ! 0 |  1923 | `						zIn++;` |
|     ! 0 |  1924 | `					}` |
|     ! 0 |  1925 | `					continue;` |
|       - |  1926 | `				}` |
|      35 |  1927 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       9 |  1928 | `					break;` |
|       - |  1929 | `				}` |
|      27 |  1930 | `				zIn++;` |
|       1 |  1931 | `			}` |
|      13 |  1932 | `			if( zIn > zName ){` |
|      19 |  1933 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      12 |  1934 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      13 |  1935 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  1936 | `					return SXERR_ABORT;` |
|       - |  1937 | `				}` |
|       6 |  1938 | `			}` |
|      13 |  1939 | `			continue;` |
|       - |  1940 | `		}` |
|     133 |  1941 | `		zIn++;` |
|       1 |  1942 | `	}` |
|      15 |  1943 | `	return SXRET_OK;` |
|       8 |  1944 |  |
|       - |  1945 | `/*` |
|       - |  1946 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  1947 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  1948 | ` *   - plain $<id> pairs` |
|       - |  1949 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  1950 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  1951 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  1952 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  1953 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  1954 | ` *     are never mistakenly captured.` |
|       - |  1955 | ` */` |
|     104 |  1956 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  1957 | `	ph7_gen_state *pGen,` |
|       - |  1958 | `	ph7_vm_func *pFunc,` |
|       - |  1959 | `	SyToken *pStart,` |
|       - |  1960 | `	SyToken *pEnd,` |
|       - |  1961 | `	SyString *aShadow,` |
|       - |  1962 | `	sxu32 nShadow)` |
|       1 |  1963 |  |
|     105 |  1964 | `	SyToken *pScan = pStart;` |
|       - |  1965 | `	sxi32 rc;` |
|     389 |  1966 | `	while( pScan < pEnd ){` |
|     285 |  1967 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      22 |  1968 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       7 |  1969 | `				pScan->sData.zString,` |
|      14 |  1970 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       7 |  1971 | `				aShadow,nShadow);` |
|      15 |  1972 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  1973 | `				return SXERR_ABORT;` |
|       - |  1974 | `			}` |
|      15 |  1975 | `			pScan++;` |
|      15 |  1976 | `			continue;` |
|       - |  1977 | `		}` |
|     271 |  1978 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|      21 |  1979 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|      21 |  1980 | `			SyToken *pFnKw = pScan;` |
|      20 |  1981 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|     ! 0 |  1982 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|       1 |  1983 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|     ! 0 |  1984 | `				pFnKw = &pScan[1];` |
|     ! 0 |  1985 | `				nKw = PH7_TKWRD_FN;` |
|     ! 0 |  1986 | `			}` |
|      21 |  1987 | `			if( nKw == PH7_TKWRD_FN ){` |
|       - |  1988 | `				SyToken *pInnerSigStart;` |
|       - |  1989 | `				SyToken *pInnerSigEnd;` |
|       - |  1990 | `				SyToken *pInnerBodyEnd;` |
|       - |  1991 | `				SyString *aInnerShadow;` |
|       - |  1992 | `				sxu32 nInnerShadow;` |
|       - |  1993 | `				sxu32 nInnerParamMax;` |
|       - |  1994 | `				SyToken *p;` |
|       - |  1995 | `				int iNestInner;` |
|      19 |  1996 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      19 |  1997 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  1998 | `					pScan++;` |
|     ! 0 |  1999 | `				}` |
|      19 |  2000 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2001 | `					pScan++;` |
|     ! 0 |  2002 | `					continue;` |
|       - |  2003 | `				}` |
|      19 |  2004 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      19 |  2005 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  2006 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      19 |  2007 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  2008 | `					pScan = pEnd;` |
|     ! 0 |  2009 | `					continue;` |
|       - |  2010 | `				}` |
|       - |  2011 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      19 |  2012 | `				nInnerParamMax = 0;` |
|      57 |  2013 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2014 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      13 |  2015 | `						nInnerParamMax++;` |
|       6 |  2016 | `					}` |
|      20 |  2017 | `				}` |
|      19 |  2018 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      18 |  2019 | `					&pGen->pVm->sAllocator,` |
|      18 |  2020 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      19 |  2021 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  2022 | `					return SXERR_ABORT;` |
|       - |  2023 | `				}` |
|      19 |  2024 | `				nInnerShadow = 0;` |
|      25 |  2025 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       7 |  2026 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       4 |  2027 | `				}` |
|      57 |  2028 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      39 |  2029 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|      27 |  2030 | `						continue;` |
|       - |  2031 | `					}` |
|      13 |  2032 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  2033 | `						break;` |
|       - |  2034 | `					}` |
|      13 |  2035 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2036 | `						continue;` |
|       - |  2037 | `					}` |
|      13 |  2038 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       7 |  2039 | `				}` |
|      19 |  2040 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      19 |  2041 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  2042 | `					pScan++;` |
|     ! 0 |  2043 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  2044 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  2045 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  2046 | `						pScan++;` |
|     ! 0 |  2047 | `					}` |
|     ! 0 |  2048 | `					if( pScan < pEnd` |
|     ! 0 |  2049 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  2050 | `						pScan++;` |
|     ! 0 |  2051 | `					}` |
|     ! 0 |  2052 | `				}` |
|      19 |  2053 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      19 |  2054 | `					pScan++; /* past '=>' */` |
|       9 |  2055 | `				}` |
|      19 |  2056 | `				pInnerBodyEnd = pScan;` |
|      19 |  2057 | `				iNestInner = 0;` |
|     131 |  2058 | `				while( pInnerBodyEnd < pEnd ){` |
|     113 |  2059 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  2060 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  2061 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     ! 0 |  2062 | `						break;` |
|       - |  2063 | `					}` |
|     113 |  2064 | `					if( pInnerBodyEnd->nType &` |
|       - |  2065 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       3 |  2066 | `						iNestInner++;` |
|     112 |  2067 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  2068 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       3 |  2069 | `						iNestInner--;` |
|       1 |  2070 | `					}` |
|     113 |  2071 | `					pInnerBodyEnd++;` |
|       1 |  2072 | `				}` |
|       - |  2073 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  2074 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  2075 | `				 * in the outer frame, so any free variable it references is` |
|       - |  2076 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  2077 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  2078 | `				 * or those names leak into the outer's closure environment.` |
|       - |  2079 | `				 *` |
|       - |  2080 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  2081 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  2082 | `				 * range after the '=' sign. */` |
|       - |  2083 | `				{` |
|      19 |  2084 | `					SyToken *pArgStart = pInnerSigStart;` |
|      31 |  2085 | `					while( pArgStart < pInnerSigEnd ){` |
|      13 |  2086 | `						SyToken *pArgEnd = pArgStart;` |
|      13 |  2087 | `						SyToken *pEq = 0;` |
|      13 |  2088 | `						int iNestArg = 0;` |
|      49 |  2089 | `						while( pArgEnd < pInnerSigEnd ){` |
|      38 |  2090 | `							if( iNestArg == 0` |
|      39 |  2091 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       3 |  2092 | `								break;` |
|       - |  2093 | `							}` |
|      37 |  2094 | `							if( pArgEnd->nType &` |
|       - |  2095 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  2096 | `								iNestArg++;` |
|      37 |  2097 | `							}else if( pArgEnd->nType &` |
|       - |  2098 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  2099 | `								iNestArg--;` |
|     ! 0 |  2100 | `							}` |
|      36 |  2101 | `							if( pEq == 0 && iNestArg == 0` |
|      31 |  2102 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  2103 | `								pEq = pArgEnd;` |
|       3 |  2104 | `							}` |
|      37 |  2105 | `							pArgEnd++;` |
|       1 |  2106 | `						}` |
|      13 |  2107 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  2108 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  2109 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  2110 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  2111 | `								return SXERR_ABORT;` |
|       - |  2112 | `							}` |
|       3 |  2113 | `						}` |
|      13 |  2114 | `						pArgStart = pArgEnd;` |
|      12 |  2115 | `						if( pArgStart < pInnerSigEnd` |
|       8 |  2116 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       3 |  2117 | `							pArgStart++;` |
|       1 |  2118 | `						}` |
|       1 |  2119 | `					}` |
|       - |  2120 | `				}` |
|      28 |  2121 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       9 |  2122 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      19 |  2123 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2124 | `					return SXERR_ABORT;` |
|       - |  2125 | `				}` |
|      19 |  2126 | `				pScan = pInnerBodyEnd;` |
|      19 |  2127 | `				continue;` |
|       - |  2128 | `			}` |
|       1 |  2129 | `		}` |
|     253 |  2130 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     143 |  2131 | `			pScan++;` |
|     143 |  2132 | `			continue;` |
|       - |  2133 | `		}` |
|       - |  2134 | `		{` |
|       - |  2135 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|     111 |  2136 | `			SyToken *pDollar = pScan;` |
|     165 |  2137 | `			while( &pDollar[1] < pEnd` |
|     111 |  2138 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  2139 | `				pDollar++;` |
|     ! 0 |  2140 | `			}` |
|     111 |  2141 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  2142 | `				break;` |
|       - |  2143 | `			}` |
|     111 |  2144 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  2145 | `				pScan = pDollar + 1;` |
|     ! 0 |  2146 | `				continue;` |
|       - |  2147 | `			}` |
|     166 |  2148 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|     110 |  2149 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      55 |  2150 | `				aShadow,nShadow);` |
|     111 |  2151 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  2152 | `				return SXERR_ABORT;` |
|       - |  2153 | `			}` |
|     111 |  2154 | `			pScan = pDollar + 2;` |
|       - |  2155 | `		}` |
|       1 |  2156 | `	}` |
|     105 |  2157 | `	return SXRET_OK;` |
|      53 |  2158 |  |
|       - |  2159 | `/*` |
|       - |  2160 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  2161 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  2162 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  2163 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  2164 | ` * $this is also made available.` |
|       - |  2165 | ` */` |
|      86 |  2166 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2167 |  |
|       - |  2168 | `	ph7_vm_func *pFunc;` |
|       - |  2169 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  2170 | `	GenBlock *pBlock;` |
|       - |  2171 | `	SySet *pInstrContainer;` |
|       - |  2172 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  2173 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  2174 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  2175 | `	SyToken *pSavedEnd;` |
|       - |  2176 | `	ph7_vm_func_arg *aArgs;` |
|       - |  2177 | `	char zName[512];` |
|       - |  2178 | `	static int iCnt = 1;` |
|       - |  2179 | `	char *zDup;` |
|       - |  2180 | `	sxu32 nLen;` |
|       - |  2181 | `	sxu32 nLine;` |
|      88 |  2182 | `	sxi32 iFlags = 0;` |
|      88 |  2183 | `	int bStatic = 0;` |
|       - |  2184 | `	sxi32 rc;` |
|       - |  2185 | `	sxu32 n;` |
|      43 |  2186 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2187 |  |
|      88 |  2188 | `	nLine = pGen->pIn->nLine;` |
|       - |  2189 | `	/* Optional 'static' prefix */` |
|      86 |  2190 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      88 |  2191 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       3 |  2192 | `		bStatic = 1;` |
|       3 |  2193 | `		pGen->pIn++;` |
|       1 |  2194 | `	}` |
|       - |  2195 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      86 |  2196 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      88 |  2197 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  2198 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2199 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  2200 | `		return SXERR_SYNTAX;` |
|       - |  2201 | `	}` |
|      88 |  2202 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  2203 | `	/* Optional '&' — return by reference */` |
|      88 |  2204 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  2205 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  2206 | `		pGen->pIn++;` |
|     ! 0 |  2207 | `	}` |
|       - |  2208 | `	/* Expect '(' */` |
|      88 |  2209 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  2210 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2211 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2212 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  2213 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2214 | `		}else{` |
|     ! 0 |  2215 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2216 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  2217 | `		}` |
|       3 |  2218 | `		return SXERR_SYNTAX;` |
|       - |  2219 | `	}` |
|      86 |  2220 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  2221 | `	/* Delimit the parameter list */` |
|      86 |  2222 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      86 |  2223 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  2224 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2225 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  2226 | `		return SXERR_SYNTAX;` |
|       - |  2227 | `	}` |
|       - |  2228 | `	/* Allocate the function state */` |
|      84 |  2229 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      84 |  2230 | `	if( pFunc == 0 ){` |
|     ! 0 |  2231 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2232 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2233 | `		return SXERR_ABORT;` |
|       - |  2234 | `	}` |
|       - |  2235 | `	/* Generate a unique lambda name */` |
|      84 |  2236 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     168 |  2237 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      85 |  2238 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 |  2239 | `	}` |
|      84 |  2240 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      84 |  2241 | `	if( zDup == 0 ){` |
|     ! 0 |  2242 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2243 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2244 | `		return SXERR_ABORT;` |
|       - |  2245 | `	}` |
|      84 |  2246 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  2247 | `	/* Collect function arguments */` |
|      84 |  2248 | `	if( pGen->pIn < pSigEnd ){` |
|      54 |  2249 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      54 |  2250 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2251 | `			return SXERR_ABORT;` |
|       - |  2252 | `		}` |
|      26 |  2253 | `	}` |
|       - |  2254 | `	/* Point past ')' and parse optional return type */` |
|      84 |  2255 | `	pGen->pIn = &pSigEnd[1];` |
|      84 |  2256 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      84 |  2257 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2258 | `		return SXERR_ABORT;` |
|      84 |  2259 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  2260 | `		return SXERR_SYNTAX;` |
|       - |  2261 | `	}` |
|       - |  2262 | `	/* Expect '=>' */` |
|      84 |  2263 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2264 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  2265 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  2266 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  2267 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  2268 | `		}else{` |
|     ! 0 |  2269 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  2270 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  2271 | `		}` |
|       3 |  2272 | `		return SXERR_SYNTAX;` |
|       - |  2273 | `	}` |
|      81 |  2274 | `	pGen->pIn++; /* Jump '=>' */` |
|      81 |  2275 | `	pBodyStart = pGen->pIn;` |
|      81 |  2276 | `	pBodyEnd = pGen->pEnd;` |
|       - |  2277 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  2278 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  2279 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  2280 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      81 |  2281 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  2282 | `	{` |
|      81 |  2283 | `		SyString *aShadow = 0;` |
|      81 |  2284 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      81 |  2285 | `		if( nShadow > 0 ){` |
|      51 |  2286 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      50 |  2287 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      51 |  2288 | `			if( aShadow == 0 ){` |
|     ! 0 |  2289 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2290 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2291 | `				return SXERR_ABORT;` |
|       - |  2292 | `			}` |
|     107 |  2293 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      57 |  2294 | `				aShadow[n] = aArgs[n].sName;` |
|      29 |  2295 | `			}` |
|      25 |  2296 | `		}` |
|     121 |  2297 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      40 |  2298 | `			aShadow,nShadow);` |
|      81 |  2299 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2300 | `			return SXERR_ABORT;` |
|       - |  2301 | `		}` |
|       - |  2302 | `	}` |
|       - |  2303 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  2304 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  2305 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  2306 | `	 * $this. */` |
|      81 |  2307 | `	if( !bStatic ){` |
|       - |  2308 | `		char *zThisDup;` |
|      79 |  2309 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      79 |  2310 | `		if( zThisDup == 0 ){` |
|     ! 0 |  2311 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2312 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2313 | `			return SXERR_ABORT;` |
|       - |  2314 | `		}` |
|      79 |  2315 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      79 |  2316 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      79 |  2317 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      79 |  2318 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      79 |  2319 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      39 |  2320 | `	}` |
|       - |  2321 | `	/* Arrow functions are always closures */` |
|      81 |  2322 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       - |  2323 | `	/* Compile the body expression as an implicit return */` |
|     121 |  2324 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      40 |  2325 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      81 |  2326 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2327 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2328 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  2329 | `		return SXERR_ABORT;` |
|       - |  2330 | `	}` |
|      81 |  2331 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      81 |  2332 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      81 |  2333 | `	pSavedEnd = pGen->pEnd;` |
|      81 |  2334 | `	pGen->pIn = pBodyStart;` |
|      81 |  2335 | `	pGen->pEnd = pBodyEnd;` |
|      81 |  2336 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      81 |  2337 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2338 | `		return SXERR_ABORT;` |
|       - |  2339 | `	}` |
|       - |  2340 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  2341 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  2342 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  2343 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      81 |  2344 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      81 |  2345 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      81 |  2346 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      81 |  2347 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      81 |  2348 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  2349 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      81 |  2350 | `	pGen->pIn = pBodyEnd;` |
|      81 |  2351 | `	pGen->pEnd = pSavedEnd;` |
|       - |  2352 | `	/* Emit the load-closure instruction */` |
|      81 |  2353 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      81 |  2354 | `	return SXRET_OK;` |
|      45 |  2355 |  |
|       - |  2356 | `/*` |
|       - |  2357 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  2358 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  2359 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  2360 | ` * expression's value.` |
|       - |  2361 | ` */` |
|     346 |  2362 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  2363 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       2 |  2364 |  |
|       - |  2365 | `	SySet *pInstrContainer;` |
|       - |  2366 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  2367 | `	GenBlock *pArmBlock;` |
|       - |  2368 | `	sxi32 rc;` |
|     348 |  2369 | `	pTmpIn  = pGen->pIn;` |
|     348 |  2370 | `	pTmpEnd = pGen->pEnd;` |
|     348 |  2371 | `	pGen->pIn  = pStart;` |
|     348 |  2372 | `	pGen->pEnd = pStop;` |
|     348 |  2373 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     348 |  2374 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  2375 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  2376 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  2377 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  2378 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  2379 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     521 |  2380 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     173 |  2381 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     348 |  2382 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2383 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  2384 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  2385 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  2386 | `		return SXERR_ABORT;` |
|       - |  2387 | `	}` |
|     348 |  2388 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     348 |  2389 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     348 |  2390 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     348 |  2391 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     348 |  2392 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     348 |  2393 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     348 |  2394 | `	pGen->pIn  = pTmpIn;` |
|     348 |  2395 | `	pGen->pEnd = pTmpEnd;` |
|     348 |  2396 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2397 | `		return SXERR_ABORT;` |
|       - |  2398 | `	}` |
|     348 |  2399 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  2400 | `		return SXERR_EMPTY;` |
|       - |  2401 | `	}` |
|     348 |  2402 | `	return SXRET_OK;` |
|     175 |  2403 |  |
|       - |  2404 | `/*` |
|       - |  2405 | ` * Compile a PHP 8.0 match expression:` |
|       - |  2406 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  2407 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  2408 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  2409 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  2410 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  2411 | ` */` |
|       - |  2412 | `/*` |
|       - |  2413 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  2414 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  2415 | ` * caller can bail out of the current expression.` |
|       - |  2416 | ` */` |
|       2 |  2417 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  2418 |  |
|       - |  2419 | `	va_list ap;` |
|       - |  2420 | `	sxi32 rc;` |
|       - |  2421 | `	SyBlob sMsg;` |
|       3 |  2422 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  2423 | `	va_start(ap,zFmt);` |
|       3 |  2424 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  2425 | `	va_end(ap);` |
|       3 |  2426 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  2427 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  2428 | `	SyBlobRelease(&sMsg);` |
|       3 |  2429 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2430 | `		return SXERR_ABORT;` |
|       - |  2431 | `	}` |
|       3 |  2432 | `	return SXERR_SYNTAX;` |
|       2 |  2433 |  |
|       - |  2434 | `/*` |
|       - |  2435 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  2436 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  2437 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  2438 | ` */` |
|     348 |  2439 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       2 |  2440 |  |
|     350 |  2441 | `	SyToken *pCur = pStart;` |
|     350 |  2442 | `	int iNest = 0;` |
|     812 |  2443 | `	while( pCur < pEnd ){` |
|     778 |  2444 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      13 |  2445 | `			iNest++;` |
|     772 |  2446 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      13 |  2447 | `			iNest--;` |
|     760 |  2448 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     316 |  2449 | `			return pCur;` |
|       - |  2450 | `		}` |
|     464 |  2451 | `		pCur++;` |
|       2 |  2452 | `	}` |
|      36 |  2453 | `	return pEnd;` |
|     176 |  2454 |  |
|      70 |  2455 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2456 |  |
|       - |  2457 | `	ph7_match *pMatch;` |
|       - |  2458 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      72 |  2459 | `	int bHasDefault = 0;` |
|       - |  2460 | `	sxu32 nLine;` |
|       - |  2461 | `	sxi32 rc;` |
|      35 |  2462 | `	SXUNUSED(iCompileFlag);` |
|      72 |  2463 | `	nLine = pGen->pIn->nLine;` |
|      72 |  2464 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  2465 | `	/* Expect '(' */` |
|      72 |  2466 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  2467 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2468 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  2469 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  2470 | `	}` |
|      72 |  2471 | `	pGen->pIn++; /* Jump '(' */` |
|      72 |  2472 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      72 |  2473 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  2474 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2475 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  2476 | `	}` |
|      72 |  2477 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  2478 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2479 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  2480 | `	}` |
|       - |  2481 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      72 |  2482 | `	pSavedEnd = pGen->pEnd;` |
|      72 |  2483 | `	pGen->pEnd = pSubjEnd;` |
|      72 |  2484 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      72 |  2485 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  2486 | `		return SXERR_ABORT;` |
|       - |  2487 | `	}` |
|      72 |  2488 | `	pGen->pEnd = pSavedEnd;` |
|      72 |  2489 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  2490 | `	/* Expect '{' */` |
|      72 |  2491 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  2492 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  2493 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  2494 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  2495 | `	}` |
|      72 |  2496 | `	pGen->pIn++; /* Jump '{' */` |
|      72 |  2497 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      72 |  2498 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  2499 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  2500 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  2501 | `	}` |
|       - |  2502 | `	/* Allocate ph7_match container */` |
|      72 |  2503 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      72 |  2504 | `	if( pMatch == 0 ){` |
|     ! 0 |  2505 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  2506 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2507 | `		return SXERR_ABORT;` |
|       - |  2508 | `	}` |
|      72 |  2509 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      72 |  2510 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  2511 | `	/* Iterate arms */` |
|     250 |  2512 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  2513 | `		ph7_match_arm sArm;` |
|       - |  2514 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     184 |  2515 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     184 |  2516 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     184 |  2517 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     184 |  2518 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  2519 | `		/* 'default' arm? */` |
|     182 |  2520 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     103 |  2521 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      22 |  2522 | `			if( bHasDefault ){` |
|       3 |  2523 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  2524 | `					"Match expressions may only contain one default arm");` |
|       4 |  2525 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  2526 | `			}` |
|      20 |  2527 | `			sArm.bDefault = 1;` |
|      20 |  2528 | `			bHasDefault = 1;` |
|      20 |  2529 | `			pGen->pIn++;` |
|      20 |  2530 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  2531 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2532 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  2533 | `			}` |
|      20 |  2534 | `			pGen->pIn++; /* Jump '=>' */` |
|      11 |  2535 | `		}else{` |
|       - |  2536 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     164 |  2537 | `			pCondStart = pGen->pIn;` |
|     164 |  2538 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  2539 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     172 |  2540 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  2541 | `				SySet sCondBc;` |
|       9 |  2542 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  2543 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  2544 | `						"syntax error, empty match condition expression");` |
|       - |  2545 | `				}` |
|       9 |  2546 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  2547 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  2548 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2549 | `					return SXERR_ABORT;` |
|       - |  2550 | `				}` |
|       9 |  2551 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  2552 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  2553 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  2554 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  2555 | `			}` |
|     164 |  2556 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  2557 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2558 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  2559 | `			}` |
|     162 |  2560 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  2561 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  2562 | `					"syntax error, empty match condition expression");` |
|       - |  2563 | `			}` |
|       - |  2564 | `			{` |
|       - |  2565 | `				SySet sCondBc;` |
|     162 |  2566 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     162 |  2567 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     162 |  2568 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2569 | `					return SXERR_ABORT;` |
|       - |  2570 | `				}` |
|     162 |  2571 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  2572 | `			}` |
|     162 |  2573 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  2574 | `		}` |
|       - |  2575 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     180 |  2576 | `		pResStart = pGen->pIn;` |
|     180 |  2577 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     180 |  2578 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  2579 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  2580 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  2581 | `		}` |
|     180 |  2582 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     180 |  2583 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2584 | `			return SXERR_ABORT;` |
|       - |  2585 | `		}` |
|     180 |  2586 | `		pGen->pIn = pResEnd;` |
|     180 |  2587 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     148 |  2588 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      73 |  2589 | `		}` |
|     180 |  2590 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       2 |  2591 | `	}` |
|      68 |  2592 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      68 |  2593 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      68 |  2594 | `	return SXRET_OK;` |
|      37 |  2595 |  |
|       - |  2596 | `/*` |
|       - |  2597 | ` * Compile a backtick quoted string.` |
|       - |  2598 | ` */` |
|       4 |  2599 | `static sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  2600 |  |
|       - |  2601 | `	/* TICKET 1433-40: This construct is disabled in the current release of the PH7 engine.` |
|       - |  2602 | `	 * If you want this feature,please contact symisc systems via contact@symisc.net` |
|       - |  2603 | `	 */` |
|       7 |  2604 | `	PH7_GenCompileError(&(*pGen),E_NOTICE,pGen->pIn->nLine,` |
|       - |  2605 | `		"Command line invocation is disabled in the current release of the PH7(%s) engine",` |
|       2 |  2606 | `		ph7_lib_version()` |
|       - |  2607 | `		);` |
|       - |  2608 | `	/* Load NULL */` |
|       5 |  2609 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2610 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  2611 | `	/* Node successfully compiled */` |
|       5 |  2612 | `	return SXRET_OK;` |
|       1 |  2613 |  |
|       - |  2614 | `/*` |
|       - |  2615 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  2616 | ` * construct.` |
|       - |  2617 | ` */` |
|      74 |  2618 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2619 |  |
|       - |  2620 | `	SyString *pName;` |
|       - |  2621 | `	sxu32 nKeyID;` |
|       - |  2622 | `	sxi32 rc;` |
|       - |  2623 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|      76 |  2624 | `	pName = &pGen->pIn->sData;` |
|      76 |  2625 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  2626 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|      76 |  2627 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       9 |  2628 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  2629 | `		/* Compile arguments one after one */` |
|       9 |  2630 | `		pTmp = pGen->pEnd;` |
|       - |  2631 | `		/* Symisc eXtension to the PHP programming language:` |
|       - |  2632 | `		 * 'echo' can be used in the context of a function which` |
|       - |  2633 | `		 *  mean that the following expression is valid:` |
|       - |  2634 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|       - |  2635 | `		 */` |
|       9 |  2636 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|      17 |  2637 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       9 |  2638 | `			if( pGen->pIn < pNext ){` |
|       9 |  2639 | `				pGen->pEnd = pNext;` |
|       9 |  2640 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       9 |  2641 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  2642 | `					return SXERR_ABORT;` |
|       - |  2643 | `				}` |
|       9 |  2644 | `				if( rc != SXERR_EMPTY ){` |
|       - |  2645 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  2646 | `					 * without the overhead of a function call.` |
|       - |  2647 | `					 * This is a very powerful optimization that improve` |
|       - |  2648 | `					 * performance greatly.` |
|       - |  2649 | `					 */` |
|       9 |  2650 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       4 |  2651 | `				}` |
|       4 |  2652 | `			}` |
|       - |  2653 | `			/* Jump trailing commas */` |
|       9 |  2654 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 |  2655 | `				pNext++;` |
|     ! 0 |  2656 | `			}` |
|       9 |  2657 | `			pGen->pIn = pNext;` |
|       1 |  2658 | `		}` |
|       - |  2659 | `		/* Restore token stream */` |
|       9 |  2660 | `		pGen->pEnd = pTmp;` |
|       5 |  2661 | `	}else{` |
|      68 |  2662 | `		sxi32 nArg = 0;` |
|      68 |  2663 | `		sxu32 nIdx = 0;` |
|      68 |  2664 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      68 |  2665 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2666 | `			return SXERR_ABORT;` |
|      68 |  2667 | `		}else if(rc != SXERR_EMPTY ){` |
|      68 |  2668 | `			nArg = 1;` |
|      33 |  2669 | `		}` |
|      68 |  2670 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - |  2671 | `			ph7_value *pObj;` |
|       - |  2672 | `			/* Emit the call instruction */` |
|      20 |  2673 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 |  2674 | `			if( pObj == 0 ){` |
|     ! 0 |  2675 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2676 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2677 | `				return SXERR_ABORT;` |
|       - |  2678 | `			}` |
|      20 |  2679 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - |  2680 | `			/* Install in the literal table */` |
|      20 |  2681 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       9 |  2682 | `		}` |
|       - |  2683 | `		/* Emit the call instruction */` |
|      68 |  2684 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      68 |  2685 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - |  2686 | `	}` |
|       - |  2687 | `	/* Node successfully compiled */` |
|      76 |  2688 | `	return SXRET_OK;` |
|      39 |  2689 |  |
|       - |  2690 | `/*` |
|       - |  2691 | ` * Compile a node holding a variable declaration.` |
|       - |  2692 | ` * According to the PHP language reference` |
|       - |  2693 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - |  2694 | ` *  The variable name is case-sensitive.` |
|       - |  2695 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - |  2696 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  2697 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - |  2698 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - |  2699 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - |  2700 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - |  2701 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - |  2702 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - |  2703 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - |  2704 | ` *  the chapter on Expressions.` |
|       - |  2705 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - |  2706 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - |  2707 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - |  2708 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - |  2709 | ` *  is being assigned (the source variable).` |
|       - |  2710 | ` */` |
|  919568 |  2711 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2712 |  |
|  919570 |  2713 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  2714 | `	sxi32 iVv;` |
|       - |  2715 | `	sxi32 iP1;` |
|       - |  2716 | `	void *p3;` |
|       - |  2717 | `	sxi32 rc;` |
|  919570 |  2718 | `	iVv = -1; /* Variable variable counter */` |
| 1839150 |  2719 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
|  919582 |  2720 | `		pGen->pIn++;` |
|  919582 |  2721 | `		iVv++;` |
|       2 |  2722 | `	}` |
|  919570 |  2723 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - |  2724 | `		/* Invalid variable name */` |
|     ! 0 |  2725 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Invalid variable name");` |
|     ! 0 |  2726 | `		if( rc == SXERR_ABORT ){` |
|       - |  2727 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  2728 | `			return SXERR_ABORT;` |
|       - |  2729 | `		}` |
|     ! 0 |  2730 | `		return SXRET_OK;` |
|       - |  2731 | `	}` |
|  919570 |  2732 | `	p3  = 0;` |
|  919570 |  2733 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - |  2734 | `		/* Dynamic variable creation */` |
|      18 |  2735 | `		pGen->pIn++;  /* Jump the open curly */` |
|      18 |  2736 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      18 |  2737 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  2738 | `			/* Empty expression */` |
|       3 |  2739 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Invalid variable name");` |
|       3 |  2740 | `			return SXRET_OK;` |
|       - |  2741 | `		}` |
|       - |  2742 | `		/* Compile the expression holding the variable name */` |
|      16 |  2743 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      16 |  2744 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  2745 | `			return SXERR_ABORT;` |
|      16 |  2746 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 |  2747 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLineLocal,"Missing variable name");` |
|       3 |  2748 | `			return SXRET_OK;` |
|       - |  2749 | `		}` |
|       7 |  2750 | `	}else{` |
|       - |  2751 | `		SyHashEntry *pEntry;` |
|       - |  2752 | `		SyString *pName;` |
|  919554 |  2753 | `		char *zName = 0;` |
|       - |  2754 | `		/* Extract variable name */` |
|  919554 |  2755 | `		pName = &pGen->pIn->sData;` |
|       - |  2756 | `		/* Advance the stream cursor */` |
|  919554 |  2757 | `		pGen->pIn++;` |
|  919554 |  2758 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
|  919554 |  2759 | `		if( pEntry == 0 ){` |
|       - |  2760 | `			/* Duplicate name */` |
|  123420 |  2761 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  123420 |  2762 | `			if( zName == 0 ){` |
|     ! 0 |  2763 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2764 | `				return SXERR_ABORT;` |
|       - |  2765 | `			}` |
|       - |  2766 | `			/* Install in the hashtable */` |
|  123420 |  2767 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   61711 |  2768 | `		}else{` |
|       - |  2769 | `			/* Name already available */` |
|  796136 |  2770 | `			zName = (char *)pEntry->pUserData;` |
|       - |  2771 | `		}` |
|  919554 |  2772 | `		p3 = (void *)zName;` |
|       - |  2773 | `	}` |
|  919566 |  2774 | `	iP1 = 0;` |
|  919566 |  2775 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  335224 |  2776 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - |  2777 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  328766 |  2778 | `			iP1 = 1;` |
|  164382 |  2779 | `		}` |
|  167611 |  2780 | `	}` |
|       - |  2781 | `	/* Emit the load instruction */` |
|  919566 |  2782 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
|  919578 |  2783 | `	while( iVv > 0 ){` |
|      13 |  2784 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|      13 |  2785 | `		iVv--;` |
|       1 |  2786 | `	}` |
|       - |  2787 | `	/* Node successfully compiled */` |
|  919566 |  2788 | `	return SXRET_OK;` |
|  459786 |  2789 |  |
|       - |  2790 | `/*` |
|       - |  2791 | ` * Load a literal.` |
|       - |  2792 | ` */` |
|  646240 |  2793 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       2 |  2794 |  |
|  646242 |  2795 | `	SyToken *pToken = pGen->pIn;` |
|       - |  2796 | `	ph7_value *pObj;` |
|       - |  2797 | `	SyString *pStr;` |
|       - |  2798 | `	sxu32 nIdx;` |
|       - |  2799 | `	/* Extract token value */` |
|  646242 |  2800 | `	pStr = &pToken->sData;` |
|       - |  2801 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first */` |
|  646242 |  2802 | `	if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  137006 |  2803 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - |  2804 | `			/* NULL constant are always indexed at 0 */` |
|   50464 |  2805 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   50464 |  2806 | `			return SXRET_OK;` |
|   86544 |  2807 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - |  2808 | `			/* TRUE constant are always indexed at 1 */` |
|     524 |  2809 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     524 |  2810 | `			return SXRET_OK;` |
|       2 |  2811 | `		}` |
|  603510 |  2812 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  102524 |  2813 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - |  2814 | `			/* FALSE constant are always indexed at 2 */` |
|   38754 |  2815 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   38754 |  2816 | `			return SXRET_OK;` |
|  516471 |  2817 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   91970 |  2818 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - |  2819 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|    8812 |  2820 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    8812 |  2821 | `			if( pObj == 0 ){` |
|     ! 0 |  2822 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2823 | `				return SXERR_ABORT;` |
|       - |  2824 | `			}` |
|    8812 |  2825 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - |  2826 | `			/* Emit the load constant instruction */` |
|    8812 |  2827 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    8812 |  2828 | `			return SXRET_OK;` |
|  476489 |  2829 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   29626 |  2830 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - |  2831 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       7 |  2832 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       7 |  2833 | `			if( pObj == 0 ){` |
|     ! 0 |  2834 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2835 | `				return SXERR_ABORT;` |
|       - |  2836 | `			}` |
|       7 |  2837 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - |  2838 | `				SyString sNs;` |
|       7 |  2839 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 |  2840 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 |  2841 | `			}else{` |
|     ! 0 |  2842 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - |  2843 | `			}` |
|       7 |  2844 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       7 |  2845 | `			return SXRET_OK;` |
|  475610 |  2846 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   12348 |  2847 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  469430 |  2848 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   15538 |  2849 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      11 |  2850 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - |  2851 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|      21 |  2852 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|       - |  2853 | `				/* Point to the upper block */` |
|      11 |  2854 | `				pBlock = pBlock->pParent;` |
|       1 |  2855 | `			}` |
|      11 |  2856 | `			if( pBlock == 0 ){` |
|       - |  2857 | `				/* Called in the global scope,load NULL */` |
|       5 |  2858 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 |  2859 | `			}else{` |
|       - |  2860 | `				/* Extract the target function/method */` |
|       7 |  2861 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       7 |  2862 | `				if( pStr->zString[2] == 'M' /* METHOD */ && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|       - |  2863 | `					/* Not a class method,Load null */` |
|       3 |  2864 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       2 |  2865 | `				}else{` |
|       5 |  2866 | `					pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       5 |  2867 | `					if( pObj == 0 ){` |
|     ! 0 |  2868 | `						PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  2869 | `						return SXERR_ABORT;` |
|       - |  2870 | `					}` |
|       5 |  2871 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|       - |  2872 | `					/* Emit the load constant instruction */` |
|       5 |  2873 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - |  2874 | `				}` |
|       - |  2875 | `			}` |
|      11 |  2876 | `			return SXRET_OK;` |
|       - |  2877 | `	}` |
|       - |  2878 | `	/* Query literal table */` |
|  547680 |  2879 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|       - |  2880 | `		ph7_value *pLitObj;` |
|       - |  2881 | `		/* Unknown literal,install it in the literal table */` |
|  227486 |  2882 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  227486 |  2883 | `		if( pLitObj == 0 ){` |
|     ! 0 |  2884 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2885 | `			return SXERR_ABORT;` |
|       - |  2886 | `		}` |
|  227486 |  2887 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  227486 |  2888 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  113742 |  2889 | `	}` |
|       - |  2890 | `	/* Emit the load constant instruction */` |
|  547680 |  2891 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
|  547680 |  2892 | `	return SXRET_OK;` |
|  323122 |  2893 |  |
|       - |  2894 | `/*` |
|       - |  2895 | ` * Resolve a namespace path or simply load a literal.` |
|       - |  2896 | ` * If the token stream contains namespace separators (backslashes),` |
|       - |  2897 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - |  2898 | ` * Otherwise, load the simple literal directly.` |
|       - |  2899 | ` */` |
|  646274 |  2900 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       2 |  2901 |  |
|       - |  2902 | `	sxi32 rc;` |
|  646276 |  2903 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  2904 | `		return SXRET_OK;` |
|       - |  2905 | `	}` |
|       - |  2906 | `	/* Check if this is a multi-token namespace path */` |
|  646276 |  2907 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - |  2908 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|      36 |  2909 | `		SyBlob *pWorker = &pGen->sWorker;` |
|      36 |  2910 | `		int isAbsolute = 0;` |
|      36 |  2911 | `		SyBlobReset(pWorker);` |
|       - |  2912 | `		/* Check for leading backslash (absolute path) */` |
|      36 |  2913 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      34 |  2914 | `			isAbsolute = 1;` |
|      34 |  2915 | `			pGen->pIn++; /* Skip leading backslash */` |
|      16 |  2916 | `		}` |
|       - |  2917 | `		/* For relative qualified names in a namespace, prepend the NS */` |
|      36 |  2918 | `		if( !isAbsolute && SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       3 |  2919 | `			SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       3 |  2920 | `			SyBlobAppend(pWorker,"\\",1);` |
|       1 |  2921 | `		}` |
|       - |  2922 | `		/* Collect all path components */` |
|     132 |  2923 | `		while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     132 |  2924 | `			if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      50 |  2925 | `				SyBlobAppend(pWorker,"\\",1);` |
|      26 |  2926 | `			}else{` |
|      84 |  2927 | `				SyBlobAppend(pWorker,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  2928 | `			}` |
|     132 |  2929 | `			if( pGen->pIn == &pGen->pEnd[-1] ){` |
|      36 |  2930 | `				pGen->pIn++;` |
|      36 |  2931 | `				break;` |
|       - |  2932 | `			}` |
|      98 |  2933 | `			pGen->pIn++;` |
|       2 |  2934 | `		}` |
|      36 |  2935 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - |  2936 | `			ph7_value *pObj;` |
|       - |  2937 | `			SyString sPath;` |
|       - |  2938 | `			sxu32 nIdx;` |
|      36 |  2939 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - |  2940 | `			/* Install in the literal table */` |
|      36 |  2941 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|      18 |  2942 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 |  2943 | `				if( pObj == 0 ){` |
|     ! 0 |  2944 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  2945 | `					return SXERR_ABORT;` |
|       - |  2946 | `				}` |
|      18 |  2947 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|      18 |  2948 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       8 |  2949 | `			}` |
|       - |  2950 | `			/* Emit the load constant instruction.` |
|       - |  2951 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - |  2952 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|      53 |  2953 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|      17 |  2954 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|      17 |  2955 | `				nIdx,0,0);` |
|      36 |  2956 | `			return SXRET_OK;` |
|       - |  2957 | `		}` |
|     ! 0 |  2958 | `	}` |
|       - |  2959 | `	/* Single-token literal: load directly */` |
|  646242 |  2960 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  646242 |  2961 | `	return rc;` |
|  323139 |  2962 |  |
|       - |  2963 | `/*` |
|       - |  2964 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - |  2965 | ` */` |
|  646274 |  2966 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       2 |  2967 |  |
|       - |  2968 | `	sxi32 rc;` |
|  646276 |  2969 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  646276 |  2970 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  2971 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 |  2972 | `		return rc;` |
|       - |  2973 | `	}` |
|       - |  2974 | `	/* Node successfully compiled */` |
|  646276 |  2975 | `	return SXRET_OK;` |
|  323139 |  2976 |  |
|       - |  2977 | `/*` |
|       - |  2978 | ` * Recover from a compile-time error. In other words synchronize` |
|       - |  2979 | ` * the token stream cursor with the first semi-colon seen.` |
|       - |  2980 | ` */` |
|       8 |  2981 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|       1 |  2982 |  |
|       - |  2983 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      17 |  2984 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|       9 |  2985 | `		pGen->pIn++;` |
|       1 |  2986 | `	}` |
|       9 |  2987 | `	return SXRET_OK;` |
|       1 |  2988 |  |
|       - |  2989 | `/*` |
|       - |  2990 | ` * Check if the given identifier name is reserved or not.` |
|       - |  2991 | ` * Return TRUE if reserved.FALSE otherwise.` |
|       - |  2992 | ` */` |
|      56 |  2993 | `static int GenStateIsReservedConstant(SyString *pName)` |
|       2 |  2994 |  |
|      58 |  2995 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      26 |  2996 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|       3 |  2997 | `			return TRUE;` |
|      24 |  2998 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|       5 |  2999 | `			return TRUE;` |
|       2 |  3000 | `		}` |
|      43 |  3001 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       3 |  3002 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|       3 |  3003 | `			return TRUE;` |
|       - |  3004 | `		}` |
|     ! 0 |  3005 | `	}` |
|       - |  3006 | `	/* Not a reserved constant */` |
|      50 |  3007 | `	return FALSE;` |
|      30 |  3008 |  |
|       - |  3009 | `/*` |
|       - |  3010 | ` * Compile the 'const' statement.` |
|       - |  3011 | ` * According to the PHP language reference` |
|       - |  3012 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|       - |  3013 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|       - |  3014 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|       - |  3015 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|       - |  3016 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - |  3017 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|       - |  3018 | ` *  Syntax` |
|       - |  3019 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|       - |  3020 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|       - |  3021 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|       - |  3022 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|       - |  3023 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|       - |  3024 | ` *  to get a list of all defined constants.` |
|       - |  3025 | ` *` |
|       - |  3026 | ` * Symisc eXtension.` |
|       - |  3027 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|       - |  3028 | ` *  would allow only simple scalar value.` |
|       - |  3029 | ` *  Example` |
|       - |  3030 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  3031 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  3032 | ` */` |
|      32 |  3033 | `static sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|       2 |  3034 |  |
|       - |  3035 | `	SySet *pConsCode,*pInstrContainer;` |
|      34 |  3036 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  3037 | `	SyString *pName;` |
|       - |  3038 | `	sxi32 rc;` |
|      34 |  3039 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      34 |  3040 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  3041 | `		/* Invalid constant name */` |
|       7 |  3042 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Invalid constant name");` |
|       7 |  3043 | `		if( rc == SXERR_ABORT ){` |
|       - |  3044 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3045 | `			return SXERR_ABORT;` |
|       - |  3046 | `		}` |
|       7 |  3047 | `		goto Synchronize;` |
|       - |  3048 | `	}` |
|       - |  3049 | `	/* Peek constant name */` |
|      28 |  3050 | `	pName = &pGen->pIn->sData;` |
|       - |  3051 | `	/* Make sure the constant name isn't reserved */` |
|      28 |  3052 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  3053 | `		/* Reserved constant */` |
|       9 |  3054 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Cannot redeclare a reserved constant '%z'",pName);` |
|       9 |  3055 | `		if( rc == SXERR_ABORT ){` |
|       - |  3056 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3057 | `			return SXERR_ABORT;` |
|       - |  3058 | `		}` |
|       9 |  3059 | `		goto Synchronize;` |
|       - |  3060 | `	}` |
|      20 |  3061 | `	pGen->pIn++;` |
|      20 |  3062 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  3063 | `		/* Invalid statement*/` |
|       5 |  3064 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"const: Expected '=' after constant name");` |
|       5 |  3065 | `		if( rc == SXERR_ABORT ){` |
|       - |  3066 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3067 | `			return SXERR_ABORT;` |
|       - |  3068 | `		}` |
|       5 |  3069 | `		goto Synchronize;` |
|       - |  3070 | `	}` |
|      15 |  3071 | `	pGen->pIn++; /*Jump the equal sign */` |
|       - |  3072 | `	/* Allocate a new constant value container */` |
|      15 |  3073 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      15 |  3074 | `	if( pConsCode == 0 ){` |
|     ! 0 |  3075 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3076 | `		return SXERR_ABORT;` |
|       - |  3077 | `	}` |
|      15 |  3078 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  3079 | `	/* Swap bytecode container */` |
|      15 |  3080 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      15 |  3081 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|       - |  3082 | `	/* Compile constant value */` |
|      15 |  3083 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3084 | `	/* Emit the done instruction */` |
|      15 |  3085 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      15 |  3086 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      15 |  3087 | `	if( rc == SXERR_ABORT ){` |
|       - |  3088 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  3089 | `		return SXERR_ABORT;` |
|       - |  3090 | `	}` |
|      15 |  3091 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|       - |  3092 | `	/* Register the constant with namespace-qualified name */` |
|       - |  3093 | `	{` |
|       - |  3094 | `		SyBlob sFQN;` |
|       - |  3095 | `		SyString sFQNStr;` |
|      15 |  3096 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      15 |  3097 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      15 |  3098 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      15 |  3099 | `		rc = PH7_VmRegisterConstant(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode);` |
|      15 |  3100 | `		SyBlobRelease(&sFQN);` |
|       - |  3101 | `	}` |
|      15 |  3102 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3103 | `		SySetRelease(pConsCode);` |
|     ! 0 |  3104 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|     ! 0 |  3105 | `	}` |
|      15 |  3106 | `	return SXRET_OK;` |
|       9 |  3107 | `Synchronize:` |
|       - |  3108 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|      57 |  3109 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      39 |  3110 | `		pGen->pIn++;` |
|       1 |  3111 | `	}` |
|      19 |  3112 | `	return SXRET_OK;` |
|      18 |  3113 |  |
|       - |  3114 | `/*` |
|       - |  3115 | ` * Compile the 'continue' statement.` |
|       - |  3116 | ` * According to the PHP language reference` |
|       - |  3117 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|       - |  3118 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|       - |  3119 | ` *  iteration.` |
|       - |  3120 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|       - |  3121 | ` *  the purposes of continue.` |
|       - |  3122 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|       - |  3123 | ` *  of enclosing loops it should skip to the end of.` |
|       - |  3124 | ` *  Note:` |
|       - |  3125 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|       - |  3126 | ` */` |
|       - |  3127 | `/*` |
|       - |  3128 | ` * Emit PH7_OP_POP_EXCEPTION for each exception block between the current` |
|       - |  3129 | ` * block and the target loop block. This ensures finally blocks run when` |
|       - |  3130 | ` * break/continue crosses a try boundary.` |
|       - |  3131 | ` *` |
|       - |  3132 | ` * Stop walking at catch/finally blocks (GEN_BLOCK_EXCEPTION without pUserData):` |
|       - |  3133 | ` * those are compiled into separate bytecode containers executed via VmLocalExec,` |
|       - |  3134 | ` * so we must not emit POP_EXCEPTION for the parent try from inside them.` |
|       - |  3135 | ` */` |
|    3064 |  3136 | `static void GenStateEmitExceptionPopForBreak(ph7_gen_state *pGen,GenBlock *pTarget)` |
|       2 |  3137 |  |
|    3066 |  3138 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   17930 |  3139 | `	while( pBlock && pBlock != pTarget ){` |
|   14866 |  3140 | `		if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       3 |  3141 | `			if( pBlock->pUserData ){` |
|       - |  3142 | `				/* This is a try block with an exception context — emit POP_EXCEPTION */` |
|       3 |  3143 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pBlock->pUserData,0);` |
|       2 |  3144 | `			}else{` |
|       - |  3145 | `				/* This is a catch/finally block compiled into a separate bytecode` |
|       - |  3146 | `				 * container. Stop here — we cannot cross into the parent try's` |
|       - |  3147 | `				 * exception context from a sub-execution.` |
|       - |  3148 | `				 */` |
|     ! 0 |  3149 | `				break;` |
|       - |  3150 | `			}` |
|       1 |  3151 | `		}` |
|   14866 |  3152 | `		pBlock = pBlock->pParent;` |
|       2 |  3153 | `	}` |
|    3066 |  3154 |  |
|    2980 |  3155 | `static sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|       2 |  3156 |  |
|       - |  3157 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3158 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3159 | `	sxu32 nLineLocal;` |
|       - |  3160 | `	sxi32 rc;` |
|    2982 |  3161 | `	nLineLocal = pGen->pIn->nLine;` |
|    2982 |  3162 | `	iLevel = 0;` |
|       - |  3163 | `	/* Jump the 'continue' keyword */` |
|    2982 |  3164 | `	pGen->pIn++;` |
|    2982 |  3165 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3166 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3167 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3168 | `		 */` |
|       - |  3169 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3170 | `		char *zAlloc = 0;` |
|       - |  3171 | `		SyString sNum;` |
|      16 |  3172 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3173 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3174 | `			return SXERR_ABORT;` |
|       - |  3175 | `		}` |
|      16 |  3176 | `		if( rc == SXRET_OK ){` |
|      20 |  3177 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3178 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3179 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3180 | `				return SXERR_ABORT;` |
|       - |  3181 | `			}` |
|      14 |  3182 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3183 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3184 | `		}` |
|      16 |  3185 | `		if( iLevel < 2 ){` |
|       3 |  3186 | `			iLevel = 0;` |
|       1 |  3187 | `		}` |
|      16 |  3188 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3189 | `	}` |
|       - |  3190 | `	/* Point to the target loop */` |
|    2982 |  3191 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|    2982 |  3192 | `	if( pLoop == 0 ){` |
|       - |  3193 | `		/* Illegal continue */` |
|      11 |  3194 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"A 'continue' statement may only be used within a loop or switch");` |
|      11 |  3195 | `		if( rc == SXERR_ABORT ){` |
|       - |  3196 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3197 | `			return SXERR_ABORT;` |
|       - |  3198 | `		}` |
|       6 |  3199 | `	}else{` |
|    2972 |  3200 | `		sxu32 nInstrIdx = 0;` |
|       - |  3201 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|    2972 |  3202 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|    2972 |  3203 | `		if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|       - |  3204 | `			/* According to the PHP language reference manual` |
|       - |  3205 | `			 *  Note that unlike some other languages, the continue statement applies to switch` |
|       - |  3206 | `			 *  and acts similar to break. If you have a switch inside a loop and wish to continue` |
|       - |  3207 | `			 *  to the next iteration of the outer loop, use continue 2.` |
|       - |  3208 | `			 */` |
|       5 |  3209 | `			rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|       5 |  3210 | `			if( rc == SXRET_OK ){` |
|       5 |  3211 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|       2 |  3212 | `			}` |
|       3 |  3213 | `		}else{` |
|       - |  3214 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|    2968 |  3215 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pLoop->nFirstInstr,0,&nInstrIdx);` |
|    2968 |  3216 | `			if( pLoop->bPostContinue == TRUE ){` |
|       - |  3217 | `				JumpFixup sJumpFix;` |
|       - |  3218 | `				/* Post-continue */` |
|      14 |  3219 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|      14 |  3220 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|      14 |  3221 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|       6 |  3222 | `			}` |
|       - |  3223 | `		}` |
|       - |  3224 | `	}` |
|    2982 |  3225 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3226 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3227 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|     ! 0 |  3228 | `	}` |
|       - |  3229 | `	/* Statement successfully compiled */` |
|    2982 |  3230 | `	return SXRET_OK;` |
|    1492 |  3231 |  |
|       - |  3232 | `/*` |
|       - |  3233 | ` * Compile the 'break' statement.` |
|       - |  3234 | ` * According to the PHP language reference` |
|       - |  3235 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|       - |  3236 | ` *  structure.` |
|       - |  3237 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|       - |  3238 | ` *  enclosing structures are to be broken out of.` |
|       - |  3239 | ` */` |
|     110 |  3240 | `static sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|       2 |  3241 |  |
|       - |  3242 | `	GenBlock *pLoop; /* Target loop */` |
|       - |  3243 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|       - |  3244 | `	sxi32 rc;` |
|     112 |  3245 | `	iLevel = 0;` |
|       - |  3246 | `	/* Jump the 'break' keyword */` |
|     112 |  3247 | `	pGen->pIn++;` |
|     112 |  3248 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|       - |  3249 | `		/* optional numeric argument which tells us how many levels` |
|       - |  3250 | `		 * of enclosing loops we should skip to the end of.` |
|       - |  3251 | `		 */` |
|       - |  3252 | `		char zScratch[GEN_NUM_SCRATCH];` |
|      16 |  3253 | `		char *zAlloc = 0;` |
|       - |  3254 | `		SyString sNum;` |
|      16 |  3255 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|      16 |  3256 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3257 | `			return SXERR_ABORT;` |
|       - |  3258 | `		}` |
|      16 |  3259 | `		if( rc == SXRET_OK ){` |
|      20 |  3260 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|      12 |  3261 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|      14 |  3262 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  3263 | `				return SXERR_ABORT;` |
|       - |  3264 | `			}` |
|      14 |  3265 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|      14 |  3266 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|       6 |  3267 | `		}` |
|      16 |  3268 | `		if( iLevel < 2 ){` |
|       3 |  3269 | `			iLevel = 0;` |
|       1 |  3270 | `		}` |
|      16 |  3271 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|       7 |  3272 | `	}` |
|       - |  3273 | `	/* Extract the target loop */` |
|     112 |  3274 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|     112 |  3275 | `	if( pLoop == 0 ){` |
|       - |  3276 | `		/* Illegal break */` |
|      17 |  3277 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"A 'break' statement may only be used within a loop or switch");` |
|      17 |  3278 | `		if( rc == SXERR_ABORT ){` |
|       - |  3279 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3280 | `			return SXERR_ABORT;` |
|       - |  3281 | `		}` |
|       9 |  3282 | `	}else{` |
|       - |  3283 | `		sxu32 nInstrIdx;` |
|       - |  3284 | `		/* Emit POP_EXCEPTION for any try blocks between here and the loop */` |
|      96 |  3285 | `		GenStateEmitExceptionPopForBreak(&(*pGen),pLoop);` |
|      96 |  3286 | `		rc = PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nInstrIdx);` |
|      96 |  3287 | `		if( rc == SXRET_OK ){` |
|       - |  3288 | `			/* Fix the jump later when the jump destination is resolved */` |
|      96 |  3289 | `			GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|      47 |  3290 | `		}` |
|       - |  3291 | `	}` |
|     112 |  3292 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3293 | `		/* Not so fatal,emit a warning only */` |
|     ! 0 |  3294 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|     ! 0 |  3295 | `	}` |
|       - |  3296 | `	/* Statement successfully compiled */` |
|     112 |  3297 | `	return SXRET_OK;` |
|      57 |  3298 |  |
|       - |  3299 | `/*` |
|       - |  3300 | ` * Compile or record a label.` |
|       - |  3301 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|       - |  3302 | ` * Example` |
|       - |  3303 | ` *  goto LABEL;` |
|       - |  3304 | ` *   echo 'Foo';` |
|       - |  3305 | ` *  LABEL:` |
|       - |  3306 | ` *   echo 'Bar';` |
|       - |  3307 | ` */` |
|     112 |  3308 | `static sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|       2 |  3309 |  |
|       - |  3310 | `	GenBlock *pBlock;` |
|       - |  3311 | `	Label sLabel;` |
|       - |  3312 | `	/* Make sure the label does not occur inside a loop or a try{}catch(); block */` |
|     114 |  3313 | `	pBlock = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP\|GEN_BLOCK_EXCEPTION,0);` |
|     114 |  3314 | `	if( pBlock ){` |
|       - |  3315 | `		sxi32 rc;` |
|       7 |  3316 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       4 |  3317 | `			"Label '%z' inside loop or try/catch block is disallowed",&pGen->pIn->sData);` |
|       5 |  3318 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3319 | `			return SXERR_ABORT;` |
|       - |  3320 | `		}` |
|       3 |  3321 | `	}else{` |
|     110 |  3322 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3323 | `		char *zDup;` |
|       - |  3324 | `		/* Initialize label fields */` |
|     110 |  3325 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|       - |  3326 | `		/* Duplicate label name */` |
|     110 |  3327 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     110 |  3328 | `		if( zDup == 0 ){` |
|     ! 0 |  3329 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3330 | `			return SXERR_ABORT;` |
|       - |  3331 | `		}` |
|     110 |  3332 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|     110 |  3333 | `		sLabel.bRef  = FALSE;` |
|     110 |  3334 | `		sLabel.nLine = pGen->pIn->nLine;` |
|     110 |  3335 | `		pBlock = pGen->pCurrent;` |
|     218 |  3336 | `		while( pBlock ){` |
|     130 |  3337 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      22 |  3338 | `				break;` |
|       - |  3339 | `			}` |
|       - |  3340 | `			/* Point to the upper block */` |
|     110 |  3341 | `			pBlock = pBlock->pParent;` |
|       2 |  3342 | `		}` |
|     110 |  3343 | `		if( pBlock ){` |
|      22 |  3344 | `			sLabel.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      12 |  3345 | `		}else{` |
|      90 |  3346 | `			sLabel.pFunc = 0;` |
|       - |  3347 | `		}` |
|       - |  3348 | `		/* Insert in label set */` |
|     110 |  3349 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|       - |  3350 | `	}` |
|     114 |  3351 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|     114 |  3352 | `	return SXRET_OK;` |
|      58 |  3353 |  |
|       - |  3354 | `/*` |
|       - |  3355 | ` * Compile the so hated 'goto' statement.` |
|       - |  3356 | ` * You've probably been taught that gotos are bad, but this sort` |
|       - |  3357 | ` * of rewriting  happens all the time, in fact every time you run` |
|       - |  3358 | ` * a compiler it has to do this.` |
|       - |  3359 | ` * According to the PHP language reference manual` |
|       - |  3360 | ` *   The goto operator can be used to jump to another section in the program.` |
|       - |  3361 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|       - |  3362 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|       - |  3363 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|       - |  3364 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|       - |  3365 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|       - |  3366 | ` *   of a multi-level break` |
|       - |  3367 | ` */` |
|     152 |  3368 | `static sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|       2 |  3369 |  |
|       - |  3370 | `	JumpFixup sJump;` |
|       - |  3371 | `	sxi32 rc;` |
|     154 |  3372 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|     154 |  3373 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  3374 | `		/* Missing label */` |
|     ! 0 |  3375 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|     ! 0 |  3376 | `		if( rc == SXERR_ABORT ){` |
|       - |  3377 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3378 | `			return SXERR_ABORT;` |
|       - |  3379 | `		}` |
|     ! 0 |  3380 | `		return SXRET_OK;` |
|       - |  3381 | `	}` |
|     154 |  3382 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       5 |  3383 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: Invalid label name: '%z'",&pGen->pIn->sData);` |
|       5 |  3384 | `		if( rc == SXERR_ABORT ){` |
|       - |  3385 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3386 | `			return SXERR_ABORT;` |
|       - |  3387 | `		}` |
|       3 |  3388 | `	}else{` |
|     150 |  3389 | `		SyString *pTarget = &pGen->pIn->sData;` |
|       - |  3390 | `		GenBlock *pBlock;` |
|       - |  3391 | `		char *zDup;` |
|       - |  3392 | `		/* Prepare the jump destination */` |
|     150 |  3393 | `		sJump.nJumpType = PH7_OP_JMP;` |
|     150 |  3394 | `		sJump.nLine = pGen->pIn->nLine;` |
|       - |  3395 | `		/* Duplicate label name */` |
|     150 |  3396 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|     150 |  3397 | `		if( zDup == 0 ){` |
|     ! 0 |  3398 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  3399 | `			return SXERR_ABORT;` |
|       - |  3400 | `		}` |
|     150 |  3401 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|     150 |  3402 | `		pBlock = pGen->pCurrent;` |
|     312 |  3403 | `		while( pBlock ){` |
|     196 |  3404 | `			if( pBlock->iFlags & (GEN_BLOCK_FUNC\|GEN_BLOCK_EXCEPTION) ){` |
|      34 |  3405 | `				break;` |
|       - |  3406 | `			}` |
|       - |  3407 | `			/* Point to the upper block */` |
|     164 |  3408 | `			pBlock = pBlock->pParent;` |
|       2 |  3409 | `		}` |
|     150 |  3410 | `		if( pBlock && pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|       7 |  3411 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto inside try/catch block is disallowed");` |
|       7 |  3412 | `			if( rc == SXERR_ABORT ){` |
|       - |  3413 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3414 | `				return SXERR_ABORT;` |
|       - |  3415 | `			}` |
|       3 |  3416 | `		}` |
|     150 |  3417 | `		if( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC)){` |
|      28 |  3418 | `			sJump.pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      15 |  3419 | `		}else{` |
|     124 |  3420 | `			sJump.pFunc = 0;` |
|       - |  3421 | `		}` |
|       - |  3422 | `		/* Emit the unconditional jump */` |
|     150 |  3423 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|     150 |  3424 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      74 |  3425 | `		}` |
|       - |  3426 | `	}` |
|     154 |  3427 | `	pGen->pIn++; /* Jump the label name */` |
|     154 |  3428 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       3 |  3429 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|       1 |  3430 | `	}` |
|       - |  3431 | `	/* Statement successfully compiled */` |
|     154 |  3432 | `	return SXRET_OK;` |
|      78 |  3433 |  |
|       - |  3434 | `/*` |
|       - |  3435 | ` * Point to the next PHP chunk that will be processed shortly.` |
|       - |  3436 | ` * Return SXRET_OK on success. Any other return value indicates` |
|       - |  3437 | ` * failure.` |
|       - |  3438 | ` */` |
|      20 |  3439 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|       1 |  3440 |  |
|       - |  3441 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|       - |  3442 | `	sxu32 nRawObj;` |
|      10 |  3443 | `	sxu32 nObjIdx;` |
|       - |  3444 | `	/* Consume raw chunks verbatim without any processing until we get` |
|       - |  3445 | `	 * a PHP block.` |
|       - |  3446 | `	 */` |
|      10 |  3447 | `Consume:` |
|      21 |  3448 | `	nRawObj = nObjIdx = 0;` |
|      21 |  3449 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|     ! 0 |  3450 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|     ! 0 |  3451 | `		if( pRawObj == 0 ){` |
|     ! 0 |  3452 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  3453 | `			return SXERR_ABORT;` |
|       - |  3454 | `		}` |
|       - |  3455 | `		/* Mark as constant and emit the load constant instruction */` |
|     ! 0 |  3456 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|     ! 0 |  3457 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     ! 0 |  3458 | `		++nRawObj;` |
|     ! 0 |  3459 | `		pGen->pRawIn++; /* Next chunk */` |
|     ! 0 |  3460 | `	}` |
|      21 |  3461 | `	if( nRawObj > 0 ){` |
|       - |  3462 | `		/* Emit the consume instruction */` |
|     ! 0 |  3463 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     ! 0 |  3464 | `	}` |
|      21 |  3465 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|     ! 0 |  3466 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|       - |  3467 | `		/* Reset the token set */` |
|     ! 0 |  3468 | `		SySetReset(pTokenSet);` |
|       - |  3469 | `		/* Tokenize input */` |
|     ! 0 |  3470 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|     ! 0 |  3471 | `			pGen->pRawIn->nLine,pTokenSet);` |
|       - |  3472 | `		/* Point to the fresh token stream */` |
|     ! 0 |  3473 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     ! 0 |  3474 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|       - |  3475 | `		/* Advance the stream cursor */` |
|     ! 0 |  3476 | `		pGen->pRawIn++;` |
|       - |  3477 | `		/* TICKET 1433-011 */` |
|     ! 0 |  3478 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - |  3479 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - |  3480 | `			sxi32 rc;` |
|       - |  3481 | `			/* Refer to TICKET 1433-009  */` |
|     ! 0 |  3482 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|     ! 0 |  3483 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|     ! 0 |  3484 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|     ! 0 |  3485 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 |  3486 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3487 | `				return SXERR_ABORT;` |
|     ! 0 |  3488 | `			}else if( rc != SXERR_EMPTY ){` |
|     ! 0 |  3489 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  3490 | `			}` |
|     ! 0 |  3491 | `			goto Consume;` |
|       - |  3492 | `		}` |
|     ! 0 |  3493 | `	}else{` |
|       - |  3494 | `		/* No more chunks to process */` |
|      21 |  3495 | `		pGen->pIn = pGen->pEnd;` |
|      21 |  3496 | `		return SXERR_EOF;` |
|       - |  3497 | `	}` |
|     ! 0 |  3498 | `	return SXRET_OK;` |
|      11 |  3499 |  |
|       - |  3500 | `/*` |
|       - |  3501 | ` * Compile a PHP block.` |
|       - |  3502 | ` * A block is simply one or more PHP statements and expressions to compile` |
|       - |  3503 | ` * optionally delimited by braces {}.` |
|       - |  3504 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  3505 | ` * and this function takes care of generating the appropriate error` |
|       - |  3506 | ` * message.` |
|       - |  3507 | ` */` |
|  355766 |  3508 | `static sxi32 PH7_CompileBlock(` |
|       - |  3509 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  3510 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|       - |  3511 | `	)` |
|       2 |  3512 |  |
|       - |  3513 | `	sxi32 rc;` |
|       - |  3514 | `	sxu32 nLine;` |
|  355768 |  3515 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  354360 |  3516 | `		nLine = pGen->pIn->nLine;` |
|  354360 |  3517 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  354360 |  3518 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3519 | `			return SXERR_ABORT;` |
|       - |  3520 | `		}` |
|  354360 |  3521 | `		pGen->pIn++;` |
|       - |  3522 | `		/* Compile until we hit the closing braces '}' */` |
|  484156 |  3523 | `		for(;;){` |
|  968314 |  3524 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      21 |  3525 | `				rc = GenStateNextChunk(&(*pGen));` |
|      21 |  3526 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3527 | `			 	   return SXERR_ABORT;` |
|       - |  3528 | `				}` |
|      21 |  3529 | `				if( rc == SXERR_EOF ){` |
|       - |  3530 | `					/* No more token to process. Missing closing braces */` |
|      21 |  3531 | `					PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Missing closing braces '}'");` |
|      21 |  3532 | `					break;` |
|       - |  3533 | `				}` |
|     ! 0 |  3534 | `			}` |
|  968294 |  3535 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|       - |  3536 | `				/* Closing braces found,break immediately*/` |
|  354340 |  3537 | `				pGen->pIn++;` |
|  354340 |  3538 | `				break;` |
|       - |  3539 | `			}` |
|       - |  3540 | `			/* Compile a single statement */` |
|  613956 |  3541 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|  613956 |  3542 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3543 | `				return SXERR_ABORT;` |
|       - |  3544 | `			}` |
|       2 |  3545 | `		}` |
|  354360 |  3546 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  178589 |  3547 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|     ! 0 |  3548 | `		pGen->pIn++;` |
|     ! 0 |  3549 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|     ! 0 |  3550 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  3551 | `			return SXERR_ABORT;` |
|       - |  3552 | `		}` |
|       - |  3553 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|     ! 0 |  3554 | `		for(;;){` |
|     ! 0 |  3555 | `			if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  3556 | `				rc = GenStateNextChunk(&(*pGen));` |
|     ! 0 |  3557 | `				if (rc == SXERR_ABORT ){` |
|     ! 0 |  3558 | `			 	   return SXERR_ABORT;` |
|       - |  3559 | `				}` |
|     ! 0 |  3560 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|       - |  3561 | `					/* No more token to process */` |
|     ! 0 |  3562 | `					if( rc == SXERR_EOF ){` |
|     ! 0 |  3563 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|       - |  3564 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|     ! 0 |  3565 | `					}` |
|     ! 0 |  3566 | `					break;` |
|       - |  3567 | `				}` |
|     ! 0 |  3568 | `			}` |
|     ! 0 |  3569 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  3570 | `				sxi32 nKwrd;` |
|       - |  3571 | `				/* Keyword found */` |
|     ! 0 |  3572 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     ! 0 |  3573 | `				if( nKwrd == nKeywordEnd \|\|` |
|     ! 0 |  3574 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|       - |  3575 | `						/* Delimiter keyword found,break */` |
|     ! 0 |  3576 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|     ! 0 |  3577 | `							pGen->pIn++; /*  endif;endswitch... */` |
|     ! 0 |  3578 | `						}` |
|     ! 0 |  3579 | `						break;` |
|       - |  3580 | `				}` |
|     ! 0 |  3581 | `			}` |
|       - |  3582 | `			/* Compile a single statement */` |
|     ! 0 |  3583 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|     ! 0 |  3584 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  3585 | `				return SXERR_ABORT;` |
|       - |  3586 | `			}` |
|     ! 0 |  3587 | `		}` |
|     ! 0 |  3588 | `		GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  3589 | `	}else{` |
|       - |  3590 | `		/* Compile a single statement */` |
|    1410 |  3591 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|    1410 |  3592 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3593 | `			return SXERR_ABORT;` |
|       - |  3594 | `		}` |
|       - |  3595 | `	}` |
|       - |  3596 | `	/* Jump trailing semi-colons ';' */` |
|  355768 |  3597 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3598 | `		pGen->pIn++;` |
|     ! 0 |  3599 | `	}` |
|  355768 |  3600 | `	return SXRET_OK;` |
|  177885 |  3601 |  |
|       - |  3602 | `/*` |
|       - |  3603 | ` * Compile the gentle 'while' statement.` |
|       - |  3604 | ` * According to the PHP language reference` |
|       - |  3605 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|       - |  3606 | ` *  The basic form of a while statement is:` |
|       - |  3607 | ` *  while (expr)` |
|       - |  3608 | ` *   statement` |
|       - |  3609 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|       - |  3610 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|       - |  3611 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|       - |  3612 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|       - |  3613 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|       - |  3614 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|       - |  3615 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|       - |  3616 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|       - |  3617 | ` *  while (expr):` |
|       - |  3618 | ` *    statement` |
|       - |  3619 | ` *   endwhile;` |
|       - |  3620 | ` */` |
|   11846 |  3621 | `static sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|       2 |  3622 |  |
|   11848 |  3623 | `	GenBlock *pWhileBlock = 0;` |
|   11848 |  3624 | `	SyToken *pTmp,*pEnd = 0;` |
|       - |  3625 | `	sxu32 nFalseJump;` |
|       - |  3626 | `	sxu32 nLine;` |
|       - |  3627 | `	sxi32 rc;` |
|   11848 |  3628 | `	nLine = pGen->pIn->nLine;` |
|       - |  3629 | `	/* Jump the 'while' keyword */` |
|   11848 |  3630 | `	pGen->pIn++;` |
|   11848 |  3631 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3632 | `		/* Syntax error */` |
|     ! 0 |  3633 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3634 | `		if( rc == SXERR_ABORT ){` |
|       - |  3635 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3636 | `			return SXERR_ABORT;` |
|       - |  3637 | `		}` |
|     ! 0 |  3638 | `		goto Synchronize;` |
|       - |  3639 | `	}` |
|       - |  3640 | `	/* Jump the left parenthesis '(' */` |
|   11848 |  3641 | `	pGen->pIn++;` |
|       - |  3642 | `	/* Create the loop block */` |
|   11848 |  3643 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|   11848 |  3644 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3645 | `		return SXERR_ABORT;` |
|       - |  3646 | `	}` |
|       - |  3647 | `	/* Delimit the condition */` |
|   11848 |  3648 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11848 |  3649 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3650 | `		/* Empty expression */` |
|       3 |  3651 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|       3 |  3652 | `		if( rc == SXERR_ABORT ){` |
|       - |  3653 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3654 | `			return SXERR_ABORT;` |
|       - |  3655 | `		}` |
|       1 |  3656 | `	}` |
|       - |  3657 | `	/* Swap token streams */` |
|   11848 |  3658 | `	pTmp = pGen->pEnd;` |
|   11848 |  3659 | `	pGen->pEnd = pEnd;` |
|       - |  3660 | `	/* Compile the expression */` |
|   11848 |  3661 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11848 |  3662 | `	if( rc == SXERR_ABORT ){` |
|       - |  3663 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3664 | `		return SXERR_ABORT;` |
|       - |  3665 | `	}` |
|       - |  3666 | `	/* Update token stream */` |
|   11848 |  3667 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3668 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3669 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3670 | `			return SXERR_ABORT;` |
|       - |  3671 | `		}` |
|     ! 0 |  3672 | `		pGen->pIn++;` |
|     ! 0 |  3673 | `	}` |
|       - |  3674 | `	/* Synchronize pointers */` |
|   11848 |  3675 | `	pGen->pIn  = &pEnd[1];` |
|   11848 |  3676 | `	pGen->pEnd = pTmp;` |
|       - |  3677 | `	/* Emit the false jump */` |
|   11848 |  3678 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3679 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11848 |  3680 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|       - |  3681 | `	/* Compile the loop body */` |
|   11848 |  3682 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|   11848 |  3683 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3684 | `		return SXERR_ABORT;` |
|       - |  3685 | `	}` |
|       - |  3686 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11848 |  3687 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|       - |  3688 | `	/* Fix all jumps now the destination is resolved */` |
|   11848 |  3689 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3690 | `	/* Release the loop block */` |
|   11848 |  3691 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3692 | `	/* Statement successfully compiled */` |
|   11848 |  3693 | `	return SXRET_OK;` |
|     ! 0 |  3694 | `Synchronize:` |
|       - |  3695 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3696 | `	 * compiling this erroneous block.` |
|       - |  3697 | `	 */` |
|     ! 0 |  3698 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3699 | `		pGen->pIn++;` |
|     ! 0 |  3700 | `	}` |
|     ! 0 |  3701 | `	return SXRET_OK;` |
|    5925 |  3702 |  |
|       - |  3703 | `/*` |
|       - |  3704 | ` * Compile the ugly do..while() statement.` |
|       - |  3705 | ` * According to the PHP language reference` |
|       - |  3706 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|       - |  3707 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|       - |  3708 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|       - |  3709 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|       - |  3710 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|       - |  3711 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|       - |  3712 | ` *  would end immediately).` |
|       - |  3713 | ` *  There is just one syntax for do-while loops:` |
|       - |  3714 | ` *  <?php` |
|       - |  3715 | ` *  $i = 0;` |
|       - |  3716 | ` *  do {` |
|       - |  3717 | ` *   echo $i;` |
|       - |  3718 | ` *  } while ($i > 0);` |
|       - |  3719 | ` * ?>` |
|       - |  3720 | ` */` |
|       2 |  3721 | `static sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|       1 |  3722 |  |
|       3 |  3723 | `	SyToken *pTmp,*pEnd = 0;` |
|       3 |  3724 | `	GenBlock *pDoBlock = 0;` |
|       - |  3725 | `	sxu32 nLine;` |
|       - |  3726 | `	sxi32 rc;` |
|       3 |  3727 | `	nLine = pGen->pIn->nLine;` |
|       - |  3728 | `	/* Jump the 'do' keyword */` |
|       3 |  3729 | `	pGen->pIn++;` |
|       - |  3730 | `	/* Create the loop block */` |
|       3 |  3731 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       3 |  3732 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3733 | `		return SXERR_ABORT;` |
|       - |  3734 | `	}` |
|       - |  3735 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       3 |  3736 | `	pDoBlock->bPostContinue = TRUE;` |
|       3 |  3737 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       3 |  3738 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3739 | `		return SXERR_ABORT;` |
|       - |  3740 | `	}` |
|       3 |  3741 | `	if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3742 | `		nLine = pGen->pIn->nLine;` |
|     ! 0 |  3743 | `	}` |
|       3 |  3744 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|     ! 0 |  3745 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|       - |  3746 | `			/* Missing 'while' statement */` |
|       3 |  3747 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing 'while' statement after 'do' block");` |
|       3 |  3748 | `			if( rc == SXERR_ABORT ){` |
|       - |  3749 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3750 | `				return SXERR_ABORT;` |
|       - |  3751 | `			}` |
|       3 |  3752 | `			goto Synchronize;` |
|       - |  3753 | `	}` |
|       - |  3754 | `	/* Jump the 'while' keyword */` |
|     ! 0 |  3755 | `	pGen->pIn++;` |
|     ! 0 |  3756 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3757 | `		/* Syntax error */` |
|     ! 0 |  3758 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|     ! 0 |  3759 | `		if( rc == SXERR_ABORT ){` |
|       - |  3760 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3761 | `			return SXERR_ABORT;` |
|       - |  3762 | `		}` |
|     ! 0 |  3763 | `		goto Synchronize;` |
|       - |  3764 | `	}` |
|       - |  3765 | `	/* Jump the left parenthesis '(' */` |
|     ! 0 |  3766 | `	pGen->pIn++;` |
|       - |  3767 | `	/* Delimit the condition */` |
|     ! 0 |  3768 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|     ! 0 |  3769 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3770 | `		/* Empty expression */` |
|     ! 0 |  3771 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'while' keyword");` |
|     ! 0 |  3772 | `		if( rc == SXERR_ABORT ){` |
|       - |  3773 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3774 | `			return SXERR_ABORT;` |
|       - |  3775 | `		}` |
|     ! 0 |  3776 | `		goto Synchronize;` |
|       - |  3777 | `	}` |
|       - |  3778 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|     ! 0 |  3779 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|       - |  3780 | `		JumpFixup *aPost;` |
|       - |  3781 | `		VmInstr *pInstr;` |
|       - |  3782 | `		sxu32 nJumpDest;` |
|       - |  3783 | `		sxu32 n;` |
|     ! 0 |  3784 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|     ! 0 |  3785 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|     ! 0 |  3786 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|     ! 0 |  3787 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|     ! 0 |  3788 | `			if( pInstr ){` |
|       - |  3789 | `				/* Fix */` |
|     ! 0 |  3790 | `				pInstr->iP2 = nJumpDest;` |
|     ! 0 |  3791 | `			}` |
|     ! 0 |  3792 | `		}` |
|     ! 0 |  3793 | `	}` |
|       - |  3794 | `	/* Swap token streams */` |
|     ! 0 |  3795 | `	pTmp = pGen->pEnd;` |
|     ! 0 |  3796 | `	pGen->pEnd = pEnd;` |
|       - |  3797 | `	/* Compile the expression */` |
|     ! 0 |  3798 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  3799 | `	if( rc == SXERR_ABORT ){` |
|       - |  3800 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3801 | `		return SXERR_ABORT;` |
|       - |  3802 | `	}` |
|       - |  3803 | `	/* Update token stream */` |
|     ! 0 |  3804 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  3805 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  3806 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  3807 | `			return SXERR_ABORT;` |
|       - |  3808 | `		}` |
|     ! 0 |  3809 | `		pGen->pIn++;` |
|     ! 0 |  3810 | `	}` |
|     ! 0 |  3811 | `	pGen->pIn  = &pEnd[1];` |
|     ! 0 |  3812 | `	pGen->pEnd = pTmp;` |
|       - |  3813 | `	/* Emit the true jump to the beginning of the loop */` |
|     ! 0 |  3814 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|       - |  3815 | `	/* Fix all jumps now the destination is resolved */` |
|     ! 0 |  3816 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3817 | `	/* Release the loop block */` |
|     ! 0 |  3818 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3819 | `	/* Statement successfully compiled */` |
|     ! 0 |  3820 | `	return SXRET_OK;` |
|       1 |  3821 | `Synchronize:` |
|       - |  3822 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  3823 | `	 * compiling this erroneous block.` |
|       - |  3824 | `	 */` |
|       3 |  3825 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  3826 | `		pGen->pIn++;` |
|     ! 0 |  3827 | `	}` |
|       3 |  3828 | `	return SXRET_OK;` |
|       2 |  3829 |  |
|       - |  3830 | `/*` |
|       - |  3831 | ` * Compile the complex and powerful 'for' statement.` |
|       - |  3832 | ` * According to the PHP language reference` |
|       - |  3833 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|       - |  3834 | ` *  The syntax of a for loop is:` |
|       - |  3835 | ` *  for (expr1; expr2; expr3)` |
|       - |  3836 | ` *   statement` |
|       - |  3837 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|       - |  3838 | ` *  the beginning of the loop.` |
|       - |  3839 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|       - |  3840 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|       - |  3841 | ` *  to FALSE, the execution of the loop ends.` |
|       - |  3842 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|       - |  3843 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|       - |  3844 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|       - |  3845 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|       - |  3846 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|       - |  3847 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|       - |  3848 | ` *  of using the for truth expression.` |
|       - |  3849 | ` */` |
|   11850 |  3850 | `static sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|       2 |  3851 |  |
|   11852 |  3852 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   11852 |  3853 | `	GenBlock *pForBlock = 0;` |
|       - |  3854 | `	sxu32 nFalseJump;` |
|       - |  3855 | `	sxu32 nLine;` |
|       - |  3856 | `	sxi32 rc;` |
|   11852 |  3857 | `	nLine = pGen->pIn->nLine;` |
|       - |  3858 | `	/* Jump the 'for' keyword */` |
|   11852 |  3859 | `	pGen->pIn++;` |
|   11852 |  3860 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  3861 | `		/* Syntax error */` |
|     ! 0 |  3862 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|     ! 0 |  3863 | `		if( rc == SXERR_ABORT ){` |
|       - |  3864 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3865 | `			return SXERR_ABORT;` |
|       - |  3866 | `		}` |
|     ! 0 |  3867 | `		return SXRET_OK;` |
|       - |  3868 | `	}` |
|       - |  3869 | `	/* Jump the left parenthesis '(' */` |
|   11852 |  3870 | `	pGen->pIn++;` |
|       - |  3871 | `	/* Delimit the init-expr;condition;post-expr */` |
|   11852 |  3872 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   11852 |  3873 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  3874 | `		/* Empty expression */` |
|     ! 0 |  3875 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|     ! 0 |  3876 | `		if( rc == SXERR_ABORT ){` |
|       - |  3877 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3878 | `			return SXERR_ABORT;` |
|       - |  3879 | `		}` |
|       - |  3880 | `		/* Synchronize */` |
|     ! 0 |  3881 | `		pGen->pIn = pEnd;` |
|     ! 0 |  3882 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  3883 | `			pGen->pIn++;` |
|     ! 0 |  3884 | `		}` |
|     ! 0 |  3885 | `		return SXRET_OK;` |
|       - |  3886 | `	}` |
|       - |  3887 | `	/* Swap token streams */` |
|   11852 |  3888 | `	pTmp = pGen->pEnd;` |
|   11852 |  3889 | `	pGen->pEnd = pEnd;` |
|       - |  3890 | `	/* Compile initialization expressions if available */` |
|   11852 |  3891 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  3892 | `	/* Pop operand lvalues */` |
|   11852 |  3893 | `	if( rc == SXERR_ABORT ){` |
|       - |  3894 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3895 | `		return SXERR_ABORT;` |
|   11852 |  3896 | `	}else if( rc != SXERR_EMPTY ){` |
|   11850 |  3897 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5924 |  3898 | `	}` |
|   11852 |  3899 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3900 | `		/* Syntax error */` |
|     ! 0 |  3901 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3902 | `			"for: Expected ';' after initialization expressions");` |
|     ! 0 |  3903 | `		if( rc == SXERR_ABORT ){` |
|       - |  3904 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3905 | `			return SXERR_ABORT;` |
|       - |  3906 | `		}` |
|     ! 0 |  3907 | `		return SXRET_OK;` |
|       - |  3908 | `	}` |
|       - |  3909 | `	/* Jump the trailing ';' */` |
|   11852 |  3910 | `	pGen->pIn++;` |
|       - |  3911 | `	/* Create the loop block */` |
|   11852 |  3912 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   11852 |  3913 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  3914 | `		return SXERR_ABORT;` |
|       - |  3915 | `	}` |
|       - |  3916 | `	/* Deffer continue jumps */` |
|   11852 |  3917 | `	pForBlock->bPostContinue = TRUE;` |
|       - |  3918 | `	/* Compile the condition */` |
|   11852 |  3919 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11852 |  3920 | `	if( rc == SXERR_ABORT ){` |
|       - |  3921 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3922 | `		return SXERR_ABORT;` |
|   11852 |  3923 | `	}else if( rc != SXERR_EMPTY ){` |
|       - |  3924 | `		/* Emit the false jump */` |
|   11850 |  3925 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|       - |  3926 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   11850 |  3927 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    5924 |  3928 | `	}` |
|   11852 |  3929 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  3930 | `		/* Syntax error */` |
|       5 |  3931 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  3932 | `			"for: Expected ';' after conditionals expressions");` |
|       5 |  3933 | `		if( rc == SXERR_ABORT ){` |
|       - |  3934 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  3935 | `			return SXERR_ABORT;` |
|       - |  3936 | `		}` |
|       5 |  3937 | `		return SXRET_OK;` |
|       - |  3938 | `	}` |
|       - |  3939 | `	/* Jump the trailing ';' */` |
|   11848 |  3940 | `	pGen->pIn++;` |
|       - |  3941 | `	/* Save the post condition stream */` |
|   11848 |  3942 | `	pPostStart = pGen->pIn;` |
|       - |  3943 | `	/* Compile the loop body */` |
|   11848 |  3944 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   11848 |  3945 | `	pGen->pEnd = pTmp;` |
|   11848 |  3946 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   11848 |  3947 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  3948 | `		return SXERR_ABORT;` |
|       - |  3949 | `	}` |
|       - |  3950 | `	/* Fix post-continue jumps */` |
|   11848 |  3951 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|       - |  3952 | `		JumpFixup *aPost;` |
|       - |  3953 | `		VmInstr *pInstr;` |
|       - |  3954 | `		sxu32 nJumpDest;` |
|       - |  3955 | `		sxu32 n;` |
|      14 |  3956 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|      14 |  3957 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|      26 |  3958 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|      14 |  3959 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aPost[n].nInstrIdx);` |
|      14 |  3960 | `			if( pInstr ){` |
|       - |  3961 | `				/* Fix jump */` |
|      14 |  3962 | `				pInstr->iP2 = nJumpDest;` |
|       6 |  3963 | `			}` |
|       8 |  3964 | `		}` |
|       6 |  3965 | `	}` |
|       - |  3966 | `	/* compile the post-expressions if available */` |
|   11848 |  3967 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|     ! 0 |  3968 | `		pPostStart++;` |
|     ! 0 |  3969 | `	}` |
|   11848 |  3970 | `	if( pPostStart < pEnd ){` |
|       - |  3971 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   11848 |  3972 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   11848 |  3973 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   11848 |  3974 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - |  3975 | `			/* Syntax error */` |
|     ! 0 |  3976 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"for: Expected ')' after post-expressions");` |
|     ! 0 |  3977 | `			if( rc == SXERR_ABORT ){` |
|       - |  3978 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  3979 | `				return SXERR_ABORT;` |
|       - |  3980 | `			}` |
|     ! 0 |  3981 | `			return SXRET_OK;` |
|       - |  3982 | `		}` |
|   11848 |  3983 | `		RE_SWAP_DELIMITER(pGen);` |
|   11848 |  3984 | `		if( rc == SXERR_ABORT ){` |
|       - |  3985 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  3986 | `			return SXERR_ABORT;` |
|   11848 |  3987 | `		}else if( rc != SXERR_EMPTY){` |
|       - |  3988 | `			/* Pop operand lvalue */` |
|   11848 |  3989 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    5923 |  3990 | `		}` |
|    5923 |  3991 | `	}` |
|       - |  3992 | `	/* Emit the unconditional jump to the start of the loop */` |
|   11848 |  3993 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|       - |  3994 | `	/* Fix all jumps now the destination is resolved */` |
|   11848 |  3995 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  3996 | `	/* Release the loop block */` |
|   11848 |  3997 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  3998 | `	/* Statement successfully compiled */` |
|   11848 |  3999 | `	return SXRET_OK;` |
|    5927 |  4000 |  |
|       - |  4001 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|       - |  4002 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|       - |  4003 | ` * are allowed.` |
|       - |  4004 | ` */` |
|    6290 |  4005 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  4006 |  |
|    6292 |  4007 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|    6292 |  4008 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  4009 | `		/* Unexpected expression */` |
|     ! 0 |  4010 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  4011 | `			"foreach: Expecting a variable name");` |
|     ! 0 |  4012 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  4013 | `			rc = SXERR_INVALID;` |
|     ! 0 |  4014 | `		}` |
|     ! 0 |  4015 | `	}` |
|    6292 |  4016 | `	return rc;` |
|       2 |  4017 |  |
|       - |  4018 | `/*` |
|       - |  4019 | ` * Compile the 'foreach' statement.` |
|       - |  4020 | ` * According to the PHP language reference` |
|       - |  4021 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|       - |  4022 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|       - |  4023 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|       - |  4024 | ` *  is a minor but useful extension of the first:` |
|       - |  4025 | ` *  foreach (array_expression as $value)` |
|       - |  4026 | ` *    statement` |
|       - |  4027 | ` *  foreach (array_expression as $key => $value)` |
|       - |  4028 | ` *   statement` |
|       - |  4029 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|       - |  4030 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|       - |  4031 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|       - |  4032 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|       - |  4033 | ` *  to the variable $key on each loop.` |
|       - |  4034 | ` *  Note:` |
|       - |  4035 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|       - |  4036 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|       - |  4037 | ` *  Note:` |
|       - |  4038 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|       - |  4039 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|       - |  4040 | ` *  or after the foreach without resetting it.` |
|       - |  4041 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|       - |  4042 | ` *  of copying the value.` |
|       - |  4043 | ` */` |
|    3202 |  4044 | `static sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|       2 |  4045 |  |
|    3204 |  4046 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|    3204 |  4047 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|    3204 |  4048 | `	GenBlock *pForeachBlock = 0;` |
|       - |  4049 | `	ph7_foreach_info *pInfo;` |
|       - |  4050 | `	sxu32 nFalseJump;` |
|       - |  4051 | `	VmInstr *pInstr;` |
|       - |  4052 | `	sxu32 nLine;` |
|       - |  4053 | `	sxi32 rc;` |
|    3204 |  4054 | `	nLine = pGen->pIn->nLine;` |
|       - |  4055 | `	/* Jump the 'foreach' keyword */` |
|    3204 |  4056 | `	pGen->pIn++;` |
|    3204 |  4057 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4058 | `		/* Syntax error */` |
|     ! 0 |  4059 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|     ! 0 |  4060 | `		if( rc == SXERR_ABORT ){` |
|       - |  4061 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4062 | `			return SXERR_ABORT;` |
|       - |  4063 | `		}` |
|     ! 0 |  4064 | `		goto Synchronize;` |
|       - |  4065 | `	}` |
|       - |  4066 | `	/* Jump the left parenthesis '(' */` |
|    3204 |  4067 | `	pGen->pIn++;` |
|       - |  4068 | `	/* Create the loop block */` |
|    3204 |  4069 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|    3204 |  4070 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4071 | `		return SXERR_ABORT;` |
|       - |  4072 | `	}` |
|       - |  4073 | `	/* Delimit the expression */` |
|    3204 |  4074 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    3204 |  4075 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  4076 | `		/* Empty expression */` |
|     ! 0 |  4077 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|     ! 0 |  4078 | `		if( rc == SXERR_ABORT ){` |
|       - |  4079 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  4080 | `			return SXERR_ABORT;` |
|       - |  4081 | `		}` |
|       - |  4082 | `		/* Synchronize */` |
|     ! 0 |  4083 | `		pGen->pIn = pEnd;` |
|     ! 0 |  4084 | `		if( pGen->pIn < pGen->pEnd ){` |
|     ! 0 |  4085 | `			pGen->pIn++;` |
|     ! 0 |  4086 | `		}` |
|     ! 0 |  4087 | `		return SXRET_OK;` |
|       - |  4088 | `	}` |
|       - |  4089 | `	/* Compile the array expression */` |
|    3204 |  4090 | `	pCur = pGen->pIn;` |
|   21520 |  4091 | `	while( pCur < pEnd ){` |
|   21520 |  4092 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|    3214 |  4093 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|    3214 |  4094 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|       - |  4095 | `				/* Break with the first 'as' found */` |
|    3204 |  4096 | `				break;` |
|       - |  4097 | `			}` |
|       5 |  4098 | `		}` |
|       - |  4099 | `		/* Advance the stream cursor */` |
|   18318 |  4100 | `		pCur++;` |
|       2 |  4101 | `	}` |
|    3204 |  4102 | `	if( pCur <= pGen->pIn ){` |
|     ! 0 |  4103 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       - |  4104 | `			"foreach: Missing array/object expression");` |
|     ! 0 |  4105 | `		if( rc == SXERR_ABORT ){` |
|       - |  4106 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4107 | `			return SXERR_ABORT;` |
|       - |  4108 | `		}` |
|     ! 0 |  4109 | `		goto Synchronize;` |
|       - |  4110 | `	}` |
|       - |  4111 | `	/* Swap token streams */` |
|    3204 |  4112 | `	pTmp = pGen->pEnd;` |
|    3204 |  4113 | `	pGen->pEnd = pCur;` |
|    3204 |  4114 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    3204 |  4115 | `	if( rc == SXERR_ABORT ){` |
|       - |  4116 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4117 | `		return SXERR_ABORT;` |
|       - |  4118 | `	}` |
|       - |  4119 | `	/* Update token stream */` |
|    3204 |  4120 | `	while(pGen->pIn < pCur ){` |
|     ! 0 |  4121 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4122 | `		if( rc == SXERR_ABORT ){` |
|       - |  4123 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4124 | `			return SXERR_ABORT;` |
|       - |  4125 | `		}` |
|     ! 0 |  4126 | `		pGen->pIn++;` |
|     ! 0 |  4127 | `	}` |
|    3204 |  4128 | `	pCur++; /* Jump the 'as' keyword */` |
|    3204 |  4129 | `	pGen->pIn = pCur;` |
|    3204 |  4130 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4131 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|     ! 0 |  4132 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4133 | `			return SXERR_ABORT;` |
|       - |  4134 | `		}` |
|     ! 0 |  4135 | `	}` |
|       - |  4136 | `	/* Create the foreach context */` |
|    3204 |  4137 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|    3204 |  4138 | `	if( pInfo == 0 ){` |
|     ! 0 |  4139 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  4140 | `		return SXERR_ABORT;` |
|       - |  4141 | `	}` |
|       - |  4142 | `	/* Zero the structure */` |
|    3204 |  4143 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|       - |  4144 | `	/* Initialize structure fields */` |
|    3204 |  4145 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|       - |  4146 | `	/* Check if we have a key field */` |
|    9658 |  4147 | `	while( pCur < pEnd && (pCur->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|    6456 |  4148 | `		pCur++;` |
|       2 |  4149 | `	}` |
|    3204 |  4150 | `	if( pCur < pEnd ){` |
|       - |  4151 | `		/* Compile the expression holding the key name */` |
|    3100 |  4152 | `		if( pGen->pIn >= pCur ){` |
|     ! 0 |  4153 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|     ! 0 |  4154 | `			if( rc == SXERR_ABORT ){` |
|       - |  4155 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4156 | `				return SXERR_ABORT;` |
|       - |  4157 | `			}` |
|     ! 0 |  4158 | `		}else{` |
|    3100 |  4159 | `			pGen->pEnd = pCur;` |
|    3100 |  4160 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3100 |  4161 | `			if( rc == SXERR_ABORT ){` |
|       - |  4162 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4163 | `				return SXERR_ABORT;` |
|       - |  4164 | `			}` |
|    3100 |  4165 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3100 |  4166 | `			if( pInstr->p3 ){` |
|       - |  4167 | `				/* Record key name */` |
|    3100 |  4168 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1549 |  4169 | `			}` |
|    3100 |  4170 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|       - |  4171 | `		}` |
|    3100 |  4172 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    1549 |  4173 | `	}` |
|    3204 |  4174 | `	pGen->pEnd = pEnd;` |
|    3204 |  4175 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  4176 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|     ! 0 |  4177 | `		if( rc == SXERR_ABORT ){` |
|       - |  4178 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4179 | `			return SXERR_ABORT;` |
|       - |  4180 | `		}` |
|     ! 0 |  4181 | `		goto Synchronize;` |
|       - |  4182 | `	}` |
|    3204 |  4183 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|      11 |  4184 | `		pGen->pIn++;` |
|       - |  4185 | `		/* Pass by reference  */` |
|      11 |  4186 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       5 |  4187 | `	}` |
|       - |  4188 | `	/* Check if the value target is list() */` |
|    3204 |  4189 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       8 |  4190 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|       - |  4191 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|       - |  4192 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|       - |  4193 | `		 */` |
|       - |  4194 | `		static int iForeachListCnt = 0;` |
|       - |  4195 | `		char zTmp[128];` |
|       - |  4196 | `		sxu32 nLen;` |
|       - |  4197 | `		char *zDup;` |
|      10 |  4198 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|      10 |  4199 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|      10 |  4200 | `		if( zDup == 0 ){` |
|     ! 0 |  4201 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4202 | `			return SXERR_ABORT;` |
|       - |  4203 | `		}` |
|      10 |  4204 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4205 | `		/* Save list() token boundaries */` |
|      10 |  4206 | `		pListStart = pGen->pIn;` |
|       - |  4207 | `		/* Advance past list(...) — validate parentheses */` |
|      10 |  4208 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|      10 |  4209 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  4210 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn < pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  4211 | `				"foreach: Expected '(' after 'list'");` |
|       3 |  4212 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4213 | `				return SXERR_ABORT;` |
|       - |  4214 | `			}` |
|       3 |  4215 | `			goto Synchronize;` |
|       - |  4216 | `		}` |
|       7 |  4217 | `		pGen->pIn++; /* Jump '(' */` |
|       7 |  4218 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|       7 |  4219 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4220 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4221 | `				"foreach: Missing closing ')' after list");` |
|     ! 0 |  4222 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4223 | `				return SXERR_ABORT;` |
|       - |  4224 | `			}` |
|     ! 0 |  4225 | `			goto Synchronize;` |
|       - |  4226 | `		}` |
|       7 |  4227 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|       7 |  4228 | `		pListEnd = pGen->pIn;` |
|       7 |  4229 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|    3199 |  4230 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|       - |  4231 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|       - |  4232 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|       - |  4233 | `		 */` |
|       - |  4234 | `		static int iForeachShortListCnt = 0;` |
|       - |  4235 | `		char zTmp[128];` |
|       - |  4236 | `		sxu32 nLen;` |
|       - |  4237 | `		char *zDup;` |
|       3 |  4238 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       3 |  4239 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       3 |  4240 | `		if( zDup == 0 ){` |
|     ! 0 |  4241 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4242 | `			return SXERR_ABORT;` |
|       - |  4243 | `		}` |
|       3 |  4244 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|       - |  4245 | `		/* Save [...] token boundaries */` |
|       3 |  4246 | `		pListStart = pGen->pIn;` |
|       - |  4247 | `		/* Advance past [...] */` |
|       3 |  4248 | `		pGen->pIn++; /* Jump '[' */` |
|       3 |  4249 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       3 |  4250 | `		if( pListEnd >= pEnd ){` |
|     ! 0 |  4251 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  4252 | `				"foreach: Missing closing ']' after short list");` |
|     ! 0 |  4253 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4254 | `				return SXERR_ABORT;` |
|       - |  4255 | `			}` |
|     ! 0 |  4256 | `			goto Synchronize;` |
|       - |  4257 | `		}` |
|       3 |  4258 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       3 |  4259 | `		pListEnd = pGen->pIn;` |
|       3 |  4260 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       2 |  4261 | `	}else{` |
|       - |  4262 | `		/* Compile the expression holding the value name */` |
|    3194 |  4263 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|    3194 |  4264 | `		if( rc == SXERR_ABORT ){` |
|       - |  4265 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4266 | `			return SXERR_ABORT;` |
|       - |  4267 | `		}` |
|    3194 |  4268 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|    3194 |  4269 | `		if( pInstr->p3 ){` |
|       - |  4270 | `			/* Record value name */` |
|    3194 |  4271 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    1596 |  4272 | `		}` |
|       - |  4273 | `	}` |
|       - |  4274 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|    3202 |  4275 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|       - |  4276 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3202 |  4277 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|       - |  4278 | `	/* Record the first instruction to execute */` |
|    3202 |  4279 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4280 | `	/* Emit the FOREACH_STEP instruction */` |
|    3202 |  4281 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|       - |  4282 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    3202 |  4283 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|       - |  4284 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|    3202 |  4285 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|       - |  4286 | `		SyToken *pSavedIn,*pSavedEnd;` |
|       - |  4287 | `		/* Load the temporary variable holding the current value onto the stack.` |
|       - |  4288 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|       - |  4289 | `		 */` |
|       9 |  4290 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|       - |  4291 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|       - |  4292 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|       - |  4293 | `		 * picks up the delimiter and the variable names inside.` |
|       - |  4294 | `		 */` |
|       9 |  4295 | `		pSavedIn = pGen->pIn;` |
|       9 |  4296 | `		pSavedEnd = pGen->pEnd;` |
|       9 |  4297 | `		pGen->pIn = pListStart;` |
|       9 |  4298 | `		pGen->pEnd = pListEnd;` |
|       9 |  4299 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       3 |  4300 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       2 |  4301 | `		}else{` |
|       7 |  4302 | `			rc = PH7_CompileList(&(*pGen),0);` |
|       - |  4303 | `		}` |
|       9 |  4304 | `		pGen->pIn = pSavedIn;` |
|       9 |  4305 | `		pGen->pEnd = pSavedEnd;` |
|       9 |  4306 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4307 | `			return SXERR_ABORT;` |
|       - |  4308 | `		}` |
|       - |  4309 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       9 |  4310 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       4 |  4311 | `	}` |
|       - |  4312 | `	/* Compile the loop body */` |
|    3202 |  4313 | `	pGen->pIn = &pEnd[1];` |
|    3202 |  4314 | `	pGen->pEnd = pTmp;` |
|    3202 |  4315 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|    3202 |  4316 | `	if( rc == SXERR_ABORT ){` |
|       - |  4317 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  4318 | `		return SXERR_ABORT;` |
|       - |  4319 | `	}` |
|       - |  4320 | `	/* Emit the unconditional jump to the start of the loop */` |
|    3202 |  4321 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|       - |  4322 | `	/* Fix all jumps now the destination is resolved */` |
|    3202 |  4323 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  4324 | `	/* Release the loop block */` |
|    3202 |  4325 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4326 | `	/* Statement successfully compiled */` |
|    3202 |  4327 | `	return SXRET_OK;` |
|       1 |  4328 | `Synchronize:` |
|       - |  4329 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|       - |  4330 | `	 * compiling this erroneous block.` |
|       - |  4331 | `	 */` |
|       3 |  4332 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4333 | `		pGen->pIn++;` |
|     ! 0 |  4334 | `	}` |
|       3 |  4335 | `	return SXRET_OK;` |
|    1603 |  4336 |  |
|       - |  4337 | `/*` |
|       - |  4338 | ` * Compile the infamous if/elseif/else if/else statements.` |
|       - |  4339 | ` * According to the PHP language reference` |
|       - |  4340 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|       - |  4341 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|       - |  4342 | ` *  that is similar to that of C:` |
|       - |  4343 | ` *  if (expr)` |
|       - |  4344 | ` *   statement` |
|       - |  4345 | ` *  else construct:` |
|       - |  4346 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|       - |  4347 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|       - |  4348 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|       - |  4349 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|       - |  4350 | ` *   $b, and a is NOT greater than b otherwise.` |
|       - |  4351 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|       - |  4352 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|       - |  4353 | ` *  elseif` |
|       - |  4354 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|       - |  4355 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|       - |  4356 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|       - |  4357 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|       - |  4358 | ` *   than b, a equal to b or a is smaller than b:` |
|       - |  4359 | ` *   <?php` |
|       - |  4360 | ` *    if ($a > $b) {` |
|       - |  4361 | ` *     echo "a is bigger than b";` |
|       - |  4362 | ` *    } elseif ($a == $b) {` |
|       - |  4363 | ` *     echo "a is equal to b";` |
|       - |  4364 | ` *    } else {` |
|       - |  4365 | ` *     echo "a is smaller than b";` |
|       - |  4366 | ` *    }` |
|       - |  4367 | ` *    ?>` |
|       - |  4368 | ` */` |
|  123426 |  4369 | `static sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|       2 |  4370 |  |
|  123428 |  4371 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  123428 |  4372 | `	GenBlock *pCondBlock = 0;` |
|       - |  4373 | `	sxu32 nJumpIdx;` |
|       - |  4374 | `	sxu32 nKeyID;` |
|       - |  4375 | `	sxi32 rc;` |
|       - |  4376 | `	/* Jump the 'if' keyword */` |
|  123428 |  4377 | `	pGen->pIn++;` |
|  123428 |  4378 | `	pToken = pGen->pIn;` |
|       - |  4379 | `	/* Create the conditional block */` |
|  123428 |  4380 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  123428 |  4381 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  4382 | `		return SXERR_ABORT;` |
|       - |  4383 | `	}` |
|       - |  4384 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|   67598 |  4385 | `	for(;;){` |
|  135198 |  4386 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  4387 | `			/* Syntax error */` |
|     ! 0 |  4388 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4389 | `				pToken--;` |
|     ! 0 |  4390 | `			}` |
|     ! 0 |  4391 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|     ! 0 |  4392 | `			if( rc == SXERR_ABORT ){` |
|       - |  4393 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4394 | `				return SXERR_ABORT;` |
|       - |  4395 | `			}` |
|     ! 0 |  4396 | `			goto Synchronize;` |
|       - |  4397 | `		}` |
|       - |  4398 | `		/* Jump the left parenthesis '(' */` |
|  135198 |  4399 | `		pToken++;` |
|       - |  4400 | `		/* Delimit the condition */` |
|  135198 |  4401 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  135198 |  4402 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|       - |  4403 | `			/* Syntax error */` |
|     ! 0 |  4404 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  4405 | `				pToken--;` |
|     ! 0 |  4406 | `			}` |
|     ! 0 |  4407 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|     ! 0 |  4408 | `			if( rc == SXERR_ABORT ){` |
|       - |  4409 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  4410 | `				return SXERR_ABORT;` |
|       - |  4411 | `			}` |
|     ! 0 |  4412 | `			goto Synchronize;` |
|       - |  4413 | `		}` |
|       - |  4414 | `		/* Swap token streams */` |
|  135198 |  4415 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|       - |  4416 | `		/* Compile the condition */` |
|  135198 |  4417 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4418 | `		/* Update token stream */` |
|  135198 |  4419 | `		while(pGen->pIn < pEnd ){` |
|     ! 0 |  4420 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4421 | `			pGen->pIn++;` |
|     ! 0 |  4422 | `		}` |
|  135198 |  4423 | `		pGen->pIn  = &pEnd[1];` |
|  135198 |  4424 | `		pGen->pEnd = pTmp;` |
|  135198 |  4425 | `		if( rc == SXERR_ABORT ){` |
|       - |  4426 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  4427 | `			return SXERR_ABORT;` |
|       - |  4428 | `		}` |
|       - |  4429 | `		/* Emit the false jump */` |
|  135198 |  4430 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|       - |  4431 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  135198 |  4432 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|       - |  4433 | `		/* Compile the body */` |
|  135198 |  4434 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  135198 |  4435 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4436 | `			return SXERR_ABORT;` |
|       - |  4437 | `		}` |
|  135198 |  4438 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   37721 |  4439 | `			break;` |
|       - |  4440 | `		}` |
|       - |  4441 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|   59760 |  4442 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   59760 |  4443 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|   38454 |  4444 | `			break;` |
|       - |  4445 | `		}` |
|       - |  4446 | `		/* Emit the unconditional jump */` |
|   21308 |  4447 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|       - |  4448 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   21308 |  4449 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   21308 |  4450 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   15410 |  4451 | `			pToken = &pGen->pIn[1];` |
|   15410 |  4452 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    5902 |  4453 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|    4770 |  4454 | `					break;` |
|       - |  4455 | `			}` |
|    5874 |  4456 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    2936 |  4457 | `		}` |
|   11772 |  4458 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|       - |  4459 | `		/* Synchronize cursors */` |
|   11772 |  4460 | `		pToken = pGen->pIn;` |
|       - |  4461 | `		/* Fix the false jump */` |
|   11772 |  4462 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|       2 |  4463 | `	} /* For(;;) */` |
|       - |  4464 | `	/* Fix the false jump */` |
|  123428 |  4465 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  123428 |  4466 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|   47988 |  4467 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|       - |  4468 | `			/* Compile the else block */` |
|    9538 |  4469 | `			pGen->pIn++;` |
|    9538 |  4470 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|    9538 |  4471 | `			if( rc == SXERR_ABORT ){` |
|       - |  4472 |  |
|     ! 0 |  4473 | `				return SXERR_ABORT;` |
|       - |  4474 | `			}` |
|    4768 |  4475 | `	}` |
|  123428 |  4476 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|       - |  4477 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  123428 |  4478 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|       - |  4479 | `	/* Release the conditional block */` |
|  123428 |  4480 | `	GenStateLeaveBlock(pGen,0);` |
|       - |  4481 | `	/* Statement successfully compiled */` |
|  123428 |  4482 | `	return SXRET_OK;` |
|     ! 0 |  4483 | `Synchronize:` |
|       - |  4484 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|       - |  4485 | `	 */` |
|     ! 0 |  4486 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  4487 | `		pGen->pIn++;` |
|     ! 0 |  4488 | `	}` |
|     ! 0 |  4489 | `	return SXRET_OK;` |
|   61715 |  4490 |  |
|       - |  4491 | `/*` |
|       - |  4492 | ` * Compile the global construct.` |
|       - |  4493 | ` * According to the PHP language reference` |
|       - |  4494 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|       - |  4495 | ` *  to be used in that function.` |
|       - |  4496 | ` *  Example #1 Using global` |
|       - |  4497 | ` *  <?php` |
|       - |  4498 | ` *   $a = 1;` |
|       - |  4499 | ` *   $b = 2;` |
|       - |  4500 | ` *   function Sum()` |
|       - |  4501 | ` *   {` |
|       - |  4502 | ` *    global $a, $b;` |
|       - |  4503 | ` *    $b = $a + $b;` |
|       - |  4504 | ` *   }` |
|       - |  4505 | ` *   Sum();` |
|       - |  4506 | ` *   echo $b;` |
|       - |  4507 | ` *  ?>` |
|       - |  4508 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|       - |  4509 | ` *  all references to either variable will refer to the global version. There is no limit` |
|       - |  4510 | ` *  to the number of global variables that can be manipulated by a function.` |
|       - |  4511 | ` */` |
|      32 |  4512 | `static sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|       2 |  4513 |  |
|      34 |  4514 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4515 | `	sxi32 nExpr;` |
|       - |  4516 | `	sxi32 rc;` |
|       - |  4517 | `	/* Jump the 'global' keyword */` |
|      34 |  4518 | `	pGen->pIn++;` |
|      34 |  4519 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       - |  4520 | `		/* Nothing to process */` |
|     ! 0 |  4521 | `		return SXRET_OK;` |
|       - |  4522 | `	}` |
|      34 |  4523 | `	pTmp = pGen->pEnd;` |
|      34 |  4524 | `	nExpr = 0;` |
|      68 |  4525 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|      36 |  4526 | `		if( pGen->pIn < pNext ){` |
|      36 |  4527 | `			pGen->pEnd = pNext;` |
|      36 |  4528 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4529 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|     ! 0 |  4530 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  4531 | `					return SXERR_ABORT;` |
|       - |  4532 | `				}` |
|     ! 0 |  4533 | `			}else{` |
|      36 |  4534 | `				pGen->pIn++;` |
|      36 |  4535 | `				if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4536 | `					/* Emit a warning */` |
|     ! 0 |  4537 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|     ! 0 |  4538 | `				}else{` |
|      36 |  4539 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      36 |  4540 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  4541 | `						return SXERR_ABORT;` |
|      36 |  4542 | `					}else if(rc != SXERR_EMPTY ){` |
|      36 |  4543 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|      36 |  4544 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|       - |  4545 | `							/* Variable name, not a constant */` |
|      36 |  4546 | `							pLast->iP1 = 0;` |
|      17 |  4547 | `						}` |
|      36 |  4548 | `						nExpr++;` |
|      17 |  4549 | `					}` |
|       - |  4550 | `				}` |
|       - |  4551 | `			}` |
|      17 |  4552 | `		}` |
|       - |  4553 | `		/* Next expression in the stream */` |
|      36 |  4554 | `		pGen->pIn = pNext;` |
|       - |  4555 | `		/* Jump trailing commas */` |
|      38 |  4556 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  4557 | `			pGen->pIn++;` |
|       1 |  4558 | `		}` |
|       2 |  4559 | `	}` |
|       - |  4560 | `	/* Restore token stream */` |
|      34 |  4561 | `	pGen->pEnd = pTmp;` |
|      34 |  4562 | `	if( nExpr > 0 ){` |
|       - |  4563 | `		/* Emit the uplink instruction */` |
|      34 |  4564 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|      16 |  4565 | `	}` |
|      34 |  4566 | `	return SXRET_OK;` |
|      18 |  4567 |  |
|       - |  4568 | `/*` |
|       - |  4569 | ` * Compile the return statement.` |
|       - |  4570 | ` * According to the PHP language reference` |
|       - |  4571 | ` *  If called from within a function, the return() statement immediately ends execution` |
|       - |  4572 | ` *  of the current function, and returns its argument as the value of the function call.` |
|       - |  4573 | ` *  return() will also end the execution of an eval() statement or script file.` |
|       - |  4574 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|       - |  4575 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|       - |  4576 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|       - |  4577 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|       - |  4578 | ` *  from within the main script file, then script execution end.` |
|       - |  4579 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|       - |  4580 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|       - |  4581 | ` *  should do so as PHP has less work to do in this case.` |
|       - |  4582 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|       - |  4583 | ` */` |
|  194560 |  4584 | `static sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|       2 |  4585 |  |
|  194562 |  4586 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|       - |  4587 | `	sxi32 rc;` |
|       - |  4588 | `	/* Jump the 'return' keyword */` |
|  194562 |  4589 | `	pGen->pIn++;` |
|  194562 |  4590 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4591 | `		/* Compile the expression */` |
|  194538 |  4592 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  194538 |  4593 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4594 | `			return SXERR_ABORT;` |
|  194538 |  4595 | `		}else if(rc != SXERR_EMPTY ){` |
|  194538 |  4596 | `			nRet = 1;` |
|   97268 |  4597 | `		}` |
|   97268 |  4598 | `	}` |
|       - |  4599 | `	/* Emit the done instruction */` |
|  194562 |  4600 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,0,0,0);` |
|  194562 |  4601 | `	return SXRET_OK;` |
|   97282 |  4602 |  |
|       - |  4603 | `/*` |
|       - |  4604 | ` * Compile a yield expression.` |
|       - |  4605 | ` * Called from the expression code generator when a yield node is encountered.` |
|       - |  4606 | ` * Handles: yield, yield $value, yield $key => $value` |
|       - |  4607 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|       - |  4608 | ` */` |
|      34 |  4609 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  4610 |  |
|       - |  4611 | `	SyToken *pTmp, *pSplit;` |
|      36 |  4612 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|      36 |  4613 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|       - |  4614 | `	sxi32 rc;` |
|      17 |  4615 | `	(void)iCompileFlag;` |
|       - |  4616 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|      36 |  4617 | `	pGen->pIn++;` |
|       - |  4618 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|       - |  4619 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|      36 |  4620 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  4621 | `		/* Bare yield — no value */` |
|     ! 0 |  4622 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|     ! 0 |  4623 | `		return SXRET_OK;` |
|       - |  4624 | `	}` |
|       - |  4625 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|      36 |  4626 | `	pSplit = 0;` |
|       - |  4627 | `	{` |
|      36 |  4628 | `		SyToken *pCur = pGen->pIn;` |
|      36 |  4629 | `		sxi32 nNest = 0;` |
|      84 |  4630 | `		while( pCur < pGen->pEnd ){` |
|      56 |  4631 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  4632 | `				nNest++;` |
|      56 |  4633 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  4634 | `				nNest--;` |
|      56 |  4635 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|       7 |  4636 | `				pSplit = pCur;` |
|       7 |  4637 | `				break;` |
|       - |  4638 | `			}` |
|      50 |  4639 | `			pCur++;` |
|       2 |  4640 | `		}` |
|       - |  4641 | `	}` |
|      36 |  4642 | `	pTmp = pGen->pEnd;` |
|      36 |  4643 | `	if( pSplit ){` |
|       - |  4644 | `		/* yield $key => $value */` |
|       7 |  4645 | `		pGen->pEnd = pSplit;` |
|       7 |  4646 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4647 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4648 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|       7 |  4649 | `		pGen->pEnd = pTmp;` |
|       7 |  4650 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       7 |  4651 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       7 |  4652 | `		iP1 = 1;` |
|       7 |  4653 | `		iP2 = 1;` |
|       4 |  4654 | `	}else{` |
|       - |  4655 | `		/* yield $value */` |
|      30 |  4656 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      30 |  4657 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      30 |  4658 | `		if( rc != SXERR_EMPTY ){` |
|      30 |  4659 | `			iP1 = 1;` |
|      14 |  4660 | `		}` |
|       - |  4661 | `	}` |
|      36 |  4662 | `	pGen->pEnd = pTmp;` |
|      36 |  4663 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|      36 |  4664 | `	return SXRET_OK;` |
|      19 |  4665 |  |
|       - |  4666 | `/*` |
|       - |  4667 | ` * Compile the die/exit language construct.` |
|       - |  4668 | ` * The role of these constructs is to terminate execution of the script.` |
|       - |  4669 | ` * Shutdown functions will always be executed even if exit() is called.` |
|       - |  4670 | ` */` |
|      88 |  4671 | `static sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|       2 |  4672 |  |
|      90 |  4673 | `	sxi32 nExpr = 0;` |
|       - |  4674 | `	sxi32 rc;` |
|       - |  4675 | `	/* Jump the die/exit keyword */` |
|      90 |  4676 | `	pGen->pIn++;` |
|      90 |  4677 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       - |  4678 | `		/* Compile the expression */` |
|      90 |  4679 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      90 |  4680 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4681 | `			return SXERR_ABORT;` |
|      90 |  4682 | `		}else if(rc != SXERR_EMPTY ){` |
|      90 |  4683 | `			nExpr = 1;` |
|      44 |  4684 | `		}` |
|      44 |  4685 | `	}` |
|       - |  4686 | `	/* Emit the HALT instruction */` |
|      90 |  4687 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|      90 |  4688 | `	return SXRET_OK;` |
|      46 |  4689 |  |
|       - |  4690 | `/*` |
|       - |  4691 | ` * Compile the 'echo' language construct.` |
|       - |  4692 | ` */` |
|   11900 |  4693 | `static sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|       2 |  4694 |  |
|   11902 |  4695 | `	SyToken *pTmp,*pNext = 0;` |
|       - |  4696 | `	sxi32 rc;` |
|       - |  4697 | `	/* Jump the 'echo' keyword */` |
|   11902 |  4698 | `	pGen->pIn++;` |
|       - |  4699 | `	/* Compile arguments one after one */` |
|   11902 |  4700 | `	pTmp = pGen->pEnd;` |
|   24978 |  4701 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|   13078 |  4702 | `		if( pGen->pIn < pNext ){` |
|   13078 |  4703 | `			pGen->pEnd = pNext;` |
|   13078 |  4704 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|   13078 |  4705 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4706 | `				return SXERR_ABORT;` |
|   13078 |  4707 | `			}else if( rc != SXERR_EMPTY ){` |
|       - |  4708 | `				/* Emit the consume instruction */` |
|   13054 |  4709 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    6526 |  4710 | `			}` |
|    6538 |  4711 | `		}` |
|       - |  4712 | `		/* Jump trailing commas */` |
|   14254 |  4713 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    1178 |  4714 | `			pNext++;` |
|       2 |  4715 | `		}` |
|   13078 |  4716 | `		pGen->pIn = pNext;` |
|       2 |  4717 | `	}` |
|       - |  4718 | `	/* Restore token stream */` |
|   11902 |  4719 | `	pGen->pEnd = pTmp;` |
|   11902 |  4720 | `	return SXRET_OK;` |
|    5952 |  4721 |  |
|       - |  4722 | `/*` |
|       - |  4723 | ` * Compile the static statement.` |
|       - |  4724 | ` * According to the PHP language reference` |
|       - |  4725 | ` *  Another important feature of variable scoping is the static variable.` |
|       - |  4726 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|       - |  4727 | ` *  when program execution leaves this scope.` |
|       - |  4728 | ` *  Static variables also provide one way to deal with recursive functions.` |
|       - |  4729 | ` * Symisc eXtension.` |
|       - |  4730 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|       - |  4731 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  4732 | ` *  Example` |
|       - |  4733 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|       - |  4734 | ` *    Refer to the official documentation for more information on this feature.` |
|       - |  4735 | ` */` |
|       2 |  4736 | `static sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|       1 |  4737 |  |
|       - |  4738 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|       - |  4739 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|       - |  4740 | `	GenBlock *pBlock;` |
|       - |  4741 | `	SyString *pName;` |
|       - |  4742 | `	char *zDup;` |
|       - |  4743 | `	sxu32 nLine;` |
|       - |  4744 | `	sxi32 rc;` |
|       - |  4745 | `	/* Jump the static keyword */` |
|       3 |  4746 | `	nLine = pGen->pIn->nLine;` |
|       3 |  4747 | `	pGen->pIn++;` |
|       - |  4748 | `	/* Extract the enclosing function if any */` |
|       3 |  4749 | `	pBlock = pGen->pCurrent;` |
|       5 |  4750 | `	while( pBlock ){` |
|       5 |  4751 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|       3 |  4752 | `			break;` |
|       - |  4753 | `		}` |
|       - |  4754 | `		/* Point to the upper block */` |
|       3 |  4755 | `		pBlock = pBlock->pParent;` |
|       1 |  4756 | `	}` |
|       3 |  4757 | `	if( pBlock == 0 ){` |
|       - |  4758 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|     ! 0 |  4759 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  4760 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|     ! 0 |  4761 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4762 | `				return SXERR_ABORT;` |
|       - |  4763 | `			}` |
|     ! 0 |  4764 | `			goto Synchronize;` |
|       - |  4765 | `		}` |
|       - |  4766 | `		/* Compile the expression holding the variable */` |
|     ! 0 |  4767 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     ! 0 |  4768 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4769 | `			return SXERR_ABORT;` |
|     ! 0 |  4770 | `		}else if( rc != SXERR_EMPTY ){` |
|       - |  4771 | `			/* Emit the POP instruction */` |
|     ! 0 |  4772 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     ! 0 |  4773 | `		}` |
|     ! 0 |  4774 | `		return SXRET_OK;` |
|       - |  4775 | `	}` |
|       3 |  4776 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       - |  4777 | `	/* Make sure we are dealing with a valid statement */` |
|       3 |  4778 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|     ! 0 |  4779 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       3 |  4780 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Expected variable after 'static' keyword");` |
|       3 |  4781 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  4782 | `				return SXERR_ABORT;` |
|       - |  4783 | `			}` |
|       3 |  4784 | `			goto Synchronize;` |
|       - |  4785 | `	}` |
|     ! 0 |  4786 | `	pGen->pIn++;` |
|       - |  4787 | `	/* Extract variable name */` |
|     ! 0 |  4788 | `	pName = &pGen->pIn->sData;` |
|     ! 0 |  4789 | `	pGen->pIn++; /* Jump the var name */` |
|     ! 0 |  4790 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|     ! 0 |  4791 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  4792 | `		goto Synchronize;` |
|       - |  4793 | `	}` |
|       - |  4794 | `	/* Initialize the structure describing the static variable */` |
|     ! 0 |  4795 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     ! 0 |  4796 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|       - |  4797 | `	/* Duplicate variable name */` |
|     ! 0 |  4798 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     ! 0 |  4799 | `	if( zDup == 0 ){` |
|     ! 0 |  4800 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  4801 | `		return SXERR_ABORT;` |
|       - |  4802 | `	}` |
|     ! 0 |  4803 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|       - |  4804 | `	/* Check if we have an expression to compile */` |
|     ! 0 |  4805 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|       - |  4806 | `		SySet *pInstrContainer;` |
|       - |  4807 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|       - |  4808 | `		 * Static variable can take any complex expression including function` |
|       - |  4809 | `		 * call as their initialization value.` |
|       - |  4810 | `		 * Example:` |
|       - |  4811 | `		 *		static $var = foo(1,4+5,bar());` |
|       - |  4812 | `		 */` |
|     ! 0 |  4813 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|       - |  4814 | `		/* Swap bytecode container */` |
|     ! 0 |  4815 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     ! 0 |  4816 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|       - |  4817 | `		/* Compile the expression */` |
|     ! 0 |  4818 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  4819 | `		/* Emit the done instruction */` |
|     ! 0 |  4820 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       - |  4821 | `		/* Restore default bytecode container */` |
|     ! 0 |  4822 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  4823 | `	}` |
|       - |  4824 | `	/* Finally save the compiled static variable in the appropriate container */` |
|     ! 0 |  4825 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|     ! 0 |  4826 | `	return SXRET_OK;` |
|       1 |  4827 | `Synchronize:` |
|       - |  4828 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|       - |  4829 | `	 * statement.` |
|       - |  4830 | `	 */` |
|       5 |  4831 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|       3 |  4832 | `		pGen->pIn++;` |
|       1 |  4833 | `	}` |
|       3 |  4834 | `	return SXRET_OK;` |
|       2 |  4835 |  |
|       - |  4836 | `/*` |
|       - |  4837 | ` * Compile the var statement.` |
|       - |  4838 | ` * Symisc Extension:` |
|       - |  4839 | ` *      var statement can be used outside of a class definition.` |
|       - |  4840 | ` */` |
|       4 |  4841 | `static sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|       1 |  4842 |  |
|       - |  4843 | `	sxu32 nLine;` |
|       - |  4844 | `	sxi32 rc;` |
|       5 |  4845 | `	nLine = pGen->pIn->nLine;` |
|       - |  4846 | `	/* Jump the 'var' keyword */` |
|       5 |  4847 | `	pGen->pIn++;` |
|       5 |  4848 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  4849 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"var: Expecting variable name");` |
|       - |  4850 | `		/* Synchronize with the first semi-colon */` |
|     ! 0 |  4851 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0 ){` |
|     ! 0 |  4852 | `			pGen->pIn++;` |
|     ! 0 |  4853 | `		}` |
|     ! 0 |  4854 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4855 | `			return SXERR_ABORT;` |
|       - |  4856 | `		}` |
|     ! 0 |  4857 | `	}else{` |
|       - |  4858 | `		/* Compile the expression */` |
|       5 |  4859 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       5 |  4860 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  4861 | `			return SXERR_ABORT;` |
|       5 |  4862 | `		}else if( rc != SXERR_EMPTY ){` |
|       5 |  4863 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       2 |  4864 | `		}` |
|       - |  4865 | `	}` |
|       5 |  4866 | `	return SXRET_OK;` |
|       3 |  4867 |  |
|       - |  4868 | `/*` |
|       - |  4869 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|       - |  4870 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|       - |  4871 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|       - |  4872 | ` */` |
|       - |  4873 | `/*` |
|       - |  4874 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|       - |  4875 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|       - |  4876 | ` * hash and any shared references), this creates a new literal entry with the` |
|       - |  4877 | ` * qualified name and updates the instruction's operand index.` |
|       - |  4878 | ` *` |
|       - |  4879 | ` * Resolution order:` |
|       - |  4880 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|       - |  4881 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|       - |  4882 | ` *   3. Otherwise return the original literal index unchanged.` |
|       - |  4883 | ` *` |
|       - |  4884 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|       - |  4885 | ` * came from an import (step 1) and 0 otherwise.` |
|       - |  4886 | ` * Returns the (possibly new) literal index.` |
|       - |  4887 | ` */` |
|  362842 |  4888 | `static sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|       2 |  4889 |  |
|       - |  4890 | `	ph7_value *pLit;` |
|       - |  4891 | `	const char *zLit;` |
|       - |  4892 | `	SyString sQualified;` |
|       - |  4893 | `	sxu32 nLit;` |
|       - |  4894 | `	sxu32 k;` |
|       - |  4895 | `	sxu32 nNewIdx;` |
|       - |  4896 | `	int hasNsSep;` |
|       - |  4897 | `	SyHashEntry *pImport;` |
|       - |  4898 | `	ph7_value *pNew;` |
|  362844 |  4899 | `	if( pFromImport ){` |
|  347180 |  4900 | `		*pFromImport = 0;` |
|  173589 |  4901 | `	}` |
|  362844 |  4902 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  362844 |  4903 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|     ! 0 |  4904 | `		return nOrigIdx;` |
|       - |  4905 | `	}` |
|  362844 |  4906 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  362844 |  4907 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|       - |  4908 | `	/* Skip if already qualified (contains backslash) */` |
|  362844 |  4909 | `	hasNsSep = 0;` |
| 3924378 |  4910 | `	for( k = 0; k < nLit; k++ ){` |
| 3561544 |  4911 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 1780769 |  4912 | `	}` |
|  362844 |  4913 | `	if( hasNsSep ){` |
|       9 |  4914 | `		return nOrigIdx;` |
|       - |  4915 | `	}` |
|       - |  4916 | `	/* Check use imports first (works even outside namespaces) */` |
|  362836 |  4917 | `	SyBlobReset(&pGen->sWorker);` |
|  362836 |  4918 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  362836 |  4919 | `	if( pImport ){` |
|      38 |  4920 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      38 |  4921 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|      38 |  4922 | `		if( pFromImport ){` |
|      18 |  4923 | `			*pFromImport = 1;` |
|       8 |  4924 | `		}` |
|      20 |  4925 | `	}else{` |
|  362800 |  4926 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  362710 |  4927 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|       - |  4928 | `		}` |
|       - |  4929 | `		/* Prepend current namespace */` |
|      92 |  4930 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      92 |  4931 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      92 |  4932 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|       - |  4933 | `	}` |
|       - |  4934 | `	/* Look up or create a new literal for the qualified name */` |
|     128 |  4935 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|     128 |  4936 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      54 |  4937 | `		return nNewIdx; /* Already interned */` |
|       - |  4938 | `	}` |
|      76 |  4939 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      76 |  4940 | `	if( pNew == 0 ){` |
|     ! 0 |  4941 | `		return nOrigIdx; /* OOM, fall back to original */` |
|       - |  4942 | `	}` |
|      76 |  4943 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      76 |  4944 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      76 |  4945 | `	return nNewIdx;` |
|  181423 |  4946 |  |
|       - |  4947 | `/*` |
|       - |  4948 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|       - |  4949 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|       - |  4950 | ` */` |
|   32752 |  4951 | `static void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4952 |  |
|       - |  4953 | `	SyHashEntry *pImport;` |
|       - |  4954 | `	/* Check use imports first */` |
|   32754 |  4955 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)pName->zString,pName->nByte);` |
|   32754 |  4956 | `	if( pImport ){` |
|      14 |  4957 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|      14 |  4958 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|      14 |  4959 | `		return;` |
|       - |  4960 | `	}` |
|       - |  4961 | `	/* Prepend current namespace if active */` |
|   32742 |  4962 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       8 |  4963 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 |  4964 | `		SyBlobAppend(pOut,"\\",1);` |
|       3 |  4965 | `	}` |
|   32742 |  4966 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   16378 |  4967 |  |
|       - |  4968 | `/*` |
|       - |  4969 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|       - |  4970 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|       - |  4971 | ` * The caller must release pOut when done.` |
|       - |  4972 | ` */` |
|   53716 |  4973 | `static void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|       2 |  4974 |  |
|   53718 |  4975 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      60 |  4976 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      60 |  4977 | `		SyBlobAppend(pOut,"\\",1);` |
|      29 |  4978 | `	}` |
|   53718 |  4979 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|   53718 |  4980 |  |
|       - |  4981 | `/*` |
|       - |  4982 | ` * Compile a namespace statement` |
|       - |  4983 | ` * According to the PHP language reference manual` |
|       - |  4984 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|       - |  4985 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|       - |  4986 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|       - |  4987 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|       - |  4988 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|       - |  4989 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|       - |  4990 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|       - |  4991 | ` *  programming world.` |
|       - |  4992 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|       - |  4993 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|       - |  4994 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|       - |  4995 | ` *  classes/functions/constants.` |
|       - |  4996 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|       - |  4997 | ` *  readability of source code.` |
|       - |  4998 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|       - |  4999 | ` *  Here is an example of namespace syntax in PHP:` |
|       - |  5000 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|       - |  5001 | ` *       class MyClass {}` |
|       - |  5002 | ` *       function myfunction() {}` |
|       - |  5003 | ` *       const MYCONST = 1;` |
|       - |  5004 | ` *       $a = new MyClass;` |
|       - |  5005 | ` *       $c = new \my\name\MyClass;` |
|       - |  5006 | ` *       $a = strlen('hi');` |
|       - |  5007 | ` *       $d = namespace\MYCONST;` |
|       - |  5008 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|       - |  5009 | ` *       echo constant($d);` |
|       - |  5010 | ` * NOTE` |
|       - |  5011 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5012 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5013 | ` */` |
|       - |  5014 | `/*` |
|       - |  5015 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|       - |  5016 | ` */` |
|      14 |  5017 | `static const char * TokenTypeName(sxu32 nType)` |
|       1 |  5018 |  |
|      15 |  5019 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       9 |  5020 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       9 |  5021 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       9 |  5022 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       9 |  5023 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       9 |  5024 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|       3 |  5025 | `	return "token";` |
|       8 |  5026 |  |
|     104 |  5027 | `static sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|       2 |  5028 |  |
|       - |  5029 | `	sxu32 nLine;` |
|       - |  5030 | `	sxi32 rc;` |
|     106 |  5031 | `	nLine = pGen->pIn->nLine;` |
|     106 |  5032 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|       - |  5033 | `	/* Reset namespace and clear previous use imports */` |
|     106 |  5034 | `	SyBlobReset(&pGen->sNamespace);` |
|     106 |  5035 | `	SyHashRelease(&pGen->hUseImports);` |
|     106 |  5036 | `	SyHashInit(&pGen->hUseImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5037 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     106 |  5038 | `	SyHashInit(&pGen->hUseFuncImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5039 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     106 |  5040 | `	SyHashInit(&pGen->hUseConstImports,&pGen->pVm->sAllocator,0,0);` |
|     106 |  5041 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  5042 | `		/* Global namespace (bare "namespace;") */` |
|     ! 0 |  5043 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5044 | `		return SXRET_OK;` |
|       - |  5045 | `	}` |
|     106 |  5046 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|       - |  5047 | `		/* namespace; — switch to global namespace */` |
|     ! 0 |  5048 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5049 | `		return SXRET_OK;` |
|       - |  5050 | `	}` |
|     106 |  5051 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|       - |  5052 | `		/* namespace { } — global namespace block */` |
|     ! 0 |  5053 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|     ! 0 |  5054 | `		return SXRET_OK;` |
|       - |  5055 | `	}` |
|       - |  5056 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     252 |  5057 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     148 |  5058 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       - |  5059 | `			/* Append backslash separator */` |
|      24 |  5060 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      24 |  5061 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|      11 |  5062 | `			}` |
|      13 |  5063 | `		}else{` |
|       - |  5064 | `			/* Append identifier */` |
|     126 |  5065 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - |  5066 | `		}` |
|     148 |  5067 | `		pGen->pIn++;` |
|       2 |  5068 | `	}` |
|       - |  5069 | `	/* Emit a runtime namespace switch so the VM tracks the active namespace` |
|       - |  5070 | `	 * at the correct program counter, not just the last one compiled. */` |
|       - |  5071 | `	{` |
|     106 |  5072 | `		char *zNsDup = 0;` |
|     106 |  5073 | `		if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     155 |  5074 | `			zNsDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     102 |  5075 | `				(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      51 |  5076 | `		}` |
|     106 |  5077 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NSSWITCH,0,0,zNsDup,0);` |
|       - |  5078 | `	}` |
|     106 |  5079 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|       7 |  5080 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  5081 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|       4 |  5082 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       5 |  5083 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5084 | `			return SXERR_ABORT;` |
|       - |  5085 | `		}` |
|       2 |  5086 | `	}` |
|     106 |  5087 | `	return SXRET_OK;` |
|      54 |  5088 |  |
|       - |  5089 | `/*` |
|       - |  5090 | ` * Compile the 'use' statement` |
|       - |  5091 | ` * According to the PHP language reference manual` |
|       - |  5092 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|       - |  5093 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|       - |  5094 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|       - |  5095 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|       - |  5096 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|       - |  5097 | ` *  a function or constant is not supported.` |
|       - |  5098 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|       - |  5099 | ` * NOTE` |
|       - |  5100 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|       - |  5101 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|       - |  5102 | ` */` |
|      68 |  5103 | `static sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|       2 |  5104 |  |
|       - |  5105 | `	sxu32 nLine;` |
|       - |  5106 | `	sxi32 rc;` |
|       - |  5107 | `	SyBlob sPath;` |
|       - |  5108 | `	SyString sAlias;` |
|       - |  5109 | `	SyToken *pLast;` |
|       - |  5110 | `	char *zDup;` |
|       - |  5111 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|       - |  5112 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|       - |  5113 | `	SyHash *pVmHash;    /* Runtime import table (NULL if not needed) */` |
|      70 |  5114 | `	nLine = pGen->pIn->nLine;` |
|      70 |  5115 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|       - |  5116 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      70 |  5117 | `	iUseType = 0;` |
|      70 |  5118 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      30 |  5119 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|      30 |  5120 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|      16 |  5121 | `			iUseType = 1;` |
|      16 |  5122 | `			pGen->pIn++;` |
|      23 |  5123 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|      16 |  5124 | `			iUseType = 2;` |
|      16 |  5125 | `			pGen->pIn++;` |
|       7 |  5126 | `		}` |
|      14 |  5127 | `	}` |
|       - |  5128 | `	/* Select target hash tables based on import type */` |
|      70 |  5129 | `	switch( iUseType ){` |
|       7 |  5130 | `		case 1:` |
|      16 |  5131 | `			pGenHash = &pGen->hUseFuncImports;` |
|      16 |  5132 | `			pVmHash = 0; /* Function imports resolved at compile time only */` |
|      16 |  5133 | `			break;` |
|       7 |  5134 | `		case 2:` |
|      16 |  5135 | `			pGenHash = &pGen->hUseConstImports;` |
|      16 |  5136 | `			pVmHash = 0; /* Const imports use PH7_OP_USECONST for runtime scoping */` |
|      16 |  5137 | `			break;` |
|      20 |  5138 | `		default:` |
|      42 |  5139 | `			pGenHash = &pGen->hUseImports;` |
|      42 |  5140 | `			pVmHash = &pGen->pVm->hUseImports;` |
|      40 |  5141 | `			break;` |
|       - |  5142 | `	}` |
|      70 |  5143 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       - |  5144 | `	/* Process one or more use declarations separated by commas */` |
|      35 |  5145 | `	for(;;){` |
|      72 |  5146 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  5147 | `			break;` |
|       - |  5148 | `		}` |
|      72 |  5149 | `		SyBlobReset(&sPath);` |
|      72 |  5150 | `		pLast = 0;` |
|       - |  5151 | `		/* Collect the full namespace path */` |
|     258 |  5152 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|     188 |  5153 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     128 |  5154 | `				pLast = pGen->pIn;` |
|     128 |  5155 | `				if( SyBlobLength(&sPath) > 0 ){` |
|      62 |  5156 | `					SyBlobAppend(&sPath,"\\",1);` |
|      30 |  5157 | `				}` |
|     128 |  5158 | `				SyBlobAppend(&sPath,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      63 |  5159 | `			}` |
|     188 |  5160 | `			pGen->pIn++;` |
|       2 |  5161 | `		}` |
|      72 |  5162 | `		if( pLast == 0 ){` |
|       - |  5163 | `			/* Empty path */` |
|       5 |  5164 | `			break;` |
|       - |  5165 | `		}` |
|       - |  5166 | `		/* Default alias is the last component of the path */` |
|      68 |  5167 | `		sAlias = pLast->sData;` |
|       - |  5168 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      66 |  5169 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      43 |  5170 | `			&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|      18 |  5171 | `			pGen->pIn++; /* Jump 'as' */` |
|      18 |  5172 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|      18 |  5173 | `				sAlias = pGen->pIn->sData;` |
|      18 |  5174 | `				pGen->pIn++;` |
|       8 |  5175 | `			}` |
|       8 |  5176 | `		}` |
|       - |  5177 | `		/* Check for duplicate import alias (per-type) */` |
|      68 |  5178 | `		if( SyHashGet(pGenHash,sAlias.zString,sAlias.nByte) != 0 ){` |
|       7 |  5179 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  5180 | `				"Cannot use %.*s as %z because the name is already in use",` |
|       4 |  5181 | `				(int)SyBlobLength(&sPath),(const char *)SyBlobData(&sPath),&sAlias);` |
|       5 |  5182 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5183 | `				SyBlobRelease(&sPath);` |
|     ! 0 |  5184 | `				return SXERR_ABORT;` |
|       - |  5185 | `			}` |
|       2 |  5186 | `		}` |
|       - |  5187 | `		/* Register the import: alias -> FQN.` |
|       - |  5188 | `		 * Strings are allocated from the VM pool allocator and freed` |
|       - |  5189 | `		 * when the entire VM is released. SyHashRelease does not free` |
|       - |  5190 | `		 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|     101 |  5191 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      66 |  5192 | `			(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|      68 |  5193 | `		if( zDup ){` |
|      68 |  5194 | `			SyHashInsert(pGenHash,sAlias.zString,sAlias.nByte,zDup);` |
|      68 |  5195 | `			if( pVmHash ){` |
|       - |  5196 | `				/* Class imports: populate VM table directly (class resolution` |
|       - |  5197 | `				 * is compile-time only, the VM copy is kept for legacy reasons). */` |
|      40 |  5198 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      40 |  5199 | `				if( zAliasDup ){` |
|      40 |  5200 | `					SyHashInsert(pVmHash,zAliasDup,sAlias.nByte,zDup);` |
|      19 |  5201 | `				}` |
|      19 |  5202 | `			}` |
|      68 |  5203 | `			if( iUseType == 2 ){` |
|       - |  5204 | `				/* Const imports: emit a runtime instruction so imports are` |
|       - |  5205 | `				 * namespace-scoped (NSSWITCH clears the VM table). */` |
|      16 |  5206 | `				char *zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|      16 |  5207 | `				if( zAliasDup ){` |
|       - |  5208 | `					/* Encode alias length in iP1, alias string in p3 is not enough —` |
|       - |  5209 | `					 * we need both alias and FQN.  Pack them: iP1=alias length,` |
|       - |  5210 | `					 * iP2 unused, p3 points to a two-pointer struct. */` |
|      16 |  5211 | `					char **azPair = (char **)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(char*)*2);` |
|      16 |  5212 | `					if( azPair ){` |
|      16 |  5213 | `						azPair[0] = zAliasDup;` |
|      16 |  5214 | `						azPair[1] = zDup;` |
|      16 |  5215 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_USECONST,(sxi32)sAlias.nByte,0,azPair,0);` |
|       7 |  5216 | `					}` |
|       7 |  5217 | `				}` |
|       7 |  5218 | `			}` |
|      33 |  5219 | `		}` |
|       - |  5220 | `		/* Check for comma (multiple use declarations) */` |
|      68 |  5221 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  5222 | `			pGen->pIn++;` |
|       2 |  5223 | `		}else{` |
|      34 |  5224 | `			break;` |
|       - |  5225 | `		}` |
|       1 |  5226 | `	}` |
|      70 |  5227 | `	SyBlobRelease(&sPath);` |
|      70 |  5228 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       4 |  5229 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|       2 |  5230 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       3 |  5231 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5232 | `			return SXERR_ABORT;` |
|       - |  5233 | `		}` |
|       1 |  5234 | `	}` |
|      70 |  5235 | `	return SXRET_OK;` |
|      36 |  5236 |  |
|       - |  5237 | `/*` |
|       - |  5238 | ` * Compile the stupid 'declare' language construct.` |
|       - |  5239 | ` *` |
|       - |  5240 | ` * According to the PHP language reference manual.` |
|       - |  5241 | ` *  The declare construct is used to set execution directives for a block of code.` |
|       - |  5242 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|       - |  5243 | ` *  declare (directive)` |
|       - |  5244 | ` *   statement` |
|       - |  5245 | ` * The directive section allows the behavior of the declare block to be set.` |
|       - |  5246 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|       - |  5247 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|       - |  5248 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|       - |  5249 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|       - |  5250 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|       - |  5251 | ` * <?php` |
|       - |  5252 | ` * // these are the same:` |
|       - |  5253 | ` * // you can use this:` |
|       - |  5254 | ` * declare(ticks=1) {` |
|       - |  5255 | ` *   // entire script here` |
|       - |  5256 | ` * }` |
|       - |  5257 | ` * // or you can use this:` |
|       - |  5258 | ` * declare(ticks=1);` |
|       - |  5259 | ` * // entire script here` |
|       - |  5260 | ` * ?>` |
|       - |  5261 | ` *` |
|       - |  5262 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|       - |  5263 | ` */` |
|       - |  5264 | `/*` |
|       - |  5265 | ` * Match a directive name against a known literal (case-insensitive).` |
|       - |  5266 | ` */` |
|      64 |  5267 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|       2 |  5268 |  |
|      94 |  5269 | `	return SyStringLength(pName) == nWant` |
|      64 |  5270 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|       2 |  5271 |  |
|       - |  5272 |  |
|      38 |  5273 | `static sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|       2 |  5274 |  |
|      40 |  5275 | `	sxu32 nLine = pGen->pIn->nLine;` |
|      40 |  5276 | `	SyToken *pBodyEnd = 0;` |
|       - |  5277 | `	SyToken *pBodyStart;` |
|       - |  5278 | `	SyToken *pCursor;` |
|       - |  5279 | `	int bHasStrictTypes;` |
|       - |  5280 | `	int bBlockForm;` |
|       - |  5281 | `	int bPlacementOk;` |
|       - |  5282 | `	sxi32 rc;` |
|      40 |  5283 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|      40 |  5284 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|       5 |  5285 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting opening parenthesis '('");` |
|       5 |  5286 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5287 | `			return SXERR_ABORT;` |
|       - |  5288 | `		}` |
|       5 |  5289 | `		goto Synchro;` |
|       - |  5290 | `	}` |
|      36 |  5291 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|      36 |  5292 | `	pBodyStart = pGen->pIn;` |
|       - |  5293 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|      36 |  5294 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|      36 |  5295 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  5296 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Missing closing parenthesis ')'");` |
|     ! 0 |  5297 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5298 | `			return SXERR_ABORT;` |
|       - |  5299 | `		}` |
|     ! 0 |  5300 | `		return SXRET_OK;` |
|       - |  5301 | `	}` |
|       - |  5302 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|       - |  5303 | `	 * now delimits the comma-separated directive list. */` |
|      36 |  5304 | `	pGen->pIn = &pBodyEnd[1];` |
|      36 |  5305 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|     ! 0 |  5306 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|     ! 0 |  5307 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  5308 | `			return SXERR_ABORT;` |
|       - |  5309 | `		}` |
|     ! 0 |  5310 | `	}` |
|      36 |  5311 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|      36 |  5312 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|      36 |  5313 | `	bHasStrictTypes = 0;` |
|       - |  5314 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|       - |  5315 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|       - |  5316 | `	 * directive appears anywhere in the list, before validating values. */` |
|      36 |  5317 | `	pCursor = pBodyStart;` |
|      48 |  5318 | `	while( pCursor < pBodyEnd ){` |
|      44 |  5319 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      36 |  5320 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|      32 |  5321 | `				bHasStrictTypes = 1;` |
|      32 |  5322 | `				break;` |
|       - |  5323 | `			}` |
|       2 |  5324 | `		}` |
|      13 |  5325 | `		pCursor++;` |
|       1 |  5326 | `	}` |
|      36 |  5327 | `	if( bHasStrictTypes && bBlockForm ){` |
|       3 |  5328 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5329 | `			"strict_types declaration must not use block mode");` |
|       3 |  5330 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5331 | `		return SXRET_OK;` |
|       - |  5332 | `	}` |
|      34 |  5333 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|       5 |  5334 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5335 | `			"strict_types declaration must be the very first statement in the script");` |
|       5 |  5336 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       5 |  5337 | `		return SXRET_OK;` |
|       - |  5338 | `	}` |
|       - |  5339 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|      30 |  5340 | `	pCursor = pBodyStart;` |
|      58 |  5341 | `	while( pCursor < pBodyEnd ){` |
|       - |  5342 | `		SyToken *pNameTok;` |
|       - |  5343 | `		SyToken *pEqTok;` |
|       - |  5344 | `		SyToken *pValTok;` |
|       - |  5345 | `		SyString *pDirName;` |
|       - |  5346 | `		int bIsStrict;` |
|       - |  5347 | `		int iStrictValue;` |
|      32 |  5348 | `		pNameTok = pCursor;` |
|      32 |  5349 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5350 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5351 | `				"declare: Expecting a directive name");` |
|     ! 0 |  5352 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5353 | `			return SXRET_OK;` |
|       - |  5354 | `		}` |
|      32 |  5355 | `		pEqTok = pNameTok + 1;` |
|      32 |  5356 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|     ! 0 |  5357 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5358 | `				"declare: Expecting '=' after directive name");` |
|     ! 0 |  5359 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5360 | `			return SXRET_OK;` |
|       - |  5361 | `		}` |
|      32 |  5362 | `		pValTok = pEqTok + 1;` |
|      32 |  5363 | `		if( pValTok >= pBodyEnd ){` |
|     ! 0 |  5364 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5365 | `				"declare: Expecting value after '='");` |
|     ! 0 |  5366 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5367 | `			return SXRET_OK;` |
|       - |  5368 | `		}` |
|      32 |  5369 | `		pDirName = &pNameTok->sData;` |
|      32 |  5370 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|      32 |  5371 | `		if( bIsStrict ){` |
|       - |  5372 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|       - |  5373 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|      28 |  5374 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|     ! 0 |  5375 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5376 | `					"declare(strict_types) value must be a literal");` |
|     ! 0 |  5377 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5378 | `				return SXRET_OK;` |
|       - |  5379 | `			}` |
|      28 |  5380 | `			iStrictValue = -1;` |
|      28 |  5381 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|      28 |  5382 | `				const char *zv = SyStringData(&pValTok->sData);` |
|      28 |  5383 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|      28 |  5384 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|      26 |  5385 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|      13 |  5386 | `			}` |
|      28 |  5387 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|       3 |  5388 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5389 | `					"strict_types declaration must have 0 or 1 as its value");` |
|       3 |  5390 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|       3 |  5391 | `				return SXRET_OK;` |
|       - |  5392 | `			}` |
|      26 |  5393 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|      14 |  5394 | `		}else{` |
|       - |  5395 | `			/* Other directives (ticks, encoding, or unknown) remain no-ops —` |
|       - |  5396 | `			 * preserve the legacy notice so callers relying on the old` |
|       - |  5397 | `			 * behavior don't regress. */` |
|       7 |  5398 | `			PH7_GenCompileError(&(*pGen),E_NOTICE,nLine,` |
|       - |  5399 | `				"the declare construct is a no-op in the current release of the PH7(%s) engine",` |
|       2 |  5400 | `				ph7_lib_version()` |
|       - |  5401 | `				);` |
|       - |  5402 | `		}` |
|      30 |  5403 | `		pCursor = pValTok + 1;` |
|       - |  5404 | `		/* Consume separating comma (or end). */` |
|      30 |  5405 | `		if( pCursor < pBodyEnd ){` |
|       3 |  5406 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5407 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  5408 | `					"declare: Expecting ',' or ')' after directive value");` |
|     ! 0 |  5409 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|     ! 0 |  5410 | `				return SXRET_OK;` |
|       - |  5411 | `			}` |
|       3 |  5412 | `			pCursor++;` |
|       1 |  5413 | `		}` |
|       2 |  5414 | `	}` |
|       - |  5415 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|       - |  5416 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|       - |  5417 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|      28 |  5418 | `	return SXRET_OK;` |
|       2 |  5419 | `Synchro:` |
|       - |  5420 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|      15 |  5421 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      11 |  5422 | `		pGen->pIn++;` |
|       1 |  5423 | `	}` |
|       5 |  5424 | `	return SXRET_OK;` |
|      21 |  5425 |  |
|       - |  5426 | `/*` |
|       - |  5427 | ` * Process default argument values. That is,a function may define C++-style default value` |
|       - |  5428 | ` * as follows:` |
|       - |  5429 | ` * function makecoffee($type = "cappuccino")` |
|       - |  5430 | ` * {` |
|       - |  5431 | ` *   return "Making a cup of $type.\n";` |
|       - |  5432 | ` * }` |
|       - |  5433 | ` * Symisc eXtension.` |
|       - |  5434 | ` *  1 -) Default arguments value can be any complex expression [i.e: function call,annynoymous` |
|       - |  5435 | ` *      functions,array member,..] unlike the zend which would allow only single scalar value.` |
|       - |  5436 | ` *      Example: Work only with PH7,generate error under zend` |
|       - |  5437 | ` *      function test($a = 'Hello'.'World: '.rand_str(3))` |
|       - |  5438 | ` *      {` |
|       - |  5439 | ` *       var_dump($a);` |
|       - |  5440 | ` *      }` |
|       - |  5441 | ` *     //call test without args` |
|       - |  5442 | ` *      test();` |
|       - |  5443 | ` * 2 -) Full type hinting: (Arguments are automatically casted to the desired type)` |
|       - |  5444 | ` *      Example:` |
|       - |  5445 | ` *           function a(string $a){} function b(int $a,string $c,float $d){}` |
|       - |  5446 | ` * 3 -) Function overloading!!` |
|       - |  5447 | ` *      Example:` |
|       - |  5448 | ` *      function foo($a) {` |
|       - |  5449 | ` *   	  return $a.PHP_EOL;` |
|       - |  5450 | ` *	    }` |
|       - |  5451 | ` *	    function foo($a, $b) {` |
|       - |  5452 | ` *   	  return $a + $b;` |
|       - |  5453 | ` *	    }` |
|       - |  5454 | ` *	    echo foo(5); // Prints "5"` |
|       - |  5455 | ` *	    echo foo(5, 2); // Prints "7"` |
|       - |  5456 | ` *      // Same arg` |
|       - |  5457 | ` *	   function foo(string $a)` |
|       - |  5458 | ` *	   {` |
|       - |  5459 | ` *	     echo "a is a string\n";` |
|       - |  5460 | ` *	     var_dump($a);` |
|       - |  5461 | ` *	   }` |
|       - |  5462 | ` *	  function foo(int $a)` |
|       - |  5463 | ` *	  {` |
|       - |  5464 | ` *	    echo "a is integer\n";` |
|       - |  5465 | ` *	    var_dump($a);` |
|       - |  5466 | ` *	  }` |
|       - |  5467 | ` *	  function foo(array $a)` |
|       - |  5468 | ` *	  {` |
|       - |  5469 | ` * 	    echo "a is an array\n";` |
|       - |  5470 | ` * 	    var_dump($a);` |
|       - |  5471 | ` *	  }` |
|       - |  5472 | ` *	  foo('This is a great feature'); // a is a string [first foo]` |
|       - |  5473 | ` *	  foo(52); // a is integer [second foo]` |
|       - |  5474 | ` *    foo(array(14,__TIME__,__DATE__)); // a is an array [third foo]` |
|       - |  5475 | ` * Please refer to the official documentation for more information on the powerful extension` |
|       - |  5476 | ` * introduced by the PH7 engine.` |
|       - |  5477 | ` */` |
|   55814 |  5478 | `static sxi32 GenStateProcessArgValue(ph7_gen_state *pGen,ph7_vm_func_arg *pArg,SyToken *pIn,SyToken *pEnd)` |
|       2 |  5479 |  |
|       - |  5480 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5481 | `	SySet *pInstrContainer;` |
|       - |  5482 | `	sxi32 rc;` |
|       - |  5483 | `	/* Swap token stream */` |
|   55816 |  5484 | `	SWAP_DELIMITER(pGen,pIn,pEnd);` |
|   55816 |  5485 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   55816 |  5486 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pArg->aByteCode);` |
|       - |  5487 | `	/* Compile the expression holding the argument value */` |
|   55816 |  5488 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  5489 | `	/* Emit the done instruction */` |
|   55816 |  5490 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   55816 |  5491 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   55816 |  5492 | `	RE_SWAP_DELIMITER(pGen);` |
|   55816 |  5493 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  5494 | `		return SXERR_ABORT;` |
|       - |  5495 | `	}` |
|   55816 |  5496 | `	return SXRET_OK;` |
|   27909 |  5497 |  |
|       - |  5498 | `/*` |
|       - |  5499 | ` * Collect function arguments one after one.` |
|       - |  5500 | ` * According to the PHP language reference manual.` |
|       - |  5501 | ` * Information may be passed to functions via the argument list, which is a comma-delimited` |
|       - |  5502 | ` * list of expressions.` |
|       - |  5503 | ` * PHP supports passing arguments by value (the default), passing by reference` |
|       - |  5504 | ` * and default argument values. Variable-length argument lists are also supported,` |
|       - |  5505 | ` * see also the function references for func_num_args(), func_get_arg(), and func_get_args()` |
|       - |  5506 | ` * for more information.` |
|       - |  5507 | ` * Example #1 Passing arrays to functions` |
|       - |  5508 | ` * <?php` |
|       - |  5509 | ` * function takes_array($input)` |
|       - |  5510 | ` * {` |
|       - |  5511 | ` *    echo "$input[0] + $input[1] = ", $input[0]+$input[1];` |
|       - |  5512 | ` * }` |
|       - |  5513 | ` * ?>` |
|       - |  5514 | ` * Making arguments be passed by reference` |
|       - |  5515 | ` * By default, function arguments are passed by value (so that if the value of the argument` |
|       - |  5516 | ` * within the function is changed, it does not get changed outside of the function).` |
|       - |  5517 | ` * To allow a function to modify its arguments, they must be passed by reference.` |
|       - |  5518 | ` * To have an argument to a function always passed by reference, prepend an ampersand (&)` |
|       - |  5519 | ` * to the argument name in the function definition:` |
|       - |  5520 | ` * Example #2 Passing function parameters by reference` |
|       - |  5521 | ` * <?php` |
|       - |  5522 | ` * function add_some_extra(&$string)` |
|       - |  5523 | ` * {` |
|       - |  5524 | ` *   $string .= 'and something extra.';` |
|       - |  5525 | ` * }` |
|       - |  5526 | ` * $str = 'This is a string, ';` |
|       - |  5527 | ` * add_some_extra($str);` |
|       - |  5528 | ` * echo $str;    // outputs 'This is a string, and something extra.'` |
|       - |  5529 | ` * ?>` |
|       - |  5530 | ` *` |
|       - |  5531 | ` * PH7 have introduced powerful extension including full type hinting,function overloading` |
|       - |  5532 | ` * complex agrument values.Please refer to the official documentation for more information` |
|       - |  5533 | ` * on these extension.` |
|       - |  5534 | ` */` |
|   59580 |  5535 | `static sxi32 GenStateCollectFuncArgs(ph7_vm_func *pFunc,ph7_gen_state *pGen,SyToken *pEnd,int bCtorCtx,int bAbstractCtx)` |
|       2 |  5536 |  |
|       - |  5537 | `	ph7_vm_func_arg sArg; /* Current processed argument */` |
|       - |  5538 | `	SyToken *pIn;  /* Token stream */` |
|       - |  5539 | `	SyBlob sSig;         /* Function signature */` |
|       - |  5540 | `	char *zDup;          /* Copy of argument name */` |
|       - |  5541 | `	sxi32 rc;` |
|       - |  5542 |  |
|   59582 |  5543 | `	pIn = pGen->pIn;` |
|   59582 |  5544 | `	SyBlobInit(&sSig,&pGen->pVm->sAllocator);` |
|       - |  5545 | `	/* Process arguments one after one */` |
|   77491 |  5546 | `	for(;;){` |
|  154984 |  5547 | `		if( pIn >= pEnd ){` |
|       - |  5548 | `			/* No more arguments to process */` |
|   59570 |  5549 | `			break;` |
|       - |  5550 | `		}` |
|   95416 |  5551 | `		SyZero(&sArg,sizeof(ph7_vm_func_arg));` |
|   95416 |  5552 | `		SySetInit(&sArg.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|   95416 |  5553 | `		SySetInit(&sArg.aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|   95416 |  5554 | `		SyStringInitFromBuf(&sArg.sTypeName,0,0);` |
|       - |  5555 | `		/* Parse optional visibility modifier (constructor property promotion, PHP 8.0+) */` |
|   95416 |  5556 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_KEYWORD) ){` |
|   52996 |  5557 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pIn->pUserData);` |
|   52996 |  5558 | `			if( nKw == PH7_TKWRD_PUBLIC \|\| nKw == PH7_TKWRD_PROTECTED \|\| nKw == PH7_TKWRD_PRIVATE ){` |
|      42 |  5559 | `				if( !bCtorCtx ){` |
|       5 |  5560 | `					if( bAbstractCtx ){` |
|       3 |  5561 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5562 | `							"Cannot declare promoted property in an abstract constructor");` |
|       2 |  5563 | `					}else{` |
|       3 |  5564 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pIn->nLine,` |
|       - |  5565 | `							"Cannot declare promoted property outside a constructor");` |
|       - |  5566 | `					}` |
|       5 |  5567 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  5568 | `						return SXERR_ABORT;` |
|       - |  5569 | `					}` |
|       5 |  5570 | `					return SXERR_SYNTAX;` |
|       - |  5571 | `				}` |
|      38 |  5572 | `				sArg.iFlags \|= VM_FUNC_ARG_PROMOTED;` |
|      38 |  5573 | `				if( nKw == PH7_TKWRD_PRIVATE ){` |
|       3 |  5574 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PRIVATE;` |
|      37 |  5575 | `				}else if( nKw == PH7_TKWRD_PROTECTED ){` |
|       3 |  5576 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PROTECTED;` |
|       2 |  5577 | `				}else{` |
|      34 |  5578 | `					sArg.iPromoteVis = PH7_CLASS_PROT_PUBLIC;` |
|       - |  5579 | `				}` |
|      38 |  5580 | `				pIn++;` |
|      18 |  5581 | `			}` |
|   26495 |  5582 | `		}` |
|       - |  5583 | `		/* Parse optional type hint (single, nullable shorthand, or union) */` |
|  126338 |  5584 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_DOLLAR) == 0` |
|   80113 |  5585 | `			&& (pIn->nType & PH7_TK_AMPER) == 0` |
|   63338 |  5586 | `			&& (pIn->nType & PH7_TK_ELLIPSIS) == 0 ){` |
|   61840 |  5587 | `			sxu32 nLineLocal = pIn->nLine;` |
|   61840 |  5588 | `			sxi32 iTFlags = 0;` |
|   61840 |  5589 | `			pGen->pIn = pIn;` |
|   61840 |  5590 | `			rc = GenStateParseUnionTypeDecl(` |
|   30919 |  5591 | `				pGen, &sArg.nType, &sArg.sClass, &sArg.aUnionAlts,` |
|   30919 |  5592 | `				&iTFlags, &sArg.sTypeName,` |
|       - |  5593 | `				VM_FUNC_ARG_NULLABLE, VM_FUNC_ARG_UNION,` |
|       - |  5594 | `				/* bAllowVoid */ 0,` |
|   30919 |  5595 | `						nLineLocal);` |
|   61840 |  5596 | `			pIn = pGen->pIn;` |
|   61840 |  5597 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  5598 | `				return SXERR_ABORT;` |
|   61840 |  5599 | `			}else if( rc == SXERR_CORRUPT ){` |
|       - |  5600 | `				/* Error already reported by GenStateParseUnionTypeDecl */` |
|       3 |  5601 | `				return SXERR_SYNTAX;` |
|   61838 |  5602 | `			}else if( rc == SXERR_SYNTAX ){` |
|       5 |  5603 | `				if( pIn < pEnd ){` |
|       7 |  5604 | `					PH7_GenCompileError(pGen,E_PARSE,pIn->nLine,` |
|       - |  5605 | `						"syntax error, unexpected token \"%z\", expecting variable",` |
|       2 |  5606 | `						&pIn->sData);` |
|       3 |  5607 | `				}else{` |
|     ! 0 |  5608 | `					PH7_GenCompileError(pGen,E_PARSE,nLineLocal,` |
|       - |  5609 | `						"syntax error, unexpected end of file");` |
|       - |  5610 | `				}` |
|       5 |  5611 | `				return SXERR_SYNTAX;` |
|       - |  5612 | `			}` |
|   61834 |  5613 | `			sArg.iFlags \|= iTFlags;` |
|   30916 |  5614 | `		}` |
|   95406 |  5615 | `		if( pIn >= pEnd ){` |
|     ! 0 |  5616 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Missing argument name");` |
|     ! 0 |  5617 | `			return rc;` |
|       - |  5618 | `		}` |
|   95406 |  5619 | `		if( pIn->nType & PH7_TK_AMPER ){` |
|       - |  5620 | `			/* Pass by reference,record that */` |
|    2964 |  5621 | `			sArg.iFlags \|= VM_FUNC_ARG_BY_REF;` |
|    2964 |  5622 | `			pIn++;` |
|    1481 |  5623 | `		}` |
|   95406 |  5624 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_ELLIPSIS) ){` |
|       - |  5625 | `			/* Variadic parameter: ...$args */` |
|      40 |  5626 | `			sArg.iFlags \|= VM_FUNC_ARG_VARIADIC;` |
|      40 |  5627 | `			pIn++;` |
|      19 |  5628 | `		}` |
|   95406 |  5629 | `		if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pIn[1] >= pEnd \|\| (pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  5630 | `			/* Invalid argument */` |
|     ! 0 |  5631 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Invalid argument name");` |
|     ! 0 |  5632 | `			return rc;` |
|       - |  5633 | `		}` |
|   95406 |  5634 | `		pIn++; /* Jump the dollar sign */` |
|       - |  5635 | `		/* Copy argument name */` |
|   95406 |  5636 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,SyStringData(&pIn->sData),SyStringLength(&pIn->sData));` |
|   95406 |  5637 | `		if( zDup == 0 ){` |
|     ! 0 |  5638 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"PH7 engine is running out of memory");` |
|     ! 0 |  5639 | `			return SXERR_ABORT;` |
|       - |  5640 | `		}` |
|   95406 |  5641 | `		SyStringInitFromBuf(&sArg.sName,zDup,SyStringLength(&pIn->sData));` |
|   95406 |  5642 | `		pIn++;` |
|   95406 |  5643 | `		if( pIn < pEnd ){` |
|   62288 |  5644 | `			if( pIn->nType & PH7_TK_EQUAL ){` |
|       - |  5645 | `				SyToken *pDefend;` |
|   55818 |  5646 | `				sxi32 iNest = 0;` |
|   55818 |  5647 | `				pIn++; /* Jump the equal sign */` |
|   55818 |  5648 | `				pDefend = pIn;` |
|       - |  5649 | `				/* Process the default value associated with this argument */` |
|  117504 |  5650 | `				while( pDefend < pEnd ){` |
|   91056 |  5651 | `					if( (pDefend->nType & PH7_TK_COMMA) && iNest <= 0 ){` |
|   29370 |  5652 | `						break;` |
|       - |  5653 | `					}` |
|   61688 |  5654 | `					if( pDefend->nType & (PH7_TK_LPAREN/*'('*/\|PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*[*/) ){` |
|       - |  5655 | `						/* Increment nesting level */` |
|    2938 |  5656 | `						iNest++;` |
|   60220 |  5657 | `					}else if( pDefend->nType & (PH7_TK_RPAREN/*')'*/\|PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*]*/) ){` |
|       - |  5658 | `						/* Decrement nesting level */` |
|    2938 |  5659 | `						iNest--;` |
|    1468 |  5660 | `					}` |
|   61688 |  5661 | `					pDefend++;` |
|       2 |  5662 | `				}` |
|   55818 |  5663 | `				if( pIn >= pDefend ){` |
|       3 |  5664 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Missing argument default value");` |
|       3 |  5665 | `					return rc;` |
|       - |  5666 | `				}` |
|       - |  5667 | `				/* Process default value */` |
|   55816 |  5668 | `				rc = GenStateProcessArgValue(&(*pGen),&sArg,pIn,pDefend);` |
|   55816 |  5669 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  5670 | `					return rc;` |
|       - |  5671 | `				}` |
|       - |  5672 | `				/* Point beyond the default value */` |
|   55816 |  5673 | `				pIn = pDefend;` |
|   27907 |  5674 | `			}` |
|   62286 |  5675 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) == 0 ){` |
|     ! 0 |  5676 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pIn->nLine,"Unexpected token '%z'",&pIn->sData);` |
|     ! 0 |  5677 | `				return rc;` |
|       - |  5678 | `			}` |
|   62286 |  5679 | `			pIn++; /* Jump the trailing comma */` |
|   31142 |  5680 | `		}` |
|       - |  5681 | `		/* Append argument signature */` |
|   95404 |  5682 | `		if( sArg.nType > 0 ){` |
|   61792 |  5683 | `			if( SyStringLength(&sArg.sClass) > 0 ){` |
|       - |  5684 | `				/* Class name — prefix with 'o' so generic object hint is a prefix match */` |
|    8826 |  5685 | `				int marker = 'o';` |
|    8826 |  5686 | `				SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|    8826 |  5687 | `				SyBlobAppend(&sSig,SyStringData(&sArg.sClass),SyStringLength(&sArg.sClass));` |
|    4414 |  5688 | `			}else{` |
|       - |  5689 | `				int c;` |
|   52968 |  5690 | `				c = 'n'; /* cc warning */` |
|       - |  5691 | `				/* Type leading character */` |
|   52968 |  5692 | `				switch(sArg.nType){` |
|     ! 0 |  5693 | `				case MEMOBJ_HASHMAP:` |
|       - |  5694 | `					/* Hashmap aka 'array' */` |
|     ! 0 |  5695 | `					c = 'h';` |
|     ! 0 |  5696 | `					break;` |
|    7374 |  5697 | `				case MEMOBJ_INT:` |
|       - |  5698 | `					/* Integer */` |
|   14750 |  5699 | `					c = 'i';` |
|   14750 |  5700 | `					break;` |
|       1 |  5701 | `				case MEMOBJ_BOOL:` |
|       - |  5702 | `					/* Bool */` |
|       3 |  5703 | `					c = 'b';` |
|       3 |  5704 | `					break;` |
|       1 |  5705 | `				case MEMOBJ_REAL:` |
|       - |  5706 | `					/* Float */` |
|       3 |  5707 | `					c = 'f';` |
|       3 |  5708 | `					break;` |
|   19100 |  5709 | `				case MEMOBJ_STRING:` |
|       - |  5710 | `					/* String */` |
|   38202 |  5711 | `					c = 's';` |
|   38202 |  5712 | `					break;` |
|       7 |  5713 | `				case MEMOBJ_OBJ:` |
|       - |  5714 | `					/* Object */` |
|      16 |  5715 | `					c = 'o';` |
|      14 |  5716 | `					break;` |
|     ! 0 |  5717 | `				default:` |
|     ! 0 |  5718 | `					break;` |
|       - |  5719 | `				}` |
|   52968 |  5720 | `				SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       - |  5721 | `			}` |
|   30897 |  5722 | `		}else{` |
|       - |  5723 | `			/* No type is associated with this parameter which mean` |
|       - |  5724 | `			 * that this function is not condidate for overloading.` |
|       - |  5725 | `			 */` |
|   33614 |  5726 | `			SyBlobRelease(&sSig);` |
|       - |  5727 | `		}` |
|       - |  5728 | `		/* Save in the argument set */` |
|   95404 |  5729 | `		SySetPut(&pFunc->aArgs,(const void *)&sArg);` |
|       2 |  5730 | `	}` |
|   59570 |  5731 | `	if( SyBlobLength(&sSig) > 0 ){` |
|       - |  5732 | `		/* Save function signature */` |
|   38284 |  5733 | `		SyStringInitFromBuf(&pFunc->sSignature,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|   19141 |  5734 | `	}` |
|   59570 |  5735 | `	return SXRET_OK;` |
|   29792 |  5736 |  |
|       - |  5737 | `/*` |
|       - |  5738 | ` * Compile function [i.e: standard function, annonymous function or closure ] body.` |
|       - |  5739 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|       - |  5740 | ` * and this routine takes care of generating the appropriate error message.` |
|       - |  5741 | ` */` |
|  183410 |  5742 | `static sxi32 GenStateCompileFuncBody(` |
|       - |  5743 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  5744 | `	ph7_vm_func *pFunc    /* Function state */` |
|       - |  5745 | `	)` |
|       2 |  5746 |  |
|       - |  5747 | `	SySet *pInstrContainer; /* Instruction container */` |
|       - |  5748 | `	GenBlock *pBlock;` |
|       - |  5749 | `	sxu32 nGotoOfft;` |
|       - |  5750 | `	sxi32 rc;` |
|       - |  5751 | `	/* Attach the new function */` |
|  183412 |  5752 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|  183412 |  5753 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  5754 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out-of-memory");` |
|       - |  5755 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5756 | `		return SXERR_ABORT;` |
|       - |  5757 | `	}` |
|  183412 |  5758 | `	nGotoOfft = SySetUsed(&pGen->aGoto);` |
|       - |  5759 | `	/* Swap bytecode containers */` |
|  183412 |  5760 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|  183412 |  5761 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|       - |  5762 | `	/* Emit constructor property promotion prologue:` |
|       - |  5763 | `	 *   $this->NAME = $NAME;` |
|       - |  5764 | `	 * for each promoted parameter. Runtime typed-property store enforcement` |
|       - |  5765 | `	 * happens through the normal PH7_OP_MEMBER/PH7_OP_STORE path. */` |
|       - |  5766 | `	{` |
|  183412 |  5767 | `		sxu32 nArg = SySetUsed(&pFunc->aArgs);` |
|       - |  5768 | `		sxu32 i;` |
|  275800 |  5769 | `		for( i = 0; i < nArg; i++ ){` |
|   92390 |  5770 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs,i);` |
|       - |  5771 | `			char *zSrc;` |
|       - |  5772 | `			sxu32 nSrc,nName;` |
|       - |  5773 | `			SySet sToken;` |
|       - |  5774 | `			SyToken *pTmpIn,*pTmpEnd;` |
|       - |  5775 | `			sxi32 rcPromote;` |
|   92390 |  5776 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   92362 |  5777 | `				continue;` |
|       - |  5778 | `			}` |
|       - |  5779 | `			/* Build "$this->NAME = $NAME" in a buffer owned by the VM allocator.` |
|       - |  5780 | `			 * Tokens keep pointers into this buffer (identifier names are not` |
|       - |  5781 | `			 * copied), so it must outlive the function — never free it. The` |
|       - |  5782 | `			 * buffer is null-terminated because PH7_OP_LOAD reads the variable` |
|       - |  5783 | `			 * name via SyStrlen() on the token's sData pointer. */` |
|      30 |  5784 | `			nName = SyStringLength(&pArg->sName);` |
|      30 |  5785 | `			nSrc = (sizeof("$this->") - 1) + nName + (sizeof(" = $") - 1) + nName;` |
|      30 |  5786 | `			zSrc = (char *)SyMemBackendAlloc(&pGen->pVm->sAllocator,nSrc + 1);` |
|      30 |  5787 | `			if( zSrc == 0 ){` |
|     ! 0 |  5788 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5789 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5790 | `				PH7_GenCompileError(pGen,E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 |  5791 | `				return SXERR_ABORT;` |
|       - |  5792 | `			}` |
|       - |  5793 | `			{` |
|      30 |  5794 | `				char *z = zSrc;` |
|      30 |  5795 | `				SyMemcpy("$this->",z,sizeof("$this->")-1);` |
|      30 |  5796 | `				z += sizeof("$this->")-1;` |
|      30 |  5797 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5798 | `				z += nName;` |
|      30 |  5799 | `				SyMemcpy(" = $",z,sizeof(" = $")-1);` |
|      30 |  5800 | `				z += sizeof(" = $")-1;` |
|      30 |  5801 | `				SyMemcpy(SyStringData(&pArg->sName),z,nName);` |
|      30 |  5802 | `				z += nName;` |
|      30 |  5803 | `				*z = 0;` |
|       - |  5804 | `			}` |
|      30 |  5805 | `			SySetInit(&sToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      30 |  5806 | `			PH7_TokenizePHP(zSrc,nSrc,1,&sToken);` |
|      30 |  5807 | `			pTmpIn = pGen->pIn;` |
|      30 |  5808 | `			pTmpEnd = pGen->pEnd;` |
|      30 |  5809 | `			pGen->pIn = (SyToken *)SySetBasePtr(&sToken);` |
|      30 |  5810 | `			pGen->pEnd = &pGen->pIn[SySetUsed(&sToken)];` |
|      30 |  5811 | `			rcPromote = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  5812 | `			pGen->pIn = pTmpIn;` |
|      30 |  5813 | `			pGen->pEnd = pTmpEnd;` |
|      30 |  5814 | `			SySetRelease(&sToken);` |
|      30 |  5815 | `			if( rcPromote == SXERR_ABORT ){` |
|     ! 0 |  5816 | `				PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  5817 | `				GenStateLeaveBlock(&(*pGen),0);` |
|     ! 0 |  5818 | `				return SXERR_ABORT;` |
|       - |  5819 | `			}` |
|       - |  5820 | `			/* Discard the assignment result — this is a statement expression. */` |
|      30 |  5821 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      16 |  5822 | `		}` |
|       - |  5823 | `	}` |
|       - |  5824 | `	/* Compile the body */` |
|  183412 |  5825 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  5826 | `	/* Fix exception jumps now the destination is resolved */` |
|  183412 |  5827 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - |  5828 | `	/* Emit the final return if not yet done */` |
|  183412 |  5829 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  5830 | `	/* Fix gotos jumps now the destination is resolved */` |
|  183412 |  5831 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),nGotoOfft) ){` |
|     ! 0 |  5832 | `		rc = SXERR_ABORT;` |
|     ! 0 |  5833 | `	}` |
|  183412 |  5834 | `	SySetTruncate(&pGen->aGoto,nGotoOfft);` |
|       - |  5835 | `	/* Restore the default container */` |
|  183412 |  5836 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  5837 | `	/* Leave function block */` |
|  183412 |  5838 | `	GenStateLeaveBlock(&(*pGen),0);` |
|  183412 |  5839 | `	if( rc == SXERR_ABORT ){` |
|       - |  5840 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  5841 | `		return SXERR_ABORT;` |
|       - |  5842 | `	}` |
|       - |  5843 | `	/* Scan for yield opcodes to detect generator functions */` |
|       - |  5844 | `	{` |
|  183412 |  5845 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  5846 | `		sxu32 i;` |
| 3586384 |  5847 | `		for( i = 0; i < SySetUsed(&pFunc->aByteCode); i++ ){` |
| 3402992 |  5848 | `			if( aInstr[i].iOp == PH7_OP_YIELD ){` |
|      20 |  5849 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      20 |  5850 | `				break;` |
|       - |  5851 | `			}` |
| 1701488 |  5852 | `		}` |
|       - |  5853 | `	}` |
|       - |  5854 | `	/* All done, function body compiled */` |
|  183412 |  5855 | `	return SXRET_OK;` |
|   91707 |  5856 |  |
|       - |  5857 | `/*` |
|       - |  5858 | ` * Compile a PHP function whether is a Standard or Annonymous function.` |
|       - |  5859 | ` * According to the PHP language reference manual.` |
|       - |  5860 | ` *  Function names follow the same rules as other labels in PHP. A valid function name` |
|       - |  5861 | ` *  starts with a letter or underscore, followed by any number of letters, numbers, or` |
|       - |  5862 | ` *  underscores. As a regular expression, it would be expressed thus:` |
|       - |  5863 | ` *     [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  5864 | ` *  Functions need not be defined before they are referenced.` |
|       - |  5865 | ` *  All functions and classes in PHP have the global scope - they can be called outside` |
|       - |  5866 | ` *  a function even if they were defined inside and vice versa.` |
|       - |  5867 | ` *  It is possible to call recursive functions in PHP. However avoid recursive function/method` |
|       - |  5868 | ` *  calls with over 32-64 recursion levels.` |
|       - |  5869 | ` *` |
|       - |  5870 | ` * PH7 have introduced powerful extension including full type hinting, function overloading,` |
|       - |  5871 | ` * complex agrument values and more. Please refer to the official documentation for more information` |
|       - |  5872 | ` * on these extension.` |
|       - |  5873 | ` */` |
|       - |  5874 | `/*` |
|       - |  5875 | ` * Case-insensitive comparison for type names (PHP type names are case-insensitive).` |
|       - |  5876 | ` */` |
|      88 |  5877 | `static int SyMemcmpNoCase(const char *zA, const char *zB, sxu32 n)` |
|       2 |  5878 |  |
|       - |  5879 | `	sxu32 i;` |
|     274 |  5880 | `	for( i = 0; i < n; i++ ){` |
|     242 |  5881 | `		int a = zA[i], b = zB[i];` |
|     242 |  5882 | `		if( a >= 'A' && a <= 'Z' ) a += 0x20;` |
|     242 |  5883 | `		if( b >= 'A' && b <= 'Z' ) b += 0x20;` |
|     242 |  5884 | `		if( a != b ) return a - b;` |
|      94 |  5885 | `	}` |
|      34 |  5886 | `	return 0;` |
|      46 |  5887 |  |
|       - |  5888 | `/*` |
|       - |  5889 | ` * Internal type-atom kinds used during union type parsing.` |
|       - |  5890 | ` * Negative values are sentinels that never collide with MEMOBJ_* bitmasks` |
|       - |  5891 | ` * (which are positive bit values stored in sxu32).` |
|       - |  5892 | ` */` |
|       - |  5893 | ``#define UTA_NULL_FLAG  ((sxu32)0xFFFFFFF0)  /* the literal `null` keyword */`` |
|       - |  5894 | ``#define UTA_VOID_FLAG  ((sxu32)0xFFFFFFF1)  /* the `void` keyword */`` |
|       - |  5895 | ``#define UTA_NEVER_FLAG ((sxu32)0xFFFFFFF2)  /* the `never` keyword */`` |
|       - |  5896 |  |
|       - |  5897 | `/* Maximum number of alternatives in a single union type declaration.` |
|       - |  5898 | ` * Picked to be larger than any union type seen in real PHP codebases` |
|       - |  5899 | ` * (typical max is 4-6, with the largest internal PHP unions around 8).` |
|       - |  5900 | ` * The atom array lives on the parser stack, so the cost is bounded:` |
|       - |  5901 | ` * 32 * sizeof(PhlTypeAtom) ≈ 1 KiB. */` |
|       - |  5902 | `#define PHL_UNION_MAX_ALTS 32` |
|       - |  5903 |  |
|       - |  5904 | `typedef struct PhlTypeAtom PhlTypeAtom;` |
|       - |  5905 | `struct PhlTypeAtom {` |
|       - |  5906 | `	sxu32 nType;       /* MEMOBJ_*, SXU32_HIGH (class), or UTA_* sentinel */` |
|       - |  5907 | `	SyString sClass;   /* class name when nType == SXU32_HIGH */` |
|       - |  5908 | `	const char *zCanon;/* canonical lowercase name for scalar/builtin atoms */` |
|       - |  5909 | `	sxu32 nCanon;` |
|       - |  5910 | `};` |
|       - |  5911 |  |
|       - |  5912 | `/*` |
|       - |  5913 | ` * Parse a single type atom (one alternative of a union, or a complete` |
|       - |  5914 | `` * single type). Recognises scalar keywords, `array`, `object`, `null`,`` |
|       - |  5915 | `` * `void`, `never`, `self`, `parent`, and class names (possibly namespaced).`` |
|       - |  5916 | ` * pGen->pIn must point at the first token of the atom; on success it` |
|       - |  5917 | `` * is advanced past the atom. The previous nullable `?` prefix must`` |
|       - |  5918 | ` * already be consumed by the caller.` |
|       - |  5919 | ` */` |
|   62166 |  5920 | `static sxi32 GenStateParseOneTypeAtom(ph7_gen_state *pGen, PhlTypeAtom *pOut)` |
|       2 |  5921 |  |
|   62168 |  5922 | `	SyToken *pIn = pGen->pIn;` |
|   62168 |  5923 | `	SyZero(pOut, sizeof(*pOut));` |
|   62168 |  5924 | `	SyStringInitFromBuf(&pOut->sClass, 0, 0);` |
|   62168 |  5925 | `	if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5926 | `		return SXERR_SYNTAX;` |
|       - |  5927 | `	}` |
|       - |  5928 | `	/* Optional leading namespace separator '\' on FQN class types */` |
|   62168 |  5929 | `	if( pIn->nType & PH7_TK_NSSEP ){` |
|       8 |  5930 | `		pIn++;` |
|       8 |  5931 | `		if( pIn >= pGen->pEnd ){` |
|     ! 0 |  5932 | `			return SXERR_SYNTAX;` |
|       - |  5933 | `		}` |
|       3 |  5934 | `	}` |
|   62168 |  5935 | `	if( (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  5936 | `		return SXERR_SYNTAX;` |
|       - |  5937 | `	}` |
|   62168 |  5938 | `	if( pIn->nType & PH7_TK_KEYWORD ){` |
|   53274 |  5939 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pIn->pUserData));` |
|   53274 |  5940 | `		if( nKey & PH7_TKWRD_ARRAY ){` |
|      16 |  5941 | `			pOut->nType = MEMOBJ_HASHMAP; pOut->zCanon = "array"; pOut->nCanon = 5;` |
|   53267 |  5942 | `		}else if( nKey & PH7_TKWRD_BOOL ){` |
|      12 |  5943 | `			pOut->nType = MEMOBJ_BOOL; pOut->zCanon = "bool"; pOut->nCanon = 4;` |
|   53255 |  5944 | `		}else if( nKey & PH7_TKWRD_INT ){` |
|   14902 |  5945 | `			pOut->nType = MEMOBJ_INT; pOut->zCanon = "int"; pOut->nCanon = 3;` |
|   45800 |  5946 | `		}else if( nKey & PH7_TKWRD_STRING ){` |
|   38296 |  5947 | `			pOut->nType = MEMOBJ_STRING; pOut->zCanon = "string"; pOut->nCanon = 6;` |
|   19203 |  5948 | `		}else if( nKey & PH7_TKWRD_FLOAT ){` |
|      26 |  5949 | `			pOut->nType = MEMOBJ_REAL; pOut->zCanon = "float"; pOut->nCanon = 5;` |
|      44 |  5950 | `		}else if( nKey & PH7_TKWRD_OBJECT ){` |
|      26 |  5951 | `			pOut->nType = MEMOBJ_OBJ; pOut->zCanon = "object"; pOut->nCanon = 6;` |
|      20 |  5952 | `		}else if( nKey == PH7_TKWRD_SELF \|\| nKey == PH7_TKWRD_PARENT` |
|       4 |  5953 | `				\|\| nKey == PH7_TKWRD_STATIC ){` |
|       6 |  5954 | `			pOut->nType = SXU32_HIGH;` |
|       6 |  5955 | `			pOut->sClass = pIn->sData;` |
|       4 |  5956 | `		}else{` |
|       3 |  5957 | `			return SXERR_SYNTAX;` |
|       - |  5958 | `		}` |
|   53272 |  5959 | `		pIn++;` |
|   26637 |  5960 | `	}else{` |
|       - |  5961 | ``		/* Identifier — `null`, `void`, `never`, or class name (possibly`` |
|       - |  5962 | `		 * namespaced as a\b\c). Match the well-known names case-insensitively. */` |
|    8896 |  5963 | `		SyString *pT = &pIn->sData;` |
|    8896 |  5964 | `		if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "null", 4) == 0 ){` |
|      12 |  5965 | `			pOut->nType = UTA_NULL_FLAG; pOut->zCanon = "null"; pOut->nCanon = 4;` |
|      12 |  5966 | `			pIn++;` |
|    8891 |  5967 | `		}else if( pT->nByte == 4 && SyMemcmpNoCase(pT->zString, "void", 4) == 0 ){` |
|      18 |  5968 | `			pOut->nType = UTA_VOID_FLAG; pOut->zCanon = "void"; pOut->nCanon = 4;` |
|      18 |  5969 | `			pIn++;` |
|    8878 |  5970 | `		}else if( pT->nByte == 5 && SyMemcmpNoCase(pT->zString, "never", 5) == 0 ){` |
|       3 |  5971 | `			pOut->nType = UTA_NEVER_FLAG; pOut->zCanon = "never"; pOut->nCanon = 5;` |
|       3 |  5972 | `			pIn++;` |
|       2 |  5973 | `		}else{` |
|       - |  5974 | `			/* Class / interface name; consume namespace path a\b\c */` |
|    8868 |  5975 | `			SyToken *pFirst = pIn;` |
|    8868 |  5976 | `			SyToken *pLast = pIn;` |
|    8868 |  5977 | `			pOut->nType = SXU32_HIGH;` |
|    8868 |  5978 | `			pOut->sClass = pIn->sData;` |
|    8868 |  5979 | `			pIn++;` |
|   13302 |  5980 | `			while( pIn + 1 < pGen->pEnd && (pIn->nType & PH7_TK_NSSEP)` |
|    8871 |  5981 | `				&& (pIn[1].nType & PH7_TK_ID) ){` |
|       3 |  5982 | `				pLast = &pIn[1];` |
|       3 |  5983 | `				pIn += 2;` |
|       1 |  5984 | `			}` |
|    8868 |  5985 | `			if( pLast != pFirst ){` |
|       3 |  5986 | `				const char *zFirst = pFirst->sData.zString;` |
|       3 |  5987 | `				const char *zEnd = pLast->sData.zString + pLast->sData.nByte;` |
|       3 |  5988 | `				pOut->sClass.zString = zFirst;` |
|       3 |  5989 | `				pOut->sClass.nByte = (sxu32)(zEnd - zFirst);` |
|       1 |  5990 | `			}` |
|       - |  5991 | `		}` |
|       - |  5992 | `	}` |
|   62166 |  5993 | `	pGen->pIn = pIn;` |
|   62166 |  5994 | `	return SXRET_OK;` |
|   31085 |  5995 |  |
|       - |  5996 |  |
|       - |  5997 | `/*` |
|       - |  5998 | ` * Build the canonical PHP-formatted type text into pBlob from a list of` |
|       - |  5999 | `` * atoms. Order matches PHP's `zend_type` rendering:`` |
|       - |  6000 | ` *   classes (in declaration order) \| object \| array \| string \| int \| float \| bool [\| null]` |
|       - |  6001 | ` * If exactly one non-null atom is present and bNullable is true, the` |
|       - |  6002 | `` * shorthand `?T` form is emitted instead of `T\|null`.`` |
|       - |  6003 | ` */` |
|   62068 |  6004 | `static void GenBuildUnionTypeText(SyBlob *pBlob, PhlTypeAtom *aAtoms, int nAtoms, int bNullable)` |
|       2 |  6005 |  |
|       - |  6006 | `	int i;` |
|   62070 |  6007 | `	int nNonNull = 0;` |
|  124220 |  6008 | `	for( i = 0; i < nAtoms; i++ ){` |
|   62152 |  6009 | `		if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   62142 |  6010 | `			nNonNull++;` |
|   31070 |  6011 | `		}` |
|   31077 |  6012 | `	}` |
|   62070 |  6013 | `	if( nNonNull == 1 && bNullable ){` |
|       - |  6014 | `		/* Shorthand: ?T */` |
|      56 |  6015 | `		for( i = 0; i < nAtoms; i++ ){` |
|      56 |  6016 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|      56 |  6017 | `			SyBlobAppend(pBlob, "?", 1);` |
|      56 |  6018 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6019 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|       7 |  6020 | `			}else{` |
|      46 |  6021 | `				SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|       - |  6022 | `			}` |
|      56 |  6023 | `			return;` |
|     ! 0 |  6024 | `		}` |
|     ! 0 |  6025 | `	}` |
|       - |  6026 | `	{` |
|   62016 |  6027 | `		int bFirst = 1;` |
|       - |  6028 | `		/* 1) Classes in declaration order */` |
|  124106 |  6029 | `		for( i = 0; i < nAtoms; i++ ){` |
|   62092 |  6030 | `			if( aAtoms[i].nType == SXU32_HIGH ){` |
|    8862 |  6031 | `				if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|    8862 |  6032 | `				SyBlobAppend(pBlob, aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|    8862 |  6033 | `				bFirst = 0;` |
|    4430 |  6034 | `			}` |
|   31047 |  6035 | `		}` |
|       - |  6036 | `		/* 2) Built-ins in canonical order */` |
|       - |  6037 | `		{` |
|       - |  6038 | `			static const sxu32 aOrder[] = { MEMOBJ_OBJ, MEMOBJ_HASHMAP, MEMOBJ_STRING,` |
|       - |  6039 | `				MEMOBJ_INT, MEMOBJ_REAL, MEMOBJ_BOOL };` |
|       - |  6040 | `			int k;` |
|  434100 |  6041 | `			for( k = 0; k < (int)(sizeof(aOrder)/sizeof(aOrder[0])); k++ ){` |
|  691322 |  6042 | `				for( i = 0; i < nAtoms; i++ ){` |
|  372450 |  6043 | `					if( aAtoms[i].nType == aOrder[k] ){` |
|   53214 |  6044 | `						if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|   53214 |  6045 | `						SyBlobAppend(pBlob, aAtoms[i].zCanon, aAtoms[i].nCanon);` |
|   53214 |  6046 | `						bFirst = 0;` |
|   53214 |  6047 | `						break;` |
|       - |  6048 | `					}` |
|  159620 |  6049 | `				}` |
|  186044 |  6050 | `			}` |
|       - |  6051 | `		}` |
|       - |  6052 | `		/* 3) null suffix */` |
|   62016 |  6053 | `		if( bNullable ){` |
|       6 |  6054 | `			if( !bFirst ) SyBlobAppend(pBlob, "\|", 1);` |
|       6 |  6055 | `			SyBlobAppend(pBlob, "null", 4);` |
|       2 |  6056 | `		}` |
|       - |  6057 | `	}` |
|   31036 |  6058 |  |
|       - |  6059 |  |
|       - |  6060 | `/*` |
|       - |  6061 | ` * Parse an entire (possibly union) type declaration starting at pGen->pIn.` |
|       - |  6062 | ` *` |
|       - |  6063 | ` * Outputs:` |
|       - |  6064 | ` *   *pnType, *pClass — single-type fast path: filled when there is exactly` |
|       - |  6065 | ` *     one non-null atom AND no union flag is set. nType is MEMOBJ_*, or` |
|       - |  6066 | ` *     SXU32_HIGH for a class.  pClass receives the duplicated class name.` |
|       - |  6067 | ` *   *pAlts            — populated only when this is a true union (≥2` |
|       - |  6068 | ` *     non-null alternatives, OR ≥1 class+null union, etc). The set must` |
|       - |  6069 | ` *     already be initialized by the caller (allocator set, etc).` |
|       - |  6070 | ` *   *piTypeFlags      — receives PH7_CLASS_ATTR_NULLABLE / VM_FUNC_ARG_NULLABLE` |
|       - |  6071 | ` *     (caller maps), and PH7_CLASS_ATTR_UNION / VM_FUNC_ARG_UNION when union.` |
|       - |  6072 | ` *     The two flag values are passed in via iNullableFlag/iUnionFlag.` |
|       - |  6073 | ` *   *pTypeText        — duplicated canonical type text for error messages.` |
|       - |  6074 | ` *` |
|       - |  6075 | ` * Returns SXRET_OK on success, SXERR_SYNTAX on bad type syntax, or` |
|       - |  6076 | ` * SXERR_ABORT on fatal compile errors.` |
|       - |  6077 | ` */` |
|   62078 |  6078 | `static sxi32 GenStateParseUnionTypeDecl(` |
|       - |  6079 | `	ph7_gen_state *pGen,` |
|       - |  6080 | `	sxu32 *pnType,` |
|       - |  6081 | `	SyString *pClass,` |
|       - |  6082 | `	SySet *pAlts,` |
|       - |  6083 | `	sxi32 *piTypeFlags,` |
|       - |  6084 | `	SyString *pTypeText,` |
|       - |  6085 | `	int iNullableFlag,` |
|       - |  6086 | `	int iUnionFlag,` |
|       - |  6087 | `	int bAllowVoid,` |
|       - |  6088 | `	sxu32 nLine` |
|       2 |  6089 | `){` |
|       - |  6090 | `	PhlTypeAtom aAtoms[PHL_UNION_MAX_ALTS];` |
|   62080 |  6091 | `	int nAtoms = 0;` |
|   62080 |  6092 | `	int bShortNullable = 0;` |
|   62080 |  6093 | `	int bExplicitNull = 0;` |
|       - |  6094 | `	sxi32 rc;` |
|   62080 |  6095 | `	*pnType = 0;` |
|   62080 |  6096 | `	if( pClass ) SyStringInitFromBuf(pClass, 0, 0);` |
|   62080 |  6097 | `	*piTypeFlags = 0;` |
|   62080 |  6098 | `	if( pTypeText ) SyStringInitFromBuf(pTypeText, 0, 0);` |
|       - |  6099 |  |
|   62080 |  6100 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6101 | `		return SXRET_OK;` |
|       - |  6102 | `	}` |
|       - |  6103 | ``	/* Optional `?` shorthand prefix */`` |
|   62078 |  6104 | `	if( (pGen->pIn->nType & PH7_TK_OP) && pGen->pIn->sData.nByte == 1` |
|      52 |  6105 | `	 && pGen->pIn->sData.zString[0] == '?' ){` |
|      52 |  6106 | `		bShortNullable = 1;` |
|      52 |  6107 | `		pGen->pIn++;` |
|      52 |  6108 | `		if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6109 | `			return SXERR_SYNTAX;` |
|       - |  6110 | `		}` |
|      25 |  6111 | `	}` |
|       - |  6112 | `	/* First atom is mandatory */` |
|   62080 |  6113 | `	rc = GenStateParseOneTypeAtom(pGen, &aAtoms[0]);` |
|   62080 |  6114 | `	if( rc != SXRET_OK ){` |
|       3 |  6115 | `		return rc;` |
|       - |  6116 | `	}` |
|   62078 |  6117 | `	nAtoms = 1;` |
|       - |  6118 | ``	/* Subsequent atoms separated by `\|` */`` |
|   93248 |  6119 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP)` |
|   62212 |  6120 | `		&& pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      92 |  6121 | `		if( bShortNullable ){` |
|       - |  6122 | ``			/* Match PHP's wording — `?T\|X` is rejected as a parse error.`` |
|       - |  6123 | `			 * Return SXERR_CORRUPT as a sentinel meaning "syntax error` |
|       - |  6124 | `			 * already reported" so callers skip their own error emission. */` |
|       3 |  6125 | `			rc = PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6126 | `				"syntax error, unexpected token \"\|\", expecting variable");` |
|       3 |  6127 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_CORRUPT;` |
|       - |  6128 | `		}` |
|      90 |  6129 | `		if( nAtoms >= PHL_UNION_MAX_ALTS ){` |
|     ! 0 |  6130 | `			rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6131 | `				"Too many alternatives in union type (limit %d)", PHL_UNION_MAX_ALTS);` |
|     ! 0 |  6132 | `			return (rc == SXERR_ABORT) ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  6133 | `		}` |
|      90 |  6134 | ``		pGen->pIn++; /* skip `\|` */`` |
|      90 |  6135 | `		rc = GenStateParseOneTypeAtom(pGen, &aAtoms[nAtoms]);` |
|      90 |  6136 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  6137 | `			return rc;` |
|       - |  6138 | `		}` |
|      90 |  6139 | `		nAtoms++;` |
|       2 |  6140 | `	}` |
|       - |  6141 | `	/* Validation pass.` |
|       - |  6142 | `	 *` |
|       - |  6143 | `	 * Order matters: the union-membership checks for void/never run *before*` |
|       - |  6144 | ``	 * the duplicate scan, and `void` standalone-ness is checked *before* the`` |
|       - |  6145 | ``	 * `?void` check below — reordering them would let `?void` slip through.`` |
|       - |  6146 | `	 */` |
|       - |  6147 | `	{` |
|       - |  6148 | `		int i, j;` |
|   62076 |  6149 | `		int bHasNonNull = 0;` |
|  124232 |  6150 | `		for( i = 0; i < nAtoms; i++ ){` |
|   62164 |  6151 | `			if( aAtoms[i].nType == UTA_VOID_FLAG ){` |
|      18 |  6152 | `				if( nAtoms > 1 ){` |
|       3 |  6153 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6154 | `						"Void can only be used as a standalone type");` |
|       3 |  6155 | `					return SXERR_SYNTAX;` |
|       - |  6156 | `				}` |
|      16 |  6157 | `				if( !bAllowVoid ){` |
|     ! 0 |  6158 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6159 | `						"void cannot be used here");` |
|     ! 0 |  6160 | `					return SXERR_SYNTAX;` |
|       - |  6161 | `				}` |
|      16 |  6162 | `				if( bShortNullable ){` |
|     ! 0 |  6163 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6164 | `						"Void type cannot be nullable");` |
|     ! 0 |  6165 | `					return SXERR_SYNTAX;` |
|       - |  6166 | `				}` |
|       7 |  6167 | `			}` |
|   62162 |  6168 | `			if( aAtoms[i].nType == UTA_NEVER_FLAG ){` |
|       - |  6169 | ``				/* `never` is parsed but not yet implemented in the type`` |
|       - |  6170 | `				 * system. Reject it explicitly rather than silently aliasing` |
|       - |  6171 | ``				 * to `void` — the two have different semantics (never =`` |
|       - |  6172 | `				 * does not return), and folding them would mislead any` |
|       - |  6173 | `				 * future return-enforcement work. */` |
|       3 |  6174 | `				if( nAtoms > 1 ){` |
|       3 |  6175 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6176 | `						"never can only be used as a standalone type");` |
|       3 |  6177 | `					return SXERR_SYNTAX;` |
|       - |  6178 | `				}` |
|     ! 0 |  6179 | `				PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6180 | `					"never type is not yet implemented");` |
|     ! 0 |  6181 | `				return SXERR_SYNTAX;` |
|       - |  6182 | `			}` |
|   62160 |  6183 | `			if( aAtoms[i].nType == UTA_NULL_FLAG ){` |
|      12 |  6184 | `				bExplicitNull = 1;` |
|       7 |  6185 | `			}else{` |
|   62150 |  6186 | `				bHasNonNull = 1;` |
|       - |  6187 | `			}` |
|       - |  6188 | `			/* Duplicate detection */` |
|   62278 |  6189 | `			for( j = 0; j < i; j++ ){` |
|     122 |  6190 | `				int bDup = 0;` |
|     122 |  6191 | `				if( aAtoms[i].nType == aAtoms[j].nType ){` |
|      16 |  6192 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|      12 |  6193 | `						if( aAtoms[i].sClass.nByte == aAtoms[j].sClass.nByte` |
|      14 |  6194 | `						 && SyMemcmpNoCase(aAtoms[i].sClass.zString,` |
|       6 |  6195 | `								aAtoms[j].sClass.zString,` |
|      12 |  6196 | `								aAtoms[i].sClass.nByte) == 0 ){` |
|     ! 0 |  6197 | `							bDup = 1;` |
|     ! 0 |  6198 | `						}` |
|       8 |  6199 | `					}else{` |
|       3 |  6200 | `						bDup = 1;` |
|       - |  6201 | `					}` |
|       7 |  6202 | `				}` |
|     122 |  6203 | `				if( bDup ){` |
|       - |  6204 | `					const char *zName;` |
|       - |  6205 | `					sxu32 nName;` |
|       3 |  6206 | `					if( aAtoms[i].nType == SXU32_HIGH ){` |
|     ! 0 |  6207 | `						zName = aAtoms[i].sClass.zString;` |
|     ! 0 |  6208 | `						nName = aAtoms[i].sClass.nByte;` |
|     ! 0 |  6209 | `					}else{` |
|       3 |  6210 | `						zName = aAtoms[i].zCanon;` |
|       3 |  6211 | `						nName = aAtoms[i].nCanon;` |
|       - |  6212 | `					}` |
|       4 |  6213 | `					PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       1 |  6214 | `						"Duplicate type %.*s is redundant", (int)nName, zName);` |
|       3 |  6215 | `					return SXERR_SYNTAX;` |
|       - |  6216 | `				}` |
|      61 |  6217 | `			}` |
|   31080 |  6218 | `		}` |
|   62070 |  6219 | `		if( !bHasNonNull && bExplicitNull ){` |
|     ! 0 |  6220 | `			PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|       - |  6221 | `				"Null can not be used as a standalone type");` |
|     ! 0 |  6222 | `			return SXERR_SYNTAX;` |
|       - |  6223 | `		}` |
|       - |  6224 | `	}` |
|       - |  6225 | `	/* Compute nullability flag */` |
|   62070 |  6226 | `	if( bShortNullable \|\| bExplicitNull ){` |
|      60 |  6227 | `		*piTypeFlags \|= iNullableFlag;` |
|      29 |  6228 | `	}` |
|       - |  6229 | `	/* Build canonical type text */` |
|   62070 |  6230 | `	if( pTypeText ){` |
|       - |  6231 | `		SyBlob sBlob;` |
|   62070 |  6232 | `		SyBlobInit(&sBlob, &pGen->pVm->sAllocator);` |
|   93080 |  6233 | `		GenBuildUnionTypeText(&sBlob, aAtoms, nAtoms,` |
|   31034 |  6234 | `			(bShortNullable \|\| bExplicitNull) ? 1 : 0);` |
|   62070 |  6235 | `		if( SyBlobLength(&sBlob) > 0 ){` |
|   93083 |  6236 | `			char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   62054 |  6237 | `				(const char *)SyBlobData(&sBlob), SyBlobLength(&sBlob));` |
|   62056 |  6238 | `			if( zDup ){` |
|   62056 |  6239 | `				SyStringInitFromBuf(pTypeText, zDup, SyBlobLength(&sBlob));` |
|   31027 |  6240 | `			}` |
|   31027 |  6241 | `		}` |
|   62070 |  6242 | `		SyBlobRelease(&sBlob);` |
|   31034 |  6243 | `	}` |
|       - |  6244 | `	/* Decide single-type vs union storage. A "union" is anything with more` |
|       - |  6245 | `	 * than one non-null atom, OR a single class atom + null. Single scalar` |
|       - |  6246 | `	 * + null collapses to the existing nullable single-type fast path. */` |
|       - |  6247 | `	{` |
|   62070 |  6248 | `		int nNonNull = 0;` |
|   62070 |  6249 | `		int iNonNullIdx = -1;` |
|       - |  6250 | `		int i;` |
|  124220 |  6251 | `		for( i = 0; i < nAtoms; i++ ){` |
|   62152 |  6252 | `			if( aAtoms[i].nType != UTA_NULL_FLAG ){` |
|   62142 |  6253 | `				nNonNull++;` |
|   62142 |  6254 | `				iNonNullIdx = i;` |
|   31070 |  6255 | `			}` |
|   31077 |  6256 | `		}` |
|   62070 |  6257 | `		if( nNonNull <= 1 ){` |
|       - |  6258 | `			/* Fast path: store as single type. */` |
|   62014 |  6259 | `			if( iNonNullIdx >= 0 ){` |
|   62014 |  6260 | `				PhlTypeAtom *pA = &aAtoms[iNonNullIdx];` |
|   62014 |  6261 | `				if( pA->nType == SXU32_HIGH ){` |
|   13268 |  6262 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    4422 |  6263 | `						pA->sClass.zString, pA->sClass.nByte);` |
|    8846 |  6264 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|    8846 |  6265 | `					*pnType = SXU32_HIGH;` |
|    8846 |  6266 | `					if( pClass ) SyStringInitFromBuf(pClass, zDup, pA->sClass.nByte);` |
|   57592 |  6267 | `				}else if( pA->nType == UTA_VOID_FLAG ){` |
|      16 |  6268 | `					*pnType = MEMOBJ_VOID;` |
|       9 |  6269 | `				}else{` |
|       - |  6270 | `					/* UTA_NEVER_FLAG never reaches here — the validation` |
|       - |  6271 | `					 * pass above rejects it as not-yet-implemented. */` |
|   53156 |  6272 | `					*pnType = pA->nType;` |
|       - |  6273 | `				}` |
|   31006 |  6274 | `			}` |
|   31008 |  6275 | `		}else{` |
|       - |  6276 | `			/* True union — populate the alts set, leave *pnType = 0. */` |
|      58 |  6277 | `			*piTypeFlags \|= iUnionFlag;` |
|     190 |  6278 | `			for( i = 0; i < nAtoms; i++ ){` |
|       - |  6279 | `				ph7_type_alt sAlt;` |
|     134 |  6280 | `				if( aAtoms[i].nType == UTA_NULL_FLAG ) continue;` |
|     130 |  6281 | `				SyZero(&sAlt, sizeof(sAlt));` |
|     130 |  6282 | `				if( aAtoms[i].nType == SXU32_HIGH ){` |
|      41 |  6283 | `					char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      13 |  6284 | `						aAtoms[i].sClass.zString, aAtoms[i].sClass.nByte);` |
|      28 |  6285 | `					if( zDup == 0 ) return SXERR_ABORT;` |
|      28 |  6286 | `					sAlt.nType = SXU32_HIGH;` |
|      28 |  6287 | `					SyStringInitFromBuf(&sAlt.sClass, zDup, aAtoms[i].sClass.nByte);` |
|      15 |  6288 | `				}else{` |
|     104 |  6289 | `					sAlt.nType = aAtoms[i].nType;` |
|     104 |  6290 | `					SyStringInitFromBuf(&sAlt.sClass, 0, 0);` |
|       - |  6291 | `				}` |
|     130 |  6292 | `				SySetPut(pAlts, (const void *)&sAlt);` |
|      66 |  6293 | `			}` |
|       - |  6294 | `		}` |
|       - |  6295 | `	}` |
|   62070 |  6296 | `	return SXRET_OK;` |
|   31041 |  6297 |  |
|       - |  6298 |  |
|       - |  6299 | `/*` |
|       - |  6300 | `` * Parse a return type declaration (`: type`) after a function/method signature.`` |
|       - |  6301 | `` * pGen->pIn should point to the token after `)`.`` |
|       - |  6302 | ` * Sets pFunc->nReturnType and pFunc->sReturnClass.` |
|       - |  6303 | `` * Handles: `: int`, `: string`, `: bool`, `: float`, `: array`, `: void`,`` |
|       - |  6304 | `` *          `: self`, `: parent`, `: static`, `: ClassName`, nullable `: ?type`,`` |
|       - |  6305 | `` *          and union types `: T\|U`.`` |
|       - |  6306 | ` */` |
|  230530 |  6307 | `static sxi32 GenStateParseReturnType(ph7_gen_state *pGen, ph7_vm_func *pFunc)` |
|       2 |  6308 |  |
|  230532 |  6309 | `	sxi32 iFlags = 0;` |
|       - |  6310 | `	sxi32 rc;` |
|       - |  6311 | `	sxu32 nLine;` |
|  230532 |  6312 | `	pFunc->nReturnType = 0;` |
|  230532 |  6313 | `	SyStringInitFromBuf(&pFunc->sReturnClass, 0, 0);` |
|  230532 |  6314 | `	SyStringInitFromBuf(&pFunc->sReturnTypeName, 0, 0);` |
|  230532 |  6315 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COLON) == 0 ){` |
|  230408 |  6316 | `		return SXRET_OK;` |
|       - |  6317 | `	}` |
|     126 |  6318 | `	pGen->pIn++; /* Skip ':' */` |
|     126 |  6319 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6320 | `		return SXRET_OK;` |
|       - |  6321 | `	}` |
|     126 |  6322 | `	nLine = pGen->pIn->nLine;` |
|     126 |  6323 | `	rc = GenStateParseUnionTypeDecl(` |
|      62 |  6324 | `		pGen,` |
|      62 |  6325 | `		&pFunc->nReturnType,` |
|      62 |  6326 | `		&pFunc->sReturnClass,` |
|      62 |  6327 | `		&pFunc->aReturnUnion,` |
|       - |  6328 | `		&iFlags,` |
|      62 |  6329 | `		&pFunc->sReturnTypeName,` |
|       - |  6330 | `		/* iNullableFlag */ 0, /* nullability for returns rides on aReturnUnion contents only */` |
|       - |  6331 | `		/* iUnionFlag */ 0,` |
|       - |  6332 | `		/* bAllowVoid */ 1,` |
|      62 |  6333 | `		nLine);` |
|      62 |  6334 | `	(void)iFlags;` |
|     126 |  6335 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6336 | `		return SXERR_ABORT;` |
|       - |  6337 | `	}` |
|     126 |  6338 | `	if( rc == SXERR_CORRUPT ){` |
|       - |  6339 | `		/* Error already reported */` |
|     ! 0 |  6340 | `		return SXERR_SYNTAX;` |
|       - |  6341 | `	}` |
|     126 |  6342 | `	if( rc == SXERR_SYNTAX ){` |
|       5 |  6343 | `		if( pGen->pIn < pGen->pEnd ){` |
|       7 |  6344 | `			PH7_GenCompileError(pGen, E_PARSE, pGen->pIn->nLine,` |
|       - |  6345 | `				"syntax error, unexpected token \"%z\" in return type declaration",` |
|       4 |  6346 | `				&pGen->pIn->sData);` |
|       3 |  6347 | `		}else{` |
|     ! 0 |  6348 | `			PH7_GenCompileError(pGen, E_PARSE, nLine,` |
|       - |  6349 | `				"syntax error, unexpected end of file in return type declaration");` |
|       - |  6350 | `		}` |
|       5 |  6351 | `		return SXERR_SYNTAX;` |
|       - |  6352 | `	}` |
|     122 |  6353 | `	return SXRET_OK;` |
|  115267 |  6354 |  |
|       - |  6355 |  |
|   39046 |  6356 | `static sxi32 GenStateCompileFunc(` |
|       - |  6357 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  6358 | `	SyString *pName,     /* Function name. NULL otherwise */` |
|       - |  6359 | `	sxi32 iFlags,        /* Control flags */` |
|       - |  6360 | `	int bHandleClosure,  /* TRUE if we are dealing with a closure */` |
|       - |  6361 | `	ph7_vm_func **ppFunc /* OUT: function state */` |
|       - |  6362 | `	)` |
|       2 |  6363 |  |
|       - |  6364 | `	ph7_vm_func *pFunc;` |
|       - |  6365 | `	SyToken *pEnd;` |
|       - |  6366 | `	sxu32 nLine;` |
|       - |  6367 | `	char *zName;` |
|       - |  6368 | `	sxi32 rc;` |
|       - |  6369 | `	/* Extract line number */` |
|   39048 |  6370 | `	nLine = pGen->pIn->nLine;` |
|       - |  6371 | `	/* Jump the left parenthesis '(' */` |
|   39048 |  6372 | `	pGen->pIn++;` |
|       - |  6373 | `	/* Delimit the function signature */` |
|   39048 |  6374 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   39048 |  6375 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  6376 | `		/* Syntax error */` |
|       7 |  6377 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after function '%z' signature",pName);` |
|       7 |  6378 | `		if( rc == SXERR_ABORT ){` |
|       - |  6379 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6380 | `			return SXERR_ABORT;` |
|       - |  6381 | `		}` |
|       7 |  6382 | `		pGen->pIn = pGen->pEnd;` |
|       7 |  6383 | `		return SXRET_OK;` |
|       - |  6384 | `	}` |
|       - |  6385 | `	/* Create the function state */` |
|   39042 |  6386 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   39042 |  6387 | `	if( pFunc == 0 ){` |
|     ! 0 |  6388 | `		goto OutOfMem;` |
|       - |  6389 | `	}` |
|       - |  6390 | `	/* Build the function name, prepending namespace if active */` |
|   39049 |  6391 | `	if( SyBlobLength(&pGen->sNamespace) > 0 && !bHandleClosure ){` |
|       - |  6392 | `		SyBlob sFQN;` |
|       - |  6393 | `		sxu32 nLen;` |
|      16 |  6394 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      16 |  6395 | `		SyBlobAppend(&sFQN,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      16 |  6396 | `		SyBlobAppend(&sFQN,"\\",1);` |
|      16 |  6397 | `		SyBlobAppend(&sFQN,pName->zString,pName->nByte);` |
|      16 |  6398 | `		nLen = (sxu32)SyBlobLength(&sFQN);` |
|      16 |  6399 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,(const char *)SyBlobData(&sFQN),nLen);` |
|      16 |  6400 | `		SyBlobRelease(&sFQN);` |
|      16 |  6401 | `		if( zName == 0 ){` |
|     ! 0 |  6402 | `			goto OutOfMem;` |
|       - |  6403 | `		}` |
|      16 |  6404 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,nLen,iFlags,0);` |
|       9 |  6405 | `	}else{` |
|   39028 |  6406 | `		zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|   39028 |  6407 | `		if( zName == 0 ){` |
|     ! 0 |  6408 | `			goto OutOfMem;` |
|       - |  6409 | `		}` |
|   39028 |  6410 | `		PH7_VmInitFuncState(pGen->pVm,pFunc,zName,pName->nByte,iFlags,0);` |
|       - |  6411 | `	}` |
|   39042 |  6412 | `	if( pGen->pIn < pEnd ){` |
|       - |  6413 | `		/* Collect function arguments */` |
|   27090 |  6414 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pEnd,0,0);` |
|   27090 |  6415 | `		if( rc == SXERR_ABORT ){` |
|       - |  6416 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6417 | `			return SXERR_ABORT;` |
|       - |  6418 | `		}` |
|   13544 |  6419 | `	}` |
|       - |  6420 | `	/* Point past ')' and parse optional return type ': type' */` |
|   39042 |  6421 | `	pGen->pIn = &pEnd[1];` |
|       - |  6422 | `	{` |
|   39042 |  6423 | `		sxi32 rcRt = GenStateParseReturnType(pGen, pFunc);` |
|   39042 |  6424 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  6425 | `			return SXERR_ABORT;` |
|   39042 |  6426 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|       5 |  6427 | `			return SXERR_SYNTAX;` |
|       - |  6428 | `		}` |
|       - |  6429 | `	}` |
|   39038 |  6430 | `	if( bHandleClosure ){` |
|       - |  6431 | `		ph7_vm_func_closure_env sEnv;` |
|     178 |  6432 | `		int got_this = 0; /* TRUE if $this have been seen */` |
|     176 |  6433 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      97 |  6434 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_USE ){` |
|      16 |  6435 | `				sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - |  6436 | `				/* Closure,record environment variable */` |
|      16 |  6437 | `				pGen->pIn++;` |
|      16 |  6438 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  6439 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Closure: Unexpected token. Expecting a left parenthesis '('");` |
|     ! 0 |  6440 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  6441 | `						return SXERR_ABORT;` |
|       - |  6442 | `					}` |
|     ! 0 |  6443 | `				}` |
|      16 |  6444 | `				pGen->pIn++; /* Jump the left parenthesis or any other unexpected token */` |
|       - |  6445 | `				/* Compile until we hit the first closing parenthesis */` |
|      34 |  6446 | `				while( pGen->pIn < pGen->pEnd ){` |
|      34 |  6447 | `					int iFlagsLocal = 0;` |
|      34 |  6448 | `					if( pGen->pIn->nType & PH7_TK_RPAREN ){` |
|      16 |  6449 | `						pGen->pIn++; /* Jump the closing parenthesis */` |
|      16 |  6450 | `						break;` |
|       - |  6451 | `					}` |
|      20 |  6452 | `					nLineLocal = pGen->pIn->nLine;` |
|      20 |  6453 | `					if( pGen->pIn->nType & PH7_TK_AMPER ){` |
|       - |  6454 | `						/* Pass by reference,record that */` |
|     ! 0 |  6455 | `						PH7_GenCompileError(pGen,E_WARNING,nLineLocal,` |
|       - |  6456 | `							"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|       - |  6457 | `							);` |
|     ! 0 |  6458 | `						iFlagsLocal = VM_FUNC_ARG_BY_REF;` |
|     ! 0 |  6459 | `						pGen->pIn++;` |
|     ! 0 |  6460 | `					}` |
|      18 |  6461 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd` |
|      20 |  6462 | `						\|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6463 | `							rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|       - |  6464 | `								"Closure: Unexpected token. Expecting a variable name");` |
|     ! 0 |  6465 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  6466 | `								return SXERR_ABORT;` |
|       - |  6467 | `							}` |
|       - |  6468 | `							/* Find the closing parenthesis */` |
|     ! 0 |  6469 | `							while( (pGen->pIn < pGen->pEnd) && (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|     ! 0 |  6470 | `								pGen->pIn++;` |
|     ! 0 |  6471 | `							}` |
|     ! 0 |  6472 | `							if(pGen->pIn < pGen->pEnd){` |
|     ! 0 |  6473 | `								pGen->pIn++;` |
|     ! 0 |  6474 | `							}` |
|     ! 0 |  6475 | `							break;` |
|       - |  6476 | `							/* TICKET 1433-95: No need for the else block below.*/` |
|     ! 0 |  6477 | `					}else{` |
|       - |  6478 | `						SyString *pNameLocal;` |
|       - |  6479 | `						char *zDup;` |
|       - |  6480 | `						/* Duplicate variable name */` |
|      20 |  6481 | `						pNameLocal = &pGen->pIn[1].sData;` |
|      20 |  6482 | `						zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pNameLocal->zString,pNameLocal->nByte);` |
|      20 |  6483 | `						if( zDup ){` |
|       - |  6484 | `							/* Zero the structure */` |
|      20 |  6485 | `							SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      20 |  6486 | `							sEnv.iFlags = iFlagsLocal;` |
|      20 |  6487 | `							PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      20 |  6488 | `							SyStringInitFromBuf(&sEnv.sName,zDup,pNameLocal->nByte);` |
|      20 |  6489 | `							if( !got_this && pNameLocal->nByte == sizeof("this")-1 &&` |
|     ! 0 |  6490 | `								SyMemcmp((const void *)zDup,(const void *)"this",sizeof("this")-1) == 0 ){` |
|     ! 0 |  6491 | `									got_this = 1;` |
|     ! 0 |  6492 | `							}` |
|       - |  6493 | `							/* Save imported variable */` |
|      20 |  6494 | `							SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      11 |  6495 | `						}else{` |
|     ! 0 |  6496 | `							 PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6497 | `							 return SXERR_ABORT;` |
|       - |  6498 | `						}` |
|       - |  6499 | `					}` |
|      20 |  6500 | `					pGen->pIn += 2; /* $ + variable name or any other unexpected token */` |
|      26 |  6501 | `					while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6502 | `						/* Ignore trailing commas */` |
|       7 |  6503 | `						pGen->pIn++;` |
|       1 |  6504 | `					}` |
|       2 |  6505 | `				}` |
|      16 |  6506 | `				if( !got_this ){` |
|       - |  6507 | `					/* Make the $this variable [Current processed Object (class instance)]` |
|       - |  6508 | `					 * available to the closure environment.` |
|       - |  6509 | `					 */` |
|      16 |  6510 | `					SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      16 |  6511 | `					sEnv.iFlags = VM_FUNC_ARG_IGNORE; /* Do not install if NULL */` |
|      16 |  6512 | `					PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      16 |  6513 | `					SyStringInitFromBuf(&sEnv.sName,"this",sizeof("this")-1);` |
|      16 |  6514 | `					SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       7 |  6515 | `				}` |
|      16 |  6516 | `				if( SySetUsed(&pFunc->aClosureEnv) > 0 ){` |
|       - |  6517 | `					/* Mark as closure */` |
|      16 |  6518 | `					pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|       7 |  6519 | `				}` |
|       7 |  6520 | `		}` |
|      88 |  6521 | `	}` |
|       - |  6522 | `	/* Compile the body */` |
|   39038 |  6523 | `	rc = GenStateCompileFuncBody(&(*pGen),pFunc);` |
|   39038 |  6524 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6525 | `		return SXERR_ABORT;` |
|       - |  6526 | `	}` |
|   39038 |  6527 | `	if( ppFunc ){` |
|     178 |  6528 | `		*ppFunc = pFunc;` |
|      88 |  6529 | `	}` |
|   39038 |  6530 | `	rc = SXRET_OK;` |
|   39038 |  6531 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       - |  6532 | `		/* Finally register the function */` |
|   39024 |  6533 | `		rc = PH7_VmInstallUserFunction(pGen->pVm,pFunc,0);` |
|   19511 |  6534 | `	}` |
|   39038 |  6535 | `	if( rc == SXRET_OK ){` |
|   39038 |  6536 | `		return SXRET_OK;` |
|       - |  6537 | `	}` |
|       - |  6538 | `	/* Fall through if something goes wrong */` |
|     ! 0 |  6539 | `OutOfMem:` |
|       - |  6540 | `	/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|       - |  6541 | `	 * a tiny chunk of memory, there is no much we can do here.` |
|       - |  6542 | `	 */` |
|     ! 0 |  6543 | `	PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|     ! 0 |  6544 | `	return SXERR_ABORT;` |
|   19525 |  6545 |  |
|       - |  6546 | `/*` |
|       - |  6547 | ` * Compile a standard PHP function.` |
|       - |  6548 | ` *  Refer to the block-comment above for more information.` |
|       - |  6549 | ` */` |
|   38876 |  6550 | `static sxi32 PH7_CompileFunction(ph7_gen_state *pGen)` |
|       2 |  6551 |  |
|       - |  6552 | `	SyString *pName;` |
|       - |  6553 | `	sxi32 iFlags;` |
|       - |  6554 | `	sxu32 nLine;` |
|       - |  6555 | `	sxi32 rc;` |
|       - |  6556 |  |
|   38878 |  6557 | `	nLine = pGen->pIn->nLine;` |
|   38878 |  6558 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|   38878 |  6559 | `	iFlags = 0;` |
|   38878 |  6560 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  6561 | `		/* Return by reference,remember that */` |
|       7 |  6562 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  6563 | `		/* Jump the '&' token */` |
|       7 |  6564 | `		pGen->pIn++;` |
|       3 |  6565 | `	}` |
|   38878 |  6566 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  6567 | `		/* Invalid function name */` |
|       5 |  6568 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid function name");` |
|       5 |  6569 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6570 | `			return SXERR_ABORT;` |
|       - |  6571 | `		}` |
|       - |  6572 | `		/* Sychronize with the next semi-colon or braces*/` |
|      17 |  6573 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      13 |  6574 | `			pGen->pIn++;` |
|       1 |  6575 | `		}` |
|       5 |  6576 | `		return SXRET_OK;` |
|       - |  6577 | `	}` |
|   38874 |  6578 | `	pName = &pGen->pIn->sData;` |
|   38874 |  6579 | `	nLine = pGen->pIn->nLine;` |
|       - |  6580 | `	/* Jump the function name */` |
|   38874 |  6581 | `	pGen->pIn++;` |
|   38874 |  6582 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  6583 | `		/* Syntax error */` |
|       3 |  6584 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after function name '%z'",pName);` |
|       3 |  6585 | `		if( rc == SXERR_ABORT ){` |
|       - |  6586 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6587 | `			return SXERR_ABORT;` |
|       - |  6588 | `		}` |
|       - |  6589 | `		/* Sychronize with the next semi-colon or '{' */` |
|       3 |  6590 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|     ! 0 |  6591 | `			pGen->pIn++;` |
|     ! 0 |  6592 | `		}` |
|       3 |  6593 | `		return SXRET_OK;` |
|       - |  6594 | `	}` |
|       - |  6595 | `	/* Compile function body */` |
|   38872 |  6596 | `	rc = GenStateCompileFunc(&(*pGen),pName,iFlags,FALSE,0);` |
|   38872 |  6597 | `	return rc;` |
|   19440 |  6598 |  |
|       - |  6599 | `/*` |
|       - |  6600 | ` * Extract the visibility level associated with a given keyword.` |
|       - |  6601 | ` * According to the PHP language reference manual` |
|       - |  6602 | ` *  Visibility:` |
|       - |  6603 | ` *  The visibility of a property or method can be defined by prefixing` |
|       - |  6604 | ` *  the declaration with the keywords public, protected or private.` |
|       - |  6605 | ` *  Class members declared public can be accessed everywhere.` |
|       - |  6606 | ` *  Members declared protected can be accessed only within the class` |
|       - |  6607 | ` *  itself and by inherited and parent classes. Members declared as private` |
|       - |  6608 | ` *  may only be accessed by the class that defines the member.` |
|       - |  6609 | ` */` |
|  247654 |  6610 | `static sxi32 GetProtectionLevel(sxi32 nKeyword)` |
|       2 |  6611 |  |
|  247656 |  6612 | `	if( nKeyword == PH7_TKWRD_PRIVATE ){` |
|    8878 |  6613 | `		return PH7_CLASS_PROT_PRIVATE;` |
|  238780 |  6614 | `	}else if( nKeyword == PH7_TKWRD_PROTECTED ){` |
|   38212 |  6615 | `		return PH7_CLASS_PROT_PROTECTED;` |
|       - |  6616 | `	}` |
|       - |  6617 | `	/* Assume public by default */` |
|  200570 |  6618 | `	return PH7_CLASS_PROT_PUBLIC;` |
|  123829 |  6619 |  |
|       - |  6620 | `/*` |
|       - |  6621 | ` * Compile a class constant.` |
|       - |  6622 | ` * According to the PHP language reference manual` |
|       - |  6623 | ` *  Class Constants` |
|       - |  6624 | ` *   It is possible to define constant values on a per-class basis remaining` |
|       - |  6625 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|       - |  6626 | ` *   you don't use the $ symbol to declare or use them.` |
|       - |  6627 | ` *   The value must be a constant expression, not (for example) a variable,` |
|       - |  6628 | ` *   a property, a result of a mathematical operation, or a function call.` |
|       - |  6629 | ` *   It's also possible for interfaces to have constants.` |
|       - |  6630 | ` * Symisc eXtension.` |
|       - |  6631 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|       - |  6632 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6633 | ` *  Example:` |
|       - |  6634 | ` *   class Test{` |
|       - |  6635 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6636 | ` *   };` |
|       - |  6637 | ` *   var_dump(TEST::MyConst);` |
|       - |  6638 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6639 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6640 | ` */` |
|      30 |  6641 | `static sxi32 GenStateCompileClassConstant(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6642 |  |
|      32 |  6643 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6644 | `	SySet *pInstrContainer;` |
|       - |  6645 | `	ph7_class_attr *pCons;` |
|       - |  6646 | `	SyString *pName;` |
|       - |  6647 | `	sxi32 rc;` |
|       - |  6648 | `	/* Extract visibility level */` |
|      32 |  6649 | `	iProtection = GetProtectionLevel(iProtection);` |
|      32 |  6650 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|      15 |  6651 | `loop:` |
|       - |  6652 | `	/* Mark as constant */` |
|      32 |  6653 | `	iFlags \|= PH7_CLASS_ATTR_CONSTANT;` |
|      32 |  6654 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  6655 | `		/* Invalid constant name */` |
|     ! 0 |  6656 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid constant name");` |
|     ! 0 |  6657 | `		if( rc == SXERR_ABORT ){` |
|       - |  6658 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6659 | `			return SXERR_ABORT;` |
|       - |  6660 | `		}` |
|     ! 0 |  6661 | `		goto Synchronize;` |
|       - |  6662 | `	}` |
|       - |  6663 | `	/* Peek constant name */` |
|      32 |  6664 | `	pName = &pGen->pIn->sData;` |
|       - |  6665 | `	/* Make sure the constant name isn't reserved */` |
|      32 |  6666 | `	if( GenStateIsReservedConstant(pName) ){` |
|       - |  6667 | `		/* Reserved constant name */` |
|     ! 0 |  6668 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Cannot redeclare a reserved constant '%z'",pName);` |
|     ! 0 |  6669 | `		if( rc == SXERR_ABORT ){` |
|       - |  6670 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6671 | `			return SXERR_ABORT;` |
|       - |  6672 | `		}` |
|     ! 0 |  6673 | `		goto Synchronize;` |
|       - |  6674 | `	}` |
|       - |  6675 | `	/* Advance the stream cursor */` |
|      32 |  6676 | `	pGen->pIn++;` |
|      32 |  6677 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|       - |  6678 | `		/* Invalid declaration */` |
|     ! 0 |  6679 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' after class constant %z'",pName);` |
|     ! 0 |  6680 | `		if( rc == SXERR_ABORT ){` |
|       - |  6681 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  6682 | `			return SXERR_ABORT;` |
|       - |  6683 | `		}` |
|     ! 0 |  6684 | `		goto Synchronize;` |
|       - |  6685 | `	}` |
|      32 |  6686 | `	pGen->pIn++; /* Jump the equal sign */` |
|       - |  6687 | `	/* Allocate a new class attribute */` |
|      32 |  6688 | `	pCons = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags);` |
|      32 |  6689 | `	if( pCons == 0 ){` |
|     ! 0 |  6690 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6691 | `		return SXERR_ABORT;` |
|       - |  6692 | `	}` |
|       - |  6693 | `	/* Swap bytecode container */` |
|      32 |  6694 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  6695 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pCons->aByteCode);` |
|       - |  6696 | `	/* Compile constant value.` |
|       - |  6697 | `	 */` |
|      32 |  6698 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      32 |  6699 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  6700 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Empty constant '%z' value",pName);` |
|       3 |  6701 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  6702 | `			return SXERR_ABORT;` |
|       - |  6703 | `		}` |
|       1 |  6704 | `	}` |
|       - |  6705 | `	/* Emit the done instruction */` |
|      32 |  6706 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      32 |  6707 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  6708 | `	if( rc == SXERR_ABORT ){` |
|       - |  6709 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|     ! 0 |  6710 | `		return SXERR_ABORT;` |
|       - |  6711 | `	}` |
|       - |  6712 | `	/* All done,install the constant */` |
|      32 |  6713 | `	rc = PH7_ClassInstallAttr(pClass,pCons);` |
|      32 |  6714 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6715 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  6716 | `		return SXERR_ABORT;` |
|       - |  6717 | `	}` |
|      32 |  6718 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  6719 | `		/* Multiple constants declarations [i.e: const min=-1,max = 10] */` |
|     ! 0 |  6720 | `		pGen->pIn++; /* Jump the comma */` |
|     ! 0 |  6721 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  6722 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  6723 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  6724 | `				pTok--;` |
|     ! 0 |  6725 | `			}` |
|     ! 0 |  6726 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  6727 | `				"Unexpected token '%z',expecting constant declaration inside class '%z'",` |
|     ! 0 |  6728 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  6729 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6730 | `				return SXERR_ABORT;` |
|       - |  6731 | `			}` |
|     ! 0 |  6732 | `		}else{` |
|     ! 0 |  6733 | `			if( pGen->pIn->nType & PH7_TK_ID ){` |
|     ! 0 |  6734 | `				goto loop;` |
|       - |  6735 | `			}` |
|       - |  6736 | `		}` |
|     ! 0 |  6737 | `	}` |
|      32 |  6738 | `	return SXRET_OK;` |
|     ! 0 |  6739 | `Synchronize:` |
|       - |  6740 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  6741 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|     ! 0 |  6742 | `		pGen->pIn++;` |
|     ! 0 |  6743 | `	}` |
|     ! 0 |  6744 | `	return SXERR_CORRUPT;` |
|      17 |  6745 |  |
|       - |  6746 | `/*` |
|       - |  6747 | ` * complie a class attribute or Properties in the PHP jargon.` |
|       - |  6748 | ` * According to the PHP language reference manual` |
|       - |  6749 | ` *  Properties` |
|       - |  6750 | ` *  Class member variables are called "properties". You may also see them referred` |
|       - |  6751 | ` *  to using other terms such as "attributes" or "fields", but for the purposes` |
|       - |  6752 | ` *  of this reference we will use "properties". They are defined by using one` |
|       - |  6753 | ` *  of the keywords public, protected, or private, followed by a normal variable` |
|       - |  6754 | ` *  declaration. This declaration may include an initialization, but this initialization` |
|       - |  6755 | ` *  must be a constant value--that is, it must be able to be evaluated at compile time` |
|       - |  6756 | ` *  and must not depend on run-time information in order to be evaluated.` |
|       - |  6757 | ` * Symisc eXtension.` |
|       - |  6758 | ` *  PH7 allow any complex expression to be associated with the attribute while` |
|       - |  6759 | ` *  the zend engine would allow only simple scalar value.` |
|       - |  6760 | ` *  Example:` |
|       - |  6761 | ` *   class Test{` |
|       - |  6762 | ` *        public static $myVar = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|       - |  6763 | ` *   };` |
|       - |  6764 | ` *   var_dump(TEST::myVar);` |
|       - |  6765 | ` *   Refer to the official documentation for more information on the powerful extension` |
|       - |  6766 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|       - |  6767 | ` */` |
|       - |  6768 | `/*` |
|       - |  6769 | ` * Lookahead: return TRUE if the tokens starting at pStart look like a typed` |
|       - |  6770 | ` * property declaration — i.e. an optional '?', optional '\', one or more` |
|       - |  6771 | ` * ID/keyword tokens (possibly separated by '\' for namespace paths), followed` |
|       - |  6772 | ` * by a '$'. This is used by the class-body dispatcher to decide whether to` |
|       - |  6773 | ` * route into the typed-attribute path vs. fall through to method/const/etc.` |
|       - |  6774 | ` */` |
|  144466 |  6775 | `static int GenStateLooksLikeTypedProperty(SyToken *pStart,SyToken *pEnd)` |
|       2 |  6776 |  |
|  144468 |  6777 | `	SyToken *p = pStart;` |
|  144468 |  6778 | `	if( p >= pEnd ) return 0;` |
|  144468 |  6779 | `	if( (p->nType & PH7_TK_OP) && p->sData.nByte == 1 && p->sData.zString[0] == '?' ){` |
|      16 |  6780 | `		p++;` |
|      16 |  6781 | `		if( p >= pEnd ) return 0;` |
|       7 |  6782 | `	}` |
|  144468 |  6783 | `	if( p->nType & PH7_TK_NSSEP ){` |
|       3 |  6784 | `		p++;` |
|       3 |  6785 | `		if( p >= pEnd ) return 0;` |
|       1 |  6786 | `	}` |
|  144468 |  6787 | `	if( (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  6788 | `		return 0;` |
|       - |  6789 | `	}` |
|       - |  6790 | `	/* Reject class-body modifier keywords that aren't types. Visibility` |
|       - |  6791 | `	 * (public/private/protected) has already been consumed by the caller,` |
|       - |  6792 | `	 * but static/final/abstract may still appear here for the initial` |
|       - |  6793 | `	 * dispatch site. */` |
|  144468 |  6794 | `	if( p->nType & PH7_TK_KEYWORD ){` |
|  144450 |  6795 | `		sxu32 k = (sxu32)(SX_PTR_TO_INT(p->pUserData));` |
|  144497 |  6796 | `		if( k == PH7_TKWRD_FUNCTION \|\| k == PH7_TKWRD_VAR \|\| k == PH7_TKWRD_CONST` |
|    3087 |  6797 | `		 \|\| k == PH7_TKWRD_STATIC \|\| k == PH7_TKWRD_FINAL \|\| k == PH7_TKWRD_ABSTRACT ){` |
|  144352 |  6798 | `			return 0;` |
|       - |  6799 | `		}` |
|      49 |  6800 | `	}` |
|     118 |  6801 | `	p++;` |
|       - |  6802 | `	/* Consume optional namespace path */` |
|     120 |  6803 | `	while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       3 |  6804 | `		p += 2;` |
|       1 |  6805 | `	}` |
|       - |  6806 | ``	/* Consume any `\| Type` union alternatives */`` |
|     192 |  6807 | `	while( p < pEnd && (p->nType & PH7_TK_OP) && p->sData.nByte == 1` |
|      78 |  6808 | `		&& p->sData.zString[0] == '\|' ){` |
|      14 |  6809 | `		p++;` |
|      14 |  6810 | `		if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){ p++; }` |
|      14 |  6811 | `		if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ) return 0;` |
|      14 |  6812 | `		p++;` |
|      14 |  6813 | `		while( p + 1 < pEnd && (p->nType & PH7_TK_NSSEP) && (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     ! 0 |  6814 | `			p += 2;` |
|     ! 0 |  6815 | `		}` |
|       2 |  6816 | `	}` |
|     118 |  6817 | `	if( p >= pEnd ) return 0;` |
|     118 |  6818 | `	return (p->nType & PH7_TK_DOLLAR) ? 1 : 0;` |
|   72235 |  6819 |  |
|       - |  6820 |  |
|       - |  6821 | `/*` |
|       - |  6822 | ` * Parse an optional property type hint starting at pGen->pIn. On return,` |
|       - |  6823 | ` * pGen->pIn points at the '$' token if a type was present (or is unchanged` |
|       - |  6824 | ` * if not). Recognized forms:` |
|       - |  6825 | ` *   ?Type, array, bool, int, float, string, object,` |
|       - |  6826 | ` *   self, parent, \Ns\ClassName, ClassName` |
|       - |  6827 | ` * The 'iterable' pseudo-type is not yet supported and is rejected earlier` |
|       - |  6828 | ` * by GenStateCompileClassAttr along with void/never/mixed/callable.` |
|       - |  6829 | ` * Returns SXRET_OK on successful parse (type or no type), SXERR_SYNTAX` |
|       - |  6830 | ` * on unrecoverable error.` |
|       - |  6831 | ` *` |
|       - |  6832 | ` * When a type is parsed:` |
|       - |  6833 | ` *   *pnType is set to MEMOBJ_* (or SXU32_HIGH for class types)` |
|       - |  6834 | ` *   *pClass is set to the class name (for class types)` |
|       - |  6835 | ` *   *piTypeFlags receives PH7_CLASS_ATTR_TYPED and optionally NULLABLE` |
|       - |  6836 | ` *   *pTypeText is set to the original text span of the type` |
|       - |  6837 | ` * Otherwise they are left unchanged (so multi-decl reuse works).` |
|       - |  6838 | ` */` |
|     116 |  6839 | `static sxi32 GenStateParsePropertyType(` |
|       - |  6840 | `	ph7_gen_state *pGen,` |
|       - |  6841 | `	sxu32 *pnType,` |
|       - |  6842 | `	SyString *pClass,` |
|       - |  6843 | `	sxi32 *piTypeFlags,` |
|       - |  6844 | `	SyString *pTypeText,` |
|       - |  6845 | `	SySet *pAlts` |
|       2 |  6846 | `){` |
|     118 |  6847 | `	sxi32 iFlags = 0;` |
|       - |  6848 | `	sxi32 rc;` |
|     118 |  6849 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  6850 | `		return SXRET_OK;` |
|       - |  6851 | `	}` |
|       - |  6852 | `	/* If the first token is '$', there's no type */` |
|     118 |  6853 | `	if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|     ! 0 |  6854 | `		return SXRET_OK;` |
|       - |  6855 | `	}` |
|     118 |  6856 | `	rc = GenStateParseUnionTypeDecl(` |
|      58 |  6857 | `		pGen, pnType, pClass, pAlts, &iFlags, pTypeText,` |
|       - |  6858 | `		PH7_CLASS_ATTR_NULLABLE,` |
|       - |  6859 | `		PH7_CLASS_ATTR_UNION,` |
|       - |  6860 | `		/* bAllowVoid */ 0,` |
|     116 |  6861 | `		pGen->pIn->nLine);` |
|     118 |  6862 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  6863 | `		return rc;` |
|       - |  6864 | `	}` |
|       - |  6865 | `	/* Verify next token is '$' (start of property name) */` |
|     118 |  6866 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  6867 | `		return SXERR_SYNTAX;` |
|       - |  6868 | `	}` |
|     118 |  6869 | `	*piTypeFlags = iFlags \| PH7_CLASS_ATTR_TYPED;` |
|     118 |  6870 | `	return SXRET_OK;` |
|      60 |  6871 |  |
|       - |  6872 |  |
|       - |  6873 | `/*` |
|       - |  6874 | ` * Return TRUE if a parsed type atom — identified by (nType, sClass) as` |
|       - |  6875 | ` * produced by GenStateParseUnionTypeDecl — names a pseudo-type that PHP` |
|       - |  6876 | `` * forbids on properties. `callable`, `mixed`, and `iterable` are parsed`` |
|       - |  6877 | ` * as class-name atoms (SXU32_HIGH, sClass = the keyword) because they` |
|       - |  6878 | `` * are not recognized scalar keywords; `void` and `never` are rejected`` |
|       - |  6879 | ` * by the type parser itself before reaching here.` |
|       - |  6880 | ` *` |
|       - |  6881 | ` * On TRUE, *pzName / *pnName point at a static canonical spelling for` |
|       - |  6882 | ` * use in the error message.` |
|       - |  6883 | ` */` |
|     182 |  6884 | `static int GenStateIsDisallowedPropertyAtom(` |
|       - |  6885 | `	sxu32 nType,` |
|       - |  6886 | `	const SyString *pClass,` |
|       - |  6887 | `	const char **pzName,` |
|       - |  6888 | `	sxu32 *pnName)` |
|       2 |  6889 |  |
|       - |  6890 | `	const char *z;` |
|       - |  6891 | `	sxu32 n;` |
|     184 |  6892 | `	if( nType != SXU32_HIGH \|\| pClass == 0 \|\| pClass->nByte == 0 ){` |
|     154 |  6893 | `		return 0;` |
|       - |  6894 | `	}` |
|      32 |  6895 | `	z = pClass->zString;` |
|      32 |  6896 | `	n = pClass->nByte;` |
|      32 |  6897 | `	if( n == 8 && SyMemcmpNoCase(z,"callable",8) == 0 ){` |
|       5 |  6898 | `		*pzName = "callable"; *pnName = 8; return 1;` |
|       - |  6899 | `	}` |
|      28 |  6900 | `	if( n == 5 && SyMemcmpNoCase(z,"mixed",5) == 0 ){` |
|     ! 0 |  6901 | `		*pzName = "mixed"; *pnName = 5; return 1;` |
|       - |  6902 | `	}` |
|      28 |  6903 | `	if( n == 8 && SyMemcmpNoCase(z,"iterable",8) == 0 ){` |
|     ! 0 |  6904 | `		*pzName = "iterable"; *pnName = 8; return 1;` |
|       - |  6905 | `	}` |
|      28 |  6906 | `	return 0;` |
|      93 |  6907 |  |
|       - |  6908 |  |
|       - |  6909 | `/*` |
|       - |  6910 | ` * Validate a parsed property type (main atom + any union alternatives)` |
|       - |  6911 | ` * against the disallowed-pseudo-types list. Emits a PHP-compatible` |
|       - |  6912 | ` * "Property C::$x cannot have type T" error on rejection, where T is` |
|       - |  6913 | ` * the full canonical type text (matching PHP's error wording for` |
|       - |  6914 | `` * unions like `callable\|int`).`` |
|       - |  6915 | ` *` |
|       - |  6916 | ` * Returns SXRET_OK if the type is acceptable, SXERR_SYNTAX on rejection` |
|       - |  6917 | ` * (error already emitted), or SXERR_ABORT on error-count overflow.` |
|       - |  6918 | ` */` |
|     154 |  6919 | `static sxi32 GenStateValidatePropertyType(` |
|       - |  6920 | `	ph7_gen_state *pGen,` |
|       - |  6921 | `	ph7_class *pClass,` |
|       - |  6922 | `	const SyString *pPropName,` |
|       - |  6923 | `	sxu32 nType,` |
|       - |  6924 | `	const SyString *pTypeClass,` |
|       - |  6925 | `	const SyString *pTypeText,` |
|       - |  6926 | `	SySet *pUnionAlts,` |
|       - |  6927 | `	sxu32 nLine)` |
|       2 |  6928 |  |
|     156 |  6929 | `	const char *zBad = 0;` |
|     156 |  6930 | `	sxu32 nBad = 0;` |
|       - |  6931 | `	SyString sFallback;` |
|       - |  6932 | `	const SyString *pBad;` |
|       - |  6933 | `	sxi32 rc;` |
|     156 |  6934 | `	int bDisallowed = 0;` |
|     156 |  6935 | `	if( GenStateIsDisallowedPropertyAtom(nType,pTypeClass,&zBad,&nBad) ){` |
|       3 |  6936 | `		bDisallowed = 1;` |
|     155 |  6937 | `	}else if( pUnionAlts ){` |
|       - |  6938 | `		sxu32 i;` |
|      42 |  6939 | `		for( i = 0; i < SySetUsed(pUnionAlts); i++ ){` |
|      30 |  6940 | `			ph7_type_alt *pAlt = (ph7_type_alt *)SySetAt(pUnionAlts,i);` |
|      30 |  6941 | `			if( GenStateIsDisallowedPropertyAtom(pAlt->nType,&pAlt->sClass,&zBad,&nBad) ){` |
|       3 |  6942 | `				bDisallowed = 1;` |
|       3 |  6943 | `				break;` |
|       - |  6944 | `			}` |
|      15 |  6945 | `		}` |
|       7 |  6946 | `	}` |
|     156 |  6947 | `	if( !bDisallowed ){` |
|     152 |  6948 | `		return SXRET_OK;` |
|       - |  6949 | `	}` |
|       - |  6950 | ``	/* Prefer the full canonical type text (PHP prints `callable\|int` for`` |
|       - |  6951 | `	 * a union, not just the offending atom). Fall back to the atom's own` |
|       - |  6952 | `	 * canonical spelling if the type text is unavailable. */` |
|       5 |  6953 | `	if( pTypeText && SyStringLength(pTypeText) > 0 ){` |
|       5 |  6954 | `		pBad = pTypeText;` |
|       3 |  6955 | `	}else{` |
|     ! 0 |  6956 | `		SyStringInitFromBuf(&sFallback,zBad,nBad);` |
|     ! 0 |  6957 | `		pBad = &sFallback;` |
|       - |  6958 | `	}` |
|       7 |  6959 | `	rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6960 | `		"Property %z::$%z cannot have type %z",` |
|       2 |  6961 | `		&pClass->sName,pPropName,pBad);` |
|       5 |  6962 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  6963 | `		return SXERR_ABORT;` |
|       - |  6964 | `	}` |
|       5 |  6965 | `	return SXERR_SYNTAX;` |
|      79 |  6966 |  |
|       - |  6967 |  |
|   56214 |  6968 | `static sxi32 GenStateCompileClassAttr(ph7_gen_state *pGen,sxi32 iProtection,sxi32 iFlags,ph7_class *pClass)` |
|       2 |  6969 |  |
|   56216 |  6970 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  6971 | `	ph7_class_attr *pAttr;` |
|       - |  6972 | `	SyString *pName;` |
|       - |  6973 | `	sxi32 rc;` |
|   56216 |  6974 | `	sxu32 nType = 0;` |
|       - |  6975 | `	SyString sTypeClass;` |
|       - |  6976 | `	SyString sTypeText;` |
|       - |  6977 | `	SySet aUnionAlts;` |
|   56216 |  6978 | `	sxi32 iTypeFlags = 0;` |
|   56216 |  6979 | `	SyStringInitFromBuf(&sTypeClass,0,0);` |
|   56216 |  6980 | `	SyStringInitFromBuf(&sTypeText,0,0);` |
|   56216 |  6981 | `	SySetInit(&aUnionAlts,&pGen->pVm->sAllocator,sizeof(ph7_type_alt));` |
|       - |  6982 | `	/* Extract visibility level */` |
|   56216 |  6983 | `	iProtection = GetProtectionLevel(iProtection);` |
|       - |  6984 | `	/* Parse optional type hint (typed properties, PHP 7.4+) */` |
|   56274 |  6985 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     118 |  6986 | `		rc = GenStateParsePropertyType(pGen,&nType,&sTypeClass,&iTypeFlags,&sTypeText,&aUnionAlts);` |
|     118 |  6987 | `		if( rc == SXERR_CORRUPT ){` |
|       - |  6988 | `			/* Error already reported by GenStateParseUnionTypeDecl */` |
|     ! 0 |  6989 | `			goto Synchronize;` |
|     118 |  6990 | `		}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  6991 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  6992 | `				"Invalid property type or declaration near '%z'",` |
|     ! 0 |  6993 | `				&pGen->pIn->sData);` |
|     ! 0 |  6994 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  6995 | `				return SXERR_ABORT;` |
|       - |  6996 | `			}` |
|     ! 0 |  6997 | `			goto Synchronize;` |
|     118 |  6998 | `		}else if( rc == SXERR_ABORT ){` |
|     ! 0 |  6999 | `			return SXERR_ABORT;` |
|       - |  7000 | `		}` |
|      58 |  7001 | `	}` |
|     ! 0 |  7002 | `loop:` |
|   56220 |  7003 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  7004 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '$' at start of property name");` |
|     ! 0 |  7005 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7006 | `			return SXERR_ABORT;` |
|       - |  7007 | `		}` |
|     ! 0 |  7008 | `		goto Synchronize;` |
|       - |  7009 | `	}` |
|   56220 |  7010 | `	pGen->pIn++; /* Jump the dollar sign */` |
|   56220 |  7011 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|       - |  7012 | `		/* Invalid attribute name */` |
|     ! 0 |  7013 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid attribute name");` |
|     ! 0 |  7014 | `		if( rc == SXERR_ABORT ){` |
|       - |  7015 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7016 | `			return SXERR_ABORT;` |
|       - |  7017 | `		}` |
|     ! 0 |  7018 | `		goto Synchronize;` |
|       - |  7019 | `	}` |
|       - |  7020 | `	/* Peek attribute name */` |
|   56220 |  7021 | `	pName = &pGen->pIn->sData;` |
|       - |  7022 | `	/* Advance the stream cursor */` |
|   56220 |  7023 | `	pGen->pIn++;` |
|   56220 |  7024 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_EQUAL/*'='*/\|PH7_TK_SEMI/*';'*/\|PH7_TK_COMMA/*','*/)) == 0 ){` |
|       - |  7025 | `		/* Invalid declaration */` |
|       3 |  7026 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '=' or ';' after attribute name '%z'",pName);` |
|       3 |  7027 | `		if( rc == SXERR_ABORT ){` |
|       - |  7028 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7029 | `			return SXERR_ABORT;` |
|       - |  7030 | `		}` |
|       3 |  7031 | `		goto Synchronize;` |
|       - |  7032 | `	}` |
|       - |  7033 | `	/* Reject disallowed pseudo-types (callable/mixed/iterable) on the main` |
|       - |  7034 | `	 * type atom or any union alternative. void/never are already rejected` |
|       - |  7035 | `	 * by the type parser. */` |
|   56218 |  7036 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     182 |  7037 | `		rc = GenStateValidatePropertyType(pGen,pClass,pName,nType,&sTypeClass,` |
|       - |  7038 | `			&sTypeText,` |
|     120 |  7039 | `			(iTypeFlags & PH7_CLASS_ATTR_UNION) ? &aUnionAlts : 0,nLine);` |
|     122 |  7040 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7041 | `			return SXERR_ABORT;` |
|     122 |  7042 | `		}else if( rc != SXRET_OK ){` |
|     ! 0 |  7043 | `			goto Synchronize;` |
|       - |  7044 | `		}` |
|      60 |  7045 | `	}` |
|       - |  7046 | `	/* Reject redeclaration (catches clash with an earlier promoted property). */` |
|   56218 |  7047 | `	if( PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte) != 0 ){` |
|       4 |  7048 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7049 | `			"Cannot redeclare %z::$%z",&pClass->sName,pName);` |
|       3 |  7050 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7051 | `			return SXERR_ABORT;` |
|       - |  7052 | `		}` |
|       3 |  7053 | `		goto Synchronize;` |
|       - |  7054 | `	}` |
|       - |  7055 | `	/* Allocate a new class attribute */` |
|   56216 |  7056 | `	pAttr = PH7_NewClassAttr(pGen->pVm,pName,nLine,iProtection,iFlags\|iTypeFlags);` |
|   56216 |  7057 | `	if( pAttr == 0 ){` |
|     ! 0 |  7058 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  7059 | `		return SXERR_ABORT;` |
|       - |  7060 | `	}` |
|   56216 |  7061 | `	if( iTypeFlags & PH7_CLASS_ATTR_TYPED ){` |
|     120 |  7062 | `		pAttr->nType = nType;` |
|     120 |  7063 | `		pAttr->sClass = sTypeClass;` |
|     120 |  7064 | `		pAttr->sTypeName = sTypeText;` |
|     120 |  7065 | `		if( iTypeFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7066 | `			/* Copy the parsed alternatives into the attribute. The class-name` |
|       - |  7067 | `			 * SyStrings inside each ph7_type_alt point to memory owned by the` |
|       - |  7068 | `			 * VM allocator (SyMemBackendStrDup'd in GenStateParseUnionTypeDecl),` |
|       - |  7069 | `			 * so it's safe for multiple attrs in a multi-decl chain to share` |
|       - |  7070 | `			 * the same backing strings — they outlive the temporary set. */` |
|       - |  7071 | `			sxu32 i;` |
|      32 |  7072 | `			for( i = 0; i < SySetUsed(&aUnionAlts); i++ ){` |
|      22 |  7073 | `				ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&aUnionAlts, i);` |
|      22 |  7074 | `				SySetPut(&pAttr->aUnionAlts, (const void *)pSrc);` |
|      12 |  7075 | `			}` |
|       5 |  7076 | `		}` |
|      59 |  7077 | `	}` |
|   56216 |  7078 | `	if( pGen->pIn->nType & PH7_TK_EQUAL /*'='*/ ){` |
|       - |  7079 | `		SySet *pInstrContainer;` |
|   17946 |  7080 | `		pGen->pIn++; /*Jump the equal sign */` |
|       - |  7081 | `		/* Swap bytecode container */` |
|   17946 |  7082 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   17946 |  7083 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pAttr->aByteCode);` |
|       - |  7084 | `		/* Compile attribute value.` |
|       - |  7085 | `		 */` |
|   17946 |  7086 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|   17946 |  7087 | `		if( rc == SXERR_EMPTY ){` |
|     ! 0 |  7088 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Attribute '%z': Missing default value",pName);` |
|     ! 0 |  7089 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7090 | `				return SXERR_ABORT;` |
|       - |  7091 | `			}` |
|     ! 0 |  7092 | `		}` |
|       - |  7093 | `		/* Emit the done instruction */` |
|   17946 |  7094 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|   17946 |  7095 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    8972 |  7096 | `	}` |
|       - |  7097 | `	/* All done,install the attribute */` |
|   56216 |  7098 | `	rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|   56216 |  7099 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7100 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7101 | `		return SXERR_ABORT;` |
|       - |  7102 | `	}` |
|   56216 |  7103 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /*','*/) ){` |
|       - |  7104 | `		/* Multiple attribute declarations [i.e: public $var1,$var2=5<<1,$var3] */` |
|       5 |  7105 | `		pGen->pIn++; /* Jump the comma */` |
|       5 |  7106 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0 ){` |
|     ! 0 |  7107 | `			SyToken *pTok = pGen->pIn;` |
|     ! 0 |  7108 | `			if( pTok >= pGen->pEnd ){` |
|     ! 0 |  7109 | `				pTok--;` |
|     ! 0 |  7110 | `			}` |
|     ! 0 |  7111 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7112 | `				"Unexpected token '%z',expecting attribute declaration inside class '%z'",` |
|     ! 0 |  7113 | `				&pTok->sData,&pClass->sName);` |
|     ! 0 |  7114 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7115 | `				return SXERR_ABORT;` |
|       - |  7116 | `			}` |
|     ! 0 |  7117 | `		}else{` |
|       5 |  7118 | `			if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       5 |  7119 | `				goto loop;` |
|       - |  7120 | `			}` |
|       - |  7121 | `		}` |
|     ! 0 |  7122 | `	}` |
|   56212 |  7123 | `	SySetRelease(&aUnionAlts);` |
|   56212 |  7124 | `	return SXRET_OK;` |
|       2 |  7125 | `Synchronize:` |
|       - |  7126 | `	/* Synchronize with the first semi-colon */` |
|      11 |  7127 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|       7 |  7128 | `		pGen->pIn++;` |
|       1 |  7129 | `	}` |
|       5 |  7130 | `	SySetRelease(&aUnionAlts);` |
|       5 |  7131 | `	return SXERR_CORRUPT;` |
|   28109 |  7132 |  |
|       - |  7133 | `/*` |
|       - |  7134 | ` * Compile a class method.` |
|       - |  7135 | ` *` |
|       - |  7136 | ` * Refer to the official documentation for more information` |
|       - |  7137 | ` * on the powerful extension introduced by the PH7 engine` |
|       - |  7138 | ` * to the OO subsystem such as full type hinting,method` |
|       - |  7139 | ` * overloading and many more.` |
|       - |  7140 | ` */` |
|  191410 |  7141 | `static sxi32 GenStateCompileClassMethod(` |
|       - |  7142 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - |  7143 | `	sxi32 iProtection,   /* Visibility level */` |
|       - |  7144 | `	sxi32 iFlags,        /* Configuration flags */` |
|       - |  7145 | `	int doBody,          /* TRUE to process method body */` |
|       - |  7146 | `	ph7_class *pClass    /* Class this method belongs */` |
|       - |  7147 | `	)` |
|       2 |  7148 |  |
|  191412 |  7149 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7150 | `	ph7_class_method *pMeth;` |
|       - |  7151 | `	sxi32 iFuncFlags;` |
|       - |  7152 | `	SyString *pName;` |
|       - |  7153 | `	SyToken *pEnd;` |
|       - |  7154 | `	sxi32 rc;` |
|       - |  7155 | `	/* Extract visibility level */` |
|  191412 |  7156 | `	iProtection = GetProtectionLevel(iProtection);` |
|  191412 |  7157 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|  191412 |  7158 | `	iFuncFlags = 0;` |
|  191412 |  7159 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7160 | `		/* Invalid method name */` |
|     ! 0 |  7161 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7162 | `		if( rc == SXERR_ABORT ){` |
|       - |  7163 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7164 | `			return SXERR_ABORT;` |
|       - |  7165 | `		}` |
|     ! 0 |  7166 | `		goto Synchronize;` |
|       - |  7167 | `	}` |
|  191412 |  7168 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|       - |  7169 | `		/* Return by reference,remember that */` |
|     ! 0 |  7170 | `		iFuncFlags \|= VM_FUNC_REF_RETURN;` |
|       - |  7171 | `		/* Jump the '&' token */` |
|     ! 0 |  7172 | `		pGen->pIn++;` |
|     ! 0 |  7173 | `	}` |
|  191412 |  7174 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  7175 | `		/* Invalid method name */` |
|     ! 0 |  7176 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Invalid method name");` |
|     ! 0 |  7177 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7178 | `			return SXERR_ABORT;` |
|       - |  7179 | `		}` |
|     ! 0 |  7180 | `		goto Synchronize;` |
|       - |  7181 | `	}` |
|       - |  7182 | `	/* Peek method name */` |
|  191412 |  7183 | `	pName = &pGen->pIn->sData;` |
|  191412 |  7184 | `	nLine = pGen->pIn->nLine;` |
|       - |  7185 | `	/* Jump the method name */` |
|  191412 |  7186 | `	pGen->pIn++;` |
|  191412 |  7187 | `	if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       - |  7188 | `		/* Abstract method */` |
|   47028 |  7189 | `		if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     ! 0 |  7190 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7191 | `				"Access type for abstract method '%z::%z' cannot be 'private'",` |
|     ! 0 |  7192 | `				&pClass->sName,pName);` |
|     ! 0 |  7193 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7194 | `				return SXERR_ABORT;` |
|       - |  7195 | `			}` |
|     ! 0 |  7196 | `		}` |
|       - |  7197 | `		/* Assemble method signature only */` |
|   47028 |  7198 | `		doBody = FALSE;` |
|   23513 |  7199 | `	}` |
|  191412 |  7200 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  7201 | `		/* Syntax error */` |
|     ! 0 |  7202 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after method name '%z'",pName);` |
|     ! 0 |  7203 | `		if( rc == SXERR_ABORT ){` |
|       - |  7204 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7205 | `			return SXERR_ABORT;` |
|       - |  7206 | `		}` |
|     ! 0 |  7207 | `		goto Synchronize;` |
|       - |  7208 | `	}` |
|       - |  7209 | `	/* Allocate a new class_method instance */` |
|  191412 |  7210 | `	pMeth = PH7_NewClassMethod(pGen->pVm,pClass,pName,nLine,iProtection,iFlags,iFuncFlags);` |
|  191412 |  7211 | `	if( pMeth == 0 ){` |
|     ! 0 |  7212 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7213 | `		return SXERR_ABORT;` |
|       - |  7214 | `	}` |
|       - |  7215 | `	/* Jump the left parenthesis '(' */` |
|  191412 |  7216 | `	pGen->pIn++;` |
|  191412 |  7217 | `	pEnd = 0; /* cc warning */` |
|       - |  7218 | `	/* Delimit the method signature */` |
|  191412 |  7219 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  191412 |  7220 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7221 | `		/* Syntax error */` |
|       3 |  7222 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing ')' after method '%z' declaration",pName);` |
|       3 |  7223 | `		if( rc == SXERR_ABORT ){` |
|       - |  7224 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7225 | `			return SXERR_ABORT;` |
|       - |  7226 | `		}` |
|       3 |  7227 | `		goto Synchronize;` |
|       - |  7228 | `	}` |
|       - |  7229 | `	{` |
|  191410 |  7230 | `		int bIsCtor = 0;` |
|  191410 |  7231 | `		int bAbstractCtor = 0;` |
|  278259 |  7232 | `		if( (pName->nByte == sizeof("__construct") - 1` |
|  114846 |  7233 | `				&& SyMemcmp(pName->zString,"__construct",sizeof("__construct") - 1) == 0)` |
|  182557 |  7234 | `		 \|\| SyStringCmp(pName,&pClass->sName,SyMemcmp) == 0 ){` |
|   17708 |  7235 | `			if( iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|       3 |  7236 | `				bAbstractCtor = 1;` |
|       2 |  7237 | `			}else{` |
|   17706 |  7238 | `				bIsCtor = 1;` |
|       - |  7239 | `			}` |
|    8853 |  7240 | `		}` |
|  191410 |  7241 | `		if( pGen->pIn < pEnd ){` |
|       - |  7242 | `			/* Collect method arguments */` |
|   32442 |  7243 | `			rc = GenStateCollectFuncArgs(&pMeth->sFunc,&(*pGen),pEnd,bIsCtor,bAbstractCtor);` |
|   32442 |  7244 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7245 | `				return SXERR_ABORT;` |
|       - |  7246 | `			}` |
|   16220 |  7247 | `		}` |
|       - |  7248 | `	}` |
|       - |  7249 | `	/* Point past ')' and parse optional return type ': type' */` |
|  191410 |  7250 | `	pGen->pIn = &pEnd[1];` |
|       - |  7251 | `	{` |
|  191410 |  7252 | `		sxi32 rcRt = GenStateParseReturnType(pGen, &pMeth->sFunc);` |
|  191410 |  7253 | `		if( rcRt == SXERR_ABORT ){` |
|     ! 0 |  7254 | `			return SXERR_ABORT;` |
|  191410 |  7255 | `		}else if( rcRt == SXERR_SYNTAX ){` |
|     ! 0 |  7256 | `			goto Synchronize;` |
|       - |  7257 | `		}` |
|       - |  7258 | `	}` |
|       - |  7259 | `	/* Install promoted constructor properties as class attributes. Runtime` |
|       - |  7260 | `	 * property init/typecheck is handled by the generic typed-property path` |
|       - |  7261 | `	 * since we mint real ph7_class_attr entries. */` |
|       - |  7262 | `	{` |
|  191410 |  7263 | `		sxu32 nArg = SySetUsed(&pMeth->sFunc.aArgs);` |
|       - |  7264 | `		sxu32 i;` |
|  250308 |  7265 | `		for( i = 0; i < nArg; i++ ){` |
|   58908 |  7266 | `			ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pMeth->sFunc.aArgs,i);` |
|       - |  7267 | `			ph7_class_attr *pAttr;` |
|   58908 |  7268 | `			sxi32 iAttrFlags = 0;` |
|   58908 |  7269 | `			if( (pArg->iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   58872 |  7270 | `				continue;` |
|       - |  7271 | `			}` |
|      38 |  7272 | `			if( pArg->iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       3 |  7273 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7274 | `					"Cannot declare variadic promoted property");` |
|       3 |  7275 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7276 | `					return SXERR_ABORT;` |
|       - |  7277 | `				}` |
|       3 |  7278 | `				goto Synchronize;` |
|       - |  7279 | `			}` |
|       - |  7280 | `			/* Reject the same disallowed pseudo-types (callable/mixed/iterable)` |
|       - |  7281 | `			 * that GenStateCompileClassAttr rejects — including when they` |
|       - |  7282 | `			 * appear as an alternative of a union type. */` |
|      34 |  7283 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0` |
|       6 |  7284 | `			 \|\| (pArg->iFlags & VM_FUNC_ARG_UNION) ){` |
|      53 |  7285 | `				rc = GenStateValidatePropertyType(pGen,pClass,&pArg->sName,` |
|      34 |  7286 | `					pArg->nType,&pArg->sClass,&pArg->sTypeName,` |
|      34 |  7287 | `					(pArg->iFlags & VM_FUNC_ARG_UNION) ? &pArg->aUnionAlts : 0,` |
|      17 |  7288 | `					nLine);` |
|      36 |  7289 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7290 | `					return SXERR_ABORT;` |
|      36 |  7291 | `				}else if( rc != SXRET_OK ){` |
|       5 |  7292 | `					goto Synchronize;` |
|       - |  7293 | `				}` |
|      15 |  7294 | `			}` |
|       - |  7295 | `			/* Reject duplicate property (explicit property declared earlier with same name). */` |
|      32 |  7296 | `			if( PH7_ClassExtractAttribute(pClass,SyStringData(&pArg->sName),SyStringLength(&pArg->sName)) != 0 ){` |
|       4 |  7297 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       1 |  7298 | `					"Cannot redeclare %z::$%z",&pClass->sName,&pArg->sName);` |
|       3 |  7299 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7300 | `					return SXERR_ABORT;` |
|       - |  7301 | `				}` |
|       3 |  7302 | `				goto Synchronize;` |
|       - |  7303 | `			}` |
|      30 |  7304 | `			if( pArg->nType > 0 \|\| SyStringLength(&pArg->sClass) > 0 ){` |
|      28 |  7305 | `				iAttrFlags \|= PH7_CLASS_ATTR_TYPED;` |
|      13 |  7306 | `			}` |
|      30 |  7307 | `			if( pArg->iFlags & VM_FUNC_ARG_NULLABLE ){` |
|       3 |  7308 | `				iAttrFlags \|= PH7_CLASS_ATTR_NULLABLE;` |
|       1 |  7309 | `			}` |
|      30 |  7310 | `			if( pArg->iFlags & VM_FUNC_ARG_UNION ){` |
|       3 |  7311 | `				iAttrFlags \|= PH7_CLASS_ATTR_UNION;` |
|       1 |  7312 | `			}` |
|      30 |  7313 | `			pAttr = PH7_NewClassAttr(pGen->pVm,&pArg->sName,nLine,pArg->iPromoteVis,iAttrFlags);` |
|      30 |  7314 | `			if( pAttr == 0 ){` |
|     ! 0 |  7315 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7316 | `				return SXERR_ABORT;` |
|       - |  7317 | `			}` |
|      30 |  7318 | `			if( iAttrFlags & PH7_CLASS_ATTR_TYPED ){` |
|      28 |  7319 | `				pAttr->nType = pArg->nType;` |
|      28 |  7320 | `				pAttr->sClass = pArg->sClass;` |
|      28 |  7321 | `				pAttr->sTypeName = pArg->sTypeName;` |
|      28 |  7322 | `				if( iAttrFlags & PH7_CLASS_ATTR_UNION ){` |
|       - |  7323 | `					sxu32 k;` |
|     ! 0 |  7324 | `					for( k = 0; k < SySetUsed(&pArg->aUnionAlts); k++ ){` |
|     ! 0 |  7325 | `						ph7_type_alt *pSrc = (ph7_type_alt *)SySetAt(&pArg->aUnionAlts,k);` |
|     ! 0 |  7326 | `						SySetPut(&pAttr->aUnionAlts,(const void *)pSrc);` |
|     ! 0 |  7327 | `					}` |
|     ! 0 |  7328 | `				}` |
|      13 |  7329 | `			}` |
|      30 |  7330 | `			rc = PH7_ClassInstallAttr(pClass,pAttr);` |
|      30 |  7331 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7332 | `				PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7333 | `				return SXERR_ABORT;` |
|       - |  7334 | `			}` |
|      16 |  7335 | `		}` |
|       - |  7336 | `	}` |
|  191402 |  7337 | `	if( doBody ){` |
|       - |  7338 | `		/* Compile method body */` |
|  144376 |  7339 | `		rc = GenStateCompileFuncBody(&(*pGen),&pMeth->sFunc);` |
|  144376 |  7340 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  7341 | `			return SXERR_ABORT;` |
|       - |  7342 | `		}` |
|   72189 |  7343 | `	}else{` |
|       - |  7344 | `		/* Only method signature is allowed */` |
|   47028 |  7345 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /* ';'*/) == 0 ){` |
|     ! 0 |  7346 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7347 | `				"Expected ';' after method signature '%z'",pName);` |
|     ! 0 |  7348 | `				if( rc == SXERR_ABORT ){` |
|       - |  7349 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7350 | `					return SXERR_ABORT;` |
|       - |  7351 | `				}` |
|     ! 0 |  7352 | `				return SXERR_CORRUPT;` |
|       - |  7353 | `			}` |
|       - |  7354 | `	}` |
|       - |  7355 | `	/* All done,install the method */` |
|  191402 |  7356 | `	rc = PH7_ClassInstallMethod(pClass,pMeth);` |
|  191402 |  7357 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7358 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7359 | `		return SXERR_ABORT;` |
|       - |  7360 | `	}` |
|  191402 |  7361 | `	return SXRET_OK;` |
|       5 |  7362 | `Synchronize:` |
|       - |  7363 | `	/* Synchronize with the first semi-colon */` |
|      31 |  7364 | `	while(pGen->pIn < pGen->pEnd && ((pGen->pIn->nType & PH7_TK_SEMI/*';'*/) == 0) ){` |
|      21 |  7365 | `		pGen->pIn++;` |
|       1 |  7366 | `	}` |
|      11 |  7367 | `	return SXERR_CORRUPT;` |
|   95707 |  7368 |  |
|       - |  7369 | `/*` |
|       - |  7370 | ` * Compile an object interface.` |
|       - |  7371 | ` *  According to the PHP language reference manual` |
|       - |  7372 | ` *   Object Interfaces:` |
|       - |  7373 | ` *   Object interfaces allow you to create code which specifies which methods` |
|       - |  7374 | ` *   a class must implement, without having to define how these methods are handled.` |
|       - |  7375 | ` *   Interfaces are defined using the interface keyword, in the same way as a standard` |
|       - |  7376 | ` *   class, but without any of the methods having their contents defined.` |
|       - |  7377 | ` *   All methods declared in an interface must be public, this is the nature of an interface.` |
|       - |  7378 | ` */` |
|   11786 |  7379 | `static sxi32 PH7_CompileClassInterface(ph7_gen_state *pGen)` |
|       2 |  7380 |  |
|   11788 |  7381 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7382 | `	ph7_class *pClass,*pBase;` |
|       - |  7383 | `	SyToken *pEnd,*pTmp;` |
|       - |  7384 | `	SyString *pName;` |
|       - |  7385 | `	sxi32 nKwrd;` |
|       - |  7386 | `	sxi32 rc;` |
|       - |  7387 | `	/* Jump the 'interface' keyword */` |
|   11788 |  7388 | `	pGen->pIn++;` |
|       - |  7389 | `	/* Extract interface name */` |
|   11788 |  7390 | `	pName = &pGen->pIn->sData;` |
|       - |  7391 | `	/* Advance the stream cursor */` |
|   11788 |  7392 | `	pGen->pIn++;` |
|       - |  7393 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  7394 | `		SyBlob sFQN;` |
|       - |  7395 | `		SyString sFQNStr;` |
|   11788 |  7396 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   11788 |  7397 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   11788 |  7398 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   11788 |  7399 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   11788 |  7400 | `		SyBlobRelease(&sFQN);` |
|       - |  7401 | `	}` |
|   11788 |  7402 | `	if( pClass == 0 ){` |
|     ! 0 |  7403 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7404 | `		return SXERR_ABORT;` |
|       - |  7405 | `	}` |
|       - |  7406 | `	/* Mark as an interface */` |
|   11788 |  7407 | `	pClass->iFlags = PH7_CLASS_INTERFACE;` |
|       - |  7408 | `	/* Assume no base class is given */` |
|   11788 |  7409 | `	pBase = 0;` |
|   11788 |  7410 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       8 |  7411 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       8 |  7412 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* interface b extends a */ ){` |
|       - |  7413 | `			SyBlob sResolved;` |
|       - |  7414 | `			SyString sBaseName;` |
|       - |  7415 | `			sxu32 nRefLine;` |
|       - |  7416 | `			/* Extract base interface */` |
|       8 |  7417 | `			pGen->pIn++;` |
|       8 |  7418 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|       8 |  7419 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       8 |  7420 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  7421 | `				SyBlobRelease(&sResolved);` |
|     ! 0 |  7422 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  7423 | `					"Expected 'interface_name' after 'extends' keyword inside interface '%z'",` |
|     ! 0 |  7424 | `					pName);` |
|     ! 0 |  7425 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7426 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7427 | `					return SXERR_ABORT;` |
|       - |  7428 | `				}` |
|     ! 0 |  7429 | `				return SXRET_OK;` |
|       - |  7430 | `			}` |
|      11 |  7431 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|       6 |  7432 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       8 |  7433 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  7434 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  7435 | `			/* Only interfaces is allowed */` |
|       8 |  7436 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  7437 | `				pBase = pBase->pNextName;` |
|     ! 0 |  7438 | `			}` |
|       8 |  7439 | `			if( pBase == 0 ){` |
|     ! 0 |  7440 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  7441 | `					"Nonexistent base interface '%z'",&sBaseName);` |
|     ! 0 |  7442 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7443 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  7444 | `					return SXERR_ABORT;` |
|       - |  7445 | `				}` |
|     ! 0 |  7446 | `			}` |
|       8 |  7447 | `			SyBlobRelease(&sResolved);` |
|       3 |  7448 | `		}` |
|       3 |  7449 | `	}` |
|   11788 |  7450 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  7451 | `		/* Syntax error */` |
|     ! 0 |  7452 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after interface '%z' definition",pName);` |
|     ! 0 |  7453 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7454 | `		if( rc == SXERR_ABORT ){` |
|       - |  7455 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7456 | `			return SXERR_ABORT;` |
|       - |  7457 | `		}` |
|     ! 0 |  7458 | `		return SXRET_OK;` |
|       - |  7459 | `	}` |
|   11788 |  7460 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   11788 |  7461 | `	pEnd = 0; /* cc warning */` |
|       - |  7462 | `	/* Delimit the interface body */` |
|   11788 |  7463 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   11788 |  7464 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  7465 | `		/* Syntax error */` |
|     ! 0 |  7466 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing '}' after interface '%z' definition",pName);` |
|     ! 0 |  7467 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  7468 | `		if( rc == SXERR_ABORT ){` |
|       - |  7469 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  7470 | `			return SXERR_ABORT;` |
|       - |  7471 | `		}` |
|     ! 0 |  7472 | `		return SXRET_OK;` |
|       - |  7473 | `	}` |
|       - |  7474 | `	/* Swap token stream */` |
|   11788 |  7475 | `	pTmp = pGen->pEnd;` |
|   11788 |  7476 | `	pGen->pEnd = pEnd;` |
|       - |  7477 | `	/* Start the parse process` |
|       - |  7478 | `	 * Note (According to the PHP reference manual):` |
|       - |  7479 | `	 *  Only constants and function signatures(without body) are allowed.` |
|       - |  7480 | `	 *  Only 'public' visibility is allowed.` |
|       - |  7481 | `	 */` |
|   29400 |  7482 | `	for(;;){` |
|       - |  7483 | `		/* Jump leading/trailing semi-colons */` |
|  105816 |  7484 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   47016 |  7485 | `			pGen->pIn++;` |
|       2 |  7486 | `		}` |
|   58802 |  7487 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  7488 | `			/* End of interface body */` |
|   11786 |  7489 | `			break;` |
|       - |  7490 | `		}` |
|   47018 |  7491 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7492 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  7493 | `				"Unexpected token '%z'.Expecting method signature or constant declaration inside interface '%z'",` |
|     ! 0 |  7494 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  7495 | `			if( rc == SXERR_ABORT ){` |
|       - |  7496 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7497 | `				return SXERR_ABORT;` |
|       - |  7498 | `			}` |
|     ! 0 |  7499 | `			goto done;` |
|       - |  7500 | `		}` |
|       - |  7501 | `		/* Extract the current keyword */` |
|   47018 |  7502 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   47018 |  7503 | `		if( nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       - |  7504 | `			/* Fatal error: interface members must be public (PHP 7.1-8.0 behavior).` |
|       - |  7505 | `			 * Peek ahead to distinguish constant vs method and extract the member name. */` |
|       3 |  7506 | `			const char *zKind = "member";` |
|       3 |  7507 | `			SyString *pMemberName = 0;` |
|       3 |  7508 | `			if( (pGen->pIn + 1) < pGen->pEnd ){` |
|       3 |  7509 | `				sxi32 nNext = SX_PTR_TO_INT((pGen->pIn + 1)->pUserData);` |
|       3 |  7510 | `				if( nNext == PH7_TKWRD_CONST ){` |
|       3 |  7511 | `					zKind = "constant";` |
|       3 |  7512 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|       3 |  7513 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|       2 |  7514 | `					}` |
|       1 |  7515 | `				}else if( nNext == PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7516 | `					zKind = "method";` |
|     ! 0 |  7517 | `					if( (pGen->pIn + 2) < pGen->pEnd && ((pGen->pIn + 2)->nType & PH7_TK_ID) ){` |
|     ! 0 |  7518 | `						pMemberName = &(pGen->pIn + 2)->sData;` |
|     ! 0 |  7519 | `					}` |
|     ! 0 |  7520 | `				}` |
|       1 |  7521 | `			}` |
|       3 |  7522 | `			if( pMemberName ){` |
|       4 |  7523 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|       1 |  7524 | `					"Access type for interface %s %z::%z must be public",zKind,pName,pMemberName);` |
|       2 |  7525 | `			}else{` |
|     ! 0 |  7526 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7527 | `					"Access type for interface %s must be public",zKind);` |
|       - |  7528 | `			}` |
|       3 |  7529 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  7530 | `				return SXERR_ABORT;` |
|       - |  7531 | `			}` |
|       3 |  7532 | `			goto done;` |
|       - |  7533 | `		}` |
|   47016 |  7534 | `		if( nKwrd != PH7_TKWRD_PUBLIC && nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7535 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7536 | `				"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7537 | `			if( rc == SXERR_ABORT ){` |
|       - |  7538 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  7539 | `				return SXERR_ABORT;` |
|       - |  7540 | `			}` |
|     ! 0 |  7541 | `			goto done;` |
|       - |  7542 | `		}` |
|   47016 |  7543 | `		if( nKwrd == PH7_TKWRD_PUBLIC ){` |
|       - |  7544 | `			/* Advance the stream cursor */` |
|   47012 |  7545 | `			pGen->pIn++;` |
|   47012 |  7546 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  7547 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7548 | `					"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7549 | `				if( rc == SXERR_ABORT ){` |
|       - |  7550 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7551 | `					return SXERR_ABORT;` |
|       - |  7552 | `				}` |
|     ! 0 |  7553 | `				goto done;` |
|       - |  7554 | `			}` |
|   47012 |  7555 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   47012 |  7556 | `			if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_CONST && nKwrd != PH7_TKWRD_STATIC ){` |
|     ! 0 |  7557 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7558 | `					"Expecting method signature or constant declaration inside interface '%z'",pName);` |
|     ! 0 |  7559 | `				if( rc == SXERR_ABORT ){` |
|       - |  7560 | `					/* Error count limit reached,abort immediately */` |
|     ! 0 |  7561 | `					return SXERR_ABORT;` |
|       - |  7562 | `				}` |
|     ! 0 |  7563 | `				goto done;` |
|       - |  7564 | `			}` |
|   23505 |  7565 | `		}` |
|   47016 |  7566 | `		if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  7567 | `			/* Parse constant */` |
|       3 |  7568 | `			rc = GenStateCompileClassConstant(&(*pGen),0,0,pClass);` |
|       3 |  7569 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7570 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7571 | `					return SXERR_ABORT;` |
|       - |  7572 | `				}` |
|     ! 0 |  7573 | `				goto done;` |
|       - |  7574 | `			}` |
|       2 |  7575 | `		}else{` |
|   47014 |  7576 | `			sxi32 iFlags = PH7_CLASS_ATTR_ABSTRACT; /* Interface methods are implicitly abstract */` |
|   47014 |  7577 | `			if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  7578 | `				/* Static method,record that */` |
|     ! 0 |  7579 | `				iFlags \|= PH7_CLASS_ATTR_STATIC;` |
|       - |  7580 | `				/* Advance the stream cursor */` |
|     ! 0 |  7581 | `				pGen->pIn++;` |
|     ! 0 |  7582 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     ! 0 |  7583 | `					\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  7584 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  7585 | `							"Expecting method signature inside interface '%z'",pName);` |
|     ! 0 |  7586 | `						if( rc == SXERR_ABORT ){` |
|       - |  7587 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  7588 | `							return SXERR_ABORT;` |
|       - |  7589 | `						}` |
|     ! 0 |  7590 | `						goto done;` |
|       - |  7591 | `				}` |
|     ! 0 |  7592 | `			}` |
|       - |  7593 | `			/* Process method signature (no body for interface methods) */` |
|   47014 |  7594 | `			rc = GenStateCompileClassMethod(&(*pGen),0,iFlags,FALSE,pClass);` |
|   47014 |  7595 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  7596 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7597 | `					return SXERR_ABORT;` |
|       - |  7598 | `				}` |
|     ! 0 |  7599 | `				goto done;` |
|       - |  7600 | `			}` |
|       - |  7601 | `		}` |
|       2 |  7602 | `	}` |
|       - |  7603 | `	/* Install the interface */` |
|   11786 |  7604 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   11786 |  7605 | `	if( rc == SXRET_OK && pBase ){` |
|       - |  7606 | `		/* Inherit from the base interface */` |
|       8 |  7607 | `		rc = PH7_ClassInterfaceInherit(pClass,pBase);` |
|       3 |  7608 | `	}` |
|   11786 |  7609 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  7610 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  7611 | `		return SXERR_ABORT;` |
|       - |  7612 | `	}` |
|    5892 |  7613 | `done:` |
|       - |  7614 | `	/* Point beyond the interface body */` |
|   11788 |  7615 | `	pGen->pIn  = &pEnd[1];` |
|   11788 |  7616 | `	pGen->pEnd = pTmp;` |
|   11788 |  7617 | `	return PH7_OK;` |
|    5895 |  7618 |  |
|       - |  7619 | `/*` |
|       - |  7620 | ` * Compile a user-defined class.` |
|       - |  7621 | ` * According to the PHP language reference manual` |
|       - |  7622 | ` *  class` |
|       - |  7623 | ` *  Basic class definitions begin with the keyword class, followed by a class` |
|       - |  7624 | ` *  name, followed by a pair of curly braces which enclose the definitions` |
|       - |  7625 | ` *  of the properties and methods belonging to the class.` |
|       - |  7626 | ` *  The class name can be any valid label which is a not a PHP reserved word.` |
|       - |  7627 | ` *  A valid class name starts with a letter or underscore, followed by any number` |
|       - |  7628 | ` *  of letters, numbers, or underscores. As a regular expression, it would be expressed` |
|       - |  7629 | ` *  thus: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*.` |
|       - |  7630 | ` *  A class may contain its own constants, variables (called "properties"), and functions` |
|       - |  7631 | ` *  (called "methods").` |
|       - |  7632 | ` */` |
|       - |  7633 | `/* Per-use-statement entry: the traits listed in one 'use' plus its optional { } block */` |
|       - |  7634 | `typedef struct TraitUseEntry TraitUseEntry;` |
|       - |  7635 | `struct TraitUseEntry {` |
|       - |  7636 | `	SySet aTraits;             /* SySet of ph7_class* — traits in this use statement */` |
|       - |  7637 | `	SyToken *pResolvStart;     /* Start of resolution block tokens (NULL if none) */` |
|       - |  7638 | `	SyToken *pResolvEnd;       /* End of resolution block tokens */` |
|       - |  7639 | `};` |
|       - |  7640 | `/*` |
|       - |  7641 | ` * Validate that methods implementing interface contracts have compatible` |
|       - |  7642 | ` * signatures: public visibility and at least as many parameters as declared.` |
|       - |  7643 | ` */` |
|   41846 |  7644 | `static sxi32 GenStateCheckInterfaceSignatures(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7645 |  |
|       - |  7646 | `	ph7_class **apIface;` |
|       - |  7647 | `	sxu32 nIface,i;` |
|       - |  7648 | `	sxi32 rc;` |
|   41848 |  7649 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|     ! 0 |  7650 | `		return SXRET_OK;` |
|       - |  7651 | `	}` |
|   41848 |  7652 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   41848 |  7653 | `	nIface = SySetUsed(&pClass->aInterface);` |
|   50698 |  7654 | `	for(i = 0; i < nIface; i++){` |
|    8852 |  7655 | `		ph7_class *pIface = apIface[i];` |
|       - |  7656 | `		SyHashEntry *pEntry;` |
|    8852 |  7657 | `		SyHashResetLoopCursor(&pIface->hMethod);` |
|   70618 |  7658 | `		while((pEntry = SyHashGetNextEntry(&pIface->hMethod)) != 0 ){` |
|   61768 |  7659 | `			ph7_class_method *pIfaceMeth = (ph7_class_method *)pEntry->pUserData;` |
|       - |  7660 | `			ph7_class_method *pImplMeth;` |
|   61768 |  7661 | `			SyString *pMName = &pIfaceMeth->sFunc.sName;` |
|       - |  7662 | `			/* Find the implementing method in the class */` |
|   61768 |  7663 | `			pImplMeth = PH7_ClassExtractMethod(pClass,pMName->zString,pMName->nByte);` |
|   61768 |  7664 | `			if( pImplMeth == 0 \|\| (pImplMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 |  7665 | `				continue; /* Missing implementations caught by GenStateCheckAbstractMethods */` |
|       - |  7666 | `			}` |
|       - |  7667 | `			/* Check visibility: interface methods must be implemented as public */` |
|   61754 |  7668 | `			if( pImplMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       4 |  7669 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7670 | `					"Access level to %z::%z() must be public (as in class %z)",` |
|       1 |  7671 | `					&pClass->sName,pMName,&pIface->sName);` |
|       3 |  7672 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  7673 | `					return SXERR_ABORT;` |
|       - |  7674 | `				}` |
|       1 |  7675 | `			}` |
|       - |  7676 | `			/* Check parameter compatibility: implementation must accept at least as many` |
|       - |  7677 | `			 * required parameters. Extra parameters are allowed only if they have defaults.` |
|       - |  7678 | `			 */` |
|       - |  7679 | `			{` |
|   61754 |  7680 | `				sxu32 nIfaceArgs = SySetUsed(&pIfaceMeth->sFunc.aArgs);` |
|   61754 |  7681 | `				sxu32 nImplArgs = SySetUsed(&pImplMeth->sFunc.aArgs);` |
|   61754 |  7682 | `				int sigError = 0;` |
|   61754 |  7683 | `				if( nImplArgs < nIfaceArgs ){` |
|       3 |  7684 | `					sigError = 1;` |
|   61753 |  7685 | `				}else if( nImplArgs > nIfaceArgs ){` |
|       - |  7686 | `					/* Extra parameters must all have default values */` |
|       5 |  7687 | `					ph7_vm_func_arg *aImplArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|       - |  7688 | `					sxu32 k;` |
|       7 |  7689 | `					for(k = nIfaceArgs; k < nImplArgs; k++){` |
|       5 |  7690 | `						if( SySetUsed(&aImplArgs[k].aByteCode) == 0 ){` |
|       3 |  7691 | `							sigError = 1;` |
|       3 |  7692 | `							break;` |
|       - |  7693 | `						}` |
|       2 |  7694 | `					}` |
|       2 |  7695 | `				}` |
|   61754 |  7696 | `				if( sigError ){` |
|       - |  7697 | `					SyBlob sImplSig, sIfaceSig;` |
|       - |  7698 | `					ph7_vm_func_arg *aArgs;` |
|       - |  7699 | `					sxu32 j;` |
|       5 |  7700 | `					SyBlobInit(&sImplSig,&pGen->pVm->sAllocator);` |
|       5 |  7701 | `					SyBlobInit(&sIfaceSig,&pGen->pVm->sAllocator);` |
|       - |  7702 | `					/* Build implementing method signature */` |
|       5 |  7703 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pImplMeth->sFunc.aArgs);` |
|      11 |  7704 | `					for(j = 0; j < nImplArgs; j++){` |
|       7 |  7705 | `						if( j > 0 ) SyBlobAppend(&sImplSig,", ",2);` |
|       7 |  7706 | `						SyBlobAppend(&sImplSig,"$",1);` |
|       7 |  7707 | `						SyBlobAppend(&sImplSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7708 | `					}` |
|       - |  7709 | `					/* Build interface method signature */` |
|       5 |  7710 | `					aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pIfaceMeth->sFunc.aArgs);` |
|      11 |  7711 | `					for(j = 0; j < nIfaceArgs; j++){` |
|       7 |  7712 | `						if( j > 0 ) SyBlobAppend(&sIfaceSig,", ",2);` |
|       7 |  7713 | `						SyBlobAppend(&sIfaceSig,"$",1);` |
|       7 |  7714 | `						SyBlobAppend(&sIfaceSig,aArgs[j].sName.zString,aArgs[j].sName.nByte);` |
|       4 |  7715 | `					}` |
|       7 |  7716 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pImplMeth->nLine,` |
|       - |  7717 | `						"Declaration of %z::%z(%.*s) must be compatible with %z::%z(%.*s)",` |
|       2 |  7718 | `						&pClass->sName,pMName,` |
|       4 |  7719 | `						(int)SyBlobLength(&sImplSig),(const char *)SyBlobData(&sImplSig),` |
|       2 |  7720 | `						&pIface->sName,pMName,` |
|       4 |  7721 | `						(int)SyBlobLength(&sIfaceSig),(const char *)SyBlobData(&sIfaceSig));` |
|       5 |  7722 | `					SyBlobRelease(&sImplSig);` |
|       5 |  7723 | `					SyBlobRelease(&sIfaceSig);` |
|       5 |  7724 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  7725 | `						return SXERR_ABORT;` |
|       - |  7726 | `					}` |
|       2 |  7727 | `				}` |
|       - |  7728 | `			}` |
|       2 |  7729 | `		}` |
|    4427 |  7730 | `	}` |
|   41848 |  7731 | `	return SXRET_OK;` |
|   20925 |  7732 |  |
|       - |  7733 | `/*` |
|       - |  7734 | ` * Check that a concrete class has no remaining abstract methods.` |
|       - |  7735 | ` * If it does, emit a PHP-compatible fatal error listing them all.` |
|       - |  7736 | ` */` |
|   41846 |  7737 | `static sxi32 GenStateCheckAbstractMethods(ph7_gen_state *pGen,ph7_class *pClass)` |
|       2 |  7738 |  |
|       - |  7739 | `	ph7_class_method *pMeth;` |
|       - |  7740 | `	SyHashEntry *pEntry;` |
|       - |  7741 | `	sxu32 nAbstract;` |
|       - |  7742 | `	SyBlob sMsg;` |
|       - |  7743 | `	sxi32 rc;` |
|       - |  7744 | `	/* Abstract classes, interfaces, and traits may have unimplemented methods */` |
|   41848 |  7745 | `	if( pClass->iFlags & (PH7_CLASS_ABSTRACT\|PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|      22 |  7746 | `		return SXRET_OK;` |
|       - |  7747 | `	}` |
|       - |  7748 | `	/* Count abstract methods */` |
|   41828 |  7749 | `	nAbstract = 0;` |
|   41828 |  7750 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  395230 |  7751 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  353404 |  7752 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  353404 |  7753 | `		if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      17 |  7754 | `			nAbstract++;` |
|       8 |  7755 | `		}` |
|       2 |  7756 | `	}` |
|   41828 |  7757 | `	if( nAbstract == 0 ){` |
|   41814 |  7758 | `		return SXRET_OK;` |
|       - |  7759 | `	}` |
|       - |  7760 | `	/* Build the error message listing all abstract methods with origins */` |
|      15 |  7761 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|      15 |  7762 | `	SyBlobFormat(&sMsg,"Class %z contains %u abstract method%s and must therefore "` |
|       - |  7763 | `		"be declared abstract or implement the remaining method%s (",` |
|       7 |  7764 | `		&pClass->sName,nAbstract,` |
|       7 |  7765 | `		(nAbstract > 1 ? "s" : ""),` |
|       7 |  7766 | `		(nAbstract > 1 ? "s" : ""));` |
|       - |  7767 | `	/* Second pass: list methods with origins */` |
|       - |  7768 | `	{` |
|      15 |  7769 | `		sxu32 nListed = 0;` |
|      15 |  7770 | `		SyHashResetLoopCursor(&pClass->hMethod);` |
|      33 |  7771 | `		while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|      19 |  7772 | `			ph7_class *pOrigin = 0;` |
|       - |  7773 | `			SyString *pMName;` |
|      19 |  7774 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|      19 |  7775 | `			if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|       3 |  7776 | `				continue;` |
|       - |  7777 | `			}` |
|      17 |  7778 | `			pMName = &pMeth->sFunc.sName;` |
|      17 |  7779 | `			if( nListed > 0 ){` |
|       3 |  7780 | `				SyBlobAppend(&sMsg,", ",2);` |
|       1 |  7781 | `			}` |
|       - |  7782 | `			/* Find the origin of this abstract method.` |
|       - |  7783 | `			 * PHP priority: interfaces (walking ancestors and interface` |
|       - |  7784 | `			 * inheritance chains) take precedence for interface-declared` |
|       - |  7785 | `			 * methods. Abstract class methods only win when the class` |
|       - |  7786 | `			 * itself declared the abstract method (not inherited from` |
|       - |  7787 | `			 * an interface). Trait methods are adopted into the using` |
|       - |  7788 | `			 * class's namespace.` |
|       - |  7789 | `			 */` |
|       - |  7790 | `			{` |
|       - |  7791 | `				ph7_class **apIface;` |
|       - |  7792 | `				ph7_class **apTrait;` |
|       - |  7793 | `				ph7_class *pWalk;` |
|       - |  7794 | `				sxu32 i;` |
|       - |  7795 | `				/* 1. Check parent chain for a natively-declared abstract method` |
|       - |  7796 | `				 * (one that was written in the class body, not inherited from an` |
|       - |  7797 | `				 * interface). PHP attributes origin to the declaring class.` |
|       - |  7798 | `				 */` |
|      17 |  7799 | `				if( pClass->pBase ){` |
|       9 |  7800 | `					pWalk = pClass->pBase;` |
|      17 |  7801 | `					while( pWalk ){` |
|       - |  7802 | `						ph7_class_method *pParentMeth;` |
|      11 |  7803 | `						pParentMeth = PH7_ClassExtractMethod(pWalk,pMName->zString,pMName->nByte);` |
|      11 |  7804 | `						if( pParentMeth && (pParentMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       - |  7805 | `							/* Exclude methods that came from an interface anywhere` |
|       - |  7806 | `							 * in this class's ancestor chain.` |
|       - |  7807 | `							 */` |
|      11 |  7808 | `							int fromIface = 0;` |
|      11 |  7809 | `							ph7_class *pAnc = pWalk;` |
|      15 |  7810 | `							while( pAnc ){` |
|       - |  7811 | `								ph7_class **apPI;` |
|       - |  7812 | `								sxu32 j;` |
|      13 |  7813 | `								apPI = (ph7_class **)SySetBasePtr(&pAnc->aInterface);` |
|      13 |  7814 | `								for(j = 0; j < SySetUsed(&pAnc->aInterface); j++){` |
|       9 |  7815 | `									if( PH7_ClassExtractMethod(apPI[j],pMName->zString,pMName->nByte) ){` |
|       9 |  7816 | `										fromIface = 1;` |
|       9 |  7817 | `										break;` |
|       - |  7818 | `									}` |
|     ! 0 |  7819 | `								}` |
|      13 |  7820 | `								if( fromIface ) break;` |
|       5 |  7821 | `								pAnc = pAnc->pBase;` |
|       1 |  7822 | `							}` |
|      11 |  7823 | `							if( !fromIface ){` |
|       3 |  7824 | `								pOrigin = pWalk;` |
|       3 |  7825 | `								break;` |
|       - |  7826 | `							}` |
|       4 |  7827 | `						}` |
|       9 |  7828 | `						pWalk = pWalk->pBase;` |
|       1 |  7829 | `					}` |
|       4 |  7830 | `				}` |
|       - |  7831 | `				/* 2. Check interfaces on class and all ancestors, walking` |
|       - |  7832 | `				 * each interface's own parent chain for the deepest origin.` |
|       - |  7833 | `				 */` |
|      17 |  7834 | `				if( !pOrigin ){` |
|      15 |  7835 | `					pWalk = pClass;` |
|      37 |  7836 | `					while( pWalk && !pOrigin ){` |
|      23 |  7837 | `						apIface = (ph7_class **)SySetBasePtr(&pWalk->aInterface);` |
|      23 |  7838 | `						for(i = 0; i < SySetUsed(&pWalk->aInterface); i++){` |
|      13 |  7839 | `							ph7_class *pIface = apIface[i];` |
|      13 |  7840 | `							ph7_class *pDeepest = 0;` |
|      25 |  7841 | `							while( pIface ){` |
|      13 |  7842 | `								if( PH7_ClassExtractMethod(pIface,pMName->zString,pMName->nByte) ){` |
|      13 |  7843 | `									pDeepest = pIface;` |
|       6 |  7844 | `								}` |
|      13 |  7845 | `								pIface = pIface->pBase;` |
|       1 |  7846 | `							}` |
|      13 |  7847 | `							if( pDeepest ){` |
|      13 |  7848 | `								pOrigin = pDeepest;` |
|      13 |  7849 | `								break;` |
|       - |  7850 | `							}` |
|     ! 0 |  7851 | `						}` |
|      23 |  7852 | `						pWalk = pWalk->pBase;` |
|       1 |  7853 | `					}` |
|       7 |  7854 | `				}` |
|       - |  7855 | `				/* 3. Trait methods are adopted into the class namespace in PHP */` |
|      17 |  7856 | `				if( !pOrigin ){` |
|       3 |  7857 | `					apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|       3 |  7858 | `					for(i = 0; i < SySetUsed(&pClass->aTrait); i++){` |
|       3 |  7859 | `						if( PH7_ClassExtractMethod(apTrait[i],pMName->zString,pMName->nByte) ){` |
|       3 |  7860 | `							pOrigin = pClass;` |
|       3 |  7861 | `							break;` |
|       - |  7862 | `						}` |
|     ! 0 |  7863 | `					}` |
|       1 |  7864 | `				}` |
|       - |  7865 | `			}` |
|      17 |  7866 | `			if( pOrigin ){` |
|      17 |  7867 | `				SyBlobFormat(&sMsg,"%z::%z",&pOrigin->sName,pMName);` |
|       9 |  7868 | `			}else{` |
|       - |  7869 | `				/* Origin is the class itself (trait method adopted into class namespace) */` |
|     ! 0 |  7870 | `				SyBlobFormat(&sMsg,"%z::%z",&pClass->sName,pMName);` |
|       - |  7871 | `			}` |
|      17 |  7872 | `			nListed++;` |
|       1 |  7873 | `		}` |
|       - |  7874 | `	}` |
|      15 |  7875 | `	SyBlobAppend(&sMsg,")",1);` |
|      22 |  7876 | `	rc = PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,"%.*s",` |
|      14 |  7877 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|      15 |  7878 | `	SyBlobRelease(&sMsg);` |
|      15 |  7879 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  7880 | `		return SXERR_ABORT;` |
|       - |  7881 | `	}` |
|      15 |  7882 | `	return SXRET_OK;` |
|   20925 |  7883 |  |
|       - |  7884 | `/*` |
|       - |  7885 | ` * Parse a class/interface name reference from the current token stream.` |
|       - |  7886 | ` * Handles an optional leading '\' (absolute) and multi-segment namespaced` |
|       - |  7887 | `` * names (`Foo\Bar\Baz`). On success, writes the resolved FQN into pFqn`` |
|       - |  7888 | ` * (which must be an initialized, empty SyBlob) and advances pGen->pIn past` |
|       - |  7889 | ` * the last consumed token. Returns SXRET_OK on success, SXERR_INVALID if` |
|       - |  7890 | ` * the stream has no valid name at the current position (pGen->pIn is left` |
|       - |  7891 | ` * untouched in that case so the caller can produce its own diagnostic).` |
|       - |  7892 | ` */` |
|   32728 |  7893 | `static sxi32 GenStateParseClassReference(ph7_gen_state *pGen,SyBlob *pFqn)` |
|       2 |  7894 |  |
|   32730 |  7895 | `	int isAbsolute = 0;` |
|   32730 |  7896 | `	SyToken *pStart = pGen->pIn;` |
|       - |  7897 | `	SyBlob sName;` |
|   32730 |  7898 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) ){` |
|      28 |  7899 | `		isAbsolute = 1;` |
|      28 |  7900 | `		pGen->pIn++;` |
|      13 |  7901 | `	}` |
|   32730 |  7902 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       7 |  7903 | `		pGen->pIn = pStart;` |
|       7 |  7904 | `		return SXERR_INVALID;` |
|       - |  7905 | `	}` |
|   32724 |  7906 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   32724 |  7907 | `	SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|   32724 |  7908 | `	pGen->pIn++;` |
|   49101 |  7909 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NSSEP) &&` |
|   16381 |  7910 | `		&pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      13 |  7911 | `		SyBlobAppend(&sName,"\\",1);` |
|      13 |  7912 | `		pGen->pIn++;` |
|      13 |  7913 | `		SyBlobAppend(&sName,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      13 |  7914 | `		pGen->pIn++;` |
|       1 |  7915 | `	}` |
|   32724 |  7916 | `	if( isAbsolute ){` |
|      25 |  7917 | `		SyBlobAppend(pFqn,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|      13 |  7918 | `	}else{` |
|       - |  7919 | `		SyString sRaw;` |
|   32700 |  7920 | `		SyStringInitFromBuf(&sRaw,(const char *)SyBlobData(&sName),SyBlobLength(&sName));` |
|   32700 |  7921 | `		GenStateResolveName(pGen,&sRaw,pFqn);` |
|       - |  7922 | `	}` |
|   32724 |  7923 | `	SyBlobRelease(&sName);` |
|   32724 |  7924 | `	return SXRET_OK;` |
|   16366 |  7925 |  |
|       - |  7926 | `/*` |
|       - |  7927 | ` * Return TRUE if pInterface is Throwable or transitively extends Throwable.` |
|       - |  7928 | `` * Walks both the interface `extends` chain (pBase) and any parent-interface`` |
|       - |  7929 | ` * set (aInterface). Depth is counted for every traversal step — recursion` |
|       - |  7930 | ` * through aInterface *and* sibling iteration through pBase — so a cycle in` |
|       - |  7931 | ` * either direction cannot run unbounded.` |
|       - |  7932 | ` */` |
|       - |  7933 | `#define PH7_THROWABLE_WALK_MAX_DEPTH 64` |
|    8856 |  7934 | `static int GenStateInterfaceIsThrowableAt(ph7_class *pInterface,int iDepth)` |
|       2 |  7935 |  |
|       - |  7936 | `	ph7_class **apParent;` |
|       - |  7937 | `	sxu32 n;` |
|   11838 |  7938 | `	while( pInterface ){` |
|    8864 |  7939 | `		if( iDepth > PH7_THROWABLE_WALK_MAX_DEPTH ){` |
|     ! 0 |  7940 | `			return FALSE;` |
|       - |  7941 | `		}` |
|   11808 |  7942 | `		if( pInterface->sName.nByte == sizeof("Throwable")-1 &&` |
|    5888 |  7943 | `			SyMemcmp(pInterface->sName.zString,"Throwable",sizeof("Throwable")-1) == 0 ){` |
|    5884 |  7944 | `			return TRUE;` |
|       - |  7945 | `		}` |
|    2982 |  7946 | `		apParent = (ph7_class **)SySetBasePtr(&pInterface->aInterface);` |
|    2982 |  7947 | `		for( n = 0 ; n < SySetUsed(&pInterface->aInterface) ; ++n ){` |
|     ! 0 |  7948 | `			if( GenStateInterfaceIsThrowableAt(apParent[n],iDepth+1) ){` |
|     ! 0 |  7949 | `				return TRUE;` |
|       - |  7950 | `			}` |
|     ! 0 |  7951 | `		}` |
|    2982 |  7952 | `		pInterface = pInterface->pBase;` |
|    2982 |  7953 | `		iDepth++;` |
|       2 |  7954 | `	}` |
|    2976 |  7955 | `	return FALSE;` |
|    4430 |  7956 |  |
|    8856 |  7957 | `static int GenStateInterfaceIsThrowable(ph7_class *pInterface)` |
|       2 |  7958 |  |
|    8858 |  7959 | `	return GenStateInterfaceIsThrowableAt(pInterface,0);` |
|       2 |  7960 |  |
|       - |  7961 | `/*` |
|       - |  7962 | ` * Return TRUE if pBase is (or transitively extends) the Exception or Error` |
|       - |  7963 | ` * base class. Used to enforce that user classes can only acquire Throwable` |
|       - |  7964 | `` * via `extends Exception` / `extends Error`, matching PHP 7+ behavior.`` |
|       - |  7965 | ` */` |
|    5882 |  7966 | `static int GenStateClassIsExceptionOrError(ph7_class *pBase)` |
|       2 |  7967 |  |
|    5888 |  7968 | `	while( pBase ){` |
|      10 |  7969 | `		if( pBase->sName.nByte == sizeof("Exception")-1 &&` |
|       2 |  7970 | `			SyMemcmp(pBase->sName.zString,"Exception",sizeof("Exception")-1) == 0 ){` |
|       3 |  7971 | `			return TRUE;` |
|       - |  7972 | `		}` |
|      10 |  7973 | `		if( pBase->sName.nByte == sizeof("Error")-1 &&` |
|       6 |  7974 | `			SyMemcmp(pBase->sName.zString,"Error",sizeof("Error")-1) == 0 ){` |
|       3 |  7975 | `			return TRUE;` |
|       - |  7976 | `		}` |
|       5 |  7977 | `		pBase = pBase->pBase;` |
|       1 |  7978 | `	}` |
|    5880 |  7979 | `	return FALSE;` |
|    2943 |  7980 |  |
|   41862 |  7981 | `static sxi32 GenStateCompileClass(ph7_gen_state *pGen,sxi32 iFlags)` |
|       2 |  7982 |  |
|   41864 |  7983 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  7984 | `	ph7_class *pClass,*pBase;` |
|       - |  7985 | `	SyToken *pEnd,*pTmp;` |
|       - |  7986 | `	sxi32 iProtection;` |
|       - |  7987 | `	SySet aInterfaces;` |
|       - |  7988 | `	SySet aUseEntries;` |
|       - |  7989 | `	sxi32 iAttrflags;` |
|       - |  7990 | `	SyString *pName;` |
|       - |  7991 | `	sxi32 nKwrd;` |
|       - |  7992 | `	sxi32 rc;` |
|       - |  7993 | `	/* Jump the 'class' keyword */` |
|   41864 |  7994 | `	pGen->pIn++;` |
|   41864 |  7995 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|       - |  7996 | `		/* Syntax error */` |
|     ! 0 |  7997 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid class name");` |
|     ! 0 |  7998 | `		if( rc == SXERR_ABORT ){` |
|       - |  7999 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8000 | `			return SXERR_ABORT;` |
|       - |  8001 | `		}` |
|       - |  8002 | `		/* Synchronize with the first semi-colon or curly braces */` |
|     ! 0 |  8003 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_SEMI/*';'*/)) == 0 ){` |
|     ! 0 |  8004 | `			pGen->pIn++;` |
|     ! 0 |  8005 | `		}` |
|     ! 0 |  8006 | `		return SXRET_OK;` |
|       - |  8007 | `	}` |
|       - |  8008 | `	/* Extract class name */` |
|   41864 |  8009 | `	pName = &pGen->pIn->sData;` |
|       - |  8010 | `	/* Advance the stream cursor */` |
|   41864 |  8011 | `	pGen->pIn++;` |
|       - |  8012 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8013 | `		SyBlob sFQN;` |
|       - |  8014 | `		SyString sFQNStr;` |
|   41864 |  8015 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|   41864 |  8016 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|   41864 |  8017 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|   41864 |  8018 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|   41864 |  8019 | `		SyBlobRelease(&sFQN);` |
|       - |  8020 | `	}` |
|   41864 |  8021 | `	if( pClass == 0 ){` |
|     ! 0 |  8022 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8023 | `		return SXERR_ABORT;` |
|       - |  8024 | `	}` |
|       - |  8025 | `	/* implemented interfaces and per-use-statement trait containers */` |
|   41864 |  8026 | `	SySetInit(&aInterfaces,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|   41864 |  8027 | `	SySetInit(&aUseEntries,&pGen->pVm->sAllocator,sizeof(TraitUseEntry));` |
|       - |  8028 | `	/* Assume a standalone class */` |
|   41864 |  8029 | `	pBase = 0;` |
|   41864 |  8030 | `	if( pGen->pIn < pGen->pEnd  && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|   32478 |  8031 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   32478 |  8032 | `		if( nKwrd == PH7_TKWRD_EXTENDS /* class b extends a */ ){` |
|       - |  8033 | `			SyBlob sResolved;` |
|       - |  8034 | `			SyString sBaseName;` |
|       - |  8035 | `			sxu32 nRefLine;` |
|   23628 |  8036 | `			pGen->pIn++; /* Advance past 'extends' */` |
|   23628 |  8037 | `			nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|   23628 |  8038 | `			SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|   23628 |  8039 | `			if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       3 |  8040 | `				SyBlobRelease(&sResolved);` |
|       4 |  8041 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8042 | `					"Expected 'class_name' after 'extends' keyword inside class '%z'",` |
|       1 |  8043 | `					pName);` |
|       3 |  8044 | `				SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|       3 |  8045 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8046 | `					return SXERR_ABORT;` |
|       - |  8047 | `				}` |
|       3 |  8048 | `				return SXRET_OK;` |
|       - |  8049 | `			}` |
|   35438 |  8050 | `			pBase = PH7_VmExtractClass(pGen->pVm,` |
|   23624 |  8051 | `				(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|   23626 |  8052 | `			SyStringInitFromBuf(&sBaseName,` |
|       - |  8053 | `				(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8054 | `			/* Interfaces are not allowed */` |
|   23626 |  8055 | `			while( pBase && (pBase->iFlags & PH7_CLASS_INTERFACE) ){` |
|     ! 0 |  8056 | `				pBase = pBase->pNextName;` |
|     ! 0 |  8057 | `			}` |
|   23626 |  8058 | `			if( pBase == 0 ){` |
|     ! 0 |  8059 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8060 | `					"Nonexistent base class '%z'",&sBaseName);` |
|     ! 0 |  8061 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8062 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8063 | `					return SXERR_ABORT;` |
|       - |  8064 | `				}` |
|     ! 0 |  8065 | `			}else{` |
|   23626 |  8066 | `				if( pBase->iFlags & PH7_CLASS_FINAL ){` |
|     ! 0 |  8067 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|     ! 0 |  8068 | `						"Class '%z' may not inherit from final class '%z'",pName,&pBase->sName);` |
|     ! 0 |  8069 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8070 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8071 | `						return SXERR_ABORT;` |
|       - |  8072 | `					}` |
|     ! 0 |  8073 | `				}` |
|       - |  8074 | `			}` |
|   23626 |  8075 | `			SyBlobRelease(&sResolved);` |
|   11812 |  8076 | `		}` |
|   32476 |  8077 | `		if (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) && SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_IMPLEMENTS ){` |
|       - |  8078 | `			ph7_class *pInterface;` |
|       - |  8079 | `			/* Interface implementation */` |
|    8858 |  8080 | `			pGen->pIn++; /* Advance the stream cursor */` |
|    4428 |  8081 | `			for(;;){` |
|       - |  8082 | `				SyBlob sResolved;` |
|       - |  8083 | `				SyString sIntName;` |
|       - |  8084 | `				sxu32 nRefLine;` |
|    8858 |  8085 | `				nRefLine = (pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : nLine;` |
|    8858 |  8086 | `				SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    8858 |  8087 | `				if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|     ! 0 |  8088 | `					SyBlobRelease(&sResolved);` |
|     ! 0 |  8089 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|       - |  8090 | `						"Expected 'interface_name' after 'implements' keyword inside class '%z' declaration",` |
|     ! 0 |  8091 | `						pName);` |
|     ! 0 |  8092 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8093 | `						return SXERR_ABORT;` |
|       - |  8094 | `					}` |
|     ! 0 |  8095 | `					break;` |
|       - |  8096 | `				}` |
|   17714 |  8097 | `				pInterface = PH7_VmExtractClass(pGen->pVm,` |
|    8856 |  8098 | `					(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|    8858 |  8099 | `				SyStringInitFromBuf(&sIntName,` |
|       - |  8100 | `					(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       - |  8101 | `				/* Only interfaces are allowed */` |
|    8858 |  8102 | `				while( pInterface && (pInterface->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     ! 0 |  8103 | `					pInterface = pInterface->pNextName;` |
|     ! 0 |  8104 | `				}` |
|    8858 |  8105 | `				if( pInterface == 0 ){` |
|     ! 0 |  8106 | `					rc = PH7_GenCompileError(pGen,E_ERROR,nRefLine,` |
|       - |  8107 | `						"Nonexistent base interface '%z'",&sIntName);` |
|     ! 0 |  8108 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8109 | `						SyBlobRelease(&sResolved);` |
|     ! 0 |  8110 | `						return SXERR_ABORT;` |
|       - |  8111 | `					}` |
|     ! 0 |  8112 | `				}else{` |
|       - |  8113 | `					/* Reject user classes that try to implement Throwable` |
|       - |  8114 | `					 * directly (or via an interface that extends Throwable)` |
|       - |  8115 | `					 * unless they already extend Exception or Error.` |
|       - |  8116 | `					 * Exception and Error themselves are compiled from the` |
|       - |  8117 | `					 * built-in library and are exempt by FQN — a namespaced` |
|       - |  8118 | ``					 * `Foo\Exception` is a different class and not exempt. */`` |
|    8858 |  8119 | `					SyString *pFqn = &pClass->sName;` |
|    8858 |  8120 | `					int bIsExceptionOrError =` |
|    7364 |  8121 | `						(pFqn->nByte == sizeof("Exception")-1 &&` |
|   14752 |  8122 | `						 SyMemcmp(pFqn->zString,"Exception",sizeof("Exception")-1) == 0) \|\|` |
|    7391 |  8123 | `						(pFqn->nByte == sizeof("Error")-1 &&` |
|    2942 |  8124 | `						 SyMemcmp(pFqn->zString,"Error",sizeof("Error")-1) == 0);` |
|   14736 |  8125 | `					if( GenStateInterfaceIsThrowable(pInterface) &&` |
|    8823 |  8126 | `						!GenStateClassIsExceptionOrError(pBase) &&` |
|    2939 |  8127 | `						!bIsExceptionOrError ){` |
|      10 |  8128 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8129 | `							"Class %z cannot implement interface Throwable, extend Exception or Error instead",` |
|       3 |  8130 | `							&pClass->sName);` |
|       7 |  8131 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8132 | `							SyBlobRelease(&sResolved);` |
|     ! 0 |  8133 | `							return SXERR_ABORT;` |
|       - |  8134 | `						}` |
|       - |  8135 | `						/* Skip registration so the follow-up abstract-method` |
|       - |  8136 | `						 * check does not produce a duplicate fatal. */` |
|       4 |  8137 | `					}else{` |
|    8852 |  8138 | `						SySetPut(&aInterfaces,(const void *)&pInterface);` |
|       - |  8139 | `					}` |
|       - |  8140 | `				}` |
|    8858 |  8141 | `				SyBlobRelease(&sResolved);` |
|    8858 |  8142 | `				if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|    4430 |  8143 | `					break;` |
|       - |  8144 | `				}` |
|     ! 0 |  8145 | `				pGen->pIn++;/* Jump the comma */` |
|     ! 0 |  8146 | `			}` |
|    4428 |  8147 | `		}` |
|   16237 |  8148 | `	}` |
|   41862 |  8149 | `	if( pGen->pIn >= pGen->pEnd  \|\| (pGen->pIn->nType & PH7_TK_OCB /*'{'*/) == 0 ){` |
|       - |  8150 | `		/* Syntax error */` |
|     ! 0 |  8151 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after class '%z' declaration",pName);` |
|     ! 0 |  8152 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8153 | `		if( rc == SXERR_ABORT ){` |
|       - |  8154 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8155 | `			return SXERR_ABORT;` |
|       - |  8156 | `		}` |
|     ! 0 |  8157 | `		return SXRET_OK;` |
|       - |  8158 | `	}` |
|   41862 |  8159 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|   41862 |  8160 | `	pEnd = 0; /* cc warning */` |
|       - |  8161 | `	/* Delimit the class body */` |
|   41862 |  8162 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB/*'{'*/,PH7_TK_CCB/*'}'*/,&pEnd);` |
|   41862 |  8163 | `	if( pEnd >= pGen->pEnd ){` |
|       - |  8164 | `		/* Syntax error */` |
|     ! 0 |  8165 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces'}' after class '%z' definition",pName);` |
|     ! 0 |  8166 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8167 | `		if( rc == SXERR_ABORT ){` |
|       - |  8168 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  8169 | `			return SXERR_ABORT;` |
|       - |  8170 | `		}` |
|     ! 0 |  8171 | `		return SXRET_OK;` |
|       - |  8172 | `	}` |
|       - |  8173 | `	/* Swap token stream */` |
|   41862 |  8174 | `	pTmp = pGen->pEnd;` |
|   41862 |  8175 | `	pGen->pEnd = pEnd;` |
|       - |  8176 | `	/* Set the inherited flags */` |
|   41862 |  8177 | `	pClass->iFlags = iFlags;` |
|       - |  8178 | `	/* Start the parse process */` |
|   93114 |  8179 | `	for(;;){` |
|       - |  8180 | `		/* Jump leading/trailing semi-colons */` |
|  298726 |  8181 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI/*';'*/) ){` |
|   56268 |  8182 | `			pGen->pIn++;` |
|       2 |  8183 | `		}` |
|  242460 |  8184 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  8185 | `			/* End of class body */` |
|   41848 |  8186 | `			break;` |
|       - |  8187 | `		}` |
|  200614 |  8188 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8189 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8190 | `				"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8191 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8192 | `			if( rc == SXERR_ABORT ){` |
|       - |  8193 | `				/* Error count limit reached,abort immediately */` |
|     ! 0 |  8194 | `				return SXERR_ABORT;` |
|       - |  8195 | `			}` |
|     ! 0 |  8196 | `			goto done;` |
|       - |  8197 | `		}` |
|       - |  8198 | `		/* Assume public visibility */` |
|  200614 |  8199 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|  200614 |  8200 | `		iAttrflags = 0;` |
|  200614 |  8201 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       - |  8202 | `			/* Extract the current keyword */` |
|  200614 |  8203 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  200614 |  8204 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8205 | `				/* Trait use: use TraitA, TraitB [{ ... }]; */` |
|       - |  8206 | `				TraitUseEntry sUse;` |
|      44 |  8207 | `				SySetInit(&sUse.aTraits,&pGen->pVm->sAllocator,sizeof(ph7_class *));` |
|      44 |  8208 | `				sUse.pResolvStart = sUse.pResolvEnd = 0;` |
|      44 |  8209 | `				pGen->pIn++; /* Jump the 'use' keyword */` |
|      29 |  8210 | `				for(;;){` |
|       - |  8211 | `					ph7_class *pTrait;` |
|       - |  8212 | `					SyString *pTraitName;` |
|      52 |  8213 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8214 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8215 | `							"Expected trait name after 'use' inside class '%z'",pName);` |
|     ! 0 |  8216 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8217 | `							return SXERR_ABORT;` |
|       - |  8218 | `						}` |
|     ! 0 |  8219 | `						break;` |
|       - |  8220 | `					}` |
|      52 |  8221 | `					pTraitName = &pGen->pIn->sData;` |
|       - |  8222 | `					/* Resolve trait name through namespace/imports */ {` |
|       - |  8223 | `						SyBlob sResolved;` |
|      52 |  8224 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|      52 |  8225 | `						GenStateResolveName(pGen,pTraitName,&sResolved);` |
|     102 |  8226 | `						pTrait = PH7_VmExtractClass(pGen->pVm,` |
|      50 |  8227 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|      52 |  8228 | `						SyBlobRelease(&sResolved);` |
|       - |  8229 | `					}` |
|       - |  8230 | `					/* Only traits are allowed */` |
|      52 |  8231 | `					while( pTrait && (pTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8232 | `						pTrait = pTrait->pNextName;` |
|     ! 0 |  8233 | `					}` |
|      52 |  8234 | `					if( pTrait == 0 ){` |
|     ! 0 |  8235 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8236 | `							"'%z' is not a trait",pTraitName);` |
|     ! 0 |  8237 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8238 | `							return SXERR_ABORT;` |
|       - |  8239 | `						}` |
|     ! 0 |  8240 | `					}else{` |
|      52 |  8241 | `						SySetPut(&sUse.aTraits,(const void *)&pTrait);` |
|       - |  8242 | `					}` |
|      52 |  8243 | `					pGen->pIn++; /* Advance past trait name */` |
|      52 |  8244 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|      23 |  8245 | `						break;` |
|       - |  8246 | `					}` |
|       9 |  8247 | `					pGen->pIn++; /* Jump the comma */` |
|       1 |  8248 | `				}` |
|       - |  8249 | `				/* Expect semicolon or opening brace (for conflict resolution) */` |
|      44 |  8250 | `				if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ){` |
|       - |  8251 | `					SyToken *pBlock;` |
|       9 |  8252 | `					pGen->pIn++; /* Jump '{' */` |
|       9 |  8253 | `					PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBlock);` |
|       9 |  8254 | `					sUse.pResolvStart = pGen->pIn;` |
|       9 |  8255 | `					sUse.pResolvEnd = pBlock;` |
|       9 |  8256 | `					if( pBlock < pGen->pEnd ){` |
|       9 |  8257 | `						pGen->pIn = &pBlock[1]; /* Skip past '}' */` |
|       5 |  8258 | `					}else{` |
|     ! 0 |  8259 | `						pGen->pIn = pGen->pEnd;` |
|       - |  8260 | `					}` |
|       4 |  8261 | `				}` |
|      44 |  8262 | `				SySetPut(&aUseEntries,(const void *)&sUse);` |
|       - |  8263 | `				/* The semicolon will be consumed by the outer loop */` |
|      44 |  8264 | `				continue;` |
|       - |  8265 | `			}` |
|  200572 |  8266 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|  197516 |  8267 | `				iProtection = nKwrd;` |
|  197516 |  8268 | `				pGen->pIn++; /* Jump the visibility token */` |
|  197514 |  8269 | `				if( pGen->pIn >= pGen->pEnd` |
|  197516 |  8270 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8271 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8272 | `						"Unexpected token '%z'. Expecting attribute declaration inside class '%z'",` |
|     ! 0 |  8273 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8274 | `					if( rc == SXERR_ABORT ){` |
|       - |  8275 | `						/* Error count limit reached,abort immediately */` |
|     ! 0 |  8276 | `						return SXERR_ABORT;` |
|       - |  8277 | `					}` |
|     ! 0 |  8278 | `					goto done;` |
|       - |  8279 | `				}` |
|  197516 |  8280 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8281 | `					/* Attribute declaration (untyped) */` |
|   56078 |  8282 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|   56078 |  8283 | `					if( rc != SXRET_OK ){` |
|       3 |  8284 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8285 | `							return SXERR_ABORT;` |
|       - |  8286 | `						}` |
|       3 |  8287 | `						goto done;` |
|       - |  8288 | `					}` |
|   56076 |  8289 | `					continue;` |
|       - |  8290 | `				}` |
|  141440 |  8291 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8292 | `					/* Typed attribute declaration (PHP 7.4+) */` |
|     106 |  8293 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     106 |  8294 | `					if( rc != SXRET_OK ){` |
|       3 |  8295 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8296 | `							return SXERR_ABORT;` |
|       - |  8297 | `						}` |
|       3 |  8298 | `						goto done;` |
|       - |  8299 | `					}` |
|     104 |  8300 | `					continue;` |
|       - |  8301 | `				}` |
|       - |  8302 | `				/* Extract the keyword */` |
|  141336 |  8303 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   70667 |  8304 | `			}` |
|  144392 |  8305 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|       - |  8306 | `				/* Process constant declaration */` |
|      30 |  8307 | `				rc = GenStateCompileClassConstant(&(*pGen),iProtection,iAttrflags,pClass);` |
|      30 |  8308 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  8309 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8310 | `						return SXERR_ABORT;` |
|       - |  8311 | `					}` |
|     ! 0 |  8312 | `					goto done;` |
|       - |  8313 | `				}` |
|      16 |  8314 | `			}else{` |
|  144364 |  8315 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       - |  8316 | `					/* Static method or attribute,record that */` |
|    2976 |  8317 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|    2976 |  8318 | `					pGen->pIn++; /* Jump the static keyword */` |
|    2976 |  8319 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8320 | `						/* Extract the keyword */` |
|    2970 |  8321 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|    2970 |  8322 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8323 | `							iProtection = nKwrd;` |
|     ! 0 |  8324 | `							pGen->pIn++; /* Jump the visibility token */` |
|     ! 0 |  8325 | `						}` |
|    1484 |  8326 | `					}` |
|    2974 |  8327 | `					if( pGen->pIn >= pGen->pEnd` |
|    2976 |  8328 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8329 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8330 | `							"Unexpected token '%z',Expecting method,attribute or constant declaration inside class '%z'",` |
|     ! 0 |  8331 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8332 | `						if( rc == SXERR_ABORT ){` |
|       - |  8333 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8334 | `							return SXERR_ABORT;` |
|       - |  8335 | `						}` |
|     ! 0 |  8336 | `						goto done;` |
|       - |  8337 | `					}` |
|    2976 |  8338 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       - |  8339 | `						/* Attribute declaration */` |
|       5 |  8340 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8341 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8342 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8343 | `								return SXERR_ABORT;` |
|       - |  8344 | `							}` |
|     ! 0 |  8345 | `							goto done;` |
|       - |  8346 | `						}` |
|       5 |  8347 | `						continue;` |
|       - |  8348 | `					}` |
|    2972 |  8349 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       - |  8350 | `						/* Typed static attribute declaration */` |
|      10 |  8351 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      10 |  8352 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8353 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8354 | `								return SXERR_ABORT;` |
|       - |  8355 | `							}` |
|     ! 0 |  8356 | `							goto done;` |
|       - |  8357 | `						}` |
|      10 |  8358 | `						continue;` |
|       - |  8359 | `					}` |
|       - |  8360 | `					/* Extract the keyword */` |
|    2964 |  8361 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  142871 |  8362 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       - |  8363 | `					/* Abstract method,record that */` |
|      12 |  8364 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       - |  8365 | `					/* Mark the whole class as abstract */` |
|      12 |  8366 | `					pClass->iFlags \|= PH7_CLASS_ABSTRACT;` |
|       - |  8367 | `					/* Advance the stream cursor */` |
|      12 |  8368 | `					pGen->pIn++;` |
|      12 |  8369 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|      12 |  8370 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      12 |  8371 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      10 |  8372 | `							iProtection = nKwrd;` |
|      10 |  8373 | `							pGen->pIn++; /* Jump the visibility token */` |
|       4 |  8374 | `						}` |
|       5 |  8375 | `					}` |
|      12 |  8376 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      10 |  8377 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8378 | `							/* Static method */` |
|     ! 0 |  8379 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8380 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8381 | `					}` |
|      12 |  8382 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|      10 |  8383 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8384 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8385 | `								"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside class '%z'",` |
|     ! 0 |  8386 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8387 | `							if( rc == SXERR_ABORT ){` |
|       - |  8388 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8389 | `								return SXERR_ABORT;` |
|       - |  8390 | `							}` |
|     ! 0 |  8391 | `							goto done;` |
|       - |  8392 | `					}` |
|      12 |  8393 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|  141385 |  8394 | `				}else if( nKwrd == PH7_TKWRD_FINAL ){` |
|       - |  8395 | `					/* final method ,record that */` |
|       5 |  8396 | `					iAttrflags \|= PH7_CLASS_ATTR_FINAL;` |
|       5 |  8397 | `					pGen->pIn++; /* Jump the final keyword */` |
|       5 |  8398 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       - |  8399 | `						/* Extract the keyword */` |
|       5 |  8400 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8401 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8402 | `							iProtection = nKwrd;` |
|       5 |  8403 | `							pGen->pIn++; /* Jump the visibility token */` |
|       2 |  8404 | `						}` |
|       2 |  8405 | `					}` |
|       5 |  8406 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|       4 |  8407 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  8408 | `							/* Static method */` |
|     ! 0 |  8409 | `							iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|     ! 0 |  8410 | `							pGen->pIn++; /* Jump the static keyword */` |
|     ! 0 |  8411 | `					}` |
|       5 |  8412 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8413 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8414 | `							rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8415 | `								"Unexpected token '%z',Expecting method declaration after 'final' keyword inside class '%z'",` |
|     ! 0 |  8416 | `								&pGen->pIn->sData,pName);` |
|     ! 0 |  8417 | `							if( rc == SXERR_ABORT ){` |
|       - |  8418 | `								/* Error count limit reached,abort immediately */` |
|     ! 0 |  8419 | `								return SXERR_ABORT;` |
|       - |  8420 | `							}` |
|     ! 0 |  8421 | `							goto done;` |
|       - |  8422 | `					}` |
|       5 |  8423 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  8424 | `				}` |
|  144352 |  8425 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  8426 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8427 | `						"Unexpected token '%z',Expecting method declaration inside class '%z'",` |
|     ! 0 |  8428 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8429 | `						if( rc == SXERR_ABORT ){` |
|       - |  8430 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8431 | `							return SXERR_ABORT;` |
|       - |  8432 | `						}` |
|     ! 0 |  8433 | `						goto done;` |
|       - |  8434 | `				}` |
|  144352 |  8435 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|       7 |  8436 | `					pGen->pIn++; /* Jump the 'var' keyword */` |
|       7 |  8437 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR/*'$'*/) == 0){` |
|     ! 0 |  8438 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8439 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  8440 | `						if( rc == SXERR_ABORT ){` |
|       - |  8441 | `							/* Error count limit reached,abort immediately */` |
|     ! 0 |  8442 | `							return SXERR_ABORT;` |
|       - |  8443 | `						}` |
|     ! 0 |  8444 | `						goto done;` |
|       - |  8445 | `					}` |
|       - |  8446 | `					/* Attribute declaration */` |
|       7 |  8447 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       4 |  8448 | `				}else{` |
|       - |  8449 | `					/* Process method declaration */` |
|  144346 |  8450 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  8451 | `				}` |
|  144352 |  8452 | `				if( rc != SXRET_OK ){` |
|      11 |  8453 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8454 | `						return SXERR_ABORT;` |
|       - |  8455 | `					}` |
|      11 |  8456 | `					goto done;` |
|       - |  8457 | `				}` |
|       - |  8458 | `			}` |
|   72186 |  8459 | `		}else{` |
|       - |  8460 | `			/* Attribute declaration */` |
|     ! 0 |  8461 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8462 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8463 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8464 | `					return SXERR_ABORT;` |
|       - |  8465 | `				}` |
|     ! 0 |  8466 | `				goto done;` |
|       - |  8467 | `			}` |
|       - |  8468 | `		}` |
|       2 |  8469 | `	}` |
|       - |  8470 | `	/* Apply collected traits (per use-statement) before installing the class.` |
|       - |  8471 | `	 * Each use-statement carries its own set of traits and optional resolution block.` |
|       - |  8472 | `	 */` |
|       - |  8473 | `	{` |
|       - |  8474 | `		TraitUseEntry *apUse;` |
|       - |  8475 | `		sxu32 nU;` |
|   41848 |  8476 | `		apUse = (TraitUseEntry *)SySetBasePtr(&aUseEntries);` |
|   41890 |  8477 | `		for( nU = 0 ; nU < SySetUsed(&aUseEntries) ; nU++ ){` |
|      44 |  8478 | `			TraitUseEntry *pUse = &apUse[nU];` |
|      44 |  8479 | `			ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pUse->aTraits);` |
|      44 |  8480 | `			sxu32 nTraits = SySetUsed(&pUse->aTraits);` |
|      44 |  8481 | `			int hasResolution = (pUse->pResolvStart && pUse->pResolvStart < pUse->pResolvEnd) ? 1 : 0;` |
|       - |  8482 | `			sxu32 nT;` |
|      44 |  8483 | `			if( !hasResolution ){` |
|       - |  8484 | `				/* No conflict resolution block: use standard trait application */` |
|      76 |  8485 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      42 |  8486 | `					rc = PH7_ClassUseTrait(&(*pGen),pClass,apTrait[nT]);` |
|      42 |  8487 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8488 | `						break;` |
|       - |  8489 | `					}` |
|      22 |  8490 | `				}` |
|      19 |  8491 | `			}else{` |
|       - |  8492 | `				/* With resolution block: copy attributes, record traits,` |
|       - |  8493 | `				 * then use the block to resolve method conflicts.` |
|       - |  8494 | `				 */` |
|       - |  8495 | `				SyToken *pR;` |
|      19 |  8496 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|      11 |  8497 | `					ph7_class *pTR = apTrait[nT];` |
|       - |  8498 | `					ph7_class_attr *pAR;` |
|       - |  8499 | `					SyHashEntry *pER;` |
|       - |  8500 | `					SyString *pNR;` |
|      11 |  8501 | `					SyHashResetLoopCursor(&pTR->hAttr);` |
|      16 |  8502 | `					while((pER = SyHashGetNextEntry(&pTR->hAttr)) != 0 ){` |
|     ! 0 |  8503 | `						pAR = (ph7_class_attr *)pER->pUserData;` |
|     ! 0 |  8504 | `						pNR = &pAR->sName;` |
|     ! 0 |  8505 | `						if( SyHashGet(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|     ! 0 |  8506 | `							SyHashInsert(&pClass->hAttr,(const void *)pNR->zString,pNR->nByte,pAR);` |
|     ! 0 |  8507 | `						}` |
|     ! 0 |  8508 | `					}` |
|      11 |  8509 | `					SySetPut(&pClass->aTrait,(const void *)&pTR);` |
|       6 |  8510 | `				}` |
|       - |  8511 | `				/* Pass 1: process insteadof rules to install winning methods */` |
|       9 |  8512 | `				pR = pUse->pResolvStart;` |
|      21 |  8513 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8514 | `					SyString sTrait,sMethod;` |
|       - |  8515 | `					ph7_class *pSrcTrait;` |
|       - |  8516 | `					ph7_class_method *pMeth;` |
|       - |  8517 | `					sxi32 nRKwrd;` |
|      33 |  8518 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8519 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8520 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8521 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8522 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8523 | `					sMethod = pR->sData;` |
|      13 |  8524 | `					pR++;` |
|      13 |  8525 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8526 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8527 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8528 | `							sTrait = sMethod;` |
|       7 |  8529 | `							pR++;` |
|       7 |  8530 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8531 | `							sMethod = pR->sData;` |
|       7 |  8532 | `							pR++;` |
|       3 |  8533 | `						}` |
|       3 |  8534 | `					}` |
|      13 |  8535 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8536 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8537 | `						continue;` |
|       - |  8538 | `					}` |
|      13 |  8539 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8540 | `					pR++;` |
|      13 |  8541 | `					if( nRKwrd == PH7_TKWRD_INSTEADOF && sTrait.nByte > 0 ){` |
|       5 |  8542 | `						pSrcTrait = 0;` |
|       7 |  8543 | `						for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       7 |  8544 | `							SyString *pTN = &apTrait[nT]->sName;` |
|      10 |  8545 | `							if( pTN->nByte >= sTrait.nByte &&` |
|       6 |  8546 | `								SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       5 |  8547 | `								pSrcTrait = apTrait[nT];` |
|       5 |  8548 | `								break;` |
|       - |  8549 | `							}` |
|       2 |  8550 | `						}` |
|       5 |  8551 | `						if( pSrcTrait ){` |
|       5 |  8552 | `							pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       5 |  8553 | `							if( pMeth ){` |
|       5 |  8554 | `								SyString *pMN = &pMeth->sFunc.sName;` |
|       5 |  8555 | `								if( SyHashGet(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte) == 0 ){` |
|       5 |  8556 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pMeth);` |
|       2 |  8557 | `								}` |
|       2 |  8558 | `							}` |
|       2 |  8559 | `						}` |
|       2 |  8560 | `					}` |
|      29 |  8561 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8562 | `				}` |
|       - |  8563 | `				/* Install remaining non-conflicting methods from this use's traits */` |
|      19 |  8564 | `				for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       - |  8565 | `					ph7_class_method *pMR;` |
|       - |  8566 | `					SyHashEntry *pER;` |
|       - |  8567 | `					SyString *pNR;` |
|      11 |  8568 | `					SyHashResetLoopCursor(&apTrait[nT]->hMethod);` |
|      34 |  8569 | `					while((pER = SyHashGetNextEntry(&apTrait[nT]->hMethod)) != 0 ){` |
|      19 |  8570 | `						pMR = (ph7_class_method *)pER->pUserData;` |
|      19 |  8571 | `						pNR = &pMR->sFunc.sName;` |
|      19 |  8572 | `						if( SyHashGet(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte) == 0 ){` |
|      11 |  8573 | `							SyHashInsert(&pClass->hMethod,(const void *)pNR->zString,pNR->nByte,pMR);` |
|       5 |  8574 | `						}` |
|       1 |  8575 | `					}` |
|       6 |  8576 | `				}` |
|       - |  8577 | `				/* Pass 2: process as rules (aliases and visibility changes) */` |
|       9 |  8578 | `				pR = pUse->pResolvStart;` |
|      21 |  8579 | `				while( pR < pUse->pResolvEnd ){` |
|       - |  8580 | `					SyString sTrait,sMethod,sAlias;` |
|       - |  8581 | `					ph7_class *pSrcTrait;` |
|       - |  8582 | `					ph7_class_method *pMeth;` |
|      21 |  8583 | `					int hasQual = 0;` |
|       - |  8584 | `					sxi32 nRKwrd;` |
|      33 |  8585 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) ){ pR++; }` |
|      21 |  8586 | `					if( pR >= pUse->pResolvEnd ) break;` |
|      13 |  8587 | `					SyStringInitFromBuf(&sTrait,"",0);` |
|      13 |  8588 | `					SyStringInitFromBuf(&sMethod,"",0);` |
|      13 |  8589 | `					SyStringInitFromBuf(&sAlias,"",0);` |
|      13 |  8590 | `					if( (pR->nType & PH7_TK_ID) == 0 ){ pR++; continue; }` |
|      13 |  8591 | `					sMethod = pR->sData;` |
|      13 |  8592 | `					pR++;` |
|      13 |  8593 | `					if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_OP) ){` |
|       7 |  8594 | `						const ph7_expr_op *pOp = (const ph7_expr_op *)pR->pUserData;` |
|       7 |  8595 | `						if( pOp && pOp->iOp == EXPR_OP_DC ){` |
|       7 |  8596 | `							sTrait = sMethod;` |
|       7 |  8597 | `							hasQual = 1;` |
|       7 |  8598 | `							pR++;` |
|       7 |  8599 | `							if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_ID) == 0 ) break;` |
|       7 |  8600 | `							sMethod = pR->sData;` |
|       7 |  8601 | `							pR++;` |
|       3 |  8602 | `						}` |
|       3 |  8603 | `					}` |
|      13 |  8604 | `					if( pR >= pUse->pResolvEnd \|\| (pR->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  8605 | `						while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|     ! 0 |  8606 | `						continue;` |
|       - |  8607 | `					}` |
|      13 |  8608 | `					nRKwrd = SX_PTR_TO_INT(pR->pUserData);` |
|      13 |  8609 | `					pR++;` |
|      13 |  8610 | `					if( nRKwrd == PH7_TKWRD_AS ){` |
|       9 |  8611 | `						sxi32 iNewVis = -1;` |
|       9 |  8612 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_KEYWORD) ){` |
|       7 |  8613 | `							sxi32 nAK = SX_PTR_TO_INT(pR->pUserData);` |
|       7 |  8614 | `							if( nAK == PH7_TKWRD_PUBLIC \|\| nAK == PH7_TKWRD_PROTECTED \|\| nAK == PH7_TKWRD_PRIVATE ){` |
|       7 |  8615 | `								iNewVis = nAK;` |
|       7 |  8616 | `								pR++;` |
|       3 |  8617 | `							}` |
|       3 |  8618 | `						}` |
|       9 |  8619 | `						if( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_ID) ){` |
|       7 |  8620 | `							sAlias = pR->sData;` |
|       7 |  8621 | `							pR++;` |
|       3 |  8622 | `						}` |
|       9 |  8623 | `						pMeth = 0;` |
|       9 |  8624 | `						if( hasQual ){` |
|       3 |  8625 | `							pSrcTrait = 0;` |
|       5 |  8626 | `							for( nT = 0 ; nT < nTraits ; nT++ ){` |
|       5 |  8627 | `								SyString *pTN = &apTrait[nT]->sName;` |
|       7 |  8628 | `								if( pTN->nByte >= sTrait.nByte &&` |
|       4 |  8629 | `									SyMemcmp(&pTN->zString[pTN->nByte - sTrait.nByte],sTrait.zString,sTrait.nByte) == 0 ){` |
|       3 |  8630 | `									pSrcTrait = apTrait[nT];` |
|       3 |  8631 | `									break;` |
|       - |  8632 | `								}` |
|       2 |  8633 | `							}` |
|       3 |  8634 | `							if( pSrcTrait ){` |
|       3 |  8635 | `								pMeth = PH7_ClassExtractMethod(pSrcTrait,sMethod.zString,sMethod.nByte);` |
|       1 |  8636 | `							}` |
|       2 |  8637 | `						}else{` |
|       7 |  8638 | `							pMeth = PH7_ClassExtractMethod(pClass,sMethod.zString,sMethod.nByte);` |
|       - |  8639 | `						}` |
|       9 |  8640 | `						if( pMeth ){` |
|       9 |  8641 | `							if( sAlias.nByte > 0 ){` |
|       - |  8642 | `								/* Create a shallow copy of the method struct for the alias` |
|       - |  8643 | `								 * so it can carry its own visibility without affecting the original.` |
|       - |  8644 | `								 */` |
|       - |  8645 | `								ph7_class_method *pAlias;` |
|       - |  8646 | `								char *zAliasDup;` |
|       7 |  8647 | `								pAlias = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       7 |  8648 | `								if( pAlias ){` |
|       7 |  8649 | `									SyMemcpy(pMeth,pAlias,sizeof(ph7_class_method));` |
|       7 |  8650 | `									if( iNewVis >= 0 ){` |
|       5 |  8651 | `										if( iNewVis == PH7_TKWRD_PUBLIC ) pAlias->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8652 | `										else if( iNewVis == PH7_TKWRD_PROTECTED ) pAlias->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8653 | `										else pAlias->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       2 |  8654 | `									}` |
|       7 |  8655 | `									zAliasDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,sAlias.zString,sAlias.nByte);` |
|       7 |  8656 | `									if( zAliasDup ){` |
|       7 |  8657 | `										SyHashInsert(&pClass->hMethod,(const void *)zAliasDup,sAlias.nByte,pAlias);` |
|       3 |  8658 | `									}` |
|       4 |  8659 | `								}` |
|       6 |  8660 | `							}else if( iNewVis >= 0 ){` |
|       - |  8661 | `								/* Visibility-only change (no alias name): also needs a copy */` |
|       - |  8662 | `								ph7_class_method *pCopy;` |
|       3 |  8663 | `								pCopy = (ph7_class_method *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_class_method));` |
|       3 |  8664 | `								if( pCopy ){` |
|       3 |  8665 | `									SyString *pMN = &pMeth->sFunc.sName;` |
|       3 |  8666 | `									SyMemcpy(pMeth,pCopy,sizeof(ph7_class_method));` |
|       3 |  8667 | `									if( iNewVis == PH7_TKWRD_PUBLIC ) pCopy->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       3 |  8668 | `									else if( iNewVis == PH7_TKWRD_PROTECTED ) pCopy->iProtection = PH7_CLASS_PROT_PROTECTED;` |
|     ! 0 |  8669 | `									else pCopy->iProtection = PH7_CLASS_PROT_PRIVATE;` |
|       - |  8670 | `									/* Replace the method in the class hash */` |
|       3 |  8671 | `									SyHashDeleteEntry(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,0);` |
|       3 |  8672 | `									SyHashInsert(&pClass->hMethod,(const void *)pMN->zString,pMN->nByte,pCopy);` |
|       1 |  8673 | `								}` |
|       1 |  8674 | `							}` |
|       4 |  8675 | `						}` |
|       4 |  8676 | `						SXUNUSED(hasQual);` |
|       4 |  8677 | `					}` |
|      17 |  8678 | `					while( pR < pUse->pResolvEnd && (pR->nType & PH7_TK_SEMI) == 0 ){ pR++; }` |
|       1 |  8679 | `				}` |
|       - |  8680 | `			}` |
|      44 |  8681 | `			SySetRelease(&pUse->aTraits);` |
|      23 |  8682 | `		}` |
|       - |  8683 | `	}` |
|       - |  8684 | `	/* Install the class */` |
|   41848 |  8685 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|   41848 |  8686 | `	if( rc == SXRET_OK ){` |
|       - |  8687 | `		ph7_class **apInterface;` |
|       - |  8688 | `		sxu32 n;` |
|   41848 |  8689 | `		if( pBase ){` |
|       - |  8690 | `			/* Inherit from base class and mark as a subclass */` |
|   23626 |  8691 | `			rc = PH7_ClassInherit(&(*pGen),pClass,pBase);` |
|   11812 |  8692 | `		}` |
|   41848 |  8693 | `		apInterface = (ph7_class **)SySetBasePtr(&aInterfaces);` |
|   50698 |  8694 | `		for( n = 0 ; n < SySetUsed(&aInterfaces) ; n++ ){` |
|       - |  8695 | `			/* Implements one or more interface */` |
|    8852 |  8696 | `			rc = PH7_ClassImplement(pClass,apInterface[n]);` |
|    8852 |  8697 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  8698 | `				break;` |
|       - |  8699 | `			}` |
|    4427 |  8700 | `		}` |
|       - |  8701 | `		/* Validate interface method signatures (visibility and parameter count) */` |
|   41848 |  8702 | `		if( rc == SXRET_OK ){` |
|   41848 |  8703 | `			sxi32 rcCheck = GenStateCheckInterfaceSignatures(&(*pGen),pClass);` |
|   41848 |  8704 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8705 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8706 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8707 | `				return SXERR_ABORT;` |
|       - |  8708 | `			}` |
|   20923 |  8709 | `		}` |
|       - |  8710 | `		/* Check for unimplemented abstract methods in concrete classes */` |
|   41848 |  8711 | `		if( rc == SXRET_OK ){` |
|   41848 |  8712 | `			sxi32 rcCheck = GenStateCheckAbstractMethods(&(*pGen),pClass);` |
|   41848 |  8713 | `			if( rcCheck == SXERR_ABORT ){` |
|     ! 0 |  8714 | `				SySetRelease(&aUseEntries);` |
|     ! 0 |  8715 | `				SySetRelease(&aInterfaces);` |
|     ! 0 |  8716 | `				return SXERR_ABORT;` |
|       - |  8717 | `			}` |
|   20923 |  8718 | `		}` |
|   20923 |  8719 | `	}` |
|   41848 |  8720 | `	SySetRelease(&aUseEntries);` |
|   41848 |  8721 | `	SySetRelease(&aInterfaces);` |
|   41848 |  8722 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  8723 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8724 | `		return SXERR_ABORT;` |
|       - |  8725 | `	}` |
|   20923 |  8726 | `done:` |
|       - |  8727 | `	/* Point beyond the class body */` |
|   41862 |  8728 | `	pGen->pIn = &pEnd[1];` |
|   41862 |  8729 | `	pGen->pEnd = pTmp;` |
|   41862 |  8730 | `	return PH7_OK;` |
|   20933 |  8731 |  |
|       - |  8732 | `/*` |
|       - |  8733 | ` * Compile a user-defined abstract class.` |
|       - |  8734 | ` *  According to the PHP language reference manual` |
|       - |  8735 | ` *   PHP 5 introduces abstract classes and methods. Classes defined as abstract` |
|       - |  8736 | ` *   may not be instantiated, and any class that contains at least one abstract` |
|       - |  8737 | ` *   method must also be abstract. Methods defined as abstract simply declare` |
|       - |  8738 | ` *   the method's signature - they cannot define the implementation.` |
|       - |  8739 | ` *   When inheriting from an abstract class, all methods marked abstract in the parent's` |
|       - |  8740 | ` *   class declaration must be defined by the child; additionally, these methods must be` |
|       - |  8741 | ` *   defined with the same (or a less restricted) visibility. For example, if the abstract` |
|       - |  8742 | ` *   method is defined as protected, the function implementation must be defined as either` |
|       - |  8743 | ` *   protected or public, but not private. Furthermore the signatures of the methods must` |
|       - |  8744 | ` *   match, i.e. the type hints and the number of required arguments must be the same.` |
|       - |  8745 | ` *   This also applies to constructors as of PHP 5.4. Before 5.4 constructor signatures` |
|       - |  8746 | ` *   could differ.` |
|       - |  8747 | ` */` |
|      18 |  8748 | `static sxi32 PH7_CompileAbstractClass(ph7_gen_state *pGen)` |
|       2 |  8749 |  |
|       - |  8750 | `	sxi32 rc;` |
|      20 |  8751 | `	pGen->pIn++; /* Jump the 'abstract' keyword */` |
|      20 |  8752 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_ABSTRACT);` |
|      20 |  8753 | `	return rc;` |
|       2 |  8754 |  |
|       - |  8755 | `/*` |
|       - |  8756 | ` * Compile a user-defined final class.` |
|       - |  8757 | ` *  According to the PHP language reference manual` |
|       - |  8758 | ` *    PHP 5 introduces the final keyword, which prevents child classes from overriding` |
|       - |  8759 | ` *    a method by prefixing the definition with final. If the class itself is being defined` |
|       - |  8760 | ` *    final then it cannot be extended.` |
|       - |  8761 | ` */` |
|       2 |  8762 | `static sxi32 PH7_CompileFinalClass(ph7_gen_state *pGen)` |
|       1 |  8763 |  |
|       - |  8764 | `	sxi32 rc;` |
|       3 |  8765 | `	pGen->pIn++; /* Jump the 'final' keyword */` |
|       3 |  8766 | `	rc = GenStateCompileClass(&(*pGen),PH7_CLASS_FINAL);` |
|       3 |  8767 | `	return rc;` |
|       1 |  8768 |  |
|       - |  8769 | `/*` |
|       - |  8770 | ` * Compile a user-defined trait.` |
|       - |  8771 | ` *  Traits are similar to classes, but only intended to group functionality` |
|       - |  8772 | ` *  in a fine-grained and consistent way. It is not possible to instantiate` |
|       - |  8773 | ` *  a Trait on its own. Traits cannot extend or implement.` |
|       - |  8774 | ` */` |
|      54 |  8775 | `static sxi32 PH7_CompileTrait(ph7_gen_state *pGen)` |
|       2 |  8776 |  |
|      56 |  8777 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  8778 | `	ph7_class *pClass;` |
|       - |  8779 | `	SyToken *pEnd,*pTmp;` |
|       - |  8780 | `	sxi32 iProtection;` |
|       - |  8781 | `	sxi32 iAttrflags;` |
|       - |  8782 | `	SyString *pName;` |
|       - |  8783 | `	sxi32 nKwrd;` |
|       - |  8784 | `	sxi32 rc;` |
|       - |  8785 | `	/* Jump the 'trait' keyword */` |
|      56 |  8786 | `	pGen->pIn++;` |
|      56 |  8787 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8788 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Invalid trait name");` |
|     ! 0 |  8789 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8790 | `			return SXERR_ABORT;` |
|       - |  8791 | `		}` |
|     ! 0 |  8792 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_OCB\|PH7_TK_SEMI)) == 0 ){` |
|     ! 0 |  8793 | `			pGen->pIn++;` |
|     ! 0 |  8794 | `		}` |
|     ! 0 |  8795 | `		return SXRET_OK;` |
|       - |  8796 | `	}` |
|       - |  8797 | `	/* Extract trait name */` |
|      56 |  8798 | `	pName = &pGen->pIn->sData;` |
|      56 |  8799 | `	pGen->pIn++;` |
|       - |  8800 | `	/* Build FQN and obtain a raw class */ {` |
|       - |  8801 | `		SyBlob sFQN;` |
|       - |  8802 | `		SyString sFQNStr;` |
|      56 |  8803 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      56 |  8804 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      56 |  8805 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|      56 |  8806 | `		pClass = PH7_NewRawClass(pGen->pVm,&sFQNStr,nLine);` |
|      56 |  8807 | `		SyBlobRelease(&sFQN);` |
|       - |  8808 | `	}` |
|      56 |  8809 | `	if( pClass == 0 ){` |
|     ! 0 |  8810 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  8811 | `		return SXERR_ABORT;` |
|       - |  8812 | `	}` |
|       - |  8813 | `	/* Traits cannot extend or implement; expect opening brace directly */` |
|      56 |  8814 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  8815 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '{' after trait '%z' declaration",pName);` |
|     ! 0 |  8816 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8817 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8818 | `			return SXERR_ABORT;` |
|       - |  8819 | `		}` |
|     ! 0 |  8820 | `		return SXRET_OK;` |
|       - |  8821 | `	}` |
|      56 |  8822 | `	pGen->pIn++; /* Jump the leading curly brace */` |
|      56 |  8823 | `	pEnd = 0;` |
|      56 |  8824 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pEnd);` |
|      56 |  8825 | `	if( pEnd >= pGen->pEnd ){` |
|     ! 0 |  8826 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Missing closing braces '}' after trait '%z' definition",pName);` |
|     ! 0 |  8827 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pClass);` |
|     ! 0 |  8828 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  8829 | `			return SXERR_ABORT;` |
|       - |  8830 | `		}` |
|     ! 0 |  8831 | `		return SXRET_OK;` |
|       - |  8832 | `	}` |
|       - |  8833 | `	/* Swap token stream */` |
|      56 |  8834 | `	pTmp = pGen->pEnd;` |
|      56 |  8835 | `	pGen->pEnd = pEnd;` |
|       - |  8836 | `	/* Mark as trait */` |
|      56 |  8837 | `	pClass->iFlags = PH7_CLASS_TRAIT;` |
|       - |  8838 | `	/* Parse the body: same as a normal class (methods, attributes, visibility modifiers) */` |
|      54 |  8839 | `	for(;;){` |
|     154 |  8840 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|      26 |  8841 | `			pGen->pIn++;` |
|       2 |  8842 | `		}` |
|     130 |  8843 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      56 |  8844 | `			break;` |
|       - |  8845 | `		}` |
|      76 |  8846 | `		if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR)) == 0 ){` |
|     ! 0 |  8847 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8848 | `				"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8849 | `				&pGen->pIn->sData,pName);` |
|     ! 0 |  8850 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  8851 | `				return SXERR_ABORT;` |
|       - |  8852 | `			}` |
|     ! 0 |  8853 | `			goto done;` |
|       - |  8854 | `		}` |
|      76 |  8855 | `		iProtection = PH7_TKWRD_PUBLIC;` |
|      76 |  8856 | `		iAttrflags = 0;` |
|      76 |  8857 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      76 |  8858 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      76 |  8859 | `			if( nKwrd == PH7_TKWRD_USE ){` |
|       - |  8860 | `				/* Trait uses another trait: use OtherTrait; */` |
|       5 |  8861 | `				pGen->pIn++; /* Jump 'use' */` |
|       2 |  8862 | `				for(;;){` |
|       - |  8863 | `					ph7_class *pUsedTrait;` |
|       - |  8864 | `					SyString *pUsedName;` |
|       5 |  8865 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ID) == 0 ){` |
|     ! 0 |  8866 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  8867 | `							"Expected trait name after 'use' inside trait '%z'",pName);` |
|     ! 0 |  8868 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8869 | `							return SXERR_ABORT;` |
|       - |  8870 | `						}` |
|     ! 0 |  8871 | `						break;` |
|       - |  8872 | `					}` |
|       5 |  8873 | `					pUsedName = &pGen->pIn->sData;` |
|       - |  8874 | `					{` |
|       - |  8875 | `						SyBlob sResolved;` |
|       5 |  8876 | `						SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       5 |  8877 | `						GenStateResolveName(pGen,pUsedName,&sResolved);` |
|       7 |  8878 | `						pUsedTrait = PH7_VmExtractClass(pGen->pVm,` |
|       4 |  8879 | `							(const char *)SyBlobData(&sResolved),(sxu32)SyBlobLength(&sResolved),FALSE,0);` |
|       5 |  8880 | `						SyBlobRelease(&sResolved);` |
|       - |  8881 | `					}` |
|       5 |  8882 | `					while( pUsedTrait && (pUsedTrait->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|     ! 0 |  8883 | `						pUsedTrait = pUsedTrait->pNextName;` |
|     ! 0 |  8884 | `					}` |
|       5 |  8885 | `					if( pUsedTrait == 0 ){` |
|       4 |  8886 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       1 |  8887 | `							"'%z' is not a trait",pUsedName);` |
|       3 |  8888 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8889 | `							return SXERR_ABORT;` |
|       - |  8890 | `						}` |
|       2 |  8891 | `					}else{` |
|       3 |  8892 | `						PH7_ClassUseTrait(&(*pGen),pClass,pUsedTrait);` |
|       - |  8893 | `					}` |
|       5 |  8894 | `					pGen->pIn++;` |
|       5 |  8895 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_COMMA) == 0 ){` |
|       3 |  8896 | `						break;` |
|       - |  8897 | `					}` |
|     ! 0 |  8898 | `					pGen->pIn++;` |
|     ! 0 |  8899 | `				}` |
|       5 |  8900 | `				continue;` |
|       - |  8901 | `			}` |
|      72 |  8902 | `			if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|      68 |  8903 | `				iProtection = nKwrd;` |
|      68 |  8904 | `				pGen->pIn++;` |
|      66 |  8905 | `				if( pGen->pIn >= pGen->pEnd` |
|      68 |  8906 | `					\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8907 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8908 | `						"Unexpected token '%z'. Expecting attribute declaration inside trait '%z'",` |
|     ! 0 |  8909 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  8910 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  8911 | `						return SXERR_ABORT;` |
|       - |  8912 | `					}` |
|     ! 0 |  8913 | `					goto done;` |
|       - |  8914 | `				}` |
|      68 |  8915 | `				if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|      11 |  8916 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|      11 |  8917 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8918 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8919 | `							return SXERR_ABORT;` |
|       - |  8920 | `						}` |
|     ! 0 |  8921 | `						goto done;` |
|       - |  8922 | `					}` |
|      11 |  8923 | `					continue;` |
|       - |  8924 | `				}` |
|      58 |  8925 | `				if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|       5 |  8926 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       5 |  8927 | `					if( rc != SXRET_OK ){` |
|     ! 0 |  8928 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8929 | `							return SXERR_ABORT;` |
|       - |  8930 | `						}` |
|     ! 0 |  8931 | `						goto done;` |
|       - |  8932 | `					}` |
|       5 |  8933 | `					continue;` |
|       - |  8934 | `				}` |
|      53 |  8935 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      26 |  8936 | `			}` |
|      57 |  8937 | `			if( nKwrd == PH7_TKWRD_CONST ){` |
|     ! 0 |  8938 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8939 | `					"Traits cannot have constants");` |
|     ! 0 |  8940 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  8941 | `					return SXERR_ABORT;` |
|       - |  8942 | `				}` |
|     ! 0 |  8943 | `				goto done;` |
|     ! 0 |  8944 | `			}else{` |
|      57 |  8945 | `				if( nKwrd == PH7_TKWRD_STATIC ){` |
|       5 |  8946 | `					iAttrflags \|= PH7_CLASS_ATTR_STATIC;` |
|       5 |  8947 | `					pGen->pIn++;` |
|       5 |  8948 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       3 |  8949 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       3 |  8950 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|     ! 0 |  8951 | `							iProtection = nKwrd;` |
|     ! 0 |  8952 | `							pGen->pIn++;` |
|     ! 0 |  8953 | `						}` |
|       1 |  8954 | `					}` |
|       4 |  8955 | `					if( pGen->pIn >= pGen->pEnd` |
|       5 |  8956 | `						\|\| (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_DOLLAR\|PH7_TK_ID\|PH7_TK_OP\|PH7_TK_NSSEP)) == 0 ){` |
|     ! 0 |  8957 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8958 | `							"Unexpected token '%z',Expecting method or attribute declaration inside trait '%z'",` |
|     ! 0 |  8959 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  8960 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  8961 | `							return SXERR_ABORT;` |
|       - |  8962 | `						}` |
|     ! 0 |  8963 | `						goto done;` |
|       - |  8964 | `					}` |
|       5 |  8965 | `					if( pGen->pIn->nType & PH7_TK_DOLLAR ){` |
|       3 |  8966 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|       3 |  8967 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8968 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8969 | `								return SXERR_ABORT;` |
|       - |  8970 | `							}` |
|     ! 0 |  8971 | `							goto done;` |
|       - |  8972 | `						}` |
|       3 |  8973 | `						continue;` |
|       - |  8974 | `					}` |
|       3 |  8975 | `					if( GenStateLooksLikeTypedProperty(pGen->pIn,pGen->pEnd) ){` |
|     ! 0 |  8976 | `						rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  8977 | `						if( rc != SXRET_OK ){` |
|     ! 0 |  8978 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  8979 | `								return SXERR_ABORT;` |
|       - |  8980 | `							}` |
|     ! 0 |  8981 | `							goto done;` |
|       - |  8982 | `						}` |
|     ! 0 |  8983 | `						continue;` |
|       - |  8984 | `					}` |
|       3 |  8985 | `					nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      54 |  8986 | `				}else if( nKwrd == PH7_TKWRD_ABSTRACT ){` |
|       5 |  8987 | `					iAttrflags \|= PH7_CLASS_ATTR_ABSTRACT;` |
|       5 |  8988 | `					pGen->pIn++;` |
|       5 |  8989 | `					if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       5 |  8990 | `						nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       5 |  8991 | `						if( nKwrd == PH7_TKWRD_PUBLIC \|\| nKwrd == PH7_TKWRD_PRIVATE \|\| nKwrd == PH7_TKWRD_PROTECTED ){` |
|       5 |  8992 | `							iProtection = nKwrd;` |
|       5 |  8993 | `							pGen->pIn++;` |
|       2 |  8994 | `						}` |
|       2 |  8995 | `					}` |
|       5 |  8996 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|       4 |  8997 | `						SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FUNCTION ){` |
|     ! 0 |  8998 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  8999 | `							"Unexpected token '%z',Expecting method declaration after 'abstract' keyword inside trait '%z'",` |
|     ! 0 |  9000 | `							&pGen->pIn->sData,pName);` |
|     ! 0 |  9001 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9002 | `							return SXERR_ABORT;` |
|       - |  9003 | `						}` |
|     ! 0 |  9004 | `						goto done;` |
|       - |  9005 | `					}` |
|       5 |  9006 | `					nKwrd = PH7_TKWRD_FUNCTION;` |
|       2 |  9007 | `				}` |
|      55 |  9008 | `				if( nKwrd != PH7_TKWRD_FUNCTION && nKwrd != PH7_TKWRD_VAR ){` |
|     ! 0 |  9009 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9010 | `						"Unexpected token '%z',Expecting method declaration inside trait '%z'",` |
|     ! 0 |  9011 | `						&pGen->pIn->sData,pName);` |
|     ! 0 |  9012 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9013 | `						return SXERR_ABORT;` |
|       - |  9014 | `					}` |
|     ! 0 |  9015 | `					goto done;` |
|       - |  9016 | `				}` |
|      55 |  9017 | `				if( nKwrd == PH7_TKWRD_VAR ){` |
|     ! 0 |  9018 | `					pGen->pIn++;` |
|     ! 0 |  9019 | `					if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|     ! 0 |  9020 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - |  9021 | `							"Expecting attribute declaration after 'var' keyword");` |
|     ! 0 |  9022 | `						if( rc == SXERR_ABORT ){` |
|     ! 0 |  9023 | `							return SXERR_ABORT;` |
|       - |  9024 | `						}` |
|     ! 0 |  9025 | `						goto done;` |
|       - |  9026 | `					}` |
|     ! 0 |  9027 | `					rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9028 | `				}else{` |
|      55 |  9029 | `					rc = GenStateCompileClassMethod(&(*pGen),iProtection,iAttrflags,TRUE,pClass);` |
|       - |  9030 | `				}` |
|      55 |  9031 | `				if( rc != SXRET_OK ){` |
|     ! 0 |  9032 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9033 | `						return SXERR_ABORT;` |
|       - |  9034 | `					}` |
|     ! 0 |  9035 | `					goto done;` |
|       - |  9036 | `				}` |
|       - |  9037 | `			}` |
|      28 |  9038 | `		}else{` |
|     ! 0 |  9039 | `			rc = GenStateCompileClassAttr(&(*pGen),iProtection,iAttrflags,pClass);` |
|     ! 0 |  9040 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9041 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9042 | `					return SXERR_ABORT;` |
|       - |  9043 | `				}` |
|     ! 0 |  9044 | `				goto done;` |
|       - |  9045 | `			}` |
|       - |  9046 | `		}` |
|       1 |  9047 | `	}` |
|       - |  9048 | `	/* Install the trait */` |
|      56 |  9049 | `	rc = PH7_VmInstallClass(pGen->pVm,pClass);` |
|      56 |  9050 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9051 | `		PH7_GenCompileError(pGen,E_ERROR,nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9052 | `		return SXERR_ABORT;` |
|       - |  9053 | `	}` |
|      27 |  9054 | `done:` |
|       - |  9055 | `	/* Point beyond the trait body */` |
|      56 |  9056 | `	pGen->pIn = &pEnd[1];` |
|      56 |  9057 | `	pGen->pEnd = pTmp;` |
|      56 |  9058 | `	return PH7_OK;` |
|      29 |  9059 |  |
|       - |  9060 | `/*` |
|       - |  9061 | ` * Compile a user-defined class.` |
|       - |  9062 | ` *  According to the PHP language reference manual` |
|       - |  9063 | ` *   Basic class definitions begin with the keyword class, followed` |
|       - |  9064 | ` *   by a class name, followed by a pair of curly braces which enclose` |
|       - |  9065 | ` *   the definitions of the properties and methods belonging to the class.` |
|       - |  9066 | ` *   A class may contain its own constants, variables (called "properties")` |
|       - |  9067 | ` *   and functions (called "methods").` |
|       - |  9068 | ` */` |
|   41842 |  9069 | `static sxi32 PH7_CompileClass(ph7_gen_state *pGen)` |
|       2 |  9070 |  |
|       - |  9071 | `	sxi32 rc;` |
|   41844 |  9072 | `	rc = GenStateCompileClass(&(*pGen),0);` |
|   41844 |  9073 | `	return rc;` |
|       2 |  9074 |  |
|       - |  9075 | `/*` |
|       - |  9076 | ` * Exception handling.` |
|       - |  9077 | ` *  According to the PHP language reference manual` |
|       - |  9078 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|       - |  9079 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|       - |  9080 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|       - |  9081 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|       - |  9082 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|       - |  9083 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|       - |  9084 | ` *    (or re-thrown) within a catch block.` |
|       - |  9085 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|       - |  9086 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|       - |  9087 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|       - |  9088 | ` *    been defined with set_exception_handler().` |
|       - |  9089 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|       - |  9090 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|       - |  9091 | ` */` |
|       - |  9092 | `/*` |
|       - |  9093 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|       - |  9094 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|       - |  9095 | ` * indicates failure.` |
|       - |  9096 | ` */` |
|    8926 |  9097 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|       2 |  9098 |  |
|    8928 |  9099 | `	sxi32 rc = SXRET_OK;` |
|    8928 |  9100 | `	if( pRoot->pOp ){` |
|    8920 |  9101 | `		switch( pRoot->pOp->iOp ){` |
|    4459 |  9102 | `		case EXPR_OP_NEW:            /* new Exception() */` |
|       - |  9103 | `		case EXPR_OP_ARROW:          /* $obj->prop */` |
|       - |  9104 | `		case EXPR_OP_NULLSAFE_ARROW: /* $obj?->prop */` |
|       - |  9105 | `		case EXPR_OP_DC:             /* Cls::$p or Cls::m() */` |
|       - |  9106 | `		case EXPR_OP_SUBSCRIPT:      /* $arr[0] */` |
|       - |  9107 | `		case EXPR_OP_FUNC_CALL:      /* fn() or $obj->m() */` |
|    8920 |  9108 | `			break;` |
|     ! 0 |  9109 | `		default:` |
|       - |  9110 | `			/* Runtime will still reject non-Throwable values; the set above` |
|       - |  9111 | `			 * covers the common shapes and gives a friendlier compile error` |
|       - |  9112 | ``			 * for obvious mistakes like `throw 5`. */`` |
|     ! 0 |  9113 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9114 | `				"throw: Expecting an exception class instance");` |
|     ! 0 |  9115 | `			if( rc != SXERR_ABORT ){` |
|     ! 0 |  9116 | `				rc = SXERR_INVALID;` |
|     ! 0 |  9117 | `			}` |
|     ! 0 |  9118 | `			break;` |
|       - |  9119 | `		}` |
|    4469 |  9120 | `	}else if( pRoot->xCode != PH7_CompileVariable ){` |
|       - |  9121 | `		/* Unexpected expression */` |
|     ! 0 |  9122 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|       - |  9123 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9124 | `		if( rc != SXERR_ABORT ){` |
|     ! 0 |  9125 | `			rc = SXERR_INVALID;` |
|     ! 0 |  9126 | `		}` |
|     ! 0 |  9127 | `	}` |
|    8928 |  9128 | `	return rc;` |
|       2 |  9129 |  |
|       - |  9130 | `/*` |
|       - |  9131 | ` * Compile a 'throw' statement.` |
|       - |  9132 | ` * throw: This is how you trigger an exception.` |
|       - |  9133 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|       - |  9134 | ` */` |
|    8890 |  9135 | `static sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|       2 |  9136 |  |
|    8892 |  9137 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9138 | `	GenBlock *pBlock;` |
|       - |  9139 | `	sxu32 nIdx;` |
|       - |  9140 | `	sxi32 rc;` |
|    8892 |  9141 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|       - |  9142 | `	/* Compile the expression */` |
|    8892 |  9143 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|    8892 |  9144 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9145 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|     ! 0 |  9146 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9147 | `			return SXERR_ABORT;` |
|       - |  9148 | `		}` |
|     ! 0 |  9149 | `		return SXRET_OK;` |
|       - |  9150 | `	}` |
|    8892 |  9151 | `	pBlock = pGen->pCurrent;` |
|       - |  9152 | `	/* Point to the top most function or try block and emit the forward jump */` |
|   41270 |  9153 | `	while(pBlock->pParent){` |
|   41266 |  9154 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|    8888 |  9155 | `			break;` |
|       - |  9156 | `		}` |
|       - |  9157 | `		/* Point to the parent block */` |
|   32380 |  9158 | `		pBlock = pBlock->pParent;` |
|       2 |  9159 | `	}` |
|       - |  9160 | `	/* Emit the throw instruction */` |
|    8892 |  9161 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       - |  9162 | `	/* Emit the jump */` |
|    8892 |  9163 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|    8892 |  9164 | `	return SXRET_OK;` |
|    4447 |  9165 |  |
|       - |  9166 | `/*` |
|       - |  9167 | ` * Compile a PHP 8.0 'throw' expression.` |
|       - |  9168 | ` * Called from the expression code generator when a 'throw' keyword is` |
|       - |  9169 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|       - |  9170 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|       - |  9171 | ` * the validator guarantees the operand is a valid exception target.` |
|       - |  9172 | ` */` |
|      36 |  9173 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|       2 |  9174 |  |
|      38 |  9175 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9176 | `	GenBlock *pBlock;` |
|       - |  9177 | `	sxu32 nIdx;` |
|       - |  9178 | `	sxi32 rc;` |
|      18 |  9179 | `	(void)iCompileFlag;` |
|      38 |  9180 | `	pGen->pIn++; /* Skip 'throw' */` |
|      38 |  9181 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 |  9182 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9183 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9184 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9185 | `			return SXERR_ABORT;` |
|       - |  9186 | `		}` |
|     ! 0 |  9187 | `		return SXRET_OK;` |
|       - |  9188 | `	}` |
|      38 |  9189 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|      38 |  9190 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9191 | `		return SXERR_ABORT;` |
|       - |  9192 | `	}` |
|      38 |  9193 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  9194 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9195 | `			"throw: Expecting an exception class instance");` |
|     ! 0 |  9196 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9197 | `			return SXERR_ABORT;` |
|       - |  9198 | `		}` |
|     ! 0 |  9199 | `		return SXRET_OK;` |
|       - |  9200 | `	}` |
|       - |  9201 | `	/* Walk up to nearest exception/function block for the jump target */` |
|      38 |  9202 | `	pBlock = pGen->pCurrent;` |
|      60 |  9203 | `	while( pBlock->pParent ){` |
|      49 |  9204 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|      27 |  9205 | `			break;` |
|       - |  9206 | `		}` |
|      23 |  9207 | `		pBlock = pBlock->pParent;` |
|       1 |  9208 | `	}` |
|      38 |  9209 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|      38 |  9210 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|      38 |  9211 | `	return SXRET_OK;` |
|      20 |  9212 |  |
|       - |  9213 | `/*` |
|       - |  9214 | ` * Compile a 'catch' block.` |
|       - |  9215 | ` * Catch: A "catch" block retrieves an exception and creates` |
|       - |  9216 | ` * an object containing the exception information.` |
|       - |  9217 | ` */` |
|     214 |  9218 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|       2 |  9219 |  |
|     216 |  9220 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9221 | `	ph7_exception_block sCatch;` |
|       - |  9222 | `	SySet *pInstrContainer;` |
|       - |  9223 | `	SyString sClassName;` |
|       - |  9224 | `	GenBlock *pCatch;` |
|       - |  9225 | `	SyToken *pToken;` |
|       - |  9226 | `	SyString *pName;` |
|       - |  9227 | `	char *zDup;` |
|       - |  9228 | `	sxi32 rc;` |
|     216 |  9229 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       - |  9230 | `	/* Zero the structure */` |
|     216 |  9231 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|       - |  9232 | `	/* Initialize fields */` |
|     216 |  9233 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|     216 |  9234 | `	SySetInit(&sCatch.sByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|     216 |  9235 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|       - |  9236 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9237 | `			pToken = pGen->pIn;` |
|     ! 0 |  9238 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9239 | `				pToken--;` |
|     ! 0 |  9240 | `			}` |
|     ! 0 |  9241 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9242 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9243 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9244 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9245 | `				return SXERR_ABORT;` |
|       - |  9246 | `			}` |
|     ! 0 |  9247 | `			return SXERR_INVALID;` |
|       - |  9248 | `	}` |
|       - |  9249 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|     216 |  9250 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|     120 |  9251 | `	for(;;){` |
|       - |  9252 | `		SyBlob sResolved;` |
|     242 |  9253 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|     242 |  9254 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|       5 |  9255 | `			SyBlobRelease(&sResolved);` |
|       5 |  9256 | `			pToken = pGen->pIn;` |
|       5 |  9257 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9258 | `				pToken--;` |
|     ! 0 |  9259 | `			}` |
|       7 |  9260 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9261 | `				"syntax error, unexpected %s \"%z\"",` |
|       2 |  9262 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|       5 |  9263 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9264 | `				return SXERR_ABORT;` |
|       - |  9265 | `			}` |
|       5 |  9266 | `			return SXERR_INVALID;` |
|       - |  9267 | `		}` |
|       - |  9268 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|       - |  9269 | `		 * transient SyBlob allocation. */` |
|     356 |  9270 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     236 |  9271 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|     238 |  9272 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|     238 |  9273 | `		SyBlobRelease(&sResolved);` |
|     238 |  9274 | `		if( zDup == 0 ){` |
|     ! 0 |  9275 | `			goto Mem;` |
|       - |  9276 | `		}` |
|     238 |  9277 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|     238 |  9278 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9279 | `			goto Mem;` |
|       - |  9280 | `		}` |
|       - |  9281 | `		/* Check for '\|' (multi-catch separator) */` |
|     249 |  9282 | `		if( pGen->pIn < pGen->pEnd &&` |
|     236 |  9283 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|      28 |  9284 | `			pGen->pIn->sData.nByte == 1 &&` |
|      26 |  9285 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|      28 |  9286 | `			pGen->pIn++; /* Consume the '\|' */` |
|      28 |  9287 | `			continue;` |
|       - |  9288 | `		}` |
|     212 |  9289 | `		break;` |
|     ! 0 |  9290 | `	}` |
|     315 |  9291 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|     212 |  9292 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|       - |  9293 | `			/* Unexpected token,break immediately */` |
|     ! 0 |  9294 | `			pToken = pGen->pIn;` |
|     ! 0 |  9295 | `			if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9296 | `				pToken--;` |
|     ! 0 |  9297 | `			}` |
|     ! 0 |  9298 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9299 | `				"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9300 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9301 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9302 | `				return SXERR_ABORT;` |
|       - |  9303 | `			}` |
|     ! 0 |  9304 | `			return SXERR_INVALID;` |
|       - |  9305 | `	}` |
|     212 |  9306 | `	pGen->pIn++; /* Jump the dollar sign */` |
|       - |  9307 | `	/* Duplicate instance name */` |
|     212 |  9308 | `	pName = &pGen->pIn->sData;` |
|     212 |  9309 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|     212 |  9310 | `	if( zDup == 0 ){` |
|     ! 0 |  9311 | `		goto Mem;` |
|       - |  9312 | `	}` |
|     212 |  9313 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|     212 |  9314 | `	pGen->pIn++;` |
|     212 |  9315 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|       - |  9316 | `		/* Unexpected token,break immediately */` |
|     ! 0 |  9317 | `		pToken = pGen->pIn;` |
|     ! 0 |  9318 | `		if( pToken >= pGen->pEnd ){` |
|     ! 0 |  9319 | `			pToken--;` |
|     ! 0 |  9320 | `		}` |
|     ! 0 |  9321 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|       - |  9322 | `			"syntax error, unexpected %s \"%z\"",` |
|     ! 0 |  9323 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|     ! 0 |  9324 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9325 | `			return SXERR_ABORT;` |
|       - |  9326 | `		}` |
|     ! 0 |  9327 | `		return SXERR_INVALID;` |
|       - |  9328 | `	}` |
|       - |  9329 | `	/* Compile the block */` |
|     212 |  9330 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|       - |  9331 | `	/* Create the catch block */` |
|     212 |  9332 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|     212 |  9333 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9334 | `		return SXERR_ABORT;` |
|       - |  9335 | `	}` |
|       - |  9336 | `	/* Swap bytecode container */` |
|     212 |  9337 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     212 |  9338 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&sCatch.sByteCode);` |
|       - |  9339 | `	/* Compile the block */` |
|     212 |  9340 | `	PH7_CompileBlock(&(*pGen),0);` |
|       - |  9341 | `	/* Fix forward jumps now the destination is resolved  */` |
|     212 |  9342 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9343 | `	/* Emit the DONE instruction */` |
|     212 |  9344 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9345 | `	/* Leave the block */` |
|     212 |  9346 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9347 | `	/* Restore the default container */` |
|     212 |  9348 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9349 | `	/* Install the catch block */` |
|     212 |  9350 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|     212 |  9351 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9352 | `		goto Mem;` |
|       - |  9353 | `	}` |
|     212 |  9354 | `	return SXRET_OK;` |
|     ! 0 |  9355 | `Mem:` |
|     ! 0 |  9356 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9357 | `	return SXERR_ABORT;` |
|     109 |  9358 |  |
|       - |  9359 | `/*` |
|       - |  9360 | ` * Compile a 'try' block.` |
|       - |  9361 | ` * A function using an exception should be in a "try" block.` |
|       - |  9362 | ` * If the exception does not trigger, the code will continue` |
|       - |  9363 | ` * as normal. However if the exception triggers, an exception` |
|       - |  9364 | ` * is "thrown".` |
|       - |  9365 | ` */` |
|     218 |  9366 | `static sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|       2 |  9367 |  |
|       - |  9368 | `	ph7_exception *pException;` |
|     220 |  9369 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       - |  9370 | `	GenBlock *pTry;` |
|       - |  9371 | `	sxu32 nJmpIdx;` |
|       - |  9372 | `	sxi32 rc;` |
|       - |  9373 | `	/* Create the exception container */` |
|     220 |  9374 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|     220 |  9375 | `	if( pException == 0 ){` |
|     ! 0 |  9376 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|     ! 0 |  9377 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  9378 | `		return SXERR_ABORT;` |
|       - |  9379 | `	}` |
|       - |  9380 | `	/* Zero the structure */` |
|     220 |  9381 | `	SyZero(pException,sizeof(ph7_exception));` |
|       - |  9382 | `	/* Initialize fields */` |
|     220 |  9383 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|     220 |  9384 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     220 |  9385 | `	pException->iHasFinally = 0;` |
|     220 |  9386 | `	pException->iFinallyDone = 0;` |
|     220 |  9387 | `	pException->pVm = pGen->pVm;` |
|       - |  9388 | `	/* Create the try block */` |
|     220 |  9389 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pTry);` |
|     220 |  9390 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9391 | `		return SXERR_ABORT;` |
|       - |  9392 | `	}` |
|       - |  9393 | `	/* Store exception pointer so break/continue can emit POP_EXCEPTION */` |
|     220 |  9394 | `	pTry->pUserData = pException;` |
|       - |  9395 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|     220 |  9396 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|       - |  9397 | `	/* Fix the jump later when the destination is resolved */` |
|     220 |  9398 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|     220 |  9399 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|       - |  9400 | `	/* Compile the block */` |
|     220 |  9401 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|     220 |  9402 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9403 | `		return SXERR_ABORT;` |
|       - |  9404 | `	}` |
|       - |  9405 | `	/* Fix forward jumps now the destination is resolved */` |
|     220 |  9406 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9407 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|     220 |  9408 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       - |  9409 | `	/* Leave the block */` |
|     220 |  9410 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9411 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|     220 |  9412 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     216 |  9413 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       - |  9414 | `		/* Compile one or more catch blocks */` |
|     210 |  9415 | `		for(;;){` |
|     420 |  9416 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     323 |  9417 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|     105 |  9418 | `					break;` |
|       - |  9419 | `			}` |
|     216 |  9420 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|     216 |  9421 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9422 | `				return SXERR_ABORT;` |
|       - |  9423 | `			}` |
|       2 |  9424 | `		}` |
|     103 |  9425 | `	}` |
|       - |  9426 | `	/* Compile optional finally block */` |
|     220 |  9427 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      94 |  9428 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|       - |  9429 | `		SySet *pInstrContainer;` |
|       - |  9430 | `		GenBlock *pFinBlock;` |
|      32 |  9431 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|       - |  9432 | `		/* Create the finally block for jump fixup bookkeeping */` |
|      32 |  9433 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      32 |  9434 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9435 | `			return SXERR_ABORT;` |
|       - |  9436 | `		}` |
|       - |  9437 | `		/* Swap bytecode container */` |
|      32 |  9438 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      32 |  9439 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|       - |  9440 | `		/* Compile the finally body */` |
|      32 |  9441 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      32 |  9442 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9443 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  9444 | `			return SXERR_ABORT;` |
|       - |  9445 | `		}` |
|       - |  9446 | `		/* Fix forward jumps now the destination is resolved */` |
|      32 |  9447 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9448 | `		/* Emit DONE to terminate the finally block */` |
|      32 |  9449 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|       - |  9450 | `		/* Leave the block */` |
|      32 |  9451 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       - |  9452 | `		/* Restore the default container */` |
|      32 |  9453 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      32 |  9454 | `		pException->iHasFinally = 1;` |
|      15 |  9455 | `	}` |
|       - |  9456 | `	/* Must have at least one catch or finally */` |
|     220 |  9457 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|       7 |  9458 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  9459 | `			"Cannot use try without catch or finally");` |
|       7 |  9460 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9461 | `			return SXERR_ABORT;` |
|       - |  9462 | `		}` |
|       3 |  9463 | `	}` |
|     220 |  9464 | `	return SXRET_OK;` |
|     111 |  9465 |  |
|       - |  9466 | `/*` |
|       - |  9467 | ` * Compile a switch block.` |
|       - |  9468 | ` *  (See block-comment below for more information)` |
|       - |  9469 | ` */` |
|     108 |  9470 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|       2 |  9471 |  |
|     110 |  9472 | `	sxi32 rc = SXRET_OK;` |
|     110 |  9473 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|       - |  9474 | `		/* Unexpected token */` |
|     ! 0 |  9475 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9476 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9477 | `			return SXERR_ABORT;` |
|       - |  9478 | `		}` |
|     ! 0 |  9479 | `		pGen->pIn++;` |
|     ! 0 |  9480 | `	}` |
|     110 |  9481 | `	pGen->pIn++;` |
|       - |  9482 | `	/* First instruction to execute in this block. */` |
|     110 |  9483 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|       - |  9484 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|       - |  9485 | `	 * or the '}' token */` |
|     182 |  9486 | `	for(;;){` |
|     366 |  9487 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9488 | `			/* No more input to process */` |
|     ! 0 |  9489 | `			break;` |
|       - |  9490 | `		}` |
|     366 |  9491 | `		rc = SXRET_OK;` |
|     366 |  9492 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      70 |  9493 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|      28 |  9494 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|       - |  9495 | `					/* Unexpected token */` |
|     ! 0 |  9496 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9497 | `						&pGen->pIn->sData);` |
|     ! 0 |  9498 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9499 | `						return SXERR_ABORT;` |
|       - |  9500 | `					}` |
|       - |  9501 | `					/* FALL THROUGH */` |
|     ! 0 |  9502 | `				}` |
|      28 |  9503 | `				rc = SXERR_EOF;` |
|      28 |  9504 | `				break;` |
|       - |  9505 | `			}` |
|      23 |  9506 | `		}else{` |
|       - |  9507 | `			sxi32 nKwrd;` |
|       - |  9508 | `			/* Extract the keyword */` |
|     298 |  9509 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     298 |  9510 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|      42 |  9511 | `				break;` |
|       - |  9512 | `			}` |
|     218 |  9513 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9514 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|       - |  9515 | `					/* Unexpected token */` |
|     ! 0 |  9516 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|     ! 0 |  9517 | `						&pGen->pIn->sData);` |
|     ! 0 |  9518 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 |  9519 | `						return SXERR_ABORT;` |
|       - |  9520 | `					}` |
|       - |  9521 | `					/* FALL THROUGH */` |
|     ! 0 |  9522 | `				}` |
|       - |  9523 | `				/* Block compiled */` |
|       3 |  9524 | `				break;` |
|       - |  9525 | `			}` |
|       - |  9526 | `		}` |
|       - |  9527 | `		/* Compile block */` |
|     258 |  9528 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|     258 |  9529 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9530 | `			return SXERR_ABORT;` |
|       - |  9531 | `		}` |
|       2 |  9532 | `	}` |
|     110 |  9533 | `	return rc;` |
|      56 |  9534 |  |
|       - |  9535 | `/*` |
|       - |  9536 | ` * Compile a case eXpression.` |
|       - |  9537 | ` *  (See block-comment below for more information)` |
|       - |  9538 | ` */` |
|      88 |  9539 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|       2 |  9540 |  |
|       - |  9541 | `	SySet *pInstrContainer;` |
|       - |  9542 | `	SyToken *pEnd,*pTmp;` |
|      90 |  9543 | `	sxi32 iNest = 0;` |
|       - |  9544 | `	sxi32 rc;` |
|       - |  9545 | `	/* Delimit the expression */` |
|      90 |  9546 | `	pEnd = pGen->pIn;` |
|     186 |  9547 | `	while( pEnd < pGen->pEnd ){` |
|     186 |  9548 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|       - |  9549 | `			/* Increment nesting level */` |
|       3 |  9550 | `			iNest++;` |
|     185 |  9551 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|       - |  9552 | `			/* Decrement nesting level */` |
|       3 |  9553 | `			iNest--;` |
|     183 |  9554 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|      90 |  9555 | `			break;` |
|       - |  9556 | `		}` |
|      98 |  9557 | `		pEnd++;` |
|       2 |  9558 | `	}` |
|      90 |  9559 | `	if( pGen->pIn >= pEnd ){` |
|     ! 0 |  9560 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|     ! 0 |  9561 | `		if( rc == SXERR_ABORT ){` |
|       - |  9562 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9563 | `			return SXERR_ABORT;` |
|       - |  9564 | `		}` |
|     ! 0 |  9565 | `	}` |
|       - |  9566 | `	/* Swap token stream */` |
|      90 |  9567 | `	pTmp = pGen->pEnd;` |
|      90 |  9568 | `	pGen->pEnd = pEnd;` |
|      90 |  9569 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      90 |  9570 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|      90 |  9571 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       - |  9572 | `	/* Emit the done instruction */` |
|      90 |  9573 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      90 |  9574 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       - |  9575 | `	/* Update token stream */` |
|      90 |  9576 | `	pGen->pIn  = pEnd;` |
|      90 |  9577 | `	pGen->pEnd = pTmp;` |
|      90 |  9578 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  9579 | `		return SXERR_ABORT;` |
|       - |  9580 | `	}` |
|      90 |  9581 | `	return SXRET_OK;` |
|      46 |  9582 |  |
|       - |  9583 | `/*` |
|       - |  9584 | ` * Compile the smart switch statement.` |
|       - |  9585 | ` * According to the PHP language reference manual` |
|       - |  9586 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|       - |  9587 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|       - |  9588 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|       - |  9589 | ` *  This is exactly what the switch statement is for.` |
|       - |  9590 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|       - |  9591 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|       - |  9592 | ` *  of the outer loop, use continue 2.` |
|       - |  9593 | ` *  Note that switch/case does loose comparision.` |
|       - |  9594 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|       - |  9595 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|       - |  9596 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|       - |  9597 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|       - |  9598 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|       - |  9599 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|       - |  9600 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|       - |  9601 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|       - |  9602 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|       - |  9603 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|       - |  9604 | ` *  list for the next case.` |
|       - |  9605 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|       - |  9606 | ` *  or floating-point numbers and strings.` |
|       - |  9607 | ` */` |
|      28 |  9608 | `static sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|       2 |  9609 |  |
|       - |  9610 | `	GenBlock *pSwitchBlock;` |
|       - |  9611 | `	SyToken *pTmp,*pEnd;` |
|       - |  9612 | `	ph7_switch *pSwitch;` |
|       - |  9613 | `	sxu32 nToken;` |
|       - |  9614 | `	sxu32 nLine;` |
|       - |  9615 | `	sxi32 rc;` |
|      30 |  9616 | `	nLine = pGen->pIn->nLine;` |
|       - |  9617 | `	/* Jump the 'switch' keyword */` |
|      30 |  9618 | `	pGen->pIn++;` |
|      30 |  9619 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       - |  9620 | `		/* Syntax error */` |
|     ! 0 |  9621 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|     ! 0 |  9622 | `		if( rc == SXERR_ABORT ){` |
|       - |  9623 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9624 | `			return SXERR_ABORT;` |
|       - |  9625 | `		}` |
|     ! 0 |  9626 | `		goto Synchronize;` |
|       - |  9627 | `	}` |
|       - |  9628 | `	/* Jump the left parenthesis '(' */` |
|      30 |  9629 | `	pGen->pIn++;` |
|      30 |  9630 | `	pEnd = 0; /* cc warning */` |
|       - |  9631 | `	/* Create the loop block */` |
|      44 |  9632 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|      14 |  9633 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|      30 |  9634 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  9635 | `		return SXERR_ABORT;` |
|       - |  9636 | `	}` |
|       - |  9637 | `	/* Delimit the condition */` |
|      30 |  9638 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|      30 |  9639 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|       - |  9640 | `		/* Empty expression */` |
|     ! 0 |  9641 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|     ! 0 |  9642 | `		if( rc == SXERR_ABORT ){` |
|       - |  9643 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 |  9644 | `			return SXERR_ABORT;` |
|       - |  9645 | `		}` |
|     ! 0 |  9646 | `	}` |
|       - |  9647 | `	/* Swap token streams */` |
|      30 |  9648 | `	pTmp = pGen->pEnd;` |
|      30 |  9649 | `	pGen->pEnd = pEnd;` |
|       - |  9650 | `	/* Compile the expression */` |
|      30 |  9651 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      30 |  9652 | `	if( rc == SXERR_ABORT ){` |
|       - |  9653 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|     ! 0 |  9654 | `		return SXERR_ABORT;` |
|       - |  9655 | `	}` |
|       - |  9656 | `	/* Update token stream */` |
|      30 |  9657 | `	while(pGen->pIn < pEnd ){` |
|     ! 0 |  9658 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|     ! 0 |  9659 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|     ! 0 |  9660 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  9661 | `			return SXERR_ABORT;` |
|       - |  9662 | `		}` |
|     ! 0 |  9663 | `		pGen->pIn++;` |
|     ! 0 |  9664 | `	}` |
|      30 |  9665 | `	pGen->pIn  = &pEnd[1];` |
|      30 |  9666 | `	pGen->pEnd = pTmp;` |
|      30 |  9667 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|      28 |  9668 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|     ! 0 |  9669 | `			pTmp = pGen->pIn;` |
|     ! 0 |  9670 | `			if( pTmp >= pGen->pEnd ){` |
|     ! 0 |  9671 | `				pTmp--;` |
|     ! 0 |  9672 | `			}` |
|       - |  9673 | `			/* Unexpected token */` |
|     ! 0 |  9674 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|     ! 0 |  9675 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9676 | `				return SXERR_ABORT;` |
|       - |  9677 | `			}` |
|     ! 0 |  9678 | `			goto Synchronize;` |
|       - |  9679 | `	}` |
|       - |  9680 | `	/* Set the delimiter token */` |
|      30 |  9681 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|       3 |  9682 | `		nToken = PH7_TK_KEYWORD;` |
|       - |  9683 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|       2 |  9684 | `	}else{` |
|      28 |  9685 | `		nToken = PH7_TK_CCB; /* '}' */` |
|       - |  9686 | `	}` |
|      30 |  9687 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|       - |  9688 | `	/* Create the switch blocks container */` |
|      30 |  9689 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|      30 |  9690 | `	if( pSwitch == 0 ){` |
|       - |  9691 | `		/* Abort compilation */` |
|     ! 0 |  9692 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|     ! 0 |  9693 | `		return SXERR_ABORT;` |
|       - |  9694 | `	}` |
|       - |  9695 | `	/* Zero the structure */` |
|      30 |  9696 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|       - |  9697 | `	/* Initialize fields */` |
|      30 |  9698 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|       - |  9699 | `	/* Emit the switch instruction */` |
|      30 |  9700 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|       - |  9701 | `	/* Compile case blocks */` |
|      96 |  9702 | `	for(;;){` |
|       - |  9703 | `		sxu32 nKwrd;` |
|     112 |  9704 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - |  9705 | `			/* No more input to process */` |
|     ! 0 |  9706 | `			break;` |
|       - |  9707 | `		}` |
|     112 |  9708 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|     ! 0 |  9709 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|       - |  9710 | `				/* Unexpected token */` |
|     ! 0 |  9711 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9712 | `					&pGen->pIn->sData);` |
|     ! 0 |  9713 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9714 | `					return SXERR_ABORT;` |
|       - |  9715 | `				}` |
|       - |  9716 | `				/* FALL THROUGH */` |
|     ! 0 |  9717 | `			}` |
|       - |  9718 | `			/* Block compiled */` |
|     ! 0 |  9719 | `			break;` |
|       - |  9720 | `		}` |
|       - |  9721 | `		/* Extract the keyword */` |
|     112 |  9722 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     112 |  9723 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|       3 |  9724 | `			if( nToken != PH7_TK_KEYWORD ){` |
|       - |  9725 | `				/* Unexpected token */` |
|     ! 0 |  9726 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9727 | `					&pGen->pIn->sData);` |
|     ! 0 |  9728 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9729 | `					return SXERR_ABORT;` |
|       - |  9730 | `				}` |
|       - |  9731 | `				/* FALL THROUGH */` |
|     ! 0 |  9732 | `			}` |
|       - |  9733 | `			/* Block compiled */` |
|       3 |  9734 | `			break;` |
|       - |  9735 | `		}` |
|     110 |  9736 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|       - |  9737 | `			/*` |
|       - |  9738 | `			 * Accroding to the PHP language reference manual` |
|       - |  9739 | `			 *  A special case is the default case. This case matches anything` |
|       - |  9740 | `			 *  that wasn't matched by the other cases.` |
|       - |  9741 | `			 */` |
|      22 |  9742 | `			if( pSwitch->nDefault > 0 ){` |
|       - |  9743 | `				/* Default case already compiled */` |
|     ! 0 |  9744 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|     ! 0 |  9745 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  9746 | `					return SXERR_ABORT;` |
|       - |  9747 | `				}` |
|     ! 0 |  9748 | `			}` |
|      22 |  9749 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|       - |  9750 | `			/* Compile the default block */` |
|      22 |  9751 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|      22 |  9752 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9753 | `				return SXERR_ABORT;` |
|      22 |  9754 | `			}else if( rc == SXERR_EOF ){` |
|      20 |  9755 | `				break;` |
|       1 |  9756 | `			}` |
|      91 |  9757 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|       - |  9758 | `			ph7_case_expr sCase;` |
|       - |  9759 | `			/* Standard case block */` |
|      90 |  9760 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|       - |  9761 | `			/* initialize the structure */` |
|      90 |  9762 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  9763 | `			/* Compile the case expression */` |
|      90 |  9764 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|      90 |  9765 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9766 | `				return SXERR_ABORT;` |
|       - |  9767 | `			}` |
|       - |  9768 | `			/* Compile the case block */` |
|      90 |  9769 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|       - |  9770 | `			/* Insert in the switch container */` |
|      90 |  9771 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|      90 |  9772 | `			if( rc == SXERR_ABORT){` |
|     ! 0 |  9773 | `				return SXERR_ABORT;` |
|      90 |  9774 | `			}else if( rc == SXERR_EOF ){` |
|       9 |  9775 | `				break;` |
|       - |  9776 | `			}` |
|      42 |  9777 | `		}else{` |
|       - |  9778 | `			/* Unexpected token */` |
|     ! 0 |  9779 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|     ! 0 |  9780 | `				&pGen->pIn->sData);` |
|     ! 0 |  9781 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  9782 | `				return SXERR_ABORT;` |
|       - |  9783 | `			}` |
|     ! 0 |  9784 | `			break;` |
|       - |  9785 | `		}` |
|       2 |  9786 | `	}` |
|       - |  9787 | `	/* Fix all jumps now the destination is resolved */` |
|      30 |  9788 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|      30 |  9789 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|       - |  9790 | `	/* Release the loop block */` |
|      30 |  9791 | `	GenStateLeaveBlock(pGen,0);` |
|      30 |  9792 | `	if( pGen->pIn < pGen->pEnd ){` |
|       - |  9793 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|      30 |  9794 | `		pGen->pIn++;` |
|      14 |  9795 | `	}` |
|       - |  9796 | `	/* Statement successfully compiled */` |
|      30 |  9797 | `	return SXRET_OK;` |
|     ! 0 |  9798 | `Synchronize:` |
|       - |  9799 | `	/* Synchronize with the first semi-colon */` |
|     ! 0 |  9800 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|     ! 0 |  9801 | `		pGen->pIn++;` |
|     ! 0 |  9802 | `	}` |
|     ! 0 |  9803 | `	return SXRET_OK;` |
|      16 |  9804 |  |
|       - |  9805 | `/*` |
|       - |  9806 | ` * Chain operators participate in a postfix member-access chain.` |
|       - |  9807 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|       - |  9808 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|       - |  9809 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|       - |  9810 | ` */` |
|       - |  9811 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|       - |  9812 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|       - |  9813 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|       - |  9814 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|       - |  9815 |  |
|       - |  9816 | `/*` |
|       - |  9817 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|       - |  9818 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|       - |  9819 | ` * patched entries from the pending set.` |
|       - |  9820 | ` */` |
| 2141020 |  9821 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|       2 |  9822 |  |
| 2141022 |  9823 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|       - |  9824 | `	sxu32 nTarget;` |
|       - |  9825 | `	sxu32 *aIdx;` |
|       - |  9826 | `	sxu32 i;` |
| 2141022 |  9827 | `	if( nCur <= nBaseline ){` |
| 2140932 |  9828 | `		return;` |
|       - |  9829 | `	}` |
|      92 |  9830 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      92 |  9831 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|     190 |  9832 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|     100 |  9833 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|     100 |  9834 | `		if( pInstr ){` |
|     100 |  9835 | `			pInstr->iP2 = (sxi32)nTarget;` |
|      49 |  9836 | `		}` |
|      51 |  9837 | `	}` |
|      92 |  9838 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
| 1070512 |  9839 |  |
|       - |  9840 |  |
|       - |  9841 | `/*` |
|       - |  9842 | ` * Generate bytecode for a given expression tree.` |
|       - |  9843 | ` * If something goes wrong while generating bytecode` |
|       - |  9844 | ` * for the expression tree (A very unlikely scenario)` |
|       - |  9845 | ` * this function takes care of generating the appropriate` |
|       - |  9846 | ` * error message.` |
|       - |  9847 | ` */` |
| 2885182 |  9848 | `static sxi32 GenStateEmitExprCode(` |
|       - |  9849 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - |  9850 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|       - |  9851 | `	sxi32 iFlags /* Control flags */` |
|       - |  9852 | `	)` |
|       2 |  9853 |  |
|       - |  9854 | `	VmInstr *pInstr;` |
|       - |  9855 | `	sxu32 nJmpIdx;` |
| 2885184 |  9856 | `	sxi32 iP1 = 0;` |
| 2885184 |  9857 | `	sxu32 iP2 = 0;` |
| 2885184 |  9858 | `	void *p3  = 0;` |
|       - |  9859 | `	sxi32 iVmOp;` |
|       - |  9860 | `	sxi32 rc;` |
| 2885184 |  9861 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
| 2885184 |  9862 | `	sxu32 nRhsNsBase = 0;` |
| 2885184 |  9863 | `	if( pNode->xCode ){` |
|       - |  9864 | `		SyToken *pTmpIn,*pTmpEnd;` |
|       - |  9865 | `		/* Compile node */` |
| 1786846 |  9866 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
| 1786846 |  9867 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
| 1786846 |  9868 | `		RE_SWAP_DELIMITER(pGen);` |
| 1786846 |  9869 | `		return rc;` |
|       - |  9870 | `	}` |
| 1098340 |  9871 | `	if( pNode->pOp == 0 ){` |
|     ! 0 |  9872 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|       - |  9873 | `			"Invalid expression node,PH7 is aborting compilation");` |
|     ! 0 |  9874 | `		return SXERR_ABORT;` |
|       - |  9875 | `	}` |
| 1098340 |  9876 | `	iVmOp = pNode->pOp->iVmOp;` |
| 1098340 |  9877 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      47 |  9878 | `		sxu32 nJmp = 0;` |
|       - |  9879 | `		sxu32 nNcNsBase;` |
|       - |  9880 | `		VmInstr *pInstrFix;` |
|       - |  9881 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|       - |  9882 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|       - |  9883 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|       - |  9884 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|       - |  9885 | `		 * stack slot carries a writable nIdx. */` |
|      47 |  9886 | `		if( pNode->pRight ){` |
|      47 |  9887 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9888 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE);` |
|      47 |  9889 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9890 | `				return rc;` |
|       - |  9891 | `			}` |
|      47 |  9892 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       - |  9893 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|       - |  9894 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|       - |  9895 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|       - |  9896 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|       - |  9897 | `			 * the store, so the parent array does not need to be copied at` |
|       - |  9898 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|       - |  9899 | `			 * cascade for the actual write path stays correct. */` |
|      47 |  9900 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      47 |  9901 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      19 |  9902 | `				pInstrFix->iP2 = 3;` |
|       9 |  9903 | `			}` |
|      23 |  9904 | `		}` |
|       - |  9905 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      47 |  9906 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|       - |  9907 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      47 |  9908 | `		if( pNode->pLeft ){` |
|      47 |  9909 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      47 |  9910 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      47 |  9911 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9912 | `				return rc;` |
|       - |  9913 | `			}` |
|      47 |  9914 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|      23 |  9915 | `		}` |
|       - |  9916 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      47 |  9917 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|       - |  9918 | `		/* Patch the short-circuit jump to land after the store. */` |
|      47 |  9919 | `		if( nJmp > 0 ){` |
|      47 |  9920 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      47 |  9921 | `			if( pInstrFix ){` |
|      47 |  9922 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|      23 |  9923 | `			}` |
|      23 |  9924 | `		}` |
|      47 |  9925 | `		return SXRET_OK;` |
|       - |  9926 | `	}` |
| 1098294 |  9927 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|       - |  9928 | `		sxu32 nJz,nJmp;` |
|       - |  9929 | `		sxu32 nTernaryNsBase;` |
|       - |  9930 | `		/* Ternary operator require special handling */` |
|       - |  9931 | `		/* Phase#1: Compile the condition */` |
|    2058 |  9932 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2058 |  9933 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    2058 |  9934 | `		if( rc != SXRET_OK ){` |
|     ! 0 |  9935 | `			return rc;` |
|       - |  9936 | `		}` |
|       - |  9937 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|       - |  9938 | `		 * compiling the condition must short-circuit to the end of the` |
|       - |  9939 | `		 * condition expression, not leak past the ternary. */` |
|    2058 |  9940 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    2058 |  9941 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    2058 |  9942 | `		if( pNode->pLeft ){` |
|       - |  9943 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|       - |  9944 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    1990 |  9945 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9946 | `			/* Phase#3: Compile the 'then' expression  */` |
|    1990 |  9947 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    1990 |  9948 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    1990 |  9949 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9950 | `				return rc;` |
|       - |  9951 | `			}` |
|    1990 |  9952 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     996 |  9953 | `		}else{` |
|       - |  9954 | `			/* Elvis operator: (expr) ?: (else)` |
|       - |  9955 | `			 * Duplicate condition so original value is the 'then' result.` |
|       - |  9956 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      70 |  9957 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      70 |  9958 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|       - |  9959 | `		}` |
|       - |  9960 | `		/* Phase#4: Emit the unconditional jump */` |
|    2058 |  9961 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|       - |  9962 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    2058 |  9963 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    2058 |  9964 | `		if( pInstr ){` |
|    2058 |  9965 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1028 |  9966 | `		}` |
|    2058 |  9967 | `		if( !pNode->pLeft ){` |
|       - |  9968 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      70 |  9969 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      34 |  9970 | `		}` |
|       - |  9971 | `		/* Phase#6: Compile the 'else' expression */` |
|    2058 |  9972 | `		if( pNode->pRight ){` |
|    2058 |  9973 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    2058 |  9974 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    2058 |  9975 | `			if( rc != SXRET_OK ){` |
|     ! 0 |  9976 | `				return rc;` |
|       - |  9977 | `			}` |
|    2058 |  9978 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    1028 |  9979 | `		}` |
|    2058 |  9980 | `		if( nJmp > 0 ){` |
|       - |  9981 | `			/* Phase#7: Fix the unconditional jump */` |
|    2058 |  9982 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    2058 |  9983 | `			if( pInstr ){` |
|    2058 |  9984 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    1028 |  9985 | `			}` |
|    1028 |  9986 | `		}` |
|       - |  9987 | `		/* All done */` |
|    2058 |  9988 | `		return SXRET_OK;` |
|       - |  9989 | `	}` |
| 1096238 |  9990 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|       - |  9991 | `	/* Generate code for the left tree */` |
| 1096238 |  9992 | `	if( pNode->pLeft ){` |
| 1096200 |  9993 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
| 1096200 |  9994 | `		if( iVmOp == PH7_OP_CALL ){` |
|       - |  9995 | `			ph7_expr_node **apNode;` |
|  348102 |  9996 | `			int hasSpread = 0;` |
|  348102 |  9997 | `			int hasNamed = 0;` |
|       - |  9998 | `			sxi32 nArgs;` |
|       - |  9999 | `			sxi32 n;` |
|       - | 10000 | `			/* Recurse and generate bytecodes for function arguments */` |
|  348102 | 10001 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  348102 | 10002 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|       - | 10003 | `			/* Validate: no positional arguments after named arguments */` |
|       - | 10004 | `			{` |
|  348102 | 10005 | `				int seenNamed = 0;` |
|  689198 | 10006 | `				for( n = 0; n < nArgs; ++n ){` |
|  341100 | 10007 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     176 | 10008 | `						seenNamed = 1;` |
|     176 | 10009 | `						hasNamed = 1;` |
|  341013 | 10010 | `					}else if( seenNamed && !(apNode[n]->iFlags & EXPR_NODE_SPREAD) ){` |
|       3 | 10011 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|       - | 10012 | `							"Cannot use positional argument after named argument");` |
|       3 | 10013 | `						return SXERR_SYNTAX;` |
|       - | 10014 | `					}` |
|  170550 | 10015 | `				}` |
|       - | 10016 | `			}` |
|       - | 10017 | `			/* Read-only load */` |
|  348100 | 10018 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|  689194 | 10019 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|  341096 | 10020 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  341096 | 10021 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|  341096 | 10022 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10023 | `					return rc;` |
|       - | 10024 | `				}` |
|       - | 10025 | `				/* Each argument is an independent nullsafe scope. */` |
|  341096 | 10026 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  341096 | 10027 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       - | 10028 | `					/* Emit spread opcode to unpack this array argument */` |
|      20 | 10029 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      20 | 10030 | `					hasSpread = 1;` |
|       9 | 10031 | `				}` |
|  170549 | 10032 | `			}` |
|       - | 10033 | `			/* Total number of given arguments */` |
|  348100 | 10034 | `			iP1 = nArgs;` |
|  348100 | 10035 | `			iP2 = hasSpread;` |
|       - | 10036 | `			/* Build VmCallArgMap if named arguments are present.` |
|       - | 10037 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|  348100 | 10038 | `			if( hasNamed ){` |
|      94 | 10039 | `				sxu32 nStrBytes = 0;` |
|       - | 10040 | `				char *zBuf;` |
|     278 | 10041 | `				for( n = 0; n < nArgs; ++n ){` |
|     186 | 10042 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 | 10043 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      86 | 10044 | `					}` |
|      94 | 10045 | `				}` |
|       - | 10046 | `				{` |
|      94 | 10047 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      94 | 10048 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      92 | 10049 | `					&pGen->pVm->sAllocator, mapSize);` |
|      94 | 10050 | `				if( pMap ){` |
|      94 | 10051 | `					SyZero(pMap, mapSize);` |
|      94 | 10052 | `					pMap->bHasNamed = 1;` |
|      94 | 10053 | `					pMap->nTotal = (sxu32)nArgs;` |
|      94 | 10054 | `					pMap->aNames = (SyString *)&pMap[1];` |
|      94 | 10055 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     278 | 10056 | `					for( n = 0; n < nArgs; ++n ){` |
|     186 | 10057 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|     174 | 10058 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|     174 | 10059 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|     174 | 10060 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|     174 | 10061 | `							zBuf += nb;` |
|      86 | 10062 | `						}` |
|       - | 10063 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|      94 | 10064 | `					}` |
|      94 | 10065 | `					p3 = (void *)pMap;` |
|      46 | 10066 | `				}` |
|       - | 10067 | `				}` |
|      46 | 10068 | `			}` |
|       - | 10069 | `			/* Remove stale flags now */` |
|  348100 | 10070 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|  174049 | 10071 | `		}` |
| 1096198 | 10072 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
| 1096198 | 10073 | `		if( rc != SXRET_OK ){` |
|      31 | 10074 | `			return rc;` |
|       - | 10075 | `		}` |
| 1096168 | 10076 | `		if( !bIsChainOp ){` |
|       - | 10077 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|       - | 10078 | `			 * target the end of that LHS chain, which is right here. */` |
|  512404 | 10079 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  256201 | 10080 | `		}` |
| 1096168 | 10081 | `		if( iVmOp == PH7_OP_CALL ){` |
|  348100 | 10082 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  348100 | 10083 | `			if( pInstr ){` |
|  348100 | 10084 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|  347198 | 10085 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|       - | 10086 | `					sxu32 nQual;` |
|  347198 | 10087 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10088 | `					/* Prevent constant expansion but preserve the absolute flag` |
|       - | 10089 | `					 * so the later NEW handler (if any) can see it. */` |
|  347198 | 10090 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|       - | 10091 | `					/* Namespace-qualify the function name for CALL, unless the` |
|       - | 10092 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|       - | 10093 | `					 * imports — class imports must NOT affect function` |
|       - | 10094 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|       - | 10095 | `					 * before NEW; we store the original literal index in the` |
|       - | 10096 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|       - | 10097 | `					 * the unqualified name and re-qualify with class imports. */` |
|  347198 | 10098 | `					if( bAbsolute ){` |
|      20 | 10099 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      11 | 10100 | `					}else{` |
|  347180 | 10101 | `						int fromImport = 0;` |
|  347180 | 10102 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|  347180 | 10103 | `						pInstr->iP2 = (sxi32)nQual;` |
|  347180 | 10104 | `						if( nQual != nOrig ){` |
|       - | 10105 | `							/* Store original literal index in CALL's iP2 so the` |
|       - | 10106 | `							 * NEW handler can recover the unqualified name. */` |
|      74 | 10107 | `							iP2 = (sxi32)(nOrig + 1); /* +1 to distinguish from default 0 */` |
|      74 | 10108 | `							if( !fromImport ){` |
|       - | 10109 | `								/* Mark as namespace-qualified via VmCallArgMap */` |
|      64 | 10110 | `								if( p3 == 0 ){` |
|      64 | 10111 | `									VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      62 | 10112 | `										&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      64 | 10113 | `									if( pMap ){` |
|      64 | 10114 | `										SyZero(pMap, sizeof(VmCallArgMap));` |
|      64 | 10115 | `										p3 = (void *)pMap;` |
|      31 | 10116 | `									}` |
|      31 | 10117 | `								}` |
|      64 | 10118 | `								if( p3 ){` |
|      64 | 10119 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|      31 | 10120 | `								}` |
|      31 | 10121 | `							}` |
|      36 | 10122 | `						}` |
|       2 | 10123 | `					}` |
|  174502 | 10124 | `				}else if( pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */ \|\| pInstr->iOp == PH7_OP_NEW ){` |
|       - | 10125 | `					/* Method call,flag that */` |
|     748 | 10126 | `					pInstr->iP2 = 1;` |
|     373 | 10127 | `				}` |
|  174051 | 10128 | `			}` |
|  922119 | 10129 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|       - | 10130 | `			ph7_expr_node **apNode;` |
|       - | 10131 | `			sxi32 n;` |
|       - | 10132 | `			/* Recurse and generate bytecodes for array index */` |
|   75296 | 10133 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  135840 | 10134 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   60546 | 10135 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   60546 | 10136 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&~EXPR_FLAG_LOAD_IDX_STORE);` |
|   60546 | 10137 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 10138 | `					return rc;` |
|       - | 10139 | `				}` |
|       - | 10140 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   60546 | 10141 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   30274 | 10142 | `			}` |
|   75296 | 10143 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   60546 | 10144 | `				iP1 = 1; /* Node have an index associated with it */` |
|   30272 | 10145 | `			}` |
|   75296 | 10146 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|       - | 10147 | `				/* Create an empty entry when the desired index is not found */` |
|   29760 | 10148 | `				iP2 = 1;` |
|   14881 | 10149 | `			}` |
|  710423 | 10150 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|       - | 10151 | `			/* POP the left node */` |
|      32 | 10152 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      15 | 10153 | `		}` |
|  548083 | 10154 | `	}` |
| 1096206 | 10155 | `	rc = SXRET_OK;` |
| 1096206 | 10156 | `	nJmpIdx = 0;` |
|       - | 10157 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|       - | 10158 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|       - | 10159 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
| 1096206 | 10160 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     270 | 10161 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     270 | 10162 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     270 | 10163 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     270 | 10164 | `			int isSpecial = 0;` |
|     270 | 10165 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     182 | 10166 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     182 | 10167 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     195 | 10168 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     160 | 10169 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      83 | 10170 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      90 | 10171 | `					isSpecial = 1;` |
|      44 | 10172 | `				}` |
|     112 | 10173 | `			}` |
|     314 | 10174 | `			pInstr->iP1 = 0;` |
|     314 | 10175 | `			if( !isSpecial ){` |
|     138 | 10176 | `				pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      68 | 10177 | `			}` |
|       - | 10178 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|       - | 10179 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     226 | 10180 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     138 | 10181 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     138 | 10182 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      42 | 10183 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      44 | 10184 | `					return SXRET_OK;` |
|       - | 10185 | `				}` |
|      47 | 10186 | `			}` |
|      91 | 10187 | `		}` |
|     167 | 10188 | `	}` |
|       - | 10189 | `	/* Generate code for the right tree */` |
| 1096128 | 10190 | `	if( pNode->pRight ){` |
|  605782 | 10191 | `		if( iVmOp == PH7_OP_LAND ){` |
|       - | 10192 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    9230 | 10193 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  601168 | 10194 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|       - | 10195 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    3086 | 10196 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  595012 | 10197 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|       - | 10198 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      84 | 10199 | `			iVmOp = 0; /* No binary operator to emit */` |
|      84 | 10200 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  593478 | 10201 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|       - | 10202 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|       - | 10203 | `			 * the entire containing postfix chain to null. The jump target is` |
|       - | 10204 | `			 * patched later by the innermost non-chain ancestor (or by` |
|       - | 10205 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|       - | 10206 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|       - | 10207 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|     100 | 10208 | `			sxu32 nNsJmp = 0;` |
|     100 | 10209 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|     100 | 10210 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  593339 | 10211 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|  246054 | 10212 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|  123026 | 10213 | `		}` |
|  605782 | 10214 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  605782 | 10215 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  605782 | 10216 | `		if( !bIsChainOp ){` |
|       - | 10217 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|       - | 10218 | `			 * operator instruction is emitted. */` |
|  445452 | 10219 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|  222725 | 10220 | `		}` |
|  605782 | 10221 | `		if( iVmOp == PH7_OP_STORE ){` |
|  242934 | 10222 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|  242908 | 10223 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|       - | 10224 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|       - | 10225 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|       - | 10226 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|       - | 10227 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|       - | 10228 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|       - | 10229 | `				 */` |
|      54 | 10230 | `				iVmOp = 0;` |
|  242908 | 10231 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|  242882 | 10232 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|       - | 10233 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   67826 | 10234 | `					iP2 = 1;` |
|   33914 | 10235 | `				}else{` |
|  175058 | 10236 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10237 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   29698 | 10238 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   29698 | 10239 | `						iP1 = pInstr->iP1;` |
|   14850 | 10240 | `					}else{` |
|  145362 | 10241 | `						p3 = pInstr->p3;` |
|       - | 10242 | `					}` |
|       - | 10243 | `					/* POP the last dynamic load instruction */` |
|  175058 | 10244 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       - | 10245 | `				}` |
|  121442 | 10246 | `			}` |
|  484316 | 10247 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|      48 | 10248 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      48 | 10249 | `			if( pInstr ){` |
|      48 | 10250 | `				if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|       - | 10251 | `					/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|       - | 10252 | `					 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|       - | 10253 | `					 */` |
|      15 | 10254 | `					iVmOp = PH7_OP_STORE_IDX_REF;` |
|      15 | 10255 | `					iP1 = pInstr->iP1;` |
|      15 | 10256 | `					iP2 = pInstr->iP2;` |
|      15 | 10257 | `					p3  = pInstr->p3;` |
|       8 | 10258 | `				}else{` |
|      34 | 10259 | `					p3 = pInstr->p3;` |
|       - | 10260 | `				}` |
|      23 | 10261 | `			}` |
|      23 | 10262 | `		}` |
|  302890 | 10263 | `	}` |
| 1096128 | 10264 | `	if( iVmOp > 0 ){` |
| 1095964 | 10265 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|   12006 | 10266 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|       - | 10267 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|    8818 | 10268 | `				iP1 = 1;` |
|    4410 | 10269 | `			}` |
| 1089962 | 10270 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|       - | 10271 | `			/* Namespace-qualify the class name for NEW */ {` |
|   15482 | 10272 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   15482 | 10273 | `				VmInstr *pCallInstr = 0;` |
|   15482 | 10274 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   15466 | 10275 | `					pCallInstr = pPeek;` |
|   15466 | 10276 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    7732 | 10277 | `				}` |
|   15482 | 10278 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   15480 | 10279 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       - | 10280 | `					sxu32 nLitForClass;` |
|       - | 10281 | `					/* If the CALL handler already qualified the name using` |
|       - | 10282 | `					 * function imports, recover the original unqualified` |
|       - | 10283 | `					 * literal so we can re-qualify with class imports. */` |
|   15480 | 10284 | `					if( pCallInstr && pCallInstr->iP2 > 0 ){` |
|      34 | 10285 | `						nLitForClass = (sxu32)(pCallInstr->iP2 - 1); /* undo +1 encoding */` |
|      18 | 10286 | `					}else{` |
|   15448 | 10287 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|       - | 10288 | `					}` |
|   15480 | 10289 | `					pPeek->iP1 = 0;` |
|   15480 | 10290 | `					if( !bAbsolute ){` |
|   15464 | 10291 | `						pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|    7733 | 10292 | `					}else{` |
|      18 | 10293 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|       - | 10294 | `					}` |
|    7739 | 10295 | `				}` |
|       - | 10296 | `			}` |
|   15482 | 10297 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   15482 | 10298 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|       - | 10299 | `				VmInstr *pPrev;` |
|   15466 | 10300 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   15466 | 10301 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|       - | 10302 | `					/* Pop the call instruction, preserve named-arg map */` |
|   15466 | 10303 | `					iP1 = pInstr->iP1;` |
|   15466 | 10304 | `					if( pInstr->p3 ){` |
|      38 | 10305 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|      18 | 10306 | `					}` |
|   15466 | 10307 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    7732 | 10308 | `				}` |
|    7734 | 10309 | `			}` |
| 1076220 | 10310 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|       - | 10311 | `			/* instanceof: right operand is a class name, not a constant.` |
|       - | 10312 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|      88 | 10313 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      88 | 10314 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      88 | 10315 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      88 | 10316 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|      88 | 10317 | `				int isSpecialIs = 0;` |
|      88 | 10318 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|      84 | 10319 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|      84 | 10320 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|      87 | 10321 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      79 | 10322 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      42 | 10323 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      11 | 10324 | `						isSpecialIs = 1;` |
|       5 | 10325 | `					}` |
|      42 | 10326 | `				}` |
|      90 | 10327 | `				pInstr->iP1 = 0;` |
|      90 | 10328 | `				if( !isSpecialIs && !bAbsolute ){` |
|      68 | 10329 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      33 | 10330 | `				}` |
|      44 | 10331 | `			}` |
| 1068440 | 10332 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|       - | 10333 | `			/* Prevent constant expansion for member/property names.` |
|       - | 10334 | `			 * The right child (member name) was just compiled — its LOADC` |
|       - | 10335 | `			 * should not trigger constant lookup. */` |
|  160332 | 10336 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|  160332 | 10337 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|  160294 | 10338 | `				pInstr->iP1 = 0;` |
|   80146 | 10339 | `			}` |
|  160332 | 10340 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|       - | 10341 | `				/* Static member access,remember that */` |
|     192 | 10342 | `				iP1 = 1;` |
|     192 | 10343 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     192 | 10344 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      32 | 10345 | `					p3 = pInstr->p3;` |
|      32 | 10346 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      15 | 10347 | `				}` |
|      95 | 10348 | `			}` |
|   80165 | 10349 | `		}` |
|       - | 10350 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|       - | 10351 | `		 * This is the primary emit path for user-visible calls. */` |
| 1095962 | 10352 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|  363580 | 10353 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|  181789 | 10354 | `		}` |
|       - | 10355 | `		/* Finally,emit the VM instruction associated with this operator */` |
| 1095962 | 10356 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  547980 | 10357 | `	}` |
| 1096126 | 10358 | `	if( nJmpIdx > 0 ){` |
|       - | 10359 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   12396 | 10360 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   12396 | 10361 | `		if( pInstr ){` |
|   12396 | 10362 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    6197 | 10363 | `		}` |
|    6197 | 10364 | `	}` |
| 1096126 | 10365 | `	return rc;` |
| 1442574 | 10366 |  |
|       - | 10367 | `/*` |
|       - | 10368 | ` * Compile a PHP expression.` |
|       - | 10369 | ` * According to the PHP language reference manual:` |
|       - | 10370 | ` *  Expressions are the most important building stones of PHP.` |
|       - | 10371 | ` *  In PHP, almost anything you write is an expression.` |
|       - | 10372 | ` *  The simplest yet most accurate way to define an expression` |
|       - | 10373 | ` *  is "anything that has a value".` |
|       - | 10374 | ` * If something goes wrong while compiling the expression,this` |
|       - | 10375 | ` * function takes care of generating the appropriate error` |
|       - | 10376 | ` * message.` |
|       - | 10377 | ` */` |
|  775530 | 10378 | `static sxi32 PH7_CompileExpr(` |
|       - | 10379 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10380 | `	sxi32 iFlags,        /* Control flags */` |
|       - | 10381 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|       - | 10382 | `	)` |
|       2 | 10383 |  |
|       - | 10384 | `	ph7_expr_node *pRoot;` |
|       - | 10385 | `	SySet sExprNode;` |
|       - | 10386 | `	SyToken *pEnd;` |
|       - | 10387 | `	sxi32 nExpr;` |
|       - | 10388 | `	sxi32 iNest;` |
|       - | 10389 | `	sxi32 rc;` |
|       - | 10390 | `	sxu32 nNullsafeBase;` |
|       - | 10391 | `	/* Initialize worker variables */` |
|  775532 | 10392 | `	nExpr = 0;` |
|  775532 | 10393 | `	pRoot = 0;` |
|       - | 10394 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|       - | 10395 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  775532 | 10396 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  775532 | 10397 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  775532 | 10398 | `	SySetAlloc(&sExprNode,0x10);` |
|  775532 | 10399 | `	rc = SXRET_OK;` |
|       - | 10400 | `	/* Delimit the expression */` |
|  775532 | 10401 | `	pEnd = pGen->pIn;` |
|  775532 | 10402 | `	iNest = 0;` |
| 5190278 | 10403 | `	while( pEnd < pGen->pEnd ){` |
| 4926596 | 10404 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10405 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     330 | 10406 | `			iNest++;` |
| 4926432 | 10407 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     338 | 10408 | `			iNest--;` |
| 4926100 | 10409 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  512064 | 10410 | `			if( iNest <= 0 ){` |
|  511850 | 10411 | `				break;` |
|       - | 10412 | `			}` |
|     107 | 10413 | `		}` |
| 4414748 | 10414 | `		pEnd++;` |
|       2 | 10415 | `	}` |
|  775532 | 10416 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|   17976 | 10417 | `		SyToken *pEnd2 = pGen->pIn;` |
|   17976 | 10418 | `		iNest = 0;` |
|       - | 10419 | `		/* Stop at the first comma */` |
|   36000 | 10420 | `		while( pEnd2 < pEnd ){` |
|   18030 | 10421 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      16 | 10422 | `				iNest++;` |
|   18023 | 10423 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      16 | 10424 | `				iNest--;` |
|   18009 | 10425 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      13 | 10426 | `				if( iNest <= 0 ){` |
|       5 | 10427 | `					break;` |
|       - | 10428 | `				}` |
|       4 | 10429 | `			}` |
|   18026 | 10430 | `			pEnd2++;` |
|       2 | 10431 | `		}` |
|   17976 | 10432 | `		if( pEnd2 <pEnd ){` |
|       5 | 10433 | `			pEnd = pEnd2;` |
|       2 | 10434 | `		}` |
|    8987 | 10435 | `	}` |
|  775532 | 10436 | `	if( pEnd > pGen->pIn ){` |
|  775522 | 10437 | `		SyToken *pTmp = pGen->pEnd;` |
|       - | 10438 | `		/* Swap delimiter */` |
|  775522 | 10439 | `		pGen->pEnd = pEnd;` |
|       - | 10440 | `		/* Try to get an expression tree */` |
|  775522 | 10441 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  775522 | 10442 | `		if( rc == SXRET_OK && pRoot ){` |
|  775340 | 10443 | `			rc = SXRET_OK;` |
|  775340 | 10444 | `			if( xTreeValidator ){` |
|       - | 10445 | `				/* Call the upper layer validator callback */` |
|   21830 | 10446 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|   10914 | 10447 | `			}` |
|  775340 | 10448 | `			if( rc != SXERR_ABORT ){` |
|       - | 10449 | `				/* Generate code for the given tree */` |
|  775340 | 10450 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|       - | 10451 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|       - | 10452 | `				 * expression so they short-circuit to its end. */` |
|  775340 | 10453 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  387669 | 10454 | `			}` |
|  775340 | 10455 | `			nExpr = 1;` |
|  387669 | 10456 | `		}` |
|       - | 10457 | `		/* Release the whole tree */` |
|  775522 | 10458 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|       - | 10459 | `		/* Synchronize token stream */` |
|  775522 | 10460 | `		pGen->pEnd = pTmp;` |
|  775522 | 10461 | `		pGen->pIn  = pEnd;` |
|  775522 | 10462 | `		if( rc == SXERR_ABORT ){` |
|      11 | 10463 | `			SySetRelease(&sExprNode);` |
|      11 | 10464 | `			return SXERR_ABORT;` |
|       - | 10465 | `		}` |
|  387755 | 10466 | `	}` |
|  775522 | 10467 | `	SySetRelease(&sExprNode);` |
|  775522 | 10468 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  387767 | 10469 |  |
|       - | 10470 | `/*` |
|       - | 10471 | ` * Return a pointer to the node construct handler associated` |
|       - | 10472 | ` * with a given node type [i.e: string,integer,float,...].` |
|       - | 10473 | ` */` |
|  195022 | 10474 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|       2 | 10475 |  |
|  195024 | 10476 | `	if( nNodeType & PH7_TK_NUM ){` |
|       - | 10477 | `		/* Numeric literal: Either real or integer */` |
|  103264 | 10478 | `		return PH7_CompileNumLiteral;` |
|   91762 | 10479 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|       - | 10480 | `		/* Double quoted string */` |
|   17186 | 10481 | `		return PH7_CompileString;` |
|   74578 | 10482 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|       - | 10483 | `		/* Single quoted string */` |
|   74466 | 10484 | `		return PH7_CompileSimpleString;` |
|     114 | 10485 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|       - | 10486 | `		/* Heredoc */` |
|      66 | 10487 | `		return PH7_CompileHereDoc;` |
|      50 | 10488 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|       - | 10489 | `		/* Nowdoc */` |
|      44 | 10490 | `		return PH7_CompileNowDoc;` |
|       7 | 10491 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|       - | 10492 | `		/* Backtick quoted string */` |
|       5 | 10493 | `		return PH7_CompileBacktic;` |
|       - | 10494 | `	}` |
|       3 | 10495 | `	return 0;` |
|   97513 | 10496 |  |
|       - | 10497 | `/*` |
|       - | 10498 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|       - | 10499 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|       - | 10500 | ` * in write context" parse error.` |
|       - | 10501 | ` */` |
|    6454 | 10502 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|       2 | 10503 |  |
|       - | 10504 | `	sxi32 rc;` |
|    6456 | 10505 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|    6454 | 10506 | `		return SXRET_OK;` |
|       - | 10507 | `	}` |
|       5 | 10508 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|       2 | 10509 | `		pNode ? pNode->pStart->nLine : 1,` |
|       - | 10510 | `		"Can't use nullsafe operator in write context");` |
|       3 | 10511 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|    3229 | 10512 |  |
|       - | 10513 | `/*` |
|       - | 10514 | ` * Compile an unset() statement.` |
|       - | 10515 | ` * unset($var, $arr[$key], ...);` |
|       - | 10516 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|       - | 10517 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|       - | 10518 | ` * parent array before extracting the element to unset.` |
|       - | 10519 | ` */` |
|    2798 | 10520 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|       2 | 10521 |  |
|    2800 | 10522 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|    2800 | 10523 | `	sxu32 nIdx = 0;` |
|       - | 10524 | `	SyString sName;` |
|       - | 10525 | `	sxi32 rc;` |
|       - | 10526 | `	/* Jump the 'unset' keyword */` |
|    2800 | 10527 | `	pGen->pIn++;` |
|       - | 10528 | `	/* Save delimiter */` |
|    2800 | 10529 | `	pTmp = pGen->pEnd;` |
|       - | 10530 | `	/* Skip optional opening parenthesis and find the matching close */` |
|    2800 | 10531 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|    2800 | 10532 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|       - | 10533 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|       - | 10534 | `		SyToken *pClose;` |
|    2800 | 10535 | `		pGen->pIn++;   /* Skip '(' */` |
|    2800 | 10536 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|    2800 | 10537 | `		pEnd = pClose; /* Stop at ')' */` |
|    1399 | 10538 | `	}` |
|    2800 | 10539 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|       - | 10540 | `	/* Resolve the 'unset' builtin name once */` |
|    2800 | 10541 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|     336 | 10542 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     336 | 10543 | `		if( pObj == 0 ){` |
|     ! 0 | 10544 | `			return SXERR_ABORT;` |
|       - | 10545 | `		}` |
|     336 | 10546 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|     336 | 10547 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     167 | 10548 | `	}` |
|       - | 10549 | `	/* Compile each comma-separated argument */` |
|    9256 | 10550 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|    6458 | 10551 | `		if( pGen->pIn < pNext ){` |
|    6458 | 10552 | `			pGen->pEnd = pNext;` |
|    6458 | 10553 | `			rc = PH7_CompileExpr(&(*pGen),` |
|       - | 10554 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_STORE,` |
|       - | 10555 | `				GenStateUnsetValidator);` |
|    6458 | 10556 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10557 | `				return SXERR_ABORT;` |
|       - | 10558 | `			}` |
|    6458 | 10559 | `			if( rc != SXERR_EMPTY ){` |
|       - | 10560 | `				/* Emit call for this single argument */` |
|    6456 | 10561 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|    6456 | 10562 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|    6456 | 10563 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    3227 | 10564 | `			}` |
|    3228 | 10565 | `		}` |
|       - | 10566 | `		/* Jump trailing commas */` |
|   10118 | 10567 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|    3662 | 10568 | `			pNext++;` |
|       2 | 10569 | `		}` |
|    6458 | 10570 | `		pGen->pIn = pNext;` |
|       2 | 10571 | `	}` |
|       - | 10572 | `	/* Skip past the closing ')' if present */` |
|    2800 | 10573 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|    2800 | 10574 | `		pGen->pIn++;` |
|    1399 | 10575 | `	}` |
|       - | 10576 | `	/* Restore token stream */` |
|    2800 | 10577 | `	pGen->pEnd = pTmp;` |
|    2800 | 10578 | `	return SXRET_OK;` |
|    1401 | 10579 |  |
|       - | 10580 | `/*` |
|       - | 10581 | ` * PHP Language construct table.` |
|       - | 10582 | ` */` |
|       - | 10583 | `static const LangConstruct aLangConstruct[] = {` |
|       - | 10584 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|       - | 10585 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|       - | 10586 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|       - | 10587 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|       - | 10588 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|       - | 10589 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|       - | 10590 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|       - | 10591 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|       - | 10592 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|       - | 10593 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|       - | 10594 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|       - | 10595 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|       - | 10596 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|       - | 10597 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|       - | 10598 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|       - | 10599 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|       - | 10600 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|       - | 10601 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|       - | 10602 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|       - | 10603 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|       - | 10604 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|       - | 10605 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|       - | 10606 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|       - | 10607 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|       - | 10608 | `};` |
|       - | 10609 | `/*` |
|       - | 10610 | ` * Return a pointer to the statement handler routine associated` |
|       - | 10611 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|       - | 10612 | ` */` |
|  465048 | 10613 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|       - | 10614 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|       - | 10615 | `	SyToken *pLookahed  /* Look-ahead token */` |
|       - | 10616 | `	)` |
|       2 | 10617 |  |
|  465050 | 10618 | `	sxu32 n = 0;` |
| 1970770 | 10619 | `	for(;;){` |
| 3941542 | 10620 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|   53844 | 10621 | `			break;` |
|       - | 10622 | `		}` |
| 3887700 | 10623 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  411208 | 10624 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|     ! 0 | 10625 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|     ! 0 | 10626 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|       - | 10627 | `					/* 'static' (class context),return null */` |
|     ! 0 | 10628 | `					return 0;` |
|       - | 10629 | `				}` |
|     ! 0 | 10630 | `			}` |
|  411206 | 10631 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       2 | 10632 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       3 | 10633 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|       - | 10634 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|     ! 0 | 10635 | `				return 0;` |
|       - | 10636 | `			}` |
|       - | 10637 | `			/* Return a pointer to the handler.` |
|       - | 10638 | `			*/` |
|  411208 | 10639 | `			return aLangConstruct[n].xConstruct;` |
|       - | 10640 | `		}` |
| 3476494 | 10641 | `		n++;` |
|       2 | 10642 | `	}` |
|   53844 | 10643 | `	if( pLookahed ){` |
|   53844 | 10644 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|   11788 | 10645 | `			return PH7_CompileClassInterface;` |
|   42058 | 10646 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|   41844 | 10647 | `			return PH7_CompileClass;` |
|     216 | 10648 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      56 | 10649 | `			return PH7_CompileTrait;` |
|     160 | 10650 | `		}else if( nKeywordID == PH7_TKWRD_ABSTRACT && (pLookahed->nType & PH7_TK_KEYWORD)` |
|      21 | 10651 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|      20 | 10652 | `				return PH7_CompileAbstractClass;` |
|     142 | 10653 | `		}else if( nKeywordID == PH7_TKWRD_FINAL && (pLookahed->nType & PH7_TK_KEYWORD)` |
|       5 | 10654 | `			&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_CLASS ){` |
|       3 | 10655 | `				return PH7_CompileFinalClass;` |
|       - | 10656 | `		}` |
|      70 | 10657 | `	}` |
|       - | 10658 | `	/* Not a language construct */` |
|     142 | 10659 | `	return 0;` |
|  232526 | 10660 |  |
|       - | 10661 | `/*` |
|       - | 10662 | ` * Check if the given keyword is in fact a PHP language construct.` |
|       - | 10663 | ` * Return TRUE on success. FALSE otheriwse.` |
|       - | 10664 | ` */` |
|     140 | 10665 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|       2 | 10666 |  |
|       - | 10667 | `	int rc;` |
|     142 | 10668 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     142 | 10669 | `	if( rc == FALSE ){` |
|      44 | 10670 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      40 | 10671 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|       - | 10672 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|       - | 10673 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|       - | 10674 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|       - | 10675 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|       - | 10676 | `			*/` |
|       - | 10677 | `			){` |
|      38 | 10678 | `				rc = TRUE;` |
|      18 | 10679 | `		}` |
|      22 | 10680 | `	}` |
|     142 | 10681 | `	return rc;` |
|       2 | 10682 |  |
|       - | 10683 | `/*` |
|       - | 10684 | ` * Compile a PHP chunk.` |
|       - | 10685 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10686 | ` * takes care of generating the appropriate error message.` |
|       - | 10687 | ` */` |
|  627842 | 10688 | `static sxi32 GenStateCompileChunk(` |
|       - | 10689 | `	ph7_gen_state *pGen, /* Code generator state */` |
|       - | 10690 | `	sxi32 iFlags         /* Compile flags */` |
|       - | 10691 | `	)` |
|       2 | 10692 |  |
|       - | 10693 | `	ProcLangConstruct xCons;` |
|       - | 10694 | `	sxi32 rc;` |
|  627844 | 10695 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  430251 | 10696 | `	for(;;){` |
|  744174 | 10697 | `		int bStmtIsDeclare = 0;` |
|  744174 | 10698 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 10699 | `			/* No more input to process */` |
|   12472 | 10700 | `			break;` |
|       - | 10701 | `		}` |
|       - | 10702 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|       - | 10703 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  731704 | 10704 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  465050 | 10705 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  465050 | 10706 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|      40 | 10707 | `				bStmtIsDeclare = 1;` |
|      19 | 10708 | `			}` |
|  232524 | 10709 | `		}` |
|  731704 | 10710 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|       - | 10711 | `			/* Any non-declare top-level statement locks the strict_types` |
|       - | 10712 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|  116304 | 10713 | `			pGen->bStrictTypesLocked = 1;` |
|   58151 | 10714 | `		}` |
|  731704 | 10715 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|       - | 10716 | `			/* Compile block */` |
|      18 | 10717 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      18 | 10718 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 | 10719 | `				break;` |
|       - | 10720 | `			}` |
|      10 | 10721 | `		}else{` |
|  731688 | 10722 | `			xCons = 0;` |
|  731688 | 10723 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  465050 | 10724 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       - | 10725 | `				/* Try to extract a language construct handler */` |
|  465050 | 10726 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  465050 | 10727 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|      13 | 10728 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|       - | 10729 | `						"Syntax error: Unexpected keyword '%z'",` |
|       8 | 10730 | `						&pGen->pIn->sData);` |
|       9 | 10731 | `					if( rc == SXERR_ABORT ){` |
|     ! 0 | 10732 | `						break;` |
|       - | 10733 | `					}` |
|       - | 10734 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|       - | 10735 | `					 * this erroneous statement.` |
|       - | 10736 | `					 */` |
|       9 | 10737 | `					xCons = PH7_ErrorRecover;` |
|       4 | 10738 | `				}` |
|  499164 | 10739 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   43616 | 10740 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|       - | 10741 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|     114 | 10742 | `				xCons = PH7_CompileLabel;` |
|      56 | 10743 | `			}` |
|  731688 | 10744 | `			if( xCons == 0 ){` |
|       - | 10745 | `				/* Assume an expression an try to compile it */` |
|  266660 | 10746 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|  266660 | 10747 | `				if(  rc != SXERR_EMPTY ){` |
|       - | 10748 | `					/* Pop l-value */` |
|  266510 | 10749 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|  133254 | 10750 | `				}` |
|  133331 | 10751 | `			}else{` |
|       - | 10752 | `				/* Go compile the sucker */` |
|  465030 | 10753 | `				rc = xCons(&(*pGen));` |
|       - | 10754 | `			}` |
|  731688 | 10755 | `			if( rc == SXERR_ABORT ){` |
|       - | 10756 | `				/* Request to abort compilation */` |
|      11 | 10757 | `				break;` |
|       - | 10758 | `			}` |
|       - | 10759 | `		}` |
|       - | 10760 | `		/* Ignore trailing semi-colons ';' */` |
| 1220068 | 10761 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  488376 | 10762 | `			pGen->pIn++;` |
|       2 | 10763 | `		}` |
|  731694 | 10764 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|       - | 10765 | `			/* Compile a single statement and return */` |
|  615364 | 10766 | `			break;` |
|       - | 10767 | `		}` |
|       - | 10768 | `		/* LOOP ONE */` |
|       - | 10769 | `		/* LOOP TWO */` |
|       - | 10770 | `		/* LOOP THREE */` |
|       - | 10771 | `		/* LOOP FOUR */` |
|       2 | 10772 | `	}` |
|       - | 10773 | `	/* Return compilation status */` |
|  627844 | 10774 | `	return rc;` |
|       2 | 10775 |  |
|       - | 10776 | `/*` |
|       - | 10777 | ` * Compile a Raw PHP chunk.` |
|       - | 10778 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|       - | 10779 | ` * takes care of generating the appropriate error message.` |
|       - | 10780 | ` */` |
|   12482 | 10781 | `static sxi32 PH7_CompilePHP(` |
|       - | 10782 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|       - | 10783 | `	SySet *pTokenSet,     /* Token set */` |
|       - | 10784 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|       - | 10785 | `	)` |
|       2 | 10786 |  |
|   12484 | 10787 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|       - | 10788 | `	sxi32 rc;` |
|       - | 10789 | `	/* Reset the token set */` |
|   12484 | 10790 | `	SySetReset(&(*pTokenSet));` |
|       - | 10791 | `	/* Mark as the default token set */` |
|   12484 | 10792 | `	pGen->pTokenSet = &(*pTokenSet);` |
|       - | 10793 | `	/* Advance the stream cursor */` |
|   12484 | 10794 | `	pGen->pRawIn++;` |
|       - | 10795 | `	/* Tokenize the PHP chunk first */` |
|   12484 | 10796 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet));` |
|       - | 10797 | `	/* Point to the head and tail of the token stream. */` |
|   12484 | 10798 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|   12484 | 10799 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|   12484 | 10800 | `	if( is_expr ){` |
|     ! 0 | 10801 | `		rc = SXERR_EMPTY;` |
|     ! 0 | 10802 | `		if( pGen->pIn < pGen->pEnd ){` |
|       - | 10803 | `			/* A simple expression,compile it */` |
|     ! 0 | 10804 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|     ! 0 | 10805 | `		}` |
|       - | 10806 | `		/* Emit the DONE instruction */` |
|     ! 0 | 10807 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     ! 0 | 10808 | `		return SXRET_OK;` |
|       - | 10809 | `	}` |
|   12484 | 10810 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|       - | 10811 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|       - | 10812 | `		/*` |
|       - | 10813 | `		 * Shortcut syntax for the 'echo' language construct.` |
|       - | 10814 | `		 * According to the PHP reference manual:` |
|       - | 10815 | `		 *  echo() also has a shortcut syntax, where you can` |
|       - | 10816 | `		 *  immediately follow` |
|       - | 10817 | `		 *  the opening tag with an equals sign as follows:` |
|       - | 10818 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|       - | 10819 | `		 * Symisc extension:` |
|       - | 10820 | `		 *   This short syntax works with all PHP opening` |
|       - | 10821 | `		 *   tags unlike the default PHP engine that handle` |
|       - | 10822 | `		 *   only short tag.` |
|       - | 10823 | `		 */` |
|       - | 10824 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|       3 | 10825 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|       3 | 10826 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|       3 | 10827 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|       3 | 10828 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|       3 | 10829 | `		if( rc != SXERR_EMPTY ){` |
|       3 | 10830 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       1 | 10831 | `		}` |
|       3 | 10832 | `		return SXRET_OK;` |
|       - | 10833 | `	}` |
|       - | 10834 | `	/* Compile the PHP chunk */` |
|   12482 | 10835 | `	rc = GenStateCompileChunk(pGen,0);` |
|       - | 10836 | `	/* Fix exceptions jumps */` |
|   12482 | 10837 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|       - | 10838 | `	/* Fix gotos now, the jump destination is resolved */` |
|   12482 | 10839 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|       3 | 10840 | `		rc = SXERR_ABORT;` |
|       1 | 10841 | `	}` |
|       - | 10842 | `	/* Reset container */` |
|   12482 | 10843 | `	SySetReset(&pGen->aGoto);` |
|   12482 | 10844 | `	SySetReset(&pGen->aLabel);` |
|   12482 | 10845 | `	SySetReset(&pGen->aNullsafeJmp);` |
|       - | 10846 | `	/* Compilation result */` |
|   12482 | 10847 | `	return rc;` |
|    6243 | 10848 |  |
|       - | 10849 | `/*` |
|       - | 10850 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|       - | 10851 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|       - | 10852 | ` * This is the only compile interface exported from this file.` |
|       - | 10853 | ` */` |
|   14822 | 10854 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|       - | 10855 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|       - | 10856 | `	SyString *pScript,  /* Script to compile */` |
|       - | 10857 | `	sxi32 iFlags        /* Compile flags */` |
|       - | 10858 | `	)` |
|       2 | 10859 |  |
|       - | 10860 | `	SySet aPhpToken,aRawToken;` |
|       - | 10861 | `	ph7_gen_state *pCodeGen;` |
|       - | 10862 | `	ph7_value *pRawObj;` |
|       - | 10863 | `	sxu32 nObjIdx;` |
|       - | 10864 | `	sxi32 nRawObj;` |
|       - | 10865 | `	int is_expr;` |
|       - | 10866 | `	sxi8 bSavedStrict;` |
|       - | 10867 | `	sxi8 bSavedStrictLocked;` |
|       - | 10868 | `	sxi32 rc;` |
|   14824 | 10869 | `	if( pScript->nByte < 1 ){` |
|       - | 10870 | `		/* Nothing to compile */` |
|     ! 0 | 10871 | `		return PH7_OK;` |
|       - | 10872 | `	}` |
|       - | 10873 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|       - | 10874 | `	 * file's flags so include/require restore them on return. */` |
|   14824 | 10875 | `	pCodeGen = &pVm->sCodeGen;` |
|   14824 | 10876 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|   14824 | 10877 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|   14824 | 10878 | `	pCodeGen->bStrictTypes = 0;` |
|   14824 | 10879 | `	pCodeGen->bStrictTypesLocked = 0;` |
|       - | 10880 | `	/* Initialize the tokens containers */` |
|   14824 | 10881 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14824 | 10882 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|   14824 | 10883 | `	SySetAlloc(&aPhpToken,0xc0);` |
|   14824 | 10884 | `	is_expr = 0;` |
|   14824 | 10885 | `	if( iFlags & PH7_PHP_ONLY ){` |
|       - | 10886 | `		SyToken sTmp;` |
|       - | 10887 | `		/* PHP only: -*/` |
|    2966 | 10888 | `		sTmp.nLine = 1;` |
|    2966 | 10889 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    2966 | 10890 | `		sTmp.pUserData = 0;` |
|    2966 | 10891 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    2966 | 10892 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    2966 | 10893 | `		if( iFlags & PH7_PHP_EXPR ){` |
|       - | 10894 | `			/* A simple PHP expression */` |
|     ! 0 | 10895 | `			is_expr = 1;` |
|     ! 0 | 10896 | `		}` |
|    1484 | 10897 | `	}else{` |
|       - | 10898 | `		/* Tokenize raw text */` |
|   11860 | 10899 | `		SySetAlloc(&aRawToken,32);` |
|   11860 | 10900 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken);` |
|       - | 10901 | `	}` |
|       - | 10902 | `	/* Process high-level tokens */` |
|   14824 | 10903 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|   14824 | 10904 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|   14824 | 10905 | `	rc = PH7_OK;` |
|   14824 | 10906 | `	if( is_expr ){` |
|       - | 10907 | `		/* Compile the expression */` |
|     ! 0 | 10908 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|     ! 0 | 10909 | `		goto cleanup;` |
|       - | 10910 | `	}` |
|   14824 | 10911 | `	nObjIdx = 0;` |
|       - | 10912 | `	/* Each compilation unit starts in the global namespace.` |
|       - | 10913 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|       - | 10914 | `	 * preventing namespace bleeding across include()d files. */` |
|   14824 | 10915 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|       - | 10916 | `	/* Start the compilation process */` |
|   13344 | 10917 | `	for(;;){` |
|   39160 | 10918 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|   14812 | 10919 | `			break; /* No more tokens to process */` |
|       - | 10920 | `		}` |
|   24350 | 10921 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|       - | 10922 | `			/* Compile the PHP chunk */` |
|   12484 | 10923 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|   12484 | 10924 | `			if( rc == SXERR_ABORT ){` |
|      13 | 10925 | `				break;` |
|       - | 10926 | `			}` |
|   12472 | 10927 | `			continue;` |
|       - | 10928 | `		}` |
|       - | 10929 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|   11868 | 10930 | `		nRawObj = 0;` |
|   23734 | 10931 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|       - | 10932 | `			/* Consume the raw chunk without any processing */` |
|   11868 | 10933 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|   11868 | 10934 | `			if( pRawObj == 0 ){` |
|     ! 0 | 10935 | `				rc = SXERR_MEM;` |
|     ! 0 | 10936 | `				break;` |
|       - | 10937 | `			}` |
|       - | 10938 | `			/* Mark as constant and emit the load constant instruction */` |
|   11868 | 10939 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|   11868 | 10940 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|   11868 | 10941 | `			++nRawObj;` |
|   11868 | 10942 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|       2 | 10943 | `		}` |
|   11868 | 10944 | `		if( nRawObj > 0 ){` |
|       - | 10945 | `			/* Emit the consume instruction */` |
|   11868 | 10946 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|    5933 | 10947 | `		}` |
|    7413 | 10948 | `	}` |
|    7411 | 10949 | `cleanup:` |
|   14824 | 10950 | `	SySetRelease(&aRawToken);` |
|   14824 | 10951 | `	SySetRelease(&aPhpToken);` |
|       - | 10952 | `	/* Restore outer file's strict_types scope */` |
|   14824 | 10953 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|   14824 | 10954 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|   14824 | 10955 | `	return rc;` |
|    7413 | 10956 |  |
|       - | 10957 | `/*` |
|       - | 10958 | ` * Utility routines.Initialize the code generator.` |
|       - | 10959 | ` */` |
|    2936 | 10960 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|       - | 10961 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10962 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10963 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 10964 | `	)` |
|       2 | 10965 |  |
|    2938 | 10966 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 10967 | `	/* Zero the structure */` |
|    2938 | 10968 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|       - | 10969 | `	/* Initial state */` |
|    2938 | 10970 | `	pGen->pVm  = &(*pVm);` |
|    2938 | 10971 | `	pGen->xErr = xErr;` |
|    2938 | 10972 | `	pGen->pErrData = pErrData;` |
|    2938 | 10973 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|    2938 | 10974 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|    2938 | 10975 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|    2938 | 10976 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|    2938 | 10977 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|       - | 10978 | `	/* Error log buffer */` |
|    2938 | 10979 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|       - | 10980 | `	/* General purpose working buffer */` |
|    2938 | 10981 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|       - | 10982 | `	/* Namespace state */` |
|    2938 | 10983 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    2938 | 10984 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    2938 | 10985 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    2938 | 10986 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 10987 | `	/* Create the global scope */` |
|    2938 | 10988 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|       - | 10989 | `	/* Point to the global scope */` |
|    2938 | 10990 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    2938 | 10991 | `	return SXRET_OK;` |
|       2 | 10992 |  |
|       - | 10993 | `/*` |
|       - | 10994 | ` * Utility routines. Reset the code generator to it's initial state.` |
|       - | 10995 | ` */` |
|   17444 | 10996 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|       - | 10997 | `	ph7_vm *pVm,       /* Target VM */` |
|       - | 10998 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|       - | 10999 | `	void *pErrData     /* Last argument to xErr() */` |
|       - | 11000 | `	)` |
|       2 | 11001 |  |
|   17446 | 11002 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|       - | 11003 | `	GenBlock *pBlock,*pParent;` |
|       - | 11004 | `	/* Reset state */` |
|   17446 | 11005 | `	SySetReset(&pGen->aLabel);` |
|   17446 | 11006 | `	SySetReset(&pGen->aGoto);` |
|   17446 | 11007 | `	SySetReset(&pGen->aNullsafeJmp);` |
|   17446 | 11008 | `	SyBlobRelease(&pGen->sErrBuf);` |
|   17446 | 11009 | `	SyBlobRelease(&pGen->sWorker);` |
|   17446 | 11010 | `	SyBlobRelease(&pGen->sNamespace);` |
|   17446 | 11011 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|   17446 | 11012 | `	SyHashRelease(&pGen->hUseImports);` |
|   17446 | 11013 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|   17446 | 11014 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   17446 | 11015 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|   17446 | 11016 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   17446 | 11017 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|       - | 11018 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|       - | 11019 | `	 * They intern variable names and literal strings that are referenced by` |
|       - | 11020 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|       - | 11021 | `	 * Releasing them would either leak the interned strings or require freeing` |
|       - | 11022 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|       - | 11023 | `	 * number of unique names, which is acceptable. */` |
|       - | 11024 | `	/* Point to the global scope */` |
|   17446 | 11025 | `	pBlock = pGen->pCurrent;` |
|   17446 | 11026 | `	while( pBlock->pParent != 0 ){` |
|     ! 0 | 11027 | `		pParent = pBlock->pParent;` |
|     ! 0 | 11028 | `		GenStateFreeBlock(pBlock);` |
|     ! 0 | 11029 | `		pBlock = pParent;` |
|     ! 0 | 11030 | `	}` |
|   17446 | 11031 | `	pGen->xErr = xErr;` |
|   17446 | 11032 | `	pGen->pErrData = pErrData;` |
|   17446 | 11033 | `	pGen->pCurrent = &pGen->sGlobal;` |
|   17446 | 11034 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|   17446 | 11035 | `	pGen->pIn = pGen->pEnd = 0;` |
|   17446 | 11036 | `	pGen->nErr = 0;` |
|   17446 | 11037 | `	return SXRET_OK;` |
|       2 | 11038 |  |
|       - | 11039 | `/*` |
|       - | 11040 | ` * Generate a compile-time error message.` |
|       - | 11041 | ` * If the error count limit is reached (usually 15 error message)` |
|       - | 11042 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|       - | 11043 | ` * abort compilation immediately.` |
|       - | 11044 | ` */` |
|     570 | 11045 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|       2 | 11046 |  |
|     572 | 11047 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     572 | 11048 | `	const char *zErr = "Error";` |
|       - | 11049 | `	SyString *pFile;` |
|       - | 11050 | `	va_list ap;` |
|       - | 11051 | `	sxi32 rc;` |
|       - | 11052 | `	/* Reset the working buffer */` |
|     572 | 11053 | `	SyBlobReset(pWorker);` |
|       - | 11054 | `	/* Peek the processed file path if available */` |
|     572 | 11055 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     572 | 11056 | `	if( nErrType == E_ERROR ){` |
|       - | 11057 | `		/* Increment the error counter */` |
|     470 | 11058 | `		pGen->nErr++;` |
|     470 | 11059 | `		if( pGen->nErr > 15 ){` |
|       - | 11060 | `			/* Error count limit reached */` |
|       5 | 11061 | `			if( pGen->xErr ){` |
|       5 | 11062 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|       5 | 11063 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|       5 | 11064 | `				if( pFile ){` |
|       5 | 11065 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       2 | 11066 | `				}` |
|       5 | 11067 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       5 | 11068 | `				if( SyBlobLength(pWorker) > 0 ){` |
|       5 | 11069 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       2 | 11070 | `				}` |
|       2 | 11071 | `			}` |
|       - | 11072 | `			/* Abort immediately */` |
|       5 | 11073 | `			return SXERR_ABORT;` |
|       - | 11074 | `		}` |
|     232 | 11075 | `	}` |
|     568 | 11076 | `	if( pGen->xErr == 0 ){` |
|       - | 11077 | `		/* No available error consumer,return immediately */` |
|       3 | 11078 | `		return SXRET_OK;` |
|       - | 11079 | `	}` |
|     565 | 11080 | `	switch(nErrType){` |
|     463 | 11081 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|      27 | 11082 | `	case E_WARNING: zErr = "Warning";     break;` |
|      69 | 11083 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       9 | 11084 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|     ! 0 | 11085 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|     ! 0 | 11086 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|     ! 0 | 11087 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|     ! 0 | 11088 | `	default:` |
|     ! 0 | 11089 | `		break;` |
|       - | 11090 | `	}` |
|     565 | 11091 | `	rc = SXRET_OK;` |
|       - | 11092 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     565 | 11093 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     565 | 11094 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     565 | 11095 | `	va_start(ap,zFormat);` |
|     565 | 11096 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     565 | 11097 | `	va_end(ap);` |
|     565 | 11098 | `	if( pFile ){` |
|     565 | 11099 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|     282 | 11100 | `	}` |
|       - | 11101 | `	/* Append a new line */` |
|     565 | 11102 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     565 | 11103 | `	if( SyBlobLength(pWorker) > 0 ){` |
|       - | 11104 | `		/* Consume the generated error message */` |
|     565 | 11105 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|     282 | 11106 | `	}` |
|     565 | 11107 | `	return rc;` |
|     287 | 11108 |  |
|       - | 11109 |  |
